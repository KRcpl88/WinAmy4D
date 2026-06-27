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
 * search_io.c - output search results
 */

#include "amy.h"

#include "state_machine.h"
#include "utils.h"
#include <stddef.h>
#include <string.h>

#define PV_BUFFER_SIZE 512

static void PrintPV(char *pszPv) {
    char szPVBuffer[512];
    char *pszX;
    size_t qwLen = 21;

    strncpy(szPVBuffer, pszPv, sizeof(szPVBuffer) - 1);

    for (pszX = szPVBuffer; *pszX;) {
        char *pszY = pszX;

        while (*pszY != ' ' && *pszY != '\0')
            pszY++;
        if (*pszY == '\0')
            *(pszY + 1) = '\0';
        *pszY = '\0';

        qwLen += strlen(pszX);

        if (qwLen >= 79) {
            Print(1, "\n                    ");
            qwLen = 21 + strlen(pszX);
        }
        Print(1, "%s ", pszX);
        qwLen += 1;
        pszX = pszY + 1;
    }
    Print(1, "\n");
}

void SearchHeader(void) {
    Print(1, "It    Time   Score  principal Variation\n");
}

void SearchOutput(int nDepth, unsigned long dwTime, int nScore, char *pszLine,
                  unsigned long dwNodes) {
    char szTimeAsText[16];
    char szScoreAsText[16];

    Print(1, "%2d  %s %7s  ", nDepth,
          FormatTime(dwTime, szTimeAsText, sizeof(szTimeAsText)),
          FormatScore(nScore, szScoreAsText, sizeof(szScoreAsText)));
    PrintPV(pszLine);

    if (PostMode) {
        char *pszShortLine = _strdup(pszLine);
        int nS = nScore / 10;

        if (nS >= 9999) {
            nS = 9999;
        } else if (nS <= -9999) {
            nS = -9999;
        }

        if (pszShortLine) {
            if (strlen(pszShortLine) > 80) {
                int nIdx;
                for (nIdx = 80; nIdx > 1; nIdx--) {
                    if (pszShortLine[nIdx] == ' ' && pszShortLine[nIdx - 1] != '.') {
                        break;
                    }
                }
                pszShortLine[nIdx] = '\0';
            }
            PrintDebug(0, "%d %d %d %lu %s\n", nDepth, nS, dwTime, dwNodes,
                       pszShortLine);
            free(pszShortLine);
        }
    }
}

void SearchOutputFailHighLow(int nDepth, unsigned long dwTime, int nIsfailhigh,
                             char *pszMove, unsigned long dwNodes) {
    char szTimeAsText[16];

    if (nIsfailhigh) {
        Print(1, "%2d  %s     +++  %s\n", nDepth,
              FormatTime(dwTime, szTimeAsText, sizeof(szTimeAsText)), pszMove);
        if (PostMode) {
            PrintDebug(0, "%d 0 %d %lu %s!\n", nDepth, dwTime, dwNodes, pszMove);
        }
    } else {
        Print(1, "%2d  %s     ---  %s\n", nDepth,
              FormatTime(dwTime, szTimeAsText, sizeof(szTimeAsText)), pszMove);
        if (PostMode) {
            PrintDebug(0, "%d 0 %d %lu %s?\n", nDepth, dwTime, dwNodes, pszMove);
        }
    }
}