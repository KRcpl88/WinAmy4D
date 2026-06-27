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

void ParseEcoPgn(char *fname) {
    FILE *fin = fopen(fname, "rb");
    char buffer[1024];
    char name[128];

    if (!fin) {
        Print(0, "Cannot open file %s\n", fname);
        return;
    }

    tree_node_t *node = NULL;

    while (fgets(buffer, sizeof(buffer) - 1, fin) != NULL) {
        char *pszX;
        strtok(buffer, " \t");
        pszX = strtok(NULL, "]\n\r");
        strncpy(name, pszX, sizeof(name) - 1);

        Print(0, ".");

        CPosition *p = CPosition::Initial();

        if (fgets(buffer, 1024, fin) != NULL) {
            for (pszX = strtok(buffer, " \n\r\t"); pszX;
                 pszX = strtok(NULL, " \n\r\t")) {
                CMove move = p->ParseSAN(pszX);
                if (move != M_NONE) {
                    p->DoMove(move);
                }
            }

            hash_t hashKey = p->GetHashKey();
            node = add_node(node, (char *)&hashKey, sizeof(hashKey), name,
                            strlen(name) + 1);
        }

        CPosition::Free(p);
    }

    fclose(fin);

    FILE *fout = fopen(ECO_NAME, "wb");
    if (fout == NULL) {
        Print(0, "\nCannot save ECO database to %s: %s\n", ECO_NAME,
              strerror(errno));
        return;
    }

    save_tree(node, fout);
    fclose(fout);

    free_node(node);

    Print(0, "\nECO database created.\n");
}

static tree_node_t *EcoDB = NULL;

char *GetEcoCode(hash_t hkey) {
    char *pszRetval = NULL;

    if (EcoDB == NULL) {
        FILE *fin = fopen(ECO_NAME, "rb");

#ifdef DEFAULT_ECO_NAME
        if (fin == NULL) {
            fin = fopen(DEFAULT_ECO_NAME, "rb");
        }
#endif

        if (fin == NULL) {
            Print(0, "Can't open database: %s\n", strerror(errno));
            return NULL;
        }
        EcoDB = load_tree(fin);
        fclose(fin);
    }

    if (EcoDB != NULL) {
        pszRetval = (char *)lookup_value(EcoDB, (char *)&hkey, sizeof(hkey), NULL);
    }

    return pszRetval;
}

bool FindEcoCode(const CPosition *p, char *pszResult) {
    int ply = 0;
    char *pszRes;
    bool found = false;

    while (ply <= p->GetPly()) {
        hash_t qwKey = p->GetGameLog()[ply].gl_HashKey;
        if (ply == p->GetPly()) {
            qwKey = p->GetHashKey();
        }
        pszRes = GetEcoCode(qwKey);
        if (pszRes != NULL) {
            strcpy(pszResult, pszRes);
            found = true;
            free(pszRes);
        }
        ply++;
    }

    return found;
}