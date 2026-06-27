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

#include "dbase.h"
#include "pgn.h"
#include "safe_malloc.h"
#include "search.h"
#include "types.h"
#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define strtok_r strtok_s
#endif

#define THRESHOLD 2000

CMove get_best_move_from_comment(char *pszComment, CPosition *p,
                                  char *pszEvalBuf) {
    char *psz = pszComment;

    CMove best_move = M_NONE;
    int best_count = -1;

    while (*psz && *psz != '=') {
        psz++;
    }
    psz++;

    char *pszStartEval = psz;

    while (*psz && *psz != ';') {
        psz++;
    }
    psz++;

    size_t qwEvalLen = psz - pszStartEval - 1;
    strncpy(pszEvalBuf, pszStartEval, qwEvalLen);
    pszEvalBuf[qwEvalLen] = '\0';

    while (*psz && *psz != '[') {
        psz++;
    }
    psz++;

    size_t qwLen = strlen(psz);

    char *pszBuffer = (char *)safe_malloc(qwLen + 1);

    strncpy(pszBuffer, psz, qwLen + 1);

    char *pszBrko;

    for (char *pszElem = strtok_r(pszBuffer, ", ]", &pszBrko); pszElem;
         pszElem = strtok_r(NULL, ", ]", &pszBrko)) {
        char *pszBrki;

        char *pszMove = strtok_r(pszElem, ":", &pszBrki);
        char *pszCount = strtok_r(NULL, ":", &pszBrki);

        CMove parsed_move = p->ParseSAN(pszMove);

        if (parsed_move != M_NONE) {
            int parsed_count = atoi(pszCount);
            if (parsed_count > best_count) {
                best_move = parsed_move;
                best_count = parsed_count;
            }
        }
    }

    free(pszBuffer);

    return best_move;
}

void BlunderCheck(char *file_name) {
    struct PGNHeader header;
    char szMove[12];
    char szComment[2048];
    char szSanBuffer[16];

    FILE *fin = fopen(file_name, "r");
    if (fin == NULL) {
        Print(0, "Cannot open input file %s\n");
        return;
    }

    FILE *fout = fopen("filtered.pgn", "w");

    while (!scanHeader(fin, &header)) {
        CPosition *p;

        if (header.is_setup) {
            p = CPosition::CreateFromEPD(header.fen);
        } else {
            p = CPosition::Initial();
        }

        if (p == NULL) {
            Print(0, "Skipping game with invalid setup EPD.\n");
            continue;
        }

        print_header(fout, &header);

        CMove LastMove = M_NONE;

        while (!scanMove(fin, szMove)) {
            if (!(strlen(szMove) < 12)) {
                printf("\n<%s>\n", szMove);
                exit(1);
            }

            get_and_reset_comment(szComment, sizeof(szComment) - 1);

            if (LastMove != M_NONE && strlen(szComment) != 0) {
                char szSanbuf[16], szEvalbuf[16];

                p->UndoMove(LastMove);

                CMove best_move =
                    get_best_move_from_comment(szComment, p, szEvalbuf);

                int nSearchEvaluation;
                int nAlternateEvaluation;

                CMove OptimalMove = p->Iterate(&nSearchEvaluation, best_move,
                                                &nAlternateEvaluation);
                int nScoreDiff = nSearchEvaluation - nAlternateEvaluation;

                if (OptimalMove != best_move && nScoreDiff >= 1500) {
                    p->ShowPosition();
                    Print(1, ">>> best move from comment: %s\n",
                          p->SAN(best_move, szSanbuf));
                    Print(1, ">>> optimal move: %s\n",
                          p->SAN(OptimalMove, szSanbuf));
                    Print(1, ">>> score diff: %d\n",
                          nSearchEvaluation - nAlternateEvaluation);

                    fprintf(fout, "{ q=%s; p=[%s:100] } ", szEvalbuf,
                            p->SAN(OptimalMove, szSanbuf));
                }

                p->DoMove(LastMove);
            }

            CMove TheMove = p->ParseSAN(szMove);

            if (TheMove == M_NONE)
                break;

            if ((p->GetPly() % 2) == 0) {
                fprintf(fout, "%d. ", 1 + p->GetPly() / 2);
            }
            fprintf(fout, "%s ", p->SAN(TheMove, szSanBuffer));

            if (TheMove != M_NONE) {
                p->DoMove(TheMove);
                LastMove = TheMove;
            } else {
                break;
            }
        }

        fprintf(fout, "%s\n\n", header.result);
        get_and_reset_comment(szComment, sizeof(szComment) - 1);

        CPosition::Free(p);
    }

    Print(0, "\n");
    fclose(fin);
}
