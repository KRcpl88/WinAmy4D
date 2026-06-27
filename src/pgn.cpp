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
 * pgn.c - pgn handling routines
 */

#include "amy.h"

#include "config.h"

#include <string.h>

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "dbase.h"
#include "eco.h"
#include "pgn.h"
#include "state_machine.h"
#include "utils.h"

#define AMY_NAME "Amy " VERSION

char OpponentName[OPP_NAME_LENGTH] = "Opponent";

static CMove PGNMoveHistory[1024];
static char DateBuf[16];
static char TimeBuf[16];
static char HostNameBuf[256];

static void MakeDateTime(void) {
    time_t nTnow;
    struct tm *pNow;

    time(&nTnow);
    pNow = localtime(&nTnow);
    strftime(DateBuf, 15, "%Y.%m.%d", pNow);
    strftime(TimeBuf, 15, "%H:%M:%S", pNow);
}

static void MakeHostName(void) {
#if HAVE_GETHOSTNAME
    gethostname(HostNameBuf, 256);
#else
    strcpy(HostNameBuf, "Your computer");
#endif
}

static const char *getWhiteName(void) {
    return (ComputerSide == White) ? (AMY_NAME) : (OpponentName);
}

static const char *getBlackName(void) {
    return (ComputerSide == Black) ? (AMY_NAME) : (OpponentName);
}

void SaveGame(CPosition *p, char *pszFileName) {
    /* Do not save if no move made yet. */
    if (p->GetPly() > 0) {
        FILE *pFout = fopen(pszFileName, "w");
        if (pFout) {
            int nI;
            int nPly = p->GetPly();
            int nWidth = 0;
            const char *pszGameend;
            char szShortgameend[8] = "1/2-1/2";
            char szEco[128] = "";

            pszGameend = CurrentPosition->GameEnd();
            if (pszGameend == NULL) {
                pszGameend = "*";
                strcpy(szShortgameend, pszGameend);
            } else {
                strncpy(szShortgameend, pszGameend, 3);
                if (szShortgameend[0] == '0' || szShortgameend[2] == '0') {
                    szShortgameend[3] = '\0';
                }
            }

            fprintf(pFout, "[Event \"Amy game\"]\n");
            MakeHostName();
            fprintf(pFout, "[Site \"%s\"]\n", HostNameBuf);
            MakeDateTime();
            fprintf(pFout, "[Date \"%s\"]\n", DateBuf);
            fprintf(pFout, "[Time \"%s\"]\n", TimeBuf);
            fprintf(pFout, "[Round \"?\"]\n");
            if (FindEcoCode(p, szEco)) {
                fprintf(pFout, "[ECO \"%c%c%c\"]\n", szEco[0], szEco[1], szEco[2]);
            }
            fprintf(pFout, "[White \"%s\"]\n", getWhiteName());
            fprintf(pFout, "[Black \"%s\"]\n", getBlackName());
            fprintf(pFout, "[Result \"%s\"]\n\n", szShortgameend);

            for (nI = nPly; nI > 0; nI--) {
                CMove Move = (p->GetActLog() - 1)->gl_Move;
                PGNMoveHistory[nI - 1] = Move;
                p->UndoMove(Move);
            }

            for (nI = 0; nI < nPly; nI++) {
                CMove Move = PGNMoveHistory[nI];
                if ((nI & 1) == 0) {
                    fprintf(pFout, "%d. ", (nI / 2) + 1);
                    nWidth += 3;
                    if (nI > 18)
                        nWidth++;
                    if (nI > 98)
                        nWidth++;
                }

                char szSanBuffer[16];
                char *pszSan = p->SAN(Move, szSanBuffer);
                fprintf(pFout, "%s ", pszSan);
                nWidth += (int)strlen(pszSan) + 1;
                if (nWidth > 67) {
                    nWidth = 0;
                    fprintf(pFout, "\n");
                }
                p->DoMove(Move);
            }
            fprintf(pFout, "\n%s\n\n", pszGameend);
            fclose(pFout);
        }
    }
}

void LoadGame(CPosition *p, char *pszFileName) {
    FILE *pFin = fopen(pszFileName, "r");

    if (pFin) {
        struct PGNHeader Header;
        char szMove[16];
        if (!scanHeader(pFin, &Header)) {
            while (!scanMove(pFin, szMove)) {
                CMove TheMove = p->ParseSAN(szMove);
                if (TheMove != M_NONE) {
                    p->DoMove(TheMove);
                }
            }
        }
        fclose(pFin);
    }
}

