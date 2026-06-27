/*

    Amy - a chess playing program

    Copyright (c) 2002-2026, Thorsten Greiner
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.

*/

/*
 * commands.c - Command interpreter
 */

#include "amy.h"

#include <signal.h>
#include <string.h>

#include "bitboard.h"
#include "blunder.h"
#include "bookup.h"
#include "commands.h"
#include "dbase.h"
#include "eco.h"
#include "evaluation.h"
#include "evaluation_config.h"
#include "filter.h"
#include "heap.h"
#include "inline.h"
#include "pgn.h"
#include "search.h"
#include "state_machine.h"
#include "time_ctl.h"
#include "utils.h"

static void Quit(char *);
static void Show(char *);
static void ShowEco(char *);
static void Test(char *);
static void SetTime(char *);
static void SetXBoard(char *);
static void Go(char *);
static void Force(char *);
static void Name(char *);
static void MoveNow(char *);
static void Edit(char *);
static void Undo(char *);
static void Book(char *);
static void Post(char *);
static void NoPost(char *);
static void Easy(char *);
static void Hard(char *);
static void MovesCmd(char *);
static void SetEPD(char *);
static void Anno(char *);
static void ShowWarranty(char *);
static void ShowDistribution(char *);
static void Help(char *);
static void Benchmark(char *);
static void Perft(char *);
static void Load(char *);
static void Save(char *);
static void Prefs(char *);
static void Flatten(char *);
static void XboardTime(char *);
static void Analyze(char *);
static void StopAnalyze(char *);
static void SelfPlay(char *);
static void TestNext(char *);
static void Conf(char *);
static void SaveConf(char *);
static void ShowScore(char *);
static void TestScore(char *);
static void SetSearchDepth(char *);

// defined in search.c
extern unsigned long FHTime;
extern bool AbortSearch;

static struct SCommandEntry Commands[] = {
    {"analyze", &Analyze, false, false, "enter analyze mode (xboard)", NULL},
    {"anno", &Anno, false, false, "annotate a game", NULL},
    {"bench", &Benchmark, false, false, "run a benchmark", NULL},
    {"blunder", &BlunderCheck, false, false, "check for blunders", NULL},
    {"book", &Book, false, false, "display book moves", NULL},
    {"bk", &Book, false, false, "display book moves (xboard)", NULL},
    {"bookup", &Bookup, false, false, "create a book", NULL},
    {"conf", &Conf, false, false, "load scoring config", NULL},
    {"conf-save", &SaveConf, false, false, "save scoring config", NULL},
    {"d", &Show, true, false, "display current position", NULL},
    {"depth", &SetSearchDepth, false, false, "set maximum search depth", NULL},
    {"distribution", &ShowDistribution, true, false,
     "show terms of distribution", NULL},
    {"e", &ShowEco, false, false, "show ECO code", NULL},
    {"eco", &ParseEcoPgn, false, false, "create ECO database", NULL},
    {"easy", &Easy, true, false, "switch off permanent brain", NULL},
    {"epd", &SetEPD, false, false, "set position in EPD", NULL},
    {"edit", &Edit, false, false, "edit position (xboard!)", NULL},
    {"exit", &StopAnalyze, true, true, "exit analyze mode (xboard)", NULL},
    {"filter", &FilterQuiescentPositions, false, false,
     "filter quiescent positions", NULL},
    {"flatten", &Flatten, true, false, "flatten book", NULL},
    {"force", &Force, true, false, "switch force mode (xboard)", NULL},
    {"go", &Go, false, false, "start searching", NULL},
    {"hard", &Hard, true, false, "switch on permanent brain", NULL},
    {"help", &Help, true, false, "show help", NULL},
    {"level", &SetTime, false, false, "set time control", NULL},
    {"load", &Load, false, false, "load game from PGN file", NULL},
    {"moves", &MovesCmd, false, false, "show legal moves", NULL},
    {"name", &Name, true, false, "set the opponents name", NULL},
    {"new", &NewGame, true, true, "start new game", NULL},
    {"nopost", &NoPost, true, false, "switch off post mode (xboard)", NULL},
    {"perft", &Perft, false, false, "Run the perft benchmark", NULL},
    {"post", &Post, true, false, "switch on post mode (xboard)", NULL},
    {"prefs", &Prefs, false, false, "read opening book preferences", NULL},
    {"quit", &Quit, true, false, "quit Amy", NULL},
    {"s", &ShowScore, true, false, "display static evaluation", NULL},
    {"save", &Save, false, false, "save game to PGN file", NULL},
    {"self", &SelfPlay, false, false, "start self play", NULL},
    {"show", &Show, true, false, "display current position", NULL},
    {"test", &Test, false, false, "run EPD test suite", NULL},
    {"test-score", &TestScore, false, false,
     "run static evaluatior on EPD test suite", NULL},
    {"time", &XboardTime, true, false, "set time (xboard)", NULL},
    {"undo", &Undo, true, true, "undo last move", NULL},
    {"warranty", &ShowWarranty, true, false, "show terms of warranty", NULL},
    {"xboard", &SetXBoard, false, false, "switch to xboard compatibility",
     NULL},
    {"?", &MoveNow, true, false, "move now", NULL},
    {"tn", &TestNext, false, false, "test the move generators", NULL},
    {NULL, NULL, false, false, NULL, NULL}};

