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

#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "safe_malloc.h"
#include "search.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if HAVE_LIBPTHREAD
#include <pthread.h>
#endif

extern unsigned long DblExt, DiscExt, SingExt;
extern int ExtendDoubleCheck, ExtendDiscoveredCheck, ExtendSingularReply;
extern CMove PBMove;
extern char BestLine[2048];
extern char ShortBestLine[2048];

#if MP
extern int NumberOfCPUs;
extern bool AbortSearch;
extern void StopHelpers(void);
extern void *IterateInt(void *x);
#if HAVE_LIBPTHREAD
extern pthread_t *tids;
#endif
#endif

/*
 * Decide wether to extend the check due to the following conditions:
 *  - double check
 *  - discovered check
 *  - check with only one legal response
 *
 */

int CPosition::CheckExtend() {
    CPosition *p = this;
    int kp = p->m_rgKingSq[p->m_nTurn].BitOffset();
    CBitBoard att;

    att = p->m_rgAtkFr[kp] & p->m_rgMask[OPP(p->m_nTurn)][0];

    if ((att).CountBits() > 1) {

        /*
         * double check, the king has to move
         * count no. of flight squares, if only one, extend deeper
         *
         */

        CBitBoard ff;

        int i;
        int cnt = 0;

        DblExt++;

        ff = KingEPM[kp] & ~p->m_rgMask[p->m_nTurn][0];
        att &= p->m_SlidingPieces;

        while (att) {
            i = (att).FindSetBit();
            att.ClearLowestBit();
            ff &= ~Ray[i][kp];
        }

        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                cnt++;
            if (cnt > 1)
                return ExtendDoubleCheck;
        }
    } else {
        CBitBoard ff;
        CBitBoard def;
        CBitBoard tmp;

        int atp = (att).FindSetBit();
        int cnt = 0;
        int i;
        int nd = 0;

        /* discovered check */
        if (atp != (p->m_pActLog - 1)->gl_Move.GetToCoord().BitOffset()) {
            DiscExt++;
            nd = ExtendDiscoveredCheck;
        }

        ff = KingEPM[kp] & ~p->m_rgMask[p->m_nTurn][0];

        i = (att).FindSetBit();
        if (p->m_SlidingPieces.TstBit(i))
            ff &= ~Ray[i][kp];

        /* check for king flight squares */
        while (ff) {
            i = (ff).FindSetBit();
            ff.ClearLowestBit();
            if (!(p->m_rgAtkFr[i] & p->m_rgMask[OPP(p->m_nTurn)][0]))
                cnt++;
            if (cnt > 1)
                return nd;
        }

        /* Find all non-pinned defenders */
        def = p->m_rgMask[p->m_nTurn][0] & ~p->m_rgMask[p->m_nTurn][King];

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Bishop] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              BishopEPM[kp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][kp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        tmp = (p->m_rgMask[OPP(p->m_nTurn)][Rook] | p->m_rgMask[OPP(p->m_nTurn)][Queen]) &
              RookEPM[kp];
        while (tmp) {
            CBitBoard tmp2;
            i = (tmp).FindSetBit();
            tmp.ClearLowestBit();
            tmp2 = InterPath[i][kp];
            if (tmp2 && !(p->m_rgMask[OPP(p->m_nTurn)][0] & tmp2)) {
                tmp2 &= p->m_rgMask[p->m_nTurn][0];
                if ((tmp2).CountBits() == 1) {
                    def.ClrBit((tmp2).FindSetBit());
                }
            }
        }

        /* All non-pinned defenders are in 'def' */
        tmp = p->m_rgAtkFr[atp] & def;

        cnt += (tmp).CountBits();
        if (cnt > 1)
            return nd;

        /* if possible, try an interposition */
        if (p->m_SlidingPieces.TstBit(atp)) {
            tmp = InterPath[atp][kp];
            while (tmp) {
                CBitBoard tmp2;
                i = (tmp).FindSetBit();
                tmp.ClearLowestBit();
                if ((tmp2 = p->m_rgAtkFr[i] & def)) {
                    cnt += (tmp2).CountBits();
                }
                if (p->m_nTurn == White && (i - 8) > 0 &&
                    p->m_rgMask[White][Pawn].TstBit(i - 8) && def.TstBit(i - 8))
                    cnt++;
                if (p->m_nTurn == Black && (i + 8) < 64 &&
                    p->m_rgMask[Black][Pawn].TstBit(i + 8) && def.TstBit(i + 8))
                    cnt++;
                if (cnt > 1)
                    return nd;
            }
        }
    }

    /* If we get here, we have only one legal move. */

    SingExt++;
    return ExtendSingularReply;
}

