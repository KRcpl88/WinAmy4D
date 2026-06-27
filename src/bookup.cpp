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
 * bookup.c - opening book management routines
 */

#include "amy.h"

#include <errno.h>
#include <string.h>

#include "dbase.h"
#include "eco.h"
#include "pgn.h"
#include "random.h"
#include "safe_malloc.h"
#include "tree.h"
#include "utils.h"

#define BOOK_NAME "Book.db"
#define LEARN_NAME "Learn.db"

#ifdef BOOKDIR
#define DEFAULT_BOOK_NAME BOOKDIR PATH_SEPARATOR BOOK_NAME
#else
#define DEFAULT_BOOK_NAME "bookdir\\defaultbookname"
#endif

#define WITH_ELO 1

enum MoveCategory { GoodMove = 1, BadMove = 2 };

struct BookEntry {
    unsigned int win;  /* number of wins */
    unsigned int loss; /* number of losses */
    unsigned int draw; /* number of draws */
#if WITH_ELO
    unsigned int sumElo; /* sum of elo played */
    unsigned int nElo;   /* number of elo players */
#endif
};

struct LearnEntry {
    int flags;
    int learn_value;
};

struct BookQuery {
    struct BookEntry be;
    struct LearnEntry le;
};

static tree_node_t *BookDB = NULL;
static tree_node_t *LearnDB = NULL;

static tree_node_t *PutBookEntry(tree_node_t *pDatabase, hash_t hk, int nResult,
                                 int nElo) {
    Print(9, "storing position\n");
    struct BookEntry *pEntry = NULL;

    if (pDatabase != NULL) {
        pEntry = (struct BookEntry *)lookup_value(pDatabase, (char *)&hk,
                                                  sizeof(hk), NULL);
    }

    if (pEntry == NULL) {
        pEntry = (BookEntry *)safe_calloc(1, sizeof(BookEntry));
    }

    if (nResult == 1) {
        pEntry->win += 1;
    } else if (nResult == 0) {
        pEntry->draw += 1;
    } else if (nResult == -1) {
        pEntry->loss += 1;
    }
#if WITH_ELO
    if (nElo != 0) {
        pEntry->sumElo += nElo;
        pEntry->nElo += 1;
    }
#endif

    pDatabase = add_node(pDatabase, (char *)&hk, sizeof(hk), (char *)pEntry,
                         sizeof(struct BookEntry));
    free(pEntry);

    return pDatabase;
}

static void OpenBookFile(tree_node_t **pDb) {
    static bool s_fErrorPrinted = false;

    FILE *pFin = fopen(BOOK_NAME, "rb");

#ifdef DEFAULT_BOOK_NAME
    if (pFin == NULL) {
        pFin = fopen(DEFAULT_BOOK_NAME, "rb");
    }
#endif

    if (pFin == NULL) {
        if (!s_fErrorPrinted) {
            Print(0, "Can't open database: %s\n", strerror(errno));
            s_fErrorPrinted = true;
        }
        return;
    }

    *pDb = load_tree(pFin);
    fclose(pFin);
}

static struct BookEntry *GetBookEntry(hash_t hk) {
    struct BookEntry *pRetval = NULL;
    if (BookDB == NULL) {
        OpenBookFile(&BookDB);
    }
    if (BookDB != NULL) {
        pRetval = (struct BookEntry *)lookup_value(BookDB, (char *)&hk,
                                                   sizeof(hk), NULL);
    }

    return pRetval;
}

static struct LearnEntry *GetLearnEntry(hash_t hk) {
    struct LearnEntry *pRetval = NULL;

    if (LearnDB == NULL) {
        FILE *pFin = fopen(LEARN_NAME, "rb");
        if (pFin != NULL) {
            LearnDB = load_tree(pFin);
            fclose(pFin);
        }
    }
    if (LearnDB != NULL) {
        pRetval = (struct LearnEntry *)lookup_value(LearnDB, (char *)&hk,
                                                    sizeof(hk), NULL);
    }

    return pRetval;
}

void CloseBook(void) {
    free_node(BookDB);
    BookDB = NULL;
}