char AutoSaveFileName[64];

struct SCommand *ParseInput(char *pszLine) {
    static struct SCommand TheCommand;
    char *pszToken;
    CMove Move;
    struct SCommandEntry *pEntry;

    pszToken = nextToken(&pszLine, " \t\n\r");
    if (pszToken == NULL)
        return NULL;

    /*
     * Try to interpret as move.
     */

    Move = CurrentPosition->ParseSAN(pszToken);
    if (Move == M_NONE) {
        Move = CurrentPosition->ParseGSAN(pszToken);
    }

    if (Move != M_NONE) {
        TheCommand.move = Move;
        TheCommand.command_func = NULL;
        TheCommand.args = NULL;
        return &TheCommand;
    }

    pEntry = Commands;
    while (pEntry->name) {
        if (!strcmp(pEntry->name, pszToken)) {
            TheCommand.move = M_NONE;
            TheCommand.command_func = pEntry->command_func;
            TheCommand.allowed_during_search = pEntry->allowed_during_search;
            TheCommand.interrupts_search = pEntry->interrupts_search;
            TheCommand.args = nextToken(&pszLine, "\n\r");
            return &TheCommand;
        }
        pEntry++;
    }

    return NULL;
}

void ExecuteCommand(struct SCommand *pTheCommand) {
    if (pTheCommand->move != M_NONE) {
        CurrentPosition->DoMove(pTheCommand->move);
    } else {
        COMMAND cfunc = pTheCommand->command_func;
        cfunc(pTheCommand->args);
    }
}

static void Quit(char *pszArgs) {
    (void)pszArgs;
#if MP
    StopHelpers();
#endif
    CPosition::Free(CurrentPosition);
    Print(0, "\n\i'll be back.\n");
    exit(0);
}

static void Show(char *pszArgs) {
    (void)pszArgs;
    CurrentPosition->ShowPosition();
}

static void ShowEco(char *pszArgs) {
    (void)pszArgs;
    char szEco[128] = "";

    FindEcoCode(CurrentPosition, szEco);

    Print(0, "Eco code is %s\n", szEco);
}

