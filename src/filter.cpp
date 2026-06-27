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
#include "evaluation.h"
#include "inline.h"
#include "pgn.h"
#include "search.h"
#include "utils.h"

#include <string.h>

#define THRESHOLD 600

void FilterQuiescentPositions(char *file_name) {
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

        bool fLastPositionWasQuiet = true;

        while (!scanMove(fin, szMove)) {
            if (!(strlen(szMove) < 12)) {
                printf("\n<%s>\n", szMove);
                exit(1);
            }

            get_and_reset_comment(szComment, sizeof(szComment) - 1);

            if (fLastPositionWasQuiet) {
                fprintf(fout, "{ %s }\n", strip(szComment));
            }

            CMove TheMove = p->ParseSAN(szMove);

            if (TheMove == M_NONE)
                break;

            if ((p->GetPly() % 2) == 0) {
                fprintf(fout, "%d. ", 1 + p->GetPly() / 2);
            }
            fprintf(fout, "%s ", p->SAN(TheMove, szSanBuffer));

            fLastPositionWasQuiet = true;

            if (!p->GameEnd()) {
                const int nStaticEvaluation = EvaluatePosition(p);
                const int nDynamicEvaluation = p->QuiescenceSearch();

                const int nDiff = ABS(nStaticEvaluation - nDynamicEvaluation);

                if (nDiff > THRESHOLD) {
                    // p->ShowPosition();
                    // Print(0, "Static: %d Dynamic: %d\n", nStaticEvaluation,
                    //      nDynamicEvaluation);
                    fLastPositionWasQuiet = false;
                } else {
                    int nSearchEvaluation;
                    p->Iterate(&nSearchEvaluation, M_NONE, NULL);

                    const int nSearchDiff =
                        ABS(nStaticEvaluation - nSearchEvaluation);

                    if (nSearchDiff > THRESHOLD) {
                        fLastPositionWasQuiet = false;
                    }
                }
            }

            if (TheMove != M_NONE) {
                p->DoMove(TheMove);
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
