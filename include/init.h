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

#ifndef INIT_H
#define INIT_H

#include "bitboard.h"

extern CBitBoard FileMask[8], IsoMask[8];
extern CBitBoard RankMask[8];
extern CBitBoard ForwardRayW[64], ForwardRayB[64];
extern CBitBoard PassedMaskW[64], PassedMaskB[64];
extern CBitBoard OutpostMaskW[64], OutpostMaskB[64];
extern CBitBoard InterPath[64][64];
extern CBitBoard Ray[64][64];
extern CBitBoard WPawnEPM[64], BPawnEPM[64];
extern const CBitBoard KnightEPM[64], KingEPM[64];
extern const CBitBoard PawnEPM[2][64];
extern CBitBoard BishopEPM[64], RookEPM[64], QueenEPM[64];
extern CBitBoard SeventhRank[2], ThirdRank[2], EighthRank[2];
extern CBitBoard LeftOf[8], RightOf[8], FarLeftOf[8], FarRightOf[8];
extern CBitBoard EdgeMask;
extern CBitBoard BlackSquaresMask, WhiteSquaresMask;
extern CBitBoard KingSquareW[64], KingSquareB[64];
extern CBitBoard NotAFileMask, NotHFileMask;
extern CBitBoard CornerMaskA1, CornerMaskA8, CornerMaskH1, CornerMaskH8;
extern CBitBoard WPawnBackwardMask[64], BPawnBackwardMask[64];
extern CBitBoard KingSideMask, QueenSideMask;
extern CBitBoard ConnectedMask[64];

extern signed char NextSQ[64][64];

void InitAll(void);
void PrintBitBoard(CBitBoard);

#endif
