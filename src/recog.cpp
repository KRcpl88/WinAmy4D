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
 * recog.c - interior node recognizers
 */

/*
 * See Ernst A. Heinz, "Efficient Interior-YamlNode Recognition"
 * ICCA Journal Volume 21, No. 3, pp 156-167
 */

#include "amy.h"

#include "recog.h"
#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"

typedef int RECOGNIZER(const CPosition *, int *score);

static RECOGNIZER *Recognizers[64];
static int RecognizerAvailable[32];

static RECOGNIZER RecognizerKK;
static RECOGNIZER RecognizerKBK;
static RECOGNIZER RecognizerKBNK;
static RECOGNIZER RecognizerKNK;
static RECOGNIZER RecognizerKBKP;
static RECOGNIZER RecognizerKNKP;

static void RegisterRecognizer(RECOGNIZER *funct, int white_sig,
                               int black_sig) {
    Recognizers[CALCULATE_INDEX(white_sig, black_sig)] = funct;

    RecognizerAvailable[white_sig] |= (1 << black_sig);
    RecognizerAvailable[black_sig] |= (1 << white_sig);
}

static int sig(int pawn, int knight, int bishop, int rook, int queen) {
    return pawn | (knight << 1) | (bishop << 2) | (rook << 3) | (queen << 4);
}

