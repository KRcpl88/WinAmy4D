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
 * mates.c - mate threat detection routines
 */

#include "amy.h"

#include "dbase.h"
#include "init.h"

bool MateThreat(CPosition *p, int nSide) {
    int nOside = !nSide;
    int nEkp = p->GetKingSq(nOside).BitOffset();
    CBitBoard pcs;
    CBitBoard KSafe;
    int nFr;

    KSafe = p->GetAtkTo(nEkp) & ~p->GetMask(nOside, 0);

    /*
     * Queen checks
     */

    pcs = p->GetMask(nSide, Queen);
    while (pcs) {
        int nTo;
        CBitBoard Moves;
        nFr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        Moves = (p->GetAtkTo(nFr) & QueenEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (Moves) {
            CBitBoard Tmp;
            nTo = (Moves).FindSetBit();
            Moves.ClearLowestBit();
            /* check whether path is obstructed */
            Tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & Tmp) || (p->GetMask(Black, 0) & Tmp))
                continue;
            /* check wether all flight squares are covered */
            Tmp = KSafe & ~QueenEPM[nTo];
            if (Tmp) {
                int nFlight;
                int nFree = 0;
                do {
                    CBitBoard Att;
                    nFlight = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Att = p->GetAtkFr(nFlight) & p->GetMask(nSide, 0);
                    Att.ClrBit(nFr);
                    if (!Att)
                        nFree++;
                    if (nFree)
                        break;
                } while (Tmp);
                if (nFree)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard Ray;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                Tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                Ray = ::Ray[nTo][nFr] & p->GetAtkFr(nFr);
                if ((p->GetMask(nOside, Queen) & Ray) ||
                    (p->GetMask(nOside, Rook) & Ray) ||
                    (p->GetMask(nOside, Bishop) & Ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & Tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & Ray) ||
                    (p->GetMask(nSide, Rook) & Ray) ||
                    (p->GetMask(nSide, Queen) & Ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int nDef = 0;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                Tmp = InterPath[nTo][nEkp];
                while (Tmp) {
                    CBitBoard Tmp2;
                    nInter = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((Tmp2).CountBits() < 2)
                        continue;
                    nDef++;
                    break;
                }
                if (!nDef) {
                    return true;
                }
            }
        }
    }

    /*
     * Rook checks
     */

    pcs = p->GetMask(nSide, Rook);
    while (pcs) {
        int nTo;
        CBitBoard Moves;
        nFr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        Moves = (p->GetAtkTo(nFr) & RookEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (Moves) {
            CBitBoard Tmp;
            nTo = (Moves).FindSetBit();
            Moves.ClearLowestBit();
            /* check whether path is obstructed */
            Tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & Tmp) || (p->GetMask(Black, 0) & Tmp))
                continue;
            /* check wether all flight squares are covered */
            Tmp = KSafe & ~RookEPM[nTo];
            if (Tmp) {
                int nFlight;
                int nFree = 0;
                do {
                    CBitBoard Att;
                    nFlight = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Att = p->GetAtkFr(nFlight) & p->GetMask(nSide, 0);
                    Att.ClrBit(nFr);
                    if (!Att)
                        nFree++;
                    if (nFree)
                        break;
                } while (Tmp);
                if (nFree)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard Ray;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                Tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                Ray = ::Ray[nTo][nFr] & p->GetAtkFr(nFr);
                if ((p->GetMask(nOside, Queen) & Ray) ||
                    (p->GetMask(nOside, Rook) & Ray) ||
                    (p->GetMask(nOside, Bishop) & Ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & Tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & Ray) ||
                    (p->GetMask(nSide, Rook) & Ray) ||
                    (p->GetMask(nSide, Queen) & Ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int nDef = 0;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                Tmp = InterPath[nTo][nEkp];
                while (Tmp) {
                    CBitBoard Tmp2;
                    nInter = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((Tmp2).CountBits() < 2)
                        continue;
                    nDef++;
                    break;
                }
                if (!nDef) {
                    return true;
                }
            }
        }
    }

    /*
     * Bishop checks
     */

    pcs = p->GetMask(nSide, Bishop);
    while (pcs) {
        int nTo;
        CBitBoard Moves;
        nFr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        Moves = (p->GetAtkTo(nFr) & BishopEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (Moves) {
            CBitBoard Tmp;
            nTo = (Moves).FindSetBit();
            Moves.ClearLowestBit();
            /* check whether path is obstructed */
            Tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & Tmp) || (p->GetMask(Black, 0) & Tmp))
                continue;
            /* check wether all flight squares are covered */
            Tmp = KSafe & ~BishopEPM[nTo];
            if (Tmp) {
                int nFlight;
                int nFree = 0;
                do {
                    CBitBoard Att;
                    nFlight = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Att = p->GetAtkFr(nFlight) & p->GetMask(nSide, 0);
                    Att.ClrBit(nFr);
                    if (!Att)
                        nFree++;
                    if (nFree)
                        break;
                } while (Tmp);
                if (nFree)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard Ray;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                Tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                Ray = ::Ray[nTo][nFr] & p->GetAtkFr(nFr);
                if ((p->GetMask(nOside, Queen) & Ray) ||
                    (p->GetMask(nOside, Rook) & Ray) ||
                    (p->GetMask(nOside, Bishop) & Ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & Tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & Ray) ||
                    (p->GetMask(nSide, Rook) & Ray) ||
                    (p->GetMask(nSide, Queen) & Ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int nDef = 0;
                Tmp = p->GetAtkFr(nTo);
                Tmp.ClrBit(nFr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & Tmp)
                    continue;
                Tmp = InterPath[nTo][nEkp];
                while (Tmp) {
                    CBitBoard Tmp2;
                    nInter = (Tmp).FindSetBit();
                    Tmp.ClearLowestBit();
                    Tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((Tmp2).CountBits() < 2)
                        continue;
                    nDef++;
                    break;
                }
                if (!nDef) {
                    return true;
                }
            }
        }
    }

    /*
     * Knight checks
     */

    pcs = p->GetMask(nSide, Knight);
    while (pcs) {
        int nTo;
        CBitBoard Moves;
        nFr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        Moves = (p->GetAtkTo(nFr) & KnightEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (Moves) {
            CBitBoard def;
            nTo = (Moves).FindSetBit();
            Moves.ClearLowestBit();
            /*
             * check whether the square is defended. If so, the defender
             * must not be pinned.
             */
            def = p->GetAtkFr(nTo) & p->GetMask(nOside, 0);
            if ((def).CountBits() == 1) {
                int nDe = (def).FindSetBit();
                CBitBoard Tmp;
                if (RookEPM[nEkp] & def) {
                    Tmp = p->GetAtkFr(nDe) & ::Ray[nEkp][nDe];
                    if (!(p->GetMask(nSide, Queen) & Tmp) &&
                        !(p->GetMask(nSide, Rook) & Tmp))
                        continue;
                } else if (BishopEPM[nEkp] & def) {
                    Tmp = p->GetAtkFr(nDe) & ::Ray[nEkp][nDe];
                    if (!(p->GetMask(nSide, Queen) & Tmp) &&
                        !(p->GetMask(nSide, Bishop) & Tmp))
                        continue;
                } else
                    continue;
            } else if (def)
                continue;
            def = KSafe & ~KnightEPM[nTo];
            if (def) {
                int nFlight;
                int nFree = 0;
                do {
                    CBitBoard Att;
                    nFlight = (def).FindSetBit();
                    def.ClearLowestBit();
                    Att = p->GetAtkFr(nFlight) & p->GetMask(nSide, 0);
                    Att.ClrBit(nFr);
                    if (!Att)
                        nFree++;
                    if (nFree)
                        break;
                } while (def);
                if (nFree)
                    continue;
            }
            return true;
        }
    }

    return false;
}
