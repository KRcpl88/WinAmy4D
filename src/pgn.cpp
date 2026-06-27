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
    struct tm *now;

    time(&nTnow);
    now = localtime(&nTnow);
    strftime(DateBuf, 15, "%Y.%m.%d", now);
    strftime(TimeBuf, 15, "%H:%M:%S", now);
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

void SaveGame(CPosition *p, char *file_name) {
    /* Do not save if no move made yet. */
    if (p->GetPly() > 0) {
        FILE *fout = fopen(file_name, "w");
        if (fout) {
            int nI;
            int ply = p->GetPly();
            int width = 0;
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

            fprintf(fout, "[Event \"Amy game\"]\n");
            MakeHostName();
            fprintf(fout, "[Site \"%s\"]\n", HostNameBuf);
            MakeDateTime();
            fprintf(fout, "[Date \"%s\"]\n", DateBuf);
            fprintf(fout, "[Time \"%s\"]\n", TimeBuf);
            fprintf(fout, "[Round \"?\"]\n");
            if (FindEcoCode(p, szEco)) {
                fprintf(fout, "[ECO \"%c%c%c\"]\n", szEco[0], szEco[1], szEco[2]);
            }
            fprintf(fout, "[White \"%s\"]\n", getWhiteName());
            fprintf(fout, "[Black \"%s\"]\n", getBlackName());
            fprintf(fout, "[Result \"%s\"]\n\n", szShortgameend);

            for (nI = ply; nI > 0; nI--) {
                CMove move = (p->GetActLog() - 1)->gl_Move;
                PGNMoveHistory[nI - 1] = move;
                p->UndoMove(move);
            }

            for (nI = 0; nI < ply; nI++) {
                CMove move = PGNMoveHistory[nI];
                if ((nI & 1) == 0) {
                    fprintf(fout, "%d. ", (nI / 2) + 1);
                    width += 3;
                    if (nI > 18)
                        width++;
                    if (nI > 98)
                        width++;
                }

                char szSanBuffer[16];
                char *pszSan = p->SAN(move, szSanBuffer);
                fprintf(fout, "%s ", pszSan);
                width += (int)strlen(pszSan) + 1;
                if (width > 67) {
                    width = 0;
                    fprintf(fout, "\n");
                }
                p->DoMove(move);
            }
            fprintf(fout, "\n%s\n\n", pszGameend);
            fclose(fout);
        }
    }
}

void LoadGame(CPosition *p, char *file_name) {
    FILE *fin = fopen(file_name, "r");

    if (fin) {
        struct PGNHeader header;
        char szMove[16];
        if (!scanHeader(fin, &header)) {
            while (!scanMove(fin, szMove)) {
                CMove themove = p->ParseSAN(szMove);
                if (themove != M_NONE) {
                    p->DoMove(themove);
                }
            }
        }
        fclose(fin);
    }
}

int scanHeader(FILE *fin, struct PGNHeader *header) {
    static char buffer[1024];
    int nState = 0;

    /* Clear it */
    memset(header, 0, sizeof(struct PGNHeader));

    while (fgets(buffer, 1024, fin)) {
        if (buffer[0] == '[') {
            char *pszX = buffer + 1;
            char *pszKey = nextToken(&pszX, " \"");
            char *pszValue = nextToken(&pszX, "\"");

            if (!strcmp("Event", pszKey)) {
                strncpy(header->event, pszValue, sizeof(header->event) - 1);
            }
            if (!strcmp("Site", pszKey)) {
                strncpy(header->site, pszValue, sizeof(header->site) - 1);
            }
            if (!strcmp("Date", pszKey)) {
                strncpy(header->date, pszValue, sizeof(header->date) - 1);
            }
            if (!strcmp("Round", pszKey)) {
                strncpy(header->round, pszValue, sizeof(header->round) - 1);
            }
            if (!strcmp("White", pszKey)) {
                strncpy(header->white, pszValue, sizeof(header->white) - 1);
            }
            if (!strcmp("Black", pszKey)) {
                strncpy(header->black, pszValue, sizeof(header->black) - 1);
            }
            if (!strcmp("Result", pszKey)) {
                strncpy(header->result, pszValue, sizeof(header->result) - 1);
            }
            if (!strcmp("WhiteElo", pszKey)) {
                header->white_elo = atoi(pszValue);
            }
            if (!strcmp("BlackElo", pszKey)) {
                header->black_elo = atoi(pszValue);
            }
            if (!strcmp("FEN", pszKey)) {
                strncpy(header->fen, pszValue, sizeof(header->fen) - 1);
            }
            if (!strcmp("SetUp", pszKey)) {
                header->is_setup = atoi(pszValue);
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

int scanMove(FILE *fin, char *nextMove) {
    static char buffer[1024];
    static int haveLine = 0;
    static char *pszX;

    char *pszToken;

    int braces = 0;
    int parens = 0;

    do {
        if (!haveLine) {
            if (!fgets(buffer, 1024, fin))
                return 1;
            haveLine = 1;
            pszX = buffer;
        }

        if (braces) {
            if (*pszX == '\0') {
                haveLine = 0;
                continue;
            }

            *(comment_ptr) = *pszX;

            if (*(pszX++) == '}') {
                *comment_ptr = 0;
                braces = 0;
            }

            comment_ptr++;

            continue;
        } else if (parens) {
            if (*pszX == '\0') {
                haveLine = 0;
                continue;
            }

            if (*pszX == ')') {
                parens--;
            }
            if (*pszX == '(') {
                parens++;
            }
            pszX++;

            continue;
        } else {
            if (pszX && *pszX == '{') {
                braces = 1;
                pszX++;
                comment_ptr = comment_buffer;
                continue;
            }
            if (pszX && *pszX == '(') {
                parens = 1;
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

        strcpy(nextMove, pszToken);
        return 0;
    } while (1);

    return 1;
}

void get_and_reset_comment(char *destination, unsigned int dwLength) {
    strncpy(destination, comment_buffer, dwLength);

    comment_ptr = comment_buffer;
    *comment_buffer = 0;
}

void print_header(FILE *fout, struct PGNHeader *header) {
    fprintf(fout, "[Event \"%s\"]\n", header->event);
    fprintf(fout, "[Site \"%s\"]\n", header->site);
    fprintf(fout, "[Date \"%s\"]\n", header->date);
    fprintf(fout, "[Round \"%s\"]\n", header->round);
    fprintf(fout, "[White \"%s\"]\n", header->white);
    fprintf(fout, "[Black \"%s\"]\n", header->black);
    fprintf(fout, "[Result \"%s\"]\n", header->result);
    if (header->is_setup) {
        fprintf(fout, "[SetUp \"1\"]\n");
        fprintf(fout, "[FEN \"%s\"]\n", header->fen);
    }
    fprintf(fout, "\n");
}
