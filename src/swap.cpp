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
 * swap.c - static exchange evaluation routines
 */

#include "amy.h"

#include "dbase.h"
#include "init.h"
#include "inline.h"

static int SwapValue[] = {
    0,    100, /* Pawn */
    300,       /* Knight */
    300,       /* Bishop */
    500,       /* Rook */
    900,       /* Queen */
    10000      /* King, whose value is basically infinity */
};

static void SwapReRay(CPosition *p, int nSide, CBitBoard rgAtks[2], int nFrom,
                      int nTo, CBitBoard *pExclude) {
    CBitBoard tmp;
    int nI;
    int nPc = TYPE(p->GetPiece(nFrom));

    rgAtks[nSide].ClrBit(nFrom);
    pExclude->ClrBit(nFrom);

    if (nPc == Pawn || nPc == Bishop || nPc == Queen) {
        tmp = p->GetAtkFr(nFrom) & *pExclude & Ray[nTo][nFrom];
        if (tmp) {
            nI = (tmp).FindSetBit();
            if (TYPE(p->GetPiece(nI)) == Bishop || TYPE(p->GetPiece(nI)) == Queen) {
                if (p->GetPiece(nI) > 0) {
                    rgAtks[White].SetBit(nI);
                } else {
                    rgAtks[Black].SetBit(nI);
                }
            }
        }
    }

    if (nPc == Rook || nPc == Queen) {
        tmp = p->GetAtkFr(nFrom) & *pExclude & Ray[nTo][nFrom];
        if (tmp) {
            nI = (tmp).FindSetBit();
            if (TYPE(p->GetPiece(nI)) == Rook || TYPE(p->GetPiece(nI)) == Queen) {
                if (p->GetPiece(nI) > 0) {
                    rgAtks[White].SetBit(nI);
                } else {
                    rgAtks[Black].SetBit(nI);
                }
            }
        }
    }
}

int SwapOff(CPosition *p, CMove move) {
    int nTo = move.GetToCoord().BitOffset();
    int nFr = move.GetFromCoord().BitOffset();
    int nSide = COLOR(p->GetPiece(nFr));
    int nOside = !nSide;
    int rgSwapList[32];
    int nSwapCnt = 0;
    int nSwapVal, nSwapSide;
    int nSwapSign = -1;

    CBitBoard rgAtks[2];
    CBitBoard exclude;

    if (move.HasPromotion()) {
        nSwapVal = SwapValue[PromoType(move)];
        rgSwapList[0] = SwapValue[TYPE(p->GetPiece(nTo))] - SwapValue[Pawn] + nSwapVal;
    } else {
        nSwapVal = SwapValue[TYPE(p->GetPiece(nFr))];
        rgSwapList[0] = SwapValue[TYPE(p->GetPiece(nTo))];
    }

    nSwapSide = nOside;

    rgAtks[White] = p->GetMask(White, 0) & p->GetAtkFr(nTo);
    rgAtks[Black] = p->GetMask(Black, 0) & p->GetAtkFr(nTo);

    exclude = p->GetMask(White, 0) | p->GetMask(Black, 0);

    SwapReRay(p, nSide, rgAtks, nFr, nTo, &exclude);

    while (rgAtks[nSwapSide]) {
        int nAt;
        CBitBoard tmp;

        /* find last valuable attacker */
        tmp = p->GetMask(nSwapSide, Pawn) & rgAtks[nSwapSide];
        if (tmp)
            nAt = (tmp).FindSetBit();
        else {
            tmp = (p->GetMask(nSwapSide, Knight) | p->GetMask(nSwapSide, Bishop)) &
                  rgAtks[nSwapSide];
            if (tmp)
                nAt = (tmp).FindSetBit();
            else {
                tmp = p->GetMask(nSwapSide, Rook) & rgAtks[nSwapSide];
                if (tmp)
                    nAt = (tmp).FindSetBit();
                else {
                    tmp = p->GetMask(nSwapSide, Queen) & rgAtks[nSwapSide];
                    if (tmp)
                        nAt = (tmp).FindSetBit();
                    else
                        nAt = (p->GetMask(nSwapSide, King)).FindSetBit();
                }
            }
        }

        nSwapCnt++;
        rgSwapList[nSwapCnt] = rgSwapList[nSwapCnt - 1] + nSwapSign * nSwapVal;
        nSwapVal = SwapValue[TYPE(p->GetPiece(nAt))];
        nSwapSign = -nSwapSign;

        SwapReRay(p, nSwapSide, rgAtks, nAt, nTo, &exclude);

        nSwapSide = !nSwapSide;
    }

    if (nSwapCnt & 1)
        nSwapSign = -1;
    else
        nSwapSign = 1;
    while (nSwapCnt) {
        if (nSwapSign < 0) {
            if (rgSwapList[nSwapCnt] <= rgSwapList[nSwapCnt - 1])
                rgSwapList[nSwapCnt - 1] = rgSwapList[nSwapCnt];
        } else {
            if (rgSwapList[nSwapCnt] >= rgSwapList[nSwapCnt - 1])
                rgSwapList[nSwapCnt - 1] = rgSwapList[nSwapCnt];
        }
        nSwapCnt--;
        nSwapSign = -nSwapSign;
    }

    return (rgSwapList[0]);
}