/*
 * Compute an optimistic score for a move.
 */

int CPosition::ScoreMove(CMove move) {
    CPosition *p = this;
    int score = 0;

    if (move.IsCapture())
        score += Value[TYPE(p->m_rgPiece[move.GetToCoord().BitOffset()])];
    if (move.HasPromotion())
        score += Value[PromoType(move)] - Value[Pawn];
    else if (TYPE(p->m_rgPiece[move.GetFromCoord().BitOffset()]) == Pawn) {
        if (p->m_nTurn == White && move.GetToCoord().BitOffset() >= a7) {
            score += Value[Bishop];
        }
        if (p->m_nTurn == Black && move.GetToCoord().BitOffset() <= h2) {
            score += Value[Bishop];
        }
    }

    if (move.IsEnPassant())
        score += Value[Pawn];

    return score;
}

/**
 * Print the SAN of a move prefixed by the move number.
 */
char *CPosition::NumberedSAN(CMove move, char *buffer, size_t len) {
    CPosition *p = this;
    char san_buffer[16];
    if (p->m_nTurn == White)
        snprintf(buffer, len, "%d. %s", 1 + (p->m_wPly + 1) / 2,
                 p->SAN(move, san_buffer));
    else
        snprintf(buffer, len, "%d. .. %s", 1 + p->m_wPly / 2,
                 p->SAN(move, san_buffer));

    return buffer;
}

/*
 * Analyze the hashtable to find the principal variation.
 */

void CPosition::AnaLoop(int depth) {
    CPosition *p = this;
    CMove move;
    bool dummy = false;
    int score;

#if MP
    if (ProbeHT(p->m_ullHKey, &score, 0, &move, &dummy, 0, 0, NULL) == Useless)
        return;
#else
    if (ProbeHT(p->m_ullHKey, &score, 0, &move, &dummy, 0) == Useless)
        return;
#endif

    if (p->Repeated(true) >= 2)
        return;

    if (p->LegalMove(move)) {
        int incheck;
        char buffer[16];

        p->DoMove(move);
        incheck = p->InCheck(OPP(p->m_nTurn));
        p->UndoMove(move);

        if (p->m_nTurn == White) {
            snprintf(buffer, sizeof(buffer), "%d. ", 1 + (p->m_wPly + 1) / 2);
            strcat(BestLine, buffer);
        }

        if (incheck) {
            strcat(BestLine, "<ill>");
            strcat(ShortBestLine, "<ill>");
            return;
        }

        char *san = p->SAN(move, buffer);
        strcat(BestLine, san);
        strcat(BestLine, " ");
        strcat(ShortBestLine, san);
        strcat(ShortBestLine, " ");

        /* save move to ponder on ... */
        if (depth == 1)
            PBMove = move;

        p->DoMove(move);
        p->AnaLoop(depth + 1);
        p->UndoMove(move);
    } else if (move == M_HASHED) {
        strcat(BestLine, "..");
        strcat(ShortBestLine, "..");
    } else if (move == M_NULL) {
        strcat(BestLine, "<null>");
        strcat(ShortBestLine, "<null>");
    }
}

void CPosition::AnalyzeHT(CMove move) {
    CPosition *p = this;
    p->NumberedSAN(move, BestLine, sizeof(BestLine));
    strcat(BestLine, " ");
    char san_buffer[16];
    strcpy(ShortBestLine, p->SAN(move, san_buffer));
    strcat(ShortBestLine, " ");
    p->DoMove(move);
    p->AnaLoop(1);
    p->UndoMove(move);
}

#if MP
/*
 * In parallel search start up all helper threads
 */

void CPosition::StartHelpers() {
    CPosition *p = this;
#if HAVE_LIBPTHREAD
    pthread_attr_t attr;
    int nthread;

    if (tids) {
        StopHelpers();
    }

    if (NumberOfCPUs < 2)
        return;

    tids = (pthread_t *)safe_calloc(NumberOfCPUs - 1, sizeof(pthread_t));

    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    /*
     * Start up the helper threads.
     */

    for (nthread = 0; nthread < (NumberOfCPUs - 1); nthread++) {
        CSearchData *sd = new CSearchData(CPosition::Clone(p));
        sd->m_fMaster = false;
        pthread_create(tids + nthread, &attr, &IterateInt, sd);
    }

#endif /* HAVE_LIBPTHREAD */
}
#endif /* MP */