int scanHeader(FILE *pFin, struct PGNHeader *pHeader) {
    static char szBuffer[1024];
    int nState = 0;

    /* Clear it */
    memset(pHeader, 0, sizeof(struct PGNHeader));

    while (fgets(szBuffer, 1024, pFin)) {
        if (szBuffer[0] == '[') {
            char *pszX = szBuffer + 1;
            char *pszKey = nextToken(&pszX, " \"");
            char *pszValue = nextToken(&pszX, "\"");

            if (!strcmp("Event", pszKey)) {
                strncpy(pHeader->event, pszValue, sizeof(pHeader->event) - 1);
            }
            if (!strcmp("Site", pszKey)) {
                strncpy(pHeader->site, pszValue, sizeof(pHeader->site) - 1);
            }
            if (!strcmp("Date", pszKey)) {
                strncpy(pHeader->date, pszValue, sizeof(pHeader->date) - 1);
            }
            if (!strcmp("Round", pszKey)) {
                strncpy(pHeader->round, pszValue, sizeof(pHeader->round) - 1);
            }
            if (!strcmp("White", pszKey)) {
                strncpy(pHeader->white, pszValue, sizeof(pHeader->white) - 1);
            }
            if (!strcmp("Black", pszKey)) {
                strncpy(pHeader->black, pszValue, sizeof(pHeader->black) - 1);
            }
            if (!strcmp("Result", pszKey)) {
                strncpy(pHeader->result, pszValue, sizeof(pHeader->result) - 1);
            }
            if (!strcmp("WhiteElo", pszKey)) {
                pHeader->white_elo = atoi(pszValue);
            }
            if (!strcmp("BlackElo", pszKey)) {
                pHeader->black_elo = atoi(pszValue);
            }
            if (!strcmp("FEN", pszKey)) {
                strncpy(pHeader->fen, pszValue, sizeof(pHeader->fen) - 1);
            }
            if (!strcmp("SetUp", pszKey)) {
                pHeader->is_setup = atoi(pszValue);
            }

            nState = 1;

        } else {
            if (nState != 0)
                return 0;
        }
    }

    return 1;
}

static char comment_buffer[2048] = "";
static char *comment_ptr = comment_buffer;

int scanMove(FILE *pFin, char *pszNextMove) {
    static char szBuffer[1024];
    static int haveLine = 0;
    static char *pszX;

    char *pszToken;

    int nBraces = 0;
    int nParens = 0;

    do {
        if (!haveLine) {
            if (!fgets(szBuffer, 1024, pFin))
                return 1;
            haveLine = 1;
            pszX = szBuffer;
        }

        if (nBraces) {
            if (*pszX == '\0') {
                haveLine = 0;
                continue;
            }

            *(comment_ptr) = *pszX;

            if (*(pszX++) == '}') {
                *comment_ptr = 0;
                nBraces = 0;
            }

            comment_ptr++;

            continue;
        } else if (nParens) {
            if (*pszX == '\0') {
                haveLine = 0;
                continue;
            }

            if (*pszX == ')') {
                nParens--;
            }
            if (*pszX == '(') {
                nParens++;
            }
            pszX++;

            continue;
        } else {
            if (pszX && *pszX == '{') {
                nBraces = 1;
                pszX++;
                comment_ptr = comment_buffer;
                continue;
            }
            if (pszX && *pszX == '(') {
                nParens = 1;
                pszX++;
                continue;
            }
            pszToken = nextToken(&pszX, " .\n\r\t");

            if (pszToken == NULL) {
                haveLine = 0;
                continue;
            }

            if (*pszToken == '\0')
                continue;

            if (!strcmp("*", pszToken) || !strcmp("1-0", pszToken) ||
                !strcmp("0-1", pszToken) || !strcmp("1/2-1/2", pszToken))
                break;
            if (*pszToken >= '0' && *pszToken <= '9')
                continue;
            if (*pszToken == '$')
                continue;
        }

        strcpy(pszNextMove, pszToken);
        return 0;
    } while (1);

    return 1;
}

void get_and_reset_comment(char *pszDestination, unsigned int dwLength) {
    strncpy(pszDestination, comment_buffer, dwLength);

    comment_ptr = comment_buffer;
    *comment_buffer = 0;
}

void print_header(FILE *pFout, struct PGNHeader *pHeader) {
    fprintf(pFout, "[Event \"%s\"]\n", pHeader->event);
    fprintf(pFout, "[Site \"%s\"]\n", pHeader->site);
    fprintf(pFout, "[Date \"%s\"]\n", pHeader->date);
    fprintf(pFout, "[Round \"%s\"]\n", pHeader->round);
    fprintf(pFout, "[White \"%s\"]\n", pHeader->white);
    fprintf(pFout, "[Black \"%s\"]\n", pHeader->black);
    fprintf(pFout, "[Result \"%s\"]\n", pHeader->result);
    if (pHeader->is_setup) {
        fprintf(pFout, "[SetUp \"1\"]\n");
        fprintf(pFout, "[FEN \"%s\"]\n", pHeader->fen);
    }
    fprintf(pFout, "\n");
}
