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

#ifndef EVALUATION_H
#define EVALUATION_H

#include "bitboard.h"
#include "scoord.h"
#include "types.h"

struct PawnFacts {
    CBitBoard pf_WhitePassers;
    CBitBoard pf_BlackPassers;
    int pf_Flags;
    char pf_WhiteKingSide;
    char pf_BlackKingSide;
    char pf_WhiteQueenSide;
    char pf_BlackQueenSide;
};

/**
 * Pawn evaluation parameters
 */
extern int DoubledPawn;
extern int BackwardPawn;
extern int HiddenBackwardPawn;
extern int PawnOutrunsKing;
extern int PawnDevelopmentBlocked;
extern int PawnDuo;
extern int PawnStorm;
extern int CrampingPawn;
extern int PawnMajority;
extern int CoveredPassedPawn6th;
extern int CoveredPassedPawn7th;
extern int16_t PassedPawn[];
extern int16_t PassedPawnBlocked[];
extern int16_t PassedPawnConnected[];
extern int16_t IsolatedPawn[];
extern int16_t PawnAdvanceOpening[];
extern int16_t PawnAdvanceMiddlegame[];
extern int16_t PawnAdvanceEndgame[];
extern int16_t DistantPassedPawn[];

/**
 * Knight scoring parameters
 */
extern int KnightKingProximity;
extern int KnightBlocksCPawn;
extern int KnightEdgePenalty;
extern int16_t KnightPos[CBitBoard::SIZE];
extern int16_t KnightOutpost[CBitBoard::SIZE];

/**
 * Bishop scoring parameters
 */
extern int16_t BishopPair[];
extern int BishopMobility;
extern int BishopKingProximity;
extern int BishopTrapped;
extern int16_t BishopPos[CBitBoard::SIZE];

/**
 * Rook scoring parameters
 */
extern int RookMobility;
extern int RookOnOpenFile;
extern int RookOnSemiOpenFile;
extern int RookKingProximity;
extern int RookConnected;
extern int RookBehindPasser;
extern int RookOn7thRank;
extern int16_t RookPos[CBitBoard::SIZE];

/**
 * Queen scoring parameters
 */
extern int QueenKingProximity;
extern int16_t QueenPos[CBitBoard::SIZE];
extern int16_t QueenPosDevelopment[CBitBoard::SIZE];

/**
 * King scoring parameters
 */
extern int KingBlocksRook;
extern int KingInCenter;
extern int KingSafetyScale;
extern int16_t KingPosMiddlegame[CBitBoard::SIZE];
extern int16_t KingPosEndgame[CBitBoard::SIZE];
extern int16_t KingPosEndgameQueenSide[CBitBoard::SIZE];
extern int16_t ScaleHalfOpenFilesMine[];
extern int16_t ScaleHalfOpenFilesYours[];
extern int16_t ScaleOpenFiles[];

extern int MaxPos;

int EvaluatePosition(const CPosition *);
void InitEvaluation(const CPosition *);
int MaterialBalance(const CPosition *);

#endif /* EVALUATION_H */