static void BookupInternal(char *pszFileName, int nVerbosity) {
    CPosition *p;
    FILE *pFin;
    int nAfterEco = 0;
    tree_node_t *pDatabase = NULL;
    int nLines = 0;

    struct PGNHeader Header;
    char szMove[12];

    pFin = fopen(pszFileName, "rb");
    if (pFin == NULL) {
        Print(0, "Can't open bookfile.\n");
        return;
    } else {
        Print(nVerbosity, "   Parsing PGN file %s. '.'= 100 Games\n", pszFileName);
    }

    CloseBook();

    while (!scanHeader(pFin, &Header)) {
        int nResult;

        if (!strcmp(Header.result, "1-0"))
            nResult = 1;
        else if (!strcmp(Header.result, "0-1"))
            nResult = -1;
        else if (!strcmp(Header.result, "1/2-1/2"))
            nResult = 0;
        else
            continue;

        p = CPosition::Initial();

        while (!scanMove(pFin, szMove)) {
            if (!(strlen(szMove) < 12)) {
                printf("\n<%s>\n", szMove);
                exit(1);
            }

            CMove TheMove = p->ParseSAN(szMove);
            if (TheMove != M_NONE) {
                p->DoMove(TheMove);
                char *pszEcoCode = GetEcoCode(p->GetHashKey());
                if (pszEcoCode) {
                    nAfterEco = 0;
                    free(pszEcoCode);
                } else {
                    nAfterEco++;
                }
                if (nAfterEco <= 20) {
                    if (p->GetTurn() == Black) {
                        /* white played the move */
                        pDatabase = PutBookEntry(pDatabase, p->GetHashKey(), nResult,
                                                 Header.white_elo);
                    } else {
                        /* black played the move */
                        pDatabase = PutBookEntry(pDatabase, p->GetHashKey(), -nResult,
                                                 Header.black_elo);
                    }
                }
            }
        }

        CPosition::Free(p);

        nLines++;
        if ((nLines % 100) == 0) {
            Print(0, ".");

            if ((nLines % 7000) == 0) {
                Print(0, "(%d)\n", nLines);
            }
        }
    }

    Print(nVerbosity, "(%d)\n", nLines);
    fclose(pFin);

    FILE *pFout = fopen(BOOK_NAME, "wb");
    if (pFout == NULL) {
        Print(0, "Can't write database: %s\n", strerror(errno));
        return;
    }

    save_tree(pDatabase, pFout);
    fclose(pFout);

    free_node(pDatabase);
}

void Bookup(char *pszFileName) { BookupInternal(pszFileName, 0); }

void BookupQuiet(char *pszFileName) { BookupInternal(pszFileName, 9); }

static void GetAllBookMoves(CPosition *p, int *cnt, CMove *book_moves,
                            struct BookQuery *pEntries) {
    unsigned int i;

    heap_t heap = allocate_heap();
    p->LegalMoves(heap);

    for (i = heap->current_section->start; i < heap->current_section->end;
         i++) {
        CMove Move = heap->data[i];
        struct BookEntry *be = NULL;
        struct LearnEntry *pLe = NULL;

        p->DoMove(Move);
        /* If the move leads to a repetition, do not accept it. */
        if (!p->Repeated(false)) {
            be = GetBookEntry(p->GetHashKey());
            pLe = GetLearnEntry(p->GetHashKey());
        }
        p->UndoMove(Move);

        if (be) {
            memset(pEntries + *cnt, 0, sizeof(struct BookQuery));
            book_moves[*cnt] = Move;
            pEntries[*cnt].be = *be;
            if (pLe) {
                pEntries[*cnt].le = *pLe;
                free(pLe);
            }
            (*cnt)++;
            free(be);
        }
    }

    free_heap(heap);
}

