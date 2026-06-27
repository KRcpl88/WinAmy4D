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
    CBitBoard ksafe;
    int fr;

    ksafe = p->GetAtkTo(nEkp) & ~p->GetMask(nOside, 0);

    /*
     * Queen checks
     */

    pcs = p->GetMask(nSide, Queen);
    while (pcs) {
        int nTo;
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->GetAtkTo(fr) & QueenEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (mvs) {
            CBitBoard tmp;
            nTo = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & tmp) || (p->GetMask(Black, 0) & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~QueenEPM[nTo];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->GetAtkFr(flight) & p->GetMask(nSide, 0);
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[nTo][fr] & p->GetAtkFr(fr);
                if ((p->GetMask(nOside, Queen) & ray) ||
                    (p->GetMask(nOside, Rook) & ray) ||
                    (p->GetMask(nOside, Bishop) & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & ray) ||
                    (p->GetMask(nSide, Rook) & ray) ||
                    (p->GetMask(nSide, Queen) & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int def = 0;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                tmp = InterPath[nTo][nEkp];
                while (tmp) {
                    CBitBoard tmp2;
                    nInter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((tmp2).CountBits() < 2)
                        continue;
                    def++;
                    break;
                }
                if (!def) {
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
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->GetAtkTo(fr) & RookEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (mvs) {
            CBitBoard tmp;
            nTo = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & tmp) || (p->GetMask(Black, 0) & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~RookEPM[nTo];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->GetAtkFr(flight) & p->GetMask(nSide, 0);
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[nTo][fr] & p->GetAtkFr(fr);
                if ((p->GetMask(nOside, Queen) & ray) ||
                    (p->GetMask(nOside, Rook) & ray) ||
                    (p->GetMask(nOside, Bishop) & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & ray) ||
                    (p->GetMask(nSide, Rook) & ray) ||
                    (p->GetMask(nSide, Queen) & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int def = 0;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                tmp = InterPath[nTo][nEkp];
                while (tmp) {
                    CBitBoard tmp2;
                    nInter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((tmp2).CountBits() < 2)
                        continue;
                    def++;
                    break;
                }
                if (!def) {
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
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->GetAtkTo(fr) & BishopEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (mvs) {
            CBitBoard tmp;
            nTo = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[nEkp][nTo];
            if ((p->GetMask(White, 0) & tmp) || (p->GetMask(Black, 0) & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~BishopEPM[nTo];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->GetAtkFr(flight) & p->GetMask(nSide, 0);
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->GetAtkTo(nEkp).TstBit(nTo)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                tmp.ClrBit(nEkp);
                /* square is defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[nTo][fr] & p->GetAtkFr(fr);
                if ((p->GetMask(nOside, Queen) & ray) ||
                    (p->GetMask(nOside, Rook) & ray) ||
                    (p->GetMask(nOside, Bishop) & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->GetMask(nSide, 0) & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->GetMask(nSide, Bishop) & ray) ||
                    (p->GetMask(nSide, Rook) & ray) ||
                    (p->GetMask(nSide, Queen) & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int nInter;
                int def = 0;
                tmp = p->GetAtkFr(nTo);
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->GetMask(nOside, 0) & tmp)
                    continue;
                tmp = InterPath[nTo][nEkp];
                while (tmp) {
                    CBitBoard tmp2;
                    nInter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->GetAtkFr(nInter) & p->GetMask(nOside, 0);
                    if ((tmp2).CountBits() < 2)
                        continue;
                    def++;
                    break;
                }
                if (!def) {
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
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->GetAtkTo(fr) & KnightEPM[nEkp]) & ~p->GetMask(nSide, 0);
        while (mvs) {
            CBitBoard def;
            nTo = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /*
             * check whether the square is defended. If so, the defender
             * must not be pinned.
             */
            def = p->GetAtkFr(nTo) & p->GetMask(nOside, 0);
            if ((def).CountBits() == 1) {
                int de = (def).FindSetBit();
                CBitBoard tmp;
                if (RookEPM[nEkp] & def) {
                    tmp = p->GetAtkFr(de) & Ray[nEkp][de];
                    if (!(p->GetMask(nSide, Queen) & tmp) &&
                        !(p->GetMask(nSide, Rook) & tmp))
                        continue;
                } else if (BishopEPM[nEkp] & def) {
                    tmp = p->GetAtkFr(de) & Ray[nEkp][de];
                    if (!(p->GetMask(nSide, Queen) & tmp) &&
                        !(p->GetMask(nSide, Bishop) & tmp))
                        continue;
                } else
                    continue;
            } else if (def)
                continue;
            def = ksafe & ~KnightEPM[nTo];
            if (def) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (def).FindSetBit();
                    def.ClearLowestBit();
                    att = p->GetAtkFr(flight) & p->GetMask(nSide, 0);
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (def);
                if (free)
                    continue;
            }
            return true;
        }
    }

    return false;
}
