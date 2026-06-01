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

static void SwapReRay(CPosition *p, int side, CBitBoard atks[2], int from,
                      int to, CBitBoard *exclude) {
    CBitBoard tmp;
    int i;
    int pc = TYPE(p->GetPiece(from));

    atks[side].ClrBit(from);
    exclude->ClrBit(from);

    if (pc == Pawn || pc == Bishop || pc == Queen) {
        tmp = p->GetAtkFr(from) & *exclude & Ray[to][from];
        if (tmp) {
            i = (tmp).FindSetBit();
            if (TYPE(p->GetPiece(i)) == Bishop || TYPE(p->GetPiece(i)) == Queen) {
                if (p->GetPiece(i) > 0) {
                    atks[White].SetBit(i);
                } else {
                    atks[Black].SetBit(i);
                }
            }
        }
    }

    if (pc == Rook || pc == Queen) {
        tmp = p->GetAtkFr(from) & *exclude & Ray[to][from];
        if (tmp) {
            i = (tmp).FindSetBit();
            if (TYPE(p->GetPiece(i)) == Rook || TYPE(p->GetPiece(i)) == Queen) {
                if (p->GetPiece(i) > 0) {
                    atks[White].SetBit(i);
                } else {
                    atks[Black].SetBit(i);
                }
            }
        }
    }
}

int SwapOff(CPosition *p, CMove move) {
    int to = move.GetToCoord().BitOffset();
    int fr = move.GetFromCoord().BitOffset();
    int side = COLOR(p->GetPiece(fr));
    int oside = !side;
    int swaplist[32];
    int swapcnt = 0;
    int swapval, swapside;
    int swapsign = -1;

    CBitBoard atks[2];
    CBitBoard exclude;

    if (move.HasPromotion()) {
        swapval = SwapValue[PromoType(move)];
        swaplist[0] = SwapValue[TYPE(p->GetPiece(to))] - SwapValue[Pawn] + swapval;
    } else {
        swapval = SwapValue[TYPE(p->GetPiece(fr))];
        swaplist[0] = SwapValue[TYPE(p->GetPiece(to))];
    }

    swapside = oside;

    atks[White] = p->GetMask(White, 0) & p->GetAtkFr(to);
    atks[Black] = p->GetMask(Black, 0) & p->GetAtkFr(to);

    exclude = p->GetMask(White, 0) | p->GetMask(Black, 0);

    SwapReRay(p, side, atks, fr, to, &exclude);

    while (atks[swapside]) {
        int at;
        CBitBoard tmp;

        /* find last valuable attacker */
        tmp = p->GetMask(swapside, Pawn) & atks[swapside];
        if (tmp)
            at = (tmp).FindSetBit();
        else {
            tmp = (p->GetMask(swapside, Knight) | p->GetMask(swapside, Bishop)) &
                  atks[swapside];
            if (tmp)
                at = (tmp).FindSetBit();
            else {
                tmp = p->GetMask(swapside, Rook) & atks[swapside];
                if (tmp)
                    at = (tmp).FindSetBit();
                else {
                    tmp = p->GetMask(swapside, Queen) & atks[swapside];
                    if (tmp)
                        at = (tmp).FindSetBit();
                    else
                        at = (p->GetMask(swapside, King)).FindSetBit();
                }
            }
        }

        swapcnt++;
        swaplist[swapcnt] = swaplist[swapcnt - 1] + swapsign * swapval;
        swapval = SwapValue[TYPE(p->GetPiece(at))];
        swapsign = -swapsign;

        SwapReRay(p, swapside, atks, at, to, &exclude);

        swapside = !swapside;
    }

    if (swapcnt & 1)
        swapsign = -1;
    else
        swapsign = 1;
    while (swapcnt) {
        if (swapsign < 0) {
            if (swaplist[swapcnt] <= swaplist[swapcnt - 1])
                swaplist[swapcnt - 1] = swaplist[swapcnt];
        } else {
            if (swaplist[swapcnt] >= swaplist[swapcnt - 1])
                swaplist[swapcnt - 1] = swaplist[swapcnt];
        }
        swapcnt--;
        swapsign = -swapsign;
    }

    return (swaplist[0]);
}