static void SortBook(int cnt, CMove *pMvs, struct BookQuery *pEntries) {
    bool fDone = false;

    while (!fDone) {
        int i;

        fDone = true;

        for (i = 1; i < cnt; i++) {
            int nF1 = pEntries[i - 1].be.win + pEntries[i - 1].be.loss +
                     pEntries[i - 1].be.draw;
            int nF2 =
                pEntries[i].be.win + pEntries[i].be.loss + pEntries[i].be.draw;
            if (nF1 < nF2) {
                struct BookQuery betmp = pEntries[i];
                CMove Move;
                pEntries[i] = pEntries[i - 1];
                pEntries[i - 1] = betmp;
                Move = pMvs[i];
                pMvs[i] = pMvs[i - 1];
                pMvs[i - 1] = Move;

                fDone = false;
            }
        }
    }
}

static void CalculatePropabilities(int cnt, struct BookQuery *pEntries,
                                   double *rgdProps) {
    int nTotal = 0;
    double dTotalprops;
    int nLimit;
    int i;

    for (i = 0; i < cnt; i++) {
        nTotal += pEntries[i].be.win + pEntries[i].be.loss + pEntries[i].be.draw;
    }

    nLimit = nTotal / 16;
    dTotalprops = 0.0;

    for (i = 0; i < cnt; i++) {
        struct BookQuery *pEntry = pEntries + i;
        int nFreq = pEntry->be.win + pEntry->be.loss + pEntry->be.draw;

        rgdProps[i] = 0.0;
        if (nFreq > nLimit) {
            int nAvelo = 2000;

#if WITH_ELO
            if (pEntry->be.nElo != 0) {
                nAvelo = pEntry->be.sumElo / pEntry->be.nElo;
            }
#endif

            rgdProps[i] = nAvelo * nFreq *
                       (double)(2 * pEntry->be.win + pEntry->be.draw) /
                       (double)(nFreq);

            if (pEntry->le.flags & GoodMove) {
                rgdProps[i] *= 2;
            }

            if (pEntry->le.flags & BadMove) {
                rgdProps[i] = 0.0;
            }

            /*
             * Never choose a variation that doesn't have a single win.
             */

            if (pEntry->be.win == 0) {
                rgdProps[i] = 0.0;
            }

            dTotalprops += rgdProps[i];
        }
    }

    if (dTotalprops != 0.0) {
        dTotalprops = 1.0 / dTotalprops;
    } else {
        dTotalprops = 0.0;
    }

    for (i = 0; i < cnt; i++) {
        rgdProps[i] *= dTotalprops;
    }
}

CMove SelectBook(CPosition *p) {
    int i, cnt = 0;
    struct BookQuery be[32];
    CMove rgMoves[32];
    double rgdProps[32];
    double dRandomValue = Random();

    GetAllBookMoves(p, &cnt, rgMoves, be);

    if (cnt != 0) {
        SortBook(cnt, rgMoves, be);
        CalculatePropabilities(cnt, be, rgdProps);

        for (i = 0; i < cnt; i++) {
            if (rgdProps[i] > 0.0) {
                dRandomValue -= rgdProps[i];
                if (dRandomValue <= 0.0) {
                    return rgMoves[i];
                }
            }
        }
    }

    return M_NONE;
}

void QueryBook(CPosition *p) {
    int i, cnt = 0;
    struct BookQuery be[32];
    CMove rgMoves[32];
    double rgdProps[32];

    GetAllBookMoves(p, &cnt, rgMoves, be);
    SortBook(cnt, rgMoves, be);
    CalculatePropabilities(cnt, be, rgdProps);

    Print(0, "\tmove    count  win loss draw av. elo prop\n");
    for (i = 0; i < cnt; i++) {
        struct BookQuery *pEntry = be + i;
        int nFreq = pEntry->be.win + pEntry->be.loss + pEntry->be.draw;
        char cModifier = ' ';

        if (pEntry->le.flags & GoodMove) {
            cModifier = '!';
        }
        if (pEntry->le.flags & BadMove) {
            cModifier = '?';
        }

        char szSanBuffer[16];
        Print(0, "\t%5s%c %6d %3d%% %3d%% %3d%% %5d %3.f\n",
              p->SAN(rgMoves[i], szSanBuffer), cModifier, nFreq,
              (100 * pEntry->be.win) / nFreq, (100 * pEntry->be.loss) / nFreq,
              (100 * pEntry->be.draw) / nFreq,
#if WITH_ELO
              pEntry->be.nElo ? pEntry->be.sumElo / pEntry->be.nElo : 0,
#else
              0,
#endif
              rgdProps[i] * 100.0);
    }

    Print(0, "\n");
}