static void Test(char *pszFname) {
    CPosition *p;
    int nSolved = 0, nTotal = 0;
    FILE *fin, *fout;
    int i;
    int nBtav = 0;
    int nBtval;
    int nBsval;
    int nLctval = 1900;
    char szLine[256];

    if (!pszFname) {
        Print(0, "Usage: test <filename>\n");
        return;
    }

    fin = fopen(pszFname, "r");
    if (!fin) {
        Print(0, "Couldn't open %s for input.\n", pszFname);
        return;
    }

    fout = fopen("nsolved.epd", "w");

    for (i = 1;; i++) {
        CMove Move;
        int j;
        bool fCorrect = false;

        if (fgets(szLine, 256, fin) == NULL)
            break;
        Print(0, "Problem %d:\n", i);
        p = CPosition::CreateFromEPD(szLine);
        if (p == NULL) {
            Print(0, "Skipping invalid EPD.\n");
            continue;
        }
        p->ShowPosition();

        /* TestSwap(); */

        Move = p->Iterate(NULL, M_NONE, NULL);
        for (j = 0; goodmove[j] != M_NONE; j++)
            if (Move == goodmove[j])
                fCorrect = true;

        if (!fCorrect && badmove[0] != M_NONE) {
            fCorrect = true;

            for (j = 0; badmove[j] != M_NONE; j++)
                if (Move == badmove[j])
                    fCorrect = false;
        }

        nTotal++;
        if (fCorrect) {
            Print(0, "solved!\n");
            nSolved++;

            nBtav += (FHTime < 900) ? FHTime : 900;

            if (FHTime < 10)
                nLctval += 30;
            else if (FHTime < 30)
                nLctval += 25;
            else if (FHTime < 90)
                nLctval += 20;
            else if (FHTime < 180)
                nLctval += 15;
            else if (FHTime < 390)
                nLctval += 10;
            else if (FHTime <= 600)
                nLctval += 5;
        } else {
            Print(0, "not solved!\n");
            nBtav += 900;
            if (fout)
                fprintf(fout, "%s", szLine);
        }

        nBtval = 2630 - (nBtav / nTotal);
        nBsval = (nBtav / (17 * 60));
        nBsval = 2830 - nBsval * nBsval;

        Print(0, "solved %d out of %d  (BT2630 = %d, LCT2 = %d, BS2830 = %d)\n",
              nSolved, nTotal, nBtval, nLctval, nBsval);
        Print(0, "-----------------------------------------------\n\n");

        CPosition::Free(p);
    }

    if (fin)
        fclose(fin);
    if (fout)
        fclose(fout);
}

static void TestScore(char *pszFname) {
    CPosition *p;
    FILE *fin, *fout;
    char szLine[256];

    if (!pszFname) {
        Print(0, "Usage: test-score <filename>\n");
        return;
    }

    fin = fopen(pszFname, "r");
    if (!fin) {
        Print(0, "Couldn't open %s for input.\n", pszFname);
        return;
    }

    fout = fopen("test_score.epd", "w");

    for (;;) {

        if (fgets(szLine, 256, fin) == NULL)
            break;
        p = CPosition::CreateFromEPD(szLine);
        if (p == NULL) {
            Print(0, "Skipping invalid EPD.\n");
            continue;
        }
        InitEvaluation(p);
        int nScore = EvaluatePosition(p);

        if (fout) {
            size_t l = strlen(szLine);
            l--;
            szLine[l] = '\0';
            l--;
            if (szLine[l] == ';') {
                szLine[l] = '\0';
            }

            fprintf(fout, "%s; score %d;\n", szLine, nScore);
        }

        CPosition::Free(p);
    }

    if (fin)
        fclose(fin);
    if (fout)
        fclose(fout);
}

static void SetTime(char *pszArg) {
    char *rgArgs[3];

    rgArgs[0] = strtok(pszArg, " \t");
    rgArgs[1] = strtok(NULL, " \t");
    rgArgs[2] = strtok(NULL, " \t");

    SetTimeControl(rgArgs, XBoardMode);
}

static void SetXBoard(char *pszArgs) {
    (void)pszArgs;
    XBoardMode = true;
    g_nVerbosity = 1;

    Print(0, "\n");
    Print(0, "feature myname=\"Amy " VERSION "\"\n");
    Print(0, "feature san=1\n");
    Print(0, "feature name=1\n");
    Print(0, "feature done=1\n");

    /* Set up signal handler fuer Ctrl+C */
    signal(SIGINT, SIG_IGN);
}

static void Go(char *pszArgs) {
    (void)pszArgs;
    ForceMode = false;
    State = STATE_CALCULATING;
}

static void Force(char *pszArgs) {
    (void)pszArgs;
    ForceMode = true;
    AbortSearch = true;
}

