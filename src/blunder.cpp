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

CMove get_best_move_from_comment(char *comment, CPosition *p,
                                  char *pszEvalBuf) {
    char *ptr = comment;

    CMove best_move = M_NONE;
    int best_count = -1;

    while (*ptr && *ptr != '=') {
        ptr++;
    }
    ptr++;

    char *pszStartEval = ptr;

    while (*ptr && *ptr != ';') {
        ptr++;
    }
    ptr++;

    size_t qwEvalLen = ptr - pszStartEval - 1;
    strncpy(pszEvalBuf, pszStartEval, qwEvalLen);
    pszEvalBuf[qwEvalLen] = '\0';

    while (*ptr && *ptr != '[') {
        ptr++;
    }
    ptr++;

    size_t qwLen = strlen(ptr);

    char *buffer = (char *)safe_malloc(qwLen + 1);

    strncpy(buffer, ptr, qwLen + 1);

    char *brko;

    for (char *pszElem = strtok_r(buffer, ", ]", &brko); pszElem;
         pszElem = strtok_r(NULL, ", ]", &brko)) {
        char *brki;

        char *pszMove = strtok_r(pszElem, ":", &brki);
        char *count = strtok_r(NULL, ":", &brki);

        CMove parsed_move = p->ParseSAN(pszMove);

        if (parsed_move != M_NONE) {
            int parsed_count = atoi(count);
            if (parsed_count > best_count) {
                best_move = parsed_move;
                best_count = parsed_count;
            }
        }
    }

    free(buffer);

    return best_move;
}

void BlunderCheck(char *file_name) {
    struct PGNHeader header;
    char szMove[12];
    char comment[2048];
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

        CMove last_move = M_NONE;

        while (!scanMove(fin, szMove)) {
            if (!(strlen(szMove) < 12)) {
                printf("\n<%s>\n", szMove);
                exit(1);
            }

            get_and_reset_comment(comment, sizeof(comment) - 1);

            if (last_move != M_NONE && strlen(comment) != 0) {
                char szSanbuf[16], szEvalbuf[16];

                p->UndoMove(last_move);

                CMove best_move =
                    get_best_move_from_comment(comment, p, szEvalbuf);

                int nSearchEvaluation;
                int nAlternateEvaluation;

                CMove optimal_move = p->Iterate(&nSearchEvaluation, best_move,
                                                &nAlternateEvaluation);
                int nScoreDiff = nSearchEvaluation - nAlternateEvaluation;

                if (optimal_move != best_move && nScoreDiff >= 1500) {
                    p->ShowPosition();
                    Print(1, ">>> best move from comment: %s\n",
                          p->SAN(best_move, szSanbuf));
                    Print(1, ">>> optimal move: %s\n",
                          p->SAN(optimal_move, szSanbuf));
                    Print(1, ">>> score diff: %d\n",
                          nSearchEvaluation - nAlternateEvaluation);

                    fprintf(fout, "{ q=%s; p=[%s:100] } ", szEvalbuf,
                            p->SAN(optimal_move, szSanbuf));
                }

                p->DoMove(last_move);
            }

            CMove themove = p->ParseSAN(szMove);

            if (themove == M_NONE)
                break;

            if ((p->GetPly() % 2) == 0) {
                fprintf(fout, "%d. ", 1 + p->GetPly() / 2);
            }
            fprintf(fout, "%s ", p->SAN(themove, szSanBuffer));

            if (themove != M_NONE) {
                p->DoMove(themove);
                last_move = themove;
            } else {
                break;
            }
        }

        fprintf(fout, "%s\n\n", header.result);
        get_and_reset_comment(comment, sizeof(comment) - 1);

        CPosition::Free(p);
    }

    Print(0, "\n");
    fclose(fin);
}