static void PutLearnEntry(hash_t hk, int nLearnValue, int nFlags) {
    if (LearnDB == NULL) {
        FILE *pFin = fopen(LEARN_NAME, "rb");
        if (pFin != NULL) {
            LearnDB = load_tree(pFin);
            fclose(pFin);
        }
    }

    struct LearnEntry Entry;

    Entry.learn_value = nLearnValue;
    Entry.flags = nFlags;

    LearnDB = add_node(LearnDB, (char *)&hk, sizeof(hk), (char *)&Entry,
                       sizeof(Entry));
}

void CreateLearnDB(char *pszFileName) {
    FILE *pFin = fopen(pszFileName, "rb");
    char szBuffer[1024];
    CPosition *p;

    if (pFin == NULL) {
        Print(0, "Can't open learn file %s\n", pszFileName);
        return;
    }

    while (fgets(szBuffer, 1023, pFin)) {
        char *pszX = szBuffer;
        char *pszMove;

        if (szBuffer[0] == '#')
            continue;

        p = CPosition::Initial();

        while ((pszMove = nextToken(&pszX, " \n\r\t")) != NULL) {
            int nFlags = 0;
            char *pszModifier = pszMove + strlen(pszMove) - 1;
            CMove TheMove;

            if (*pszModifier == '!') {
                nFlags = GoodMove;
                *pszModifier = '\0';
            } else if (*pszModifier == '?') {
                nFlags = BadMove;
                *pszModifier = '\0';
            }

            TheMove = p->ParseSAN(pszMove);
            if (TheMove != M_NONE) {
                char szSanBuffer[16];
                Print(0, "%s ", p->SAN(TheMove, szSanBuffer));

                p->DoMove(TheMove);

                if (nFlags != 0) {
                    PutLearnEntry(p->GetHashKey(), 0, nFlags);
                }
            } else {
                Print(0, "can't parse >%s<\n", pszMove);
                break;
            }
        }

        Print(0, "\n");
        CPosition::Free(p);
    }

    fclose(pFin);

    if (LearnDB != NULL) {
        FILE *pFout = fopen(LEARN_NAME, "wb");
        if (pFout != NULL) {
            save_tree(LearnDB, pFout);
            fclose(pFout);
        } else {
            Print(0, "Failed to save learn file: %s\n", strerror(errno));
        }
    }
}

static tree_node_t *flatten_internal(tree_node_t *pSource, tree_node_t *pTarget,
                                     unsigned int dwThreshold, int *pRead,
                                     int *pWritten) {
    if (pSource == NULL) {
        return pTarget;
    }
    struct BookEntry *pEntry = (struct BookEntry *)pSource->value_data;
    (*pRead)++;
    if ((pEntry->win + pEntry->draw + pEntry->loss) > dwThreshold) {
        pTarget = add_node(pTarget, pSource->key_data, pSource->key_len,
                           pSource->value_data, pSource->value_len);
        (*pWritten)++;
    }

    pTarget =
        flatten_internal(pSource->left_child, pTarget, dwThreshold, pRead, pWritten);
    pTarget =
        flatten_internal(pSource->right_child, pTarget, dwThreshold, pRead, pWritten);

    return pTarget;
}

void FlattenBook(unsigned int dwThreshold) {
    int nRead = 0;
    int nWritten = 0;

    if (BookDB == NULL) {
        OpenBookFile(&BookDB);
    }
    if (BookDB != NULL) {
        tree_node_t *pFlattened =
            flatten_internal(BookDB, NULL, dwThreshold, &nRead, &nWritten);

        PrintDebug(0, "Read %d entries, wrote %d entries\n", nRead, nWritten);

        FILE *pFout = fopen("Book2.db", "wb");
        if (pFout != NULL) {
            save_tree(pFlattened, pFout);
            fclose(pFout);
        } else {
            Print(0, "Can't write database: %s\n", strerror(errno));
        }

        free_node(pFlattened);
    }
}