void NewGame(char *pszArgs) {
    (void)pszArgs;
    /*
     * Create a new save file.
     */
    GetTmpFileName(AutoSaveFileName, sizeof(AutoSaveFileName));

    ForceMode = false;
    CPosition::Free(CurrentPosition);
    CurrentPosition = CPosition::Initial();
    if (State != STATE_ANALYZING) {
        State = STATE_WAITING;
    }
    ResetTimeControl(!XBoardMode);
}

static void MoveNow(char *pszArgs) {
    (void)pszArgs;
    AbortSearch = true;
}

void Edit(char *pszArgs) {
    (void)pszArgs;
    bool fEditing = true;
    unsigned int i;
    int nSide = White;
    char szBuffer[16];
    CPosition *p = CurrentPosition;

    for (i = 0; i < CBitBoard::SIZE; i++)
        p->SetPiece(i, Neutral);
    p->GetMask(White, 0) = p->GetMask(Black, 0) = {};

    while (fEditing) {
        int nSq;

        if (!ReadLine(szBuffer, 256))
            break;

        nSq = (szBuffer[1] - 'a') + 8 * (szBuffer[2] - '1');

        switch (szBuffer[0]) {
        case '.':
            fEditing = false;
            break;
        case 'c':
            nSide = OPP(nSide);
            break;
        case 'P':
            p->SetPiece(nSq, PIECEID(Pawn, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        case 'N':
            p->SetPiece(nSq, PIECEID(Knight, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        case 'B':
            p->SetPiece(nSq, PIECEID(Bishop, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        case 'R':
            p->SetPiece(nSq, PIECEID(Rook, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        case 'Q':
            p->SetPiece(nSq, PIECEID(Queen, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        case 'K':
            p->SetPiece(nSq, PIECEID(King, nSide));
            p->GetMask(nSide, 0).SetBit(nSq);
            break;
        }
    }

    p->SetCastle(0);
    p->SetEnPassant(InvalidSquareCoord());

    p->RecalcAttacks();
    if (p->GetPiece(CASTLE_E1) == King) {
        if (p->GetPiece(CASTLE_H1) == Rook)
            p->SetCastle(p->GetCastle() | (CastleMask[White][0]));
        if (p->GetPiece(CASTLE_A1) == Rook)
            p->SetCastle(p->GetCastle() | (CastleMask[White][1]));
    }
    if (p->GetPiece(CASTLE_E8) == -King) {
        if (p->GetPiece(CASTLE_H8) == -Rook)
            p->SetCastle(p->GetCastle() | (CastleMask[Black][0]));
        if (p->GetPiece(CASTLE_A8) == -Rook)
            p->SetCastle(p->GetCastle() | (CastleMask[Black][1]));
    }
    p->RecalcAttacks();
    p->ShowPosition();
}

static void Undo(char *pszArgs) {
    (void)pszArgs;
    CurrentPosition->Undo();
}

static void Book(char *pszArgs) {
    (void)pszArgs;
    QueryBook(CurrentPosition);
}

static void Post(char *pszArgs) {
    (void)pszArgs;
    PostMode = true;
}

static void NoPost(char *pszArgs) {
    (void)pszArgs;
    PostMode = false;
}

static void Easy(char *pszArgs) {
    (void)pszArgs;
    EasyMode = true;
}

static void Hard(char *pszArgs) {
    (void)pszArgs;
    EasyMode = false;
}

static void MovesCmd(char *pszArgs) {
    (void)pszArgs;
    CurrentPosition->ShowMoves();
}

static void SetEPD(char *pszArgs) {
    if (!pszArgs) {
        Print(0, "Usage: epd <EPD>\n");
        return;
    }
    CPosition *pNewPosition = CPosition::CreateFromEPD(pszArgs);
    if (pNewPosition == NULL) {
        Print(0, "Invalid EPD.\n");
        return;
    }
    CPosition::Free(CurrentPosition);
    CurrentPosition = pNewPosition;
}

static void RunAnnotate(char *pszFname, int nSide) {
    FILE *fin = fopen(pszFname, "r");
    CPosition *p;

    if (fin) {
        struct PGNHeader Header;
        char szMove[16];

        while (!scanHeader(fin, &Header)) {
            p = CPosition::Initial();
            while (!scanMove(fin, szMove)) {
                CMove TheMove = p->ParseSAN(szMove);
                if (TheMove != M_NONE) {
                    char szSanBuffer[16];
                    p->ShowPosition();
                    Print(0, "%s(%d): ", p->GetTurn() == White ? "White" : "Black",
                          (p->GetPly() / 2) + 1);
                    Print(0, "%s\n", p->SAN(TheMove, szSanBuffer));
                    if (nSide == -1 || (nSide == p->GetTurn())) {
                        p->Iterate(NULL, M_NONE, NULL);
                    }
                    p->DoMove(TheMove);
                }
            }
            CPosition::Free(p);
        }
        fclose(fin);
    } else
        Print(0, "Couldn't open %s\n", pszFname);
}

static void Anno(char *pszArgs) {
    int nSide = -1;
    char *pszArg1 = strtok(pszArgs, " \n\r");
    char *pszArg2 = strtok(NULL, " \n\r");

    if (!pszArg1) {
        Print(0, "Usage: anno <file> [w|b|wb]\n");
        return;
    }

    if (pszArg2) {
        if (!strcmp(pszArg2, "w")) {
            nSide = White;
        } else if (!strcmp(pszArg2, "b")) {
            nSide = Black;
        }
    }

    RunAnnotate(pszArg1, nSide);
}

static const char *distribution =
    "\n    Copyright (c) 2002-2026, Thorsten Greiner\n"
    "    All rights reserved.\n"
    "\n"
    "    Redistribution and use in source and binary forms, with or without\n"
    "    modification, are permitted provided that the following conditions "
    "are met:\n"
    "\n"
    "    * Redistributions of source code must retain the above copyright "
    "notice,\n"
    "      this list of conditions and the following disclaimer.\n"
    "\n"
    "    * Redistributions in binary form must reproduce the above copyright "
    "notice,\n"
    "      this list of conditions and the following disclaimer in the "
    "documentation\n"
    "      and/or other materials provided with the distribution.\n"
    "\n";

static const char *warranty =
    "\n THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "
    "\"AS IS\"\n"
    " AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, "
    "THE\n"
    " IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR "
    "PURPOSE ARE\n"
    " DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE "
    "LIABLE\n"
    " FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR "
    "CONSEQUENTIAL\n"
    " DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS "
    "OR\n"
    " SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) "
    "HOWEVER\n"
    " CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT "
    "LIABILITY,\n"
    " OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF "
    "THE USE\n"
    " OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
    "\n";

static void ShowWarranty(char *pszArgs) {
    (void)pszArgs;
    Print(0, warranty);
}

static void ShowDistribution(char *pszArgs) {
    (void)pszArgs;
    Print(0, distribution);
    Print(0, warranty);
}

static void Help(char *pszArgs) {
    struct SCommandEntry *pEntry = Commands;

    (void)pszArgs;
    Print(2, "\nEnter a legal move (like e4, Nxd5, O-O, d1=Q+) or one of the\n"
             "following commands:\n\n");
    while (pEntry->name) {
        char szTmpl[] = ". . . . . . . . ";
        memcpy(szTmpl, pEntry->name, strlen(pEntry->name));
        Print(2, szTmpl);
        if (pEntry->short_help) {
            Print(2, "%s", pEntry->short_help);
        }
        Print(2, "\n");

        pEntry++;
    }

    Print(2, "\n");
}

static void Benchmark(char *pszArgs) {
    (void)pszArgs;
    CMove Move = make_move(hg1, hf3, 0);
    int i;
    const int nCycles = 10000000;
    unsigned long dwStart, dwEnd;
    double dElapsed;
    CPosition *p;

    p = CPosition::Initial();

    dwStart = GetTime();

    for (i = nCycles; i > 0; i--) {
        p->DoMove(Move);
        p->UndoMove(Move);
    }

    dwEnd = GetTime();

    dElapsed = (dwEnd - dwStart) / 100.0;

    Print(0, "Nf3: %.2g secs, %g moves/sec\n", dElapsed, nCycles / dElapsed);

    CPosition::Free(p);
}

static BitBoardBits SearchFully(CPosition *p, BitBoardBits qwCnt, int nDepth,
                                heap_t heap) {
    unsigned int i;

    if (nDepth <= 0) {
        return qwCnt + 1;
    }

    push_section(heap);
    p->PLegalMoves(heap);

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        CMove Move = heap->data[i];
        if (Move.IsCastle() && !p->MayCastle(Move))
            continue;

        p->DoMove(Move);
        if (!p->InCheck(OPP(p->GetTurn()))) {
            qwCnt = SearchFully(p, qwCnt, nDepth - 1, heap);
        }
        p->UndoMove(Move);
    }
    pop_section(heap);

    return qwCnt;
}

static void Perft(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: perft <depth>\n");
        return;
    }

    int nDepth;
    sscanf(pszArgs, "%d", &nDepth);

    BitBoardBits qwCnt = 0;
    heap_t heap = allocate_heap();

    unsigned long dwStart = GetTime();
    qwCnt = SearchFully(CurrentPosition, qwCnt, nDepth, heap);
    unsigned long dwEnd = GetTime();

    free_heap(heap);

    double dElapsed = (dwEnd - dwStart) / 100.0;
    double dNps = qwCnt / dElapsed;

    Print(0, "Perft(%d): %lld terminal positions in %g secs (%g nps)\n", nDepth,
          qwCnt, dElapsed, dNps);
}

static void Load(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: load <filename>\n");
        return;
    }
    CurrentPosition = CPosition::Initial();
    LoadGame(CurrentPosition, pszArgs);
}

static void Save(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: save <filename>\n");
        return;
    }
    SaveGame(CurrentPosition, pszArgs);
}

static void Prefs(char *pszArgs) { CreateLearnDB(pszArgs); }

static void Flatten(char *pszArgs) {
    int nThreshold;
    if (pszArgs == NULL) {
        Print(0, "Usage: flatten <count>\n");
        return;
    }

    nThreshold = atoi(pszArgs);
    if (nThreshold < 1) {
        nThreshold = 1;
    }

    FlattenBook(nThreshold);
}

static void XboardTime(char *pszArgs) {
    if (pszArgs != NULL) {
        int nSeconds = atoi(pszArgs) / 100;

        /*
         * xboard sends time for the side not to move.
         */

        Time[ComputerSide] = nSeconds;
    }
}

static void Analyze(char *pszArgs) {
    (void)pszArgs;
    State = STATE_ANALYZING;
}

static void StopAnalyze(char *pszArgs) {
    (void)pszArgs;
    State = STATE_WAITING;
}

static void Name(char *pszArgs) {
    if (pszArgs) {
        strncpy(OpponentName, pszArgs, OPP_NAME_LENGTH - 1);
        Print(2, "Your name is %s\n", OpponentName);
    }
}

static void SelfPlay(char *pszArgs) {
    (void)pszArgs;
    SelfPlayMode = true;
    State = STATE_CALCULATING;
}

static void TestNext(char *pszArgs) {
    (void)pszArgs;
    CurrentPosition->TestNextGenerators();
}

static void Conf(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: conf <filename>\n");
        return;
    }

    LoadEvaluationConfig(pszArgs);
    CurrentPosition->RecalcAttacks();
}

static void SaveConf(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: save-conf <filename>\n");
        return;
    }

    SaveEvaluationConfig(pszArgs);
}

static void ShowScore(char *pszArgs) {
    (void)pszArgs;
    InitEvaluation(CurrentPosition);
    int nScore = EvaluatePosition(CurrentPosition);
    Print(0, "Static evaluation: %d\n", nScore);
}

static void SetSearchDepth(char *pszArgs) {
    if (pszArgs == NULL) {
        Print(0, "Usage: depth <depth>");
        return;
    }

    SetMaxSearchDepth(atoi(pszArgs));
}
