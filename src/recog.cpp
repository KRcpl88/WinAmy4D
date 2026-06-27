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

typedef int RECOGNIZER(const CPosition *, int *score);

static RECOGNIZER *Recognizers[64];
static int RecognizerAvailable[32];

static RECOGNIZER RecognizerKK;
static RECOGNIZER RecognizerKBK;
static RECOGNIZER RecognizerKNK;

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
    int nColor = White;

    if (p->GetMaterialSignature(Black)) {
        nColor = Black;
    }

    /*
     * drawn unless all four bishops are present to cover all square colors
     */

    if ((p->GetMask(nColor, Bishop)).CountBits() <= 3) {
        *pScore = 0;
        return ExactScore;
    }

    return Useless;
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
