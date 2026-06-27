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
static RECOGNIZER RecognizerKNK;
static RECOGNIZER RecognizerKBKP;
static RECOGNIZER RecognizerKNKP;

static void RegisterRecognizer(RECOGNIZER *pFunct, int nWhiteSig,
                               int nBlackSig) {
    Recognizers[CALCULATE_INDEX(nWhiteSig, nBlackSig)] = pFunct;

    RecognizerAvailable[nWhiteSig] |= (1 << nBlackSig);
    RecognizerAvailable[nBlackSig] |= (1 << nWhiteSig);
}

static int sig(int nPawn, int nKnight, int nBishop, int nRook, int nQueen) {
    return nPawn | (nKnight << 1) | (nBishop << 2) | (nRook << 3) | (nQueen << 4);
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
    RegisterRecognizer(RecognizerKNK, sig(0, 1, 0, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKNK, sig(0, 0, 0, 0, 0), sig(0, 1, 0, 0, 0));
    RegisterRecognizer(RecognizerKNK, sig(0, 1, 0, 0, 0), sig(0, 1, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(0, 0, 1, 0, 0), sig(1, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(1, 0, 1, 0, 0), sig(0, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKBKP, sig(1, 0, 1, 0, 0), sig(1, 0, 0, 0, 0));
    RegisterRecognizer(RecognizerKNKP, sig(0, 1, 0, 0, 0), sig(1, 0, 0, 0, 0));
}

int ProbeRecognizer(const CPosition *p, int *pScore) {
    int nIndex = RECOGNIZER_INDEX(p);
    RECOGNIZER *pRec = Recognizers[nIndex];
    if (pRec != NULL) {
        if (RecognizerAvailable[p->GetMaterialSignature(White)] &
            (1 << p->GetMaterialSignature(Black))) {
            return pRec(p, pScore);
        }
    }

    return Useless;
}

static int RecognizerKK(const CPosition *p, int *pScore) {
    (void)p;
    *pScore = 0;

    return ExactScore;
}

static int RecognizerKBK(const CPosition *p, int *pScore) {
    CBitBoard pcs;
    int nColor = White;

    if (p->GetMaterialSignature(Black)) {
        nColor = Black;
    }

    pcs = p->GetMask(nColor, Bishop);

    /*
     * drawn if there is only one bishop
     */

    if ((pcs).CountBits() < 2) {
        *pScore = 0;
        return ExactScore;
    }

    /*
     * drawn if the bishops are all of the same color
     */

    if (!((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask))) {
        *pScore = 0;
        return ExactScore;
    }

    /*
     * do not recognize when losers king attacks a piece
     */

    if (p->GetAtkTo(p->GetKingSq(OPP(nColor)).BitOffset()) & p->GetMask(nColor, 0)) {
        return Useless;
    }

    /*
     * do not recognize when losers king is on the and the winners king
     * is close enough to stalemate
     */

    if (p->GetTurn() != nColor && (p->GetMask(OPP(nColor), King) & EdgeMask) &&
        (KingDist(p->GetKingSq(White), p->GetKingSq(Black)) == 2)) {
        return Useless;
    }

    /*
     * This is a win. Calculate a score which guarantuess progress.
     */

    *pScore = p->GetMaterial(nColor) + 2 * Value[Pawn] -
              250 * EdgeDist(p->GetKingSq(OPP(nColor))) -
              125 * KingDist(p->GetKingSq(White), p->GetKingSq(Black));

    if (p->GetTurn() != nColor) {
        *pScore = -*pScore;
        return UpperBound;
    }

    return LowerBound;
}

static int RecognizerKNK(const CPosition *p, int *pScore) {
    if (p->GetMaterialSignature(White) && p->GetMaterialSignature(Black)) {
        return Useless;
    } else {
        int nCnt;

        if (p->GetMaterialSignature(White)) {
            nCnt = (p->GetMask(White, Knight)).CountBits();
        } else {
            nCnt = (p->GetMask(Black, Knight)).CountBits();
        }

        if (nCnt < 3) {
            *pScore = 0;
            return ExactScore;
        }

        return Useless;
    }
}

static int RecognizerKBKP(const CPosition *p, int *pScore) {
    if (p->GetMaterialSignature(White) && p->GetMaterialSignature(Black)) {

        /*
         * This is KBKP or KBPKP
         */

        int nColor = White;

        if (p->GetMaterialSignature(Black) & SIGNATURE_BIT(Bishop)) {
            nColor = Black;
        }

        if (p->GetMaterialSignature(nColor) & SIGNATURE_BIT(Pawn)) {
            if (nColor == White) {
                if (!(p->GetMask(White, Pawn) & NotAFileMask) &&
                    !(p->GetMask(White, Bishop) & WhiteSquaresMask) &&
                    (p->GetMask(Black, King) & CornerMaskA8)) {
                    *pScore = 0;
                    return (p->GetTurn() == White) ? UpperBound : LowerBound;
                }
                if (!(p->GetMask(White, Pawn) & NotHFileMask) &&
                    !(p->GetMask(White, Bishop) & BlackSquaresMask) &&
                    (p->GetMask(Black, King) & CornerMaskH8)) {
                    *pScore = 0;
                    return (p->GetTurn() == White) ? UpperBound : LowerBound;
                }
            } else {
                if (!(p->GetMask(Black, Pawn) & NotAFileMask) &&
                    !(p->GetMask(Black, Bishop) & BlackSquaresMask) &&
                    (p->GetMask(White, King) & CornerMaskA1)) {
                    *pScore = 0;
                    return (p->GetTurn() == Black) ? UpperBound : LowerBound;
                }
                if (!(p->GetMask(Black, Pawn) & NotHFileMask) &&
                    !(p->GetMask(Black, Bishop) & WhiteSquaresMask) &&
                    (p->GetMask(White, King) & CornerMaskH1)) {
                    *pScore = 0;
                    return (p->GetTurn() == Black) ? UpperBound : LowerBound;
                }
            }

            return Useless;
        } else {
            if ((p->GetMask(nColor, Bishop)).CountBits() > 1 ||
                p->GetMask(OPP(nColor), King) & EdgeMask) {
                return Useless;
            }

            *pScore = 0;

            if (nColor == p->GetTurn()) {
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

        if (p->GetMaterialSignature(White)) {
            if (!(p->GetMask(White, Pawn) & NotAFileMask) &&
                !(p->GetMask(White, Bishop) & WhiteSquaresMask) &&
                (p->GetMask(Black, King) & CornerMaskA8)) {
                *pScore = 0;
                return ExactScore;
            }
            if (!(p->GetMask(White, Pawn) & NotHFileMask) &&
                !(p->GetMask(White, Bishop) & BlackSquaresMask) &&
                (p->GetMask(Black, King) & CornerMaskH8)) {
                *pScore = 0;
                return ExactScore;
            }
        } else {
            if (!(p->GetMask(Black, Pawn) & NotAFileMask) &&
                !(p->GetMask(Black, Bishop) & BlackSquaresMask) &&
                (p->GetMask(White, King) & CornerMaskA1)) {
                *pScore = 0;
                return ExactScore;
            }
            if (!(p->GetMask(Black, Pawn) & NotHFileMask) &&
                !(p->GetMask(Black, Bishop) & WhiteSquaresMask) &&
                (p->GetMask(White, King) & CornerMaskH1)) {
                *pScore = 0;
                return ExactScore;
            }
        }

        return Useless;
    }
}

static int RecognizerKNKP(const CPosition *p, int *pScore) {
    int nColor = White;

    if (p->GetMaterialSignature(Black) & SIGNATURE_BIT(Knight)) {
        nColor = Black;
    }

    if ((p->GetMask(nColor, Knight)).CountBits() > 1 ||
        p->GetMask(OPP(nColor), King) & EdgeMask) {
        return Useless;
    }

    *pScore = 0;

    if (nColor == p->GetTurn()) {
        return UpperBound;
    } else {
        return LowerBound;
    }
}
