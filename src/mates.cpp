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

bool MateThreat(CPosition *p, int side) {
    int oside = !side;
    int ekp = p->m_rgKingSq[oside].BitOffset();
    CBitBoard pcs;
    CBitBoard ksafe;
    int fr;

    ksafe = p->m_rgAtkTo[ekp] & ~p->m_rgMask[oside][0];

    /*
     * Queen checks
     */

    pcs = p->m_rgMask[side][Queen];
    while (pcs) {
        int to;
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->m_rgAtkTo[fr] & QueenEPM[ekp]) & ~p->m_rgMask[side][0];
        while (mvs) {
            CBitBoard tmp;
            to = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[ekp][to];
            if ((p->m_rgMask[White][0] & tmp) || (p->m_rgMask[Black][0] & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~QueenEPM[to];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->m_rgAtkFr[flight] & p->m_rgMask[side][0];
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->m_rgAtkTo[ekp].TstBit(to)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                tmp.ClrBit(ekp);
                /* square is defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[to][fr] & p->m_rgAtkFr[fr];
                if ((p->m_rgMask[oside][Queen] & ray) ||
                    (p->m_rgMask[oside][Rook] & ray) ||
                    (p->m_rgMask[oside][Bishop] & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->m_rgMask[side][0] & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->m_rgMask[side][Bishop] & ray) ||
                    (p->m_rgMask[side][Rook] & ray) ||
                    (p->m_rgMask[side][Queen] & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int inter;
                int def = 0;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                tmp = InterPath[to][ekp];
                while (tmp) {
                    CBitBoard tmp2;
                    inter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->m_rgAtkFr[inter] & p->m_rgMask[oside][0];
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

    pcs = p->m_rgMask[side][Rook];
    while (pcs) {
        int to;
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->m_rgAtkTo[fr] & RookEPM[ekp]) & ~p->m_rgMask[side][0];
        while (mvs) {
            CBitBoard tmp;
            to = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[ekp][to];
            if ((p->m_rgMask[White][0] & tmp) || (p->m_rgMask[Black][0] & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~RookEPM[to];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->m_rgAtkFr[flight] & p->m_rgMask[side][0];
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->m_rgAtkTo[ekp].TstBit(to)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                tmp.ClrBit(ekp);
                /* square is defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[to][fr] & p->m_rgAtkFr[fr];
                if ((p->m_rgMask[oside][Queen] & ray) ||
                    (p->m_rgMask[oside][Rook] & ray) ||
                    (p->m_rgMask[oside][Bishop] & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->m_rgMask[side][0] & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->m_rgMask[side][Bishop] & ray) ||
                    (p->m_rgMask[side][Rook] & ray) ||
                    (p->m_rgMask[side][Queen] & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int inter;
                int def = 0;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                tmp = InterPath[to][ekp];
                while (tmp) {
                    CBitBoard tmp2;
                    inter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->m_rgAtkFr[inter] & p->m_rgMask[oside][0];
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

    pcs = p->m_rgMask[side][Bishop];
    while (pcs) {
        int to;
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->m_rgAtkTo[fr] & BishopEPM[ekp]) & ~p->m_rgMask[side][0];
        while (mvs) {
            CBitBoard tmp;
            to = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /* check whether path is obstructed */
            tmp = InterPath[ekp][to];
            if ((p->m_rgMask[White][0] & tmp) || (p->m_rgMask[Black][0] & tmp))
                continue;
            /* check wether all flight squares are covered */
            tmp = ksafe & ~BishopEPM[to];
            if (tmp) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    att = p->m_rgAtkFr[flight] & p->m_rgMask[side][0];
                    att.ClrBit(fr);
                    if (!att)
                        free++;
                    if (free)
                        break;
                } while (tmp);
                if (free)
                    continue;
            }
            if (p->m_rgAtkTo[ekp].TstBit(to)) {
                /* contact check */
                CBitBoard ray;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                tmp.ClrBit(ekp);
                /* square is defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                /* check if we have defenders 'from behind' */
                ray = Ray[to][fr] & p->m_rgAtkFr[fr];
                if ((p->m_rgMask[oside][Queen] & ray) ||
                    (p->m_rgMask[oside][Rook] & ray) ||
                    (p->m_rgMask[oside][Bishop] & ray))
                    continue;
                /* If supported by a friendly piece, its mate! */
                if (p->m_rgMask[side][0] & tmp) {
                    return true;
                }
                /* check for supporters 'from behind' */
                if ((p->m_rgMask[side][Bishop] & ray) ||
                    (p->m_rgMask[side][Rook] & ray) ||
                    (p->m_rgMask[side][Queen] & ray)) {
                    return true;
                }
            } else {
                /* distant check */
                int inter;
                int def = 0;
                tmp = p->m_rgAtkFr[to];
                tmp.ClrBit(fr);
                /* check if defended by opponent */
                if (p->m_rgMask[oside][0] & tmp)
                    continue;
                tmp = InterPath[to][ekp];
                while (tmp) {
                    CBitBoard tmp2;
                    inter = (tmp).FindSetBit();
                    tmp.ClearLowestBit();
                    tmp2 = p->m_rgAtkFr[inter] & p->m_rgMask[oside][0];
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

    pcs = p->m_rgMask[side][Knight];
    while (pcs) {
        int to;
        CBitBoard mvs;
        fr = (pcs).FindSetBit();
        pcs.ClearLowestBit();
        mvs = (p->m_rgAtkTo[fr] & KnightEPM[ekp]) & ~p->m_rgMask[side][0];
        while (mvs) {
            CBitBoard def;
            to = (mvs).FindSetBit();
            mvs.ClearLowestBit();
            /*
             * check whether the square is defended. If so, the defender
             * must not be pinned.
             */
            def = p->m_rgAtkFr[to] & p->m_rgMask[oside][0];
            if ((def).CountBits() == 1) {
                int de = (def).FindSetBit();
                CBitBoard tmp;
                if (RookEPM[ekp] & def) {
                    tmp = p->m_rgAtkFr[de] & Ray[ekp][de];
                    if (!(p->m_rgMask[side][Queen] & tmp) &&
                        !(p->m_rgMask[side][Rook] & tmp))
                        continue;
                } else if (BishopEPM[ekp] & def) {
                    tmp = p->m_rgAtkFr[de] & Ray[ekp][de];
                    if (!(p->m_rgMask[side][Queen] & tmp) &&
                        !(p->m_rgMask[side][Bishop] & tmp))
                        continue;
                } else
                    continue;
            } else if (def)
                continue;
            def = ksafe & ~KnightEPM[to];
            if (def) {
                int flight;
                int free = 0;
                do {
                    CBitBoard att;
                    flight = (def).FindSetBit();
                    def.ClearLowestBit();
                    att = p->m_rgAtkFr[flight] & p->m_rgMask[side][0];
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