void RecogInit(void) {
    int i;

    for (i = 0; i < 64; i++) {
        Recognizers[i] = NULL;
    }

    for (i = 0; i < 32; i++) {
        RecognizerAvailable[i] = 0;
    }

    /*
     *                                       P  N  B  R  Q       P  N  B  R  Q
     */

    RegisterRecognizer(RecognizerKK, sig(0, 0, 0, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBK, sig(0, 0, 1, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBNK, sig(0, 1, 1, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBNK, sig(0, 0, 1, 0, 0), sig(0, 1, 0, 0, 0));
    RegisterRecognizer(RecognizerKNK, sig(0, 1, 0, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKNK, sig(0, 0, 0, 0, 0), sig(0, 1, 0, 0, 0));
    RegisterRecognizer(RecognizerKNK, sig(0, 1, 0, 0, 0), sig(0, 1, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(0, 0, 1, 0, 0), sig(1, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(1, 0, 1, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(1, 0, 1, 0, 0), sig(1, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKNKP, sig(0, 1, 0, 0, 0), sig(1, 0, 0, 0, 0));
}

int ProbeRecognizer(const CPosition *p, int *score) {
    int index = RECOGNIZER_INDEX(p);
    RECOGNIZER *rec = Recognizers[index];
    if (rec != NULL) {
        if (RecognizerAvailable[p->m_rgbMaterialSignature[White]] &
            (1 << p->m_rgbMaterialSignature[Black])) {
            return rec(p, score);
        }
    }

    return Useless;
}

static int RecognizerKK(const CPosition *p, int *score) {
    (void)p;
    *score = 0;

    return ExactScore;
}

static int RecognizerKBK(const CPosition *p, int *score) {
    CBitBoard pcs;
    int color = White;

    if (p->m_rgbMaterialSignature[Black]) {
        color = Black;
    }

    pcs = p->m_rgMask[color][Bishop];

    /*
     * drawn if there is only one bishop
     */

    if ((pcs).CountBits() < 2) {
        *score = 0;
        return ExactScore;
    }

    /*
     * drawn if the bishops are all of the same color
     */

    if (!((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask))) {
        *score = 0;
        return ExactScore;
    }

    /*
     * do not recognize when losers king attacks a piece
     */

    if (p->m_rgAtkTo[p->m_rgKingSq[OPP(color)].BitOffset()] & p->m_rgMask[color][0]) {
        return Useless;
    }

    /*
     * do not recognize when losers king is on the and the winners king
     * is close enough to stalemate
     */

    if (p->m_nTurn != color && (p->m_rgMask[OPP(color)][King] & EdgeMask) &&
        (KingDist(p->m_rgKingSq[White], p->m_rgKingSq[Black]) == 2)) {
        return Useless;
    }

    /*
     * This is a win. Calculate a score which guarantuess progress.
     */

    *score = p->m_rgnMaterial[color] + 2 * Value[Pawn] -
             250 * EdgeDist(p->m_rgKingSq[OPP(color)]) -
             125 * KingDist(p->m_rgKingSq[White], p->m_rgKingSq[Black]);

    if (p->m_nTurn != color) {
        *score = -*score;
        return UpperBound;
    }

    return LowerBound;
}

static int KBNKTab[] = {500, 450, 425, 400, 375, 350, 325, 300, 450, 300, 300,
                        300, 300, 300, 300, 325, 425, 300, 100, 100, 100, 100,
                        300, 350, 400, 300, 100, 0,   0,   100, 300, 375, 375,
                        300, 100, 0,   0,   100, 300, 400, 350, 300, 100, 100,
                        100, 100, 300, 425, 325, 300, 300, 300, 300, 300, 300,
                        450, 300, 325, 350, 375, 400, 425, 450, 500};

static int RecognizerKBNK(const CPosition *p, int *score) {
    if (p->m_rgbMaterialSignature[White] && p->m_rgbMaterialSignature[Black]) {

        /*
         * This is knkb
         */

        if ((p->m_rgMask[White][0] | p->m_rgMask[Black][0]).CountBits() > 4) {
            return Useless;
        }

        if (EdgeMask & (p->m_rgMask[White][King] | p->m_rgMask[Black][King])) {
            return Useless;
        }

        *score = 0;
        return ExactScore;
    } else {

        /*
         * This is kbnk
         */

        CBitBoard atkd;
        int color = White;
        int sqx = 0;

        if (p->m_rgbMaterialSignature[Black]) {
            color = Black;
        }

        /*
         * do not recognize when losers king attacks a piece
         */

        atkd = p->m_rgAtkTo[p->m_rgKingSq[OPP(color)].BitOffset()] & p->m_rgMask[color][0];
        if (atkd) {
            if (p->m_nTurn != color || (atkd).CountBits() > 1) {
                return Useless;
            }
        }

        /*
         * do not recognize when losers king is on the and the winners king
         * is close enough to stalemate
         */

        if (p->m_nTurn != color && (p->m_rgMask[OPP(color)][King] & EdgeMask) &&
            (KingDist(p->m_rgKingSq[White], p->m_rgKingSq[Black]) == 2)) {
            return Useless;
        }

        /*
         * This is a win. Calculate a score which guarantuess progress.
         */

        if (p->m_rgMask[color][Bishop] & BlackSquaresMask) {
            sqx = KBNKTab[p->m_rgKingSq[OPP(color)].BitOffset()];
        }

        if (p->m_rgMask[color][Bishop] & WhiteSquaresMask) {
            sqx = KBNKTab[7 ^ p->m_rgKingSq[OPP(color)].BitOffset()];
        }

        *score = p->m_rgnMaterial[color] + 3 * Value[Pawn] + sqx -
                 125 * KingDist(p->m_rgKingSq[White], p->m_rgKingSq[Black]);

        if (p->m_nTurn != color) {
            *score = -*score;
            return UpperBound;
        }

        return LowerBound;
    }
}

static int RecognizerKNK(const CPosition *p, int *score) {
    if (p->m_rgbMaterialSignature[White] && p->m_rgbMaterialSignature[Black]) {
        return Useless;
    } else {
        int cnt;

        if (p->m_rgbMaterialSignature[White]) {
            cnt = (p->m_rgMask[White][Knight]).CountBits();
        } else {
            cnt = (p->m_rgMask[Black][Knight]).CountBits();
        }

        if (cnt < 3) {
            *score = 0;
            return ExactScore;
        }

        return Useless;
    }
}

static int RecognizerKBKP(const CPosition *p, int *score) {
    if (p->m_rgbMaterialSignature[White] && p->m_rgbMaterialSignature[Black]) {

        /*
         * This is KBKP or KBPKP
         */

        int color = White;

        if (p->m_rgbMaterialSignature[Black] & SIGNATURE_BIT(Bishop)) {
            color = Black;
        }

        if (p->m_rgbMaterialSignature[color] & SIGNATURE_BIT(Pawn)) {
            if (color == White) {
                if (!(p->m_rgMask[White][Pawn] & NotAFileMask) &&
                    !(p->m_rgMask[White][Bishop] & WhiteSquaresMask) &&
                    (p->m_rgMask[Black][King] & CornerMaskA8)) {
                    *score = 0;
                    return (p->m_nTurn == White) ? UpperBound : LowerBound;
                }
                if (!(p->m_rgMask[White][Pawn] & NotHFileMask) &&
                    !(p->m_rgMask[White][Bishop] & BlackSquaresMask) &&
                    (p->m_rgMask[Black][King] & CornerMaskH8)) {
                    *score = 0;
                    return (p->m_nTurn == White) ? UpperBound : LowerBound;
                }
            } else {
                if (!(p->m_rgMask[Black][Pawn] & NotAFileMask) &&
                    !(p->m_rgMask[Black][Bishop] & BlackSquaresMask) &&
                    (p->m_rgMask[White][King] & CornerMaskA1)) {
                    *score = 0;
                    return (p->m_nTurn == Black) ? UpperBound : LowerBound;
                }
                if (!(p->m_rgMask[Black][Pawn] & NotHFileMask) &&
                    !(p->m_rgMask[Black][Bishop] & WhiteSquaresMask) &&
                    (p->m_rgMask[White][King] & CornerMaskH1)) {
                    *score = 0;
                    return (p->m_nTurn == Black) ? UpperBound : LowerBound;
                }
            }

            return Useless;
        } else {
            if ((p->m_rgMask[color][Bishop]).CountBits() > 1 ||
                p->m_rgMask[OPP(color)][King] & EdgeMask) {
                return Useless;
            }

            *score = 0;

            if (color == p->m_nTurn) {
                return UpperBound;
            } else {
                return LowerBound;
            }
        }
    } else {

        /*
         * This is KBPK
         *
         * Check for draws because of wrongly colored bishop
         */

        if (p->m_rgbMaterialSignature[White]) {
            if (!(p->m_rgMask[White][Pawn] & NotAFileMask) &&
                !(p->m_rgMask[White][Bishop] & WhiteSquaresMask) &&
                (p->m_rgMask[Black][King] & CornerMaskA8)) {
                *score = 0;
                return ExactScore;
            }
            if (!(p->m_rgMask[White][Pawn] & NotHFileMask) &&
                !(p->m_rgMask[White][Bishop] & BlackSquaresMask) &&
                (p->m_rgMask[Black][King] & CornerMaskH8)) {
                *score = 0;
                return ExactScore;
            }
        } else {
            if (!(p->m_rgMask[Black][Pawn] & NotAFileMask) &&
                !(p->m_rgMask[Black][Bishop] & BlackSquaresMask) &&
                (p->m_rgMask[White][King] & CornerMaskA1)) {
                *score = 0;
                return ExactScore;
            }
            if (!(p->m_rgMask[Black][Pawn] & NotHFileMask) &&
                !(p->m_rgMask[Black][Bishop] & WhiteSquaresMask) &&
                (p->m_rgMask[White][King] & CornerMaskH1)) {
                *score = 0;
                return ExactScore;
            }
        }

        return Useless;
    }
}

static int RecognizerKNKP(const CPosition *p, int *score) {
    int color = White;

    if (p->m_rgbMaterialSignature[Black] & SIGNATURE_BIT(Knight)) {
        color = Black;
    }

    if ((p->m_rgMask[color][Knight]).CountBits() > 1 ||
        p->m_rgMask[OPP(color)][King] & EdgeMask) {
        return Useless;
    }

    *score = 0;

    if (color == p->m_nTurn) {
        return UpperBound;
    } else {
        return LowerBound;
    }
}
