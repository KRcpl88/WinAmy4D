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
#include "scoord.h"

extern CBitBoard FileMask[8], IsoMask[8];
extern CBitBoard RankMask[8];
extern CBitBoard ForwardRayW[CSCoord::SIZE], ForwardRayB[CSCoord::SIZE];
extern CBitBoard PassedMaskW[CSCoord::SIZE], PassedMaskB[CSCoord::SIZE];
extern CBitBoard OutpostMaskW[CSCoord::SIZE], OutpostMaskB[CSCoord::SIZE];
extern CBitBoard InterPath[CSCoord::SIZE][CSCoord::SIZE];
extern CBitBoard Ray[CSCoord::SIZE][CSCoord::SIZE];
extern CBitBoard WPawnEPM[CSCoord::SIZE], BPawnEPM[CSCoord::SIZE];
extern const CBitBoard KnightEPM[CSCoord::SIZE], KingEPM[CSCoord::SIZE];
extern const CBitBoard PawnEPM[2][CSCoord::SIZE];
extern CBitBoard BishopEPM[CSCoord::SIZE], RookEPM[CSCoord::SIZE], QueenEPM[CSCoord::SIZE];
extern CBitBoard SeventhRank[2], ThirdRank[2], EighthRank[2];
extern CBitBoard LeftOf[8], RightOf[8], FarLeftOf[8], FarRightOf[8];
extern CBitBoard EdgeMask;
extern CBitBoard BlackSquaresMask, WhiteSquaresMask;
extern CBitBoard KingSquareW[CSCoord::SIZE], KingSquareB[CSCoord::SIZE];
extern CBitBoard NotAFileMask, NotHFileMask;
extern CBitBoard CornerMaskA1, CornerMaskA8, CornerMaskH1, CornerMaskH8;
extern CBitBoard WPawnBackwardMask[CSCoord::SIZE], BPawnBackwardMask[CSCoord::SIZE];
extern CBitBoard KingSideMask, QueenSideMask;
extern CBitBoard ConnectedMask[CSCoord::SIZE];

extern signed char NextSQ[CSCoord::SIZE][CSCoord::SIZE];

void InitAll(void);
void PrintBitBoard(CBitBoard);

#endif

