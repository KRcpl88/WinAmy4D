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
 * eco.c - ECO handling routines
 */

#include "amy.h"

#include <errno.h>
#include <string.h>

#include "dbase.h"
#include "tree.h"
#include "utils.h"

#define ECO_NAME "Eco.db"

#ifdef ECODIR
#define DEFAULT_ECO_NAME ECODIR PATH_SEPARATOR ECO_NAME
#else
#define DEFAULT_ECO_NAME "ECODIR\\ECO"
#endif

void ParseEcoPgn(char *pszFname) {
    FILE *pFin = fopen(pszFname, "rb");
    char szBuffer[1024];
    char szName[128];

    if (!pFin) {
        Print(0, "Cannot open file %s\n", pszFname);
        return;
    }

    tree_node_t *pNode = NULL;

    while (fgets(szBuffer, sizeof(szBuffer) - 1, pFin) != NULL) {
        char *pszX;
        strtok(szBuffer, " \t");
        pszX = strtok(NULL, "]\n\r");
        strncpy(szName, pszX, sizeof(szName) - 1);

        Print(0, ".");

        CPosition *p = CPosition::Initial();

        if (fgets(szBuffer, 1024, pFin) != NULL) {
            for (pszX = strtok(szBuffer, " \n\r\t"); pszX;
                 pszX = strtok(NULL, " \n\r\t")) {
                CMove Move = p->ParseSAN(pszX);
                if (Move != M_NONE) {
                    p->DoMove(Move);
                }
            }

            hash_t qwHashKey = p->GetHashKey();
            pNode = add_node(pNode, (char *)&qwHashKey, sizeof(qwHashKey), szName,
                            strlen(szName) + 1);
        }

        CPosition::Free(p);
    }

    fclose(pFin);

    FILE *pFout = fopen(ECO_NAME, "wb");
    if (pFout == NULL) {
        Print(0, "\nCannot save ECO database to %s: %s\n", ECO_NAME,
              strerror(errno));
        return;
    }

    save_tree(pNode, pFout);
    fclose(pFout);

    free_node(pNode);

    Print(0, "\nECO database created.\n");
}

static tree_node_t *EcoDB = NULL;

char *GetEcoCode(hash_t qwHkey) {
    char *pszRetval = NULL;

    if (EcoDB == NULL) {
        FILE *pFin = fopen(ECO_NAME, "rb");

#ifdef DEFAULT_ECO_NAME
        if (pFin == NULL) {
            pFin = fopen(DEFAULT_ECO_NAME, "rb");
        }
#endif

        if (pFin == NULL) {
            Print(0, "Can't open database: %s\n", strerror(errno));
            return NULL;
        }
        EcoDB = load_tree(pFin);
        fclose(pFin);
    }

    if (EcoDB != NULL) {
        pszRetval = (char *)lookup_value(EcoDB, (char *)&qwHkey, sizeof(qwHkey), NULL);
    }

    return pszRetval;
}

bool FindEcoCode(const CPosition *p, char *pszResult) {
    int nPly = 0;
    char *pszRes;
    bool fFound = false;

    while (nPly <= p->GetPly()) {
        hash_t qwKey = p->GetGameLog()[nPly].gl_HashKey;
        if (nPly == p->GetPly()) {
            qwKey = p->GetHashKey();
        }
        pszRes = GetEcoCode(qwKey);
        if (pszRes != NULL) {
            strcpy(pszResult, pszRes);
            fFound = true;
            free(pszRes);
        }
        nPly++;
    }

    return fFound;
}