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
 * evaluation.c - positional evaluation routines
 */

#include "amy.h"

#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "recog.h"
#include "scoord.h"
#include <stdint.h>

/**
 * Debugging stuff
 */

// #define DEBUG
#ifdef DEBUG

enum {
    DebugPawnStructure = 1,
    DebugKingSafety = 2,
    DebugPassedPawns = 4,
    DebugPieces = 8
};

static int DebugWhat = 0;

#endif

/**
 * Some constants for pawn structure
 */

enum {
    PawnsOnKingSide = (1 << 0),
    PawnsOnQueenSide = (1 << 1),
    FianchettoWhiteKingSide = (1 << 2),
    FianchettoWhiteQueenSide = (1 << 3),
    FianchettoBlackKingSide = (1 << 4),
    FianchettoBlackQueenSide = (1 << 5),
    QueensPawnOpening = (1 << 6)
};

/**
 * Some constants for RootGamePhase
 */

enum { Opening, Middlegame, Endgame };

const char *GamePhaseName[] = {"Opening", "Middlegame", "Endgame"};

/**
 * Pawn scoring parameters
 */

int DoubledPawn = -70;
int BackwardPawn = -100;
int HiddenBackwardPawn = -70;
int PawnOutrunsKing = 6000;
int PawnDevelopmentBlocked = -100;
int PawnDuo = 15;
int PawnStorm = 10;
int CrampingPawn = -160;
int PawnMajority = 100;

int CoveredPassedPawn6th = 200;
int CoveredPassedPawn7th = 600;

int16_t PassedPawn[] = {0, 32, 64, 128, 256, 512, 1024, 0};

int16_t PassedPawnBlocked[] = {0, 16, 48, 96, 192, 384, 768, 0};

int16_t PassedPawnConnected[] = {0, 2, 6, 12, 24, 48, 96, 0};

int16_t IsolatedPawn[] = {-70, -80, -90, -100, -100, -90, -80, -70};

int16_t PawnAdvanceOpening[] = {-10, -10, 5, 10, 10, -20, -50, -50};

int16_t PawnAdvanceMiddlegame[] = {0, 0, 10, 15, 15, 10, 0, 0};

int16_t PawnAdvanceEndgame[] = {10, 10, 10, 10, 10, 10, 10, 10};

int16_t DistantPassedPawn[] = {500, 300, 300, 300, 200, 200, 150, 150, 150,
                               100, 100, 50,  50,  50,  0,   0,   0,   0,
                               0,   0,   0,   0,   0,   0,   0,   0,   0,
                               0,   0,   0,   0,   0,   0,   0,   0};

static int16_t WPawnPos[CBitBoard::SIZE];
static int16_t BPawnPos[CBitBoard::SIZE];

/**
 * Knight scoring parameters
 */

int KnightKingProximity = 7;
int KnightBlocksCPawn = -100;
int KnightEdgePenalty = -130;

// clang-format off
static const int16_t KnightPosL7[64] = {
     -30,  -30,  -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,   60,   60,  -30,  -30,
     -30,   60,  130,  130,  130,  130,   60,  -30,
     -30,  130,  190,  190,  190,  190,  130,  -30,
       0,  130,  190,  250,  250,  190,  130,    0,
       0,  190,  250,  250,  250,  250,  190,    0,
       0,   90,  160,  160,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,    0,    0
};

static const int16_t KnightOutpostL7[64] = {
    0, 0,  0,   0,   0,  0, 0, 0,
    0, 0,  0,   0,   0,  0, 0, 0,
    0, 0,  0,   0,   0,  0, 0, 0,
    0, 0,  0,  40,  40,  0, 0, 0,
    0, 0, 80, 100, 100, 80, 0, 0,
    0, 0, 80, 120, 120, 80, 0, 0,
    0, 0, 40,  80,  80, 40, 0, 0,
    0, 0,  0,   0,   0,  0, 0, 0
};
// clang-format on

int16_t KnightPos[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
     190,

    /* Level 1 (2x2) */
     -30,  -30,
       0,    0,

    /* Level 2 (3x3) */
     -30,  -30,  -30,
       0,  250,    0,
       0,    0,    0,

    /* Level 3 (4x4) */
     -30,  -30,  -30,  -30,
     -30,  130,  130,  -30,
       0,  250,  250,    0,
       0,    0,    0,    0,

    /* Level 4 (5x5) */
     -30,  -30,  -30,  -30,  -30,
     -30,  130,  130,  130,  -30,
       0,  190,  250,  190,    0,
       0,  250,  250,  250,    0,
       0,    0,    0,    0,    0,

    /* Level 5 (6x6) */
     -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,  -30,  -30,
     -30,  130,  190,  190,  130,  -30,
       0,  130,  250,  250,  130,    0,
       0,   90,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 6 (7x7) */
     -30,  -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,   60,  -30,  -30,
     -30,   60,  130,  130,  130,   60,  -30,
       0,  130,  190,  250,  190,  130,    0,
       0,  190,  250,  250,  250,  190,    0,
       0,   90,  160,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 7 (8x8) */
     -30,  -30,  -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,   60,   60,  -30,  -30,
     -30,   60,  130,  130,  130,  130,   60,  -30,
     -30,  130,  190,  190,  190,  190,  130,  -30,
       0,  130,  190,  250,  250,  190,  130,    0,
       0,  190,  250,  250,  250,  250,  190,    0,
       0,   90,  160,  160,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,    0,    0,

    /* Level 8 (7x7) */
     -30,  -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,   60,  -30,  -30,
     -30,   60,  130,  130,  130,   60,  -30,
       0,  130,  190,  250,  190,  130,    0,
       0,  190,  250,  250,  250,  190,    0,
       0,   90,  160,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 9 (6x6) */
     -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,  -30,  -30,
     -30,  130,  190,  190,  130,  -30,
       0,  130,  250,  250,  130,    0,
       0,   90,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 10 (5x5) */
     -30,  -30,  -30,  -30,  -30,
     -30,  130,  130,  130,  -30,
       0,  190,  250,  190,    0,
       0,  250,  250,  250,    0,
       0,    0,    0,    0,    0,

    /* Level 11 (4x4) */
     -30,  -30,  -30,  -30,
     -30,  130,  130,  -30,
       0,  250,  250,    0,
       0,    0,    0,    0,

    /* Level 12 (3x3) */
     -30,  -30,  -30,
       0,  250,    0,
       0,    0,    0,

    /* Level 13 (2x2) */
     -30,  -30,
       0,    0
};
int16_t KnightOutpost[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
      40,

    /* Level 1 (2x2) */
       0,    0,
       0,    0,

    /* Level 2 (3x3) */
       0,    0,    0,
       0,  100,    0,
       0,    0,    0,

    /* Level 3 (4x4) */
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,   80,   80,    0,
       0,    0,    0,    0,

    /* Level 4 (5x5) */
       0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,
       0,   80,  100,   80,    0,
       0,   80,  120,   80,    0,
       0,    0,    0,    0,    0,

    /* Level 5 (6x6) */
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,   40,   40,    0,    0,
       0,    0,  100,  100,    0,    0,
       0,    0,   80,   80,    0,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 6 (7x7) */
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,   80,  100,   80,    0,    0,
       0,    0,   80,  120,   80,    0,    0,
       0,    0,   40,   80,   40,    0,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 7 (8x8) */
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,   40,   40,    0,    0,    0,
       0,    0,   80,  100,  100,   80,    0,    0,
       0,    0,   80,  120,  120,   80,    0,    0,
       0,    0,   40,   80,   80,   40,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,

    /* Level 8 (7x7) */
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,   80,  100,   80,    0,    0,
       0,    0,   80,  120,   80,    0,    0,
       0,    0,   40,   80,   40,    0,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 9 (6x6) */
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,   40,   40,    0,    0,
       0,    0,  100,  100,    0,    0,
       0,    0,   80,   80,    0,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 10 (5x5) */
       0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,
       0,   80,  100,   80,    0,
       0,   80,  120,   80,    0,
       0,    0,    0,    0,    0,

    /* Level 11 (4x4) */
       0,    0,    0,    0,
       0,    0,    0,    0,
       0,   80,   80,    0,
       0,    0,    0,    0,

    /* Level 12 (3x3) */
       0,    0,    0,
       0,  100,    0,
       0,    0,    0,

    /* Level 13 (2x2) */
       0,    0,
       0,    0
};

/**
 * Bishop scoring parameters
 */

/*
 * The value of the bishop pair depends on the number of white pawns.
 */
int16_t BishopPair[] = {200, 200, 200, 200, 200, 200, 200, 150, 100};

int BishopMobility = 25;
int BishopKingProximity = 7;
int BishopTrapped = -1500;

// clang-format off
static const int16_t BishopPosL7[64] = {
     60,  60,  60,  60,  60,  60,  60,  60,
     60, 250,  60,  60,  60,  60, 250,  60,
     60, 160, 160, 160, 160, 160, 160,  60,
    160, 250, 280, 340, 340, 280, 250, 160,
    160, 250, 280, 340, 340, 280, 250, 160,
    160, 250, 280, 280, 280, 280, 250, 160,
    160, 250, 250, 250, 250, 250, 250, 160,
    160, 160, 160, 160, 160, 160, 160, 160
};
// clang-format on

int16_t BishopPos[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
     340,

    /* Level 1 (2x2) */
      60,   60,
     160,  160,

    /* Level 2 (3x3) */
      60,   60,   60,
     160,  340,  160,
     160,  160,  160,

    /* Level 3 (4x4) */
      60,   60,   60,   60,
      60,  160,  160,   60,
     160,  280,  280,  160,
     160,  160,  160,  160,

    /* Level 4 (5x5) */
      60,   60,   60,   60,   60,
      60,  160,  160,  160,   60,
     160,  280,  340,  280,  160,
     160,  280,  280,  280,  160,
     160,  160,  160,  160,  160,

    /* Level 5 (6x6) */
      60,   60,   60,   60,   60,   60,
      60,  250,   60,   60,  250,   60,
     160,  250,  340,  340,  250,  160,
     160,  250,  340,  340,  250,  160,
     160,  250,  250,  250,  250,  160,
     160,  160,  160,  160,  160,  160,

    /* Level 6 (7x7) */
      60,   60,   60,   60,   60,   60,   60,
      60,  250,   60,   60,   60,  250,   60,
      60,  160,  160,  160,  160,  160,   60,
     160,  250,  280,  340,  280,  250,  160,
     160,  250,  280,  280,  280,  250,  160,
     160,  250,  250,  250,  250,  250,  160,
     160,  160,  160,  160,  160,  160,  160,

    /* Level 7 (8x8) */
      60,   60,   60,   60,   60,   60,   60,   60,
      60,  250,   60,   60,   60,   60,  250,   60,
      60,  160,  160,  160,  160,  160,  160,   60,
     160,  250,  280,  340,  340,  280,  250,  160,
     160,  250,  280,  340,  340,  280,  250,  160,
     160,  250,  280,  280,  280,  280,  250,  160,
     160,  250,  250,  250,  250,  250,  250,  160,
     160,  160,  160,  160,  160,  160,  160,  160,

    /* Level 8 (7x7) */
      60,   60,   60,   60,   60,   60,   60,
      60,  250,   60,   60,   60,  250,   60,
      60,  160,  160,  160,  160,  160,   60,
     160,  250,  280,  340,  280,  250,  160,
     160,  250,  280,  280,  280,  250,  160,
     160,  250,  250,  250,  250,  250,  160,
     160,  160,  160,  160,  160,  160,  160,

    /* Level 9 (6x6) */
      60,   60,   60,   60,   60,   60,
      60,  250,   60,   60,  250,   60,
     160,  250,  340,  340,  250,  160,
     160,  250,  340,  340,  250,  160,
     160,  250,  250,  250,  250,  160,
     160,  160,  160,  160,  160,  160,

    /* Level 10 (5x5) */
      60,   60,   60,   60,   60,
      60,  160,  160,  160,   60,
     160,  280,  340,  280,  160,
     160,  280,  280,  280,  160,
     160,  160,  160,  160,  160,

    /* Level 11 (4x4) */
      60,   60,   60,   60,
      60,  160,  160,   60,
     160,  280,  280,  160,
     160,  160,  160,  160,

    /* Level 12 (3x3) */
      60,   60,   60,
     160,  340,  160,
     160,  160,  160,

    /* Level 13 (2x2) */
      60,   60,
     160,  160
};

/**
 * Rook scoring parameters
 */

int RookMobility = 10;

int RookOnOpenFile = 100;
int RookOnSemiOpenFile = 25;

int RookKingProximity = 5;
int RookConnected = 60;

int RookBehindPasser = 12; /* will be scaled by phase */

int RookOn7thRank = 300;

// clang-format off
static const int16_t RookPosL7[64] = {
      0,  90, 130, 220, 220, 130,  90,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,
    130, 130, 130, 130, 130, 130, 130, 130,
    200, 200, 200, 200, 200, 200, 200, 200,
    200, 200, 200, 200, 200, 200, 200, 200
};
// clang-format on

int16_t RookPos[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
       0,

    /* Level 1 (2x2) */
       0,    0,
     200,  200,

    /* Level 2 (3x3) */
       0,  220,    0,
       0,    0,    0,
     200,  200,  200,

    /* Level 3 (4x4) */
       0,  130,  130,    0,
       0,    0,    0,    0,
     130,  130,  130,  130,
     200,  200,  200,  200,

    /* Level 4 (5x5) */
       0,  130,  220,  130,    0,
       0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,
     130,  130,  130,  130,  130,
     200,  200,  200,  200,  200,

    /* Level 5 (6x6) */
       0,   90,  220,  220,   90,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
     200,  200,  200,  200,  200,  200,
     200,  200,  200,  200,  200,  200,

    /* Level 6 (7x7) */
       0,   90,  130,  220,  130,   90,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
     130,  130,  130,  130,  130,  130,  130,
     200,  200,  200,  200,  200,  200,  200,
     200,  200,  200,  200,  200,  200,  200,

    /* Level 7 (8x8) */
       0,   90,  130,  220,  220,  130,   90,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,
     130,  130,  130,  130,  130,  130,  130,  130,
     200,  200,  200,  200,  200,  200,  200,  200,
     200,  200,  200,  200,  200,  200,  200,  200,

    /* Level 8 (7x7) */
       0,   90,  130,  220,  130,   90,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,
     130,  130,  130,  130,  130,  130,  130,
     200,  200,  200,  200,  200,  200,  200,
     200,  200,  200,  200,  200,  200,  200,

    /* Level 9 (6x6) */
       0,   90,  220,  220,   90,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,
     200,  200,  200,  200,  200,  200,
     200,  200,  200,  200,  200,  200,

    /* Level 10 (5x5) */
       0,  130,  220,  130,    0,
       0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,
     130,  130,  130,  130,  130,
     200,  200,  200,  200,  200,

    /* Level 11 (4x4) */
       0,  130,  130,    0,
       0,    0,    0,    0,
     130,  130,  130,  130,
     200,  200,  200,  200,

    /* Level 12 (3x3) */
       0,  220,    0,
       0,    0,    0,
     200,  200,  200,

    /* Level 13 (2x2) */
       0,    0,
     200,  200
};

/**
 * Queen scoring parameters
 */

int QueenKingProximity = 8;

// clang-format off
static const int16_t QueenPosL7[64] = {
    0, 0,  0,  0,  0,  0,  0,  0,
    0, 30, 30, 30, 30, 30, 30, 0,
    0, 30, 60, 60, 60, 60, 30, 0,
    0, 30, 60, 90, 90, 60, 30, 0,
    0, 30, 60, 90, 90, 60, 30, 0,
    0, 30, 60, 60, 60, 60, 30, 0,
    0, 30, 30, 60, 60, 30, 30, 0,
    0, 0,  0,  0,  0,  0,  0,  0
};

static const int16_t QueenPosDevelopmentL7[64] = {
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200, 30, 30, 30, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
    -200, -200,  0,  0,  0, 0, -200, -200,
};
// clang-format on

int16_t QueenPos[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
      90,

    /* Level 1 (2x2) */
       0,    0,
       0,    0,

    /* Level 2 (3x3) */
       0,    0,    0,
       0,   90,    0,
       0,    0,    0,

    /* Level 3 (4x4) */
       0,    0,    0,    0,
       0,   60,   60,    0,
       0,   60,   60,    0,
       0,    0,    0,    0,

    /* Level 4 (5x5) */
       0,    0,    0,    0,    0,
       0,   60,   60,   60,    0,
       0,   60,   90,   60,    0,
       0,   60,   60,   60,    0,
       0,    0,    0,    0,    0,

    /* Level 5 (6x6) */
       0,    0,    0,    0,    0,    0,
       0,   30,   30,   30,   30,    0,
       0,   30,   90,   90,   30,    0,
       0,   30,   90,   90,   30,    0,
       0,   30,   60,   60,   30,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 6 (7x7) */
       0,    0,    0,    0,    0,    0,    0,
       0,   30,   30,   30,   30,   30,    0,
       0,   30,   60,   60,   60,   30,    0,
       0,   30,   60,   90,   60,   30,    0,
       0,   30,   60,   60,   60,   30,    0,
       0,   30,   30,   60,   30,   30,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 7 (8x8) */
       0,    0,    0,    0,    0,    0,    0,    0,
       0,   30,   30,   30,   30,   30,   30,    0,
       0,   30,   60,   60,   60,   60,   30,    0,
       0,   30,   60,   90,   90,   60,   30,    0,
       0,   30,   60,   90,   90,   60,   30,    0,
       0,   30,   60,   60,   60,   60,   30,    0,
       0,   30,   30,   60,   60,   30,   30,    0,
       0,    0,    0,    0,    0,    0,    0,    0,

    /* Level 8 (7x7) */
       0,    0,    0,    0,    0,    0,    0,
       0,   30,   30,   30,   30,   30,    0,
       0,   30,   60,   60,   60,   30,    0,
       0,   30,   60,   90,   60,   30,    0,
       0,   30,   60,   60,   60,   30,    0,
       0,   30,   30,   60,   30,   30,    0,
       0,    0,    0,    0,    0,    0,    0,

    /* Level 9 (6x6) */
       0,    0,    0,    0,    0,    0,
       0,   30,   30,   30,   30,    0,
       0,   30,   90,   90,   30,    0,
       0,   30,   90,   90,   30,    0,
       0,   30,   60,   60,   30,    0,
       0,    0,    0,    0,    0,    0,

    /* Level 10 (5x5) */
       0,    0,    0,    0,    0,
       0,   60,   60,   60,    0,
       0,   60,   90,   60,    0,
       0,   60,   60,   60,    0,
       0,    0,    0,    0,    0,

    /* Level 11 (4x4) */
       0,    0,    0,    0,
       0,   60,   60,    0,
       0,   60,   60,    0,
       0,    0,    0,    0,

    /* Level 12 (3x3) */
       0,    0,    0,
       0,   90,    0,
       0,    0,    0,

    /* Level 13 (2x2) */
       0,    0,
       0,    0
};
int16_t QueenPosDevelopment[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
       0,

    /* Level 1 (2x2) */
    -200, -200,
    -200, -200,

    /* Level 2 (3x3) */
    -200,    0, -200,
    -200,    0, -200,
    -200,    0, -200,

    /* Level 3 (4x4) */
    -200,    0,    0, -200,
    -200,    0,    0, -200,
    -200,    0,    0, -200,
    -200,    0,    0, -200,

    /* Level 4 (5x5) */
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,

    /* Level 5 (6x6) */
    -200, -200,    0,    0, -200, -200,
    -200, -200,   30,   30, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,

    /* Level 6 (7x7) */
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,   30,   30,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,

    /* Level 7 (8x8) */
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,   30,   30,   30,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0,    0, -200, -200,

    /* Level 8 (7x7) */
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,   30,   30,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,
    -200, -200,    0,    0,    0, -200, -200,

    /* Level 9 (6x6) */
    -200, -200,    0,    0, -200, -200,
    -200, -200,   30,   30, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,
    -200, -200,    0,    0, -200, -200,

    /* Level 10 (5x5) */
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,
    -200,    0,    0,    0, -200,

    /* Level 11 (4x4) */
    -200,    0,    0, -200,
    -200,    0,    0, -200,
    -200,    0,    0, -200,
    -200,    0,    0, -200,

    /* Level 12 (3x3) */
    -200,    0, -200,
    -200,    0, -200,
    -200,    0, -200,

    /* Level 13 (2x2) */
    -200, -200,
    -200, -200
};

/**
 * King scoring parameters
 */

int KingBlocksRook = -300;
int KingInCenter = -100;
int KingSafetyScale = 1024;

// clang-format off
static const int16_t KingPosMiddlegameL7[64] = {
    -100, 0,    -200, -300, -300, -200,    0, -100,
    -100, -100, -200, -300, -300, -200, -100, -100,
    -300, -300, -300, -300, -300, -300, -300, -300,
    -400, -400, -400, -400, -400, -400, -400, -400,
    -500, -500, -500, -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600, -600, -600, -600,
    -700, -700, -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800, -800, -800};

static const int16_t KingPosEndgameL7[64] = {
    -300, -300, -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -100, -100, -200, -300,
    -300, -100,    0,  100,  100,    0, -100, -300,
    -300, -100,  100,  200,  200,  100, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100, -100, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300, -300, -300};

static const int16_t KingPosEndgameQueenSideL7[64] = {
    -300, -300, -300, -300, -300, -400, -500, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
       0,  100,  100,    0, -100, -200, -300, -600,
     100,  200,  200,  100, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
    -300, -300, -300, -300, -300, -400, -500, -600};
// clang-format on

int16_t KingPosMiddlegame[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
    -400,

    /* Level 1 (2x2) */
    -100, -100,
    -800, -800,

    /* Level 2 (3x3) */
    -100, -300, -100,
    -500, -500, -500,
    -800, -800, -800,

    /* Level 3 (4x4) */
    -100, -200, -200, -100,
    -300, -300, -300, -300,
    -600, -600, -600, -600,
    -800, -800, -800, -800,

    /* Level 4 (5x5) */
    -100, -200, -300, -200, -100,
    -300, -300, -300, -300, -300,
    -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600,
    -800, -800, -800, -800, -800,

    /* Level 5 (6x6) */
    -100,    0, -300, -300,    0, -100,
    -100, -100, -300, -300, -100, -100,
    -400, -400, -400, -400, -400, -400,
    -500, -500, -500, -500, -500, -500,
    -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800,

    /* Level 6 (7x7) */
    -100,    0, -200, -300, -200,    0, -100,
    -100, -100, -200, -300, -200, -100, -100,
    -300, -300, -300, -300, -300, -300, -300,
    -500, -500, -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600, -600, -600,
    -700, -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800, -800,

    /* Level 7 (8x8) */
    -100,    0, -200, -300, -300, -200,    0, -100,
    -100, -100, -200, -300, -300, -200, -100, -100,
    -300, -300, -300, -300, -300, -300, -300, -300,
    -400, -400, -400, -400, -400, -400, -400, -400,
    -500, -500, -500, -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600, -600, -600, -600,
    -700, -700, -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800, -800, -800,

    /* Level 8 (7x7) */
    -100,    0, -200, -300, -200,    0, -100,
    -100, -100, -200, -300, -200, -100, -100,
    -300, -300, -300, -300, -300, -300, -300,
    -500, -500, -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600, -600, -600,
    -700, -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800, -800,

    /* Level 9 (6x6) */
    -100,    0, -300, -300,    0, -100,
    -100, -100, -300, -300, -100, -100,
    -400, -400, -400, -400, -400, -400,
    -500, -500, -500, -500, -500, -500,
    -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800,

    /* Level 10 (5x5) */
    -100, -200, -300, -200, -100,
    -300, -300, -300, -300, -300,
    -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600,
    -800, -800, -800, -800, -800,

    /* Level 11 (4x4) */
    -100, -200, -200, -100,
    -300, -300, -300, -300,
    -600, -600, -600, -600,
    -800, -800, -800, -800,

    /* Level 12 (3x3) */
    -100, -300, -100,
    -500, -500, -500,
    -800, -800, -800,

    /* Level 13 (2x2) */
    -100, -100,
    -800, -800
};
int16_t KingPosEndgame[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
     200,

    /* Level 1 (2x2) */
    -300, -300,
    -300, -300,

    /* Level 2 (3x3) */
    -300, -300, -300,
    -300,  300, -300,
    -300, -300, -300,

    /* Level 3 (4x4) */
    -300, -300, -300, -300,
    -300,    0,    0, -300,
    -300,  200,  200, -300,
    -300, -300, -300, -300,

    /* Level 4 (5x5) */
    -300, -300, -300, -300, -300,
    -300,    0,  100,    0, -300,
    -300,  200,  300,  200, -300,
    -300,  200,  300,  200, -300,
    -300, -300, -300, -300, -300,

    /* Level 5 (6x6) */
    -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -200, -300,
    -300, -100,  200,  200, -100, -300,
    -300, -100,  300,  300, -100, -300,
    -300, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300,

    /* Level 6 (7x7) */
    -300, -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -100, -200, -300,
    -300, -100,    0,  100,    0, -100, -300,
    -300, -100,  200,  300,  200, -100, -300,
    -300, -100,  200,  300,  200, -100, -300,
    -300, -100, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300, -300,

    /* Level 7 (8x8) */
    -300, -300, -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -100, -100, -200, -300,
    -300, -100,    0,  100,  100,    0, -100, -300,
    -300, -100,  100,  200,  200,  100, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100, -100, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300, -300, -300,

    /* Level 8 (7x7) */
    -300, -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -100, -200, -300,
    -300, -100,    0,  100,    0, -100, -300,
    -300, -100,  200,  300,  200, -100, -300,
    -300, -100,  200,  300,  200, -100, -300,
    -300, -100, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300, -300,

    /* Level 9 (6x6) */
    -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -200, -300,
    -300, -100,  200,  200, -100, -300,
    -300, -100,  300,  300, -100, -300,
    -300, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300,

    /* Level 10 (5x5) */
    -300, -300, -300, -300, -300,
    -300,    0,  100,    0, -300,
    -300,  200,  300,  200, -300,
    -300,  200,  300,  200, -300,
    -300, -300, -300, -300, -300,

    /* Level 11 (4x4) */
    -300, -300, -300, -300,
    -300,    0,    0, -300,
    -300,  200,  200, -300,
    -300, -300, -300, -300,

    /* Level 12 (3x3) */
    -300, -300, -300,
    -300,  300, -300,
    -300, -300, -300,

    /* Level 13 (2x2) */
    -300, -300,
    -300, -300
};
int16_t KingPosEndgameQueenSide[CBitBoard::SIZE] = {
    /* Level 0 (1x1) */
     100,

    /* Level 1 (2x2) */
    -300, -600,
    -300, -600,

    /* Level 2 (3x3) */
    -300, -300, -600,
     200, -100, -600,
    -300, -300, -600,

    /* Level 3 (4x4) */
    -300, -300, -400, -600,
       0,  100, -200, -600,
     200,  300, -200, -600,
    -300, -300, -400, -600,

    /* Level 4 (5x5) */
    -300, -300, -300, -400, -600,
       0,  100, -100, -200, -600,
     200,  300, -100, -200, -600,
     200,  300, -100, -200, -600,
    -300, -300, -300, -400, -600,

    /* Level 5 (6x6) */
    -300, -300, -300, -300, -500, -600,
    -100, -100, -100, -100, -300, -600,
     100,  200,  100, -100, -300, -600,
     200,  300,  200, -100, -300, -600,
    -100, -100, -100, -100, -300, -600,
    -300, -300, -300, -300, -500, -600,

    /* Level 6 (7x7) */
    -300, -300, -300, -300, -400, -500, -600,
    -100, -100, -100, -100, -200, -300, -600,
       0,  100,  100, -100, -200, -300, -600,
     200,  300,  300, -100, -200, -300, -600,
     200,  300,  300, -100, -200, -300, -600,
    -100, -100, -100, -100, -200, -300, -600,
    -300, -300, -300, -300, -400, -500, -600,

    /* Level 7 (8x8) */
    -300, -300, -300, -300, -300, -400, -500, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
       0,  100,  100,    0, -100, -200, -300, -600,
     100,  200,  200,  100, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
    -300, -300, -300, -300, -300, -400, -500, -600,

    /* Level 8 (7x7) */
    -300, -300, -300, -300, -400, -500, -600,
    -100, -100, -100, -100, -200, -300, -600,
       0,  100,  100, -100, -200, -300, -600,
     200,  300,  300, -100, -200, -300, -600,
     200,  300,  300, -100, -200, -300, -600,
    -100, -100, -100, -100, -200, -300, -600,
    -300, -300, -300, -300, -400, -500, -600,

    /* Level 9 (6x6) */
    -300, -300, -300, -300, -500, -600,
    -100, -100, -100, -100, -300, -600,
     100,  200,  100, -100, -300, -600,
     200,  300,  200, -100, -300, -600,
    -100, -100, -100, -100, -300, -600,
    -300, -300, -300, -300, -500, -600,

    /* Level 10 (5x5) */
    -300, -300, -300, -400, -600,
       0,  100, -100, -200, -600,
     200,  300, -100, -200, -600,
     200,  300, -100, -200, -600,
    -300, -300, -300, -400, -600,

    /* Level 11 (4x4) */
    -300, -300, -400, -600,
       0,  100, -200, -600,
     200,  300, -200, -600,
    -300, -300, -400, -600,

    /* Level 12 (3x3) */
    -300, -300, -600,
     200, -100, -600,
    -300, -300, -600,

    /* Level 13 (2x2) */
    -300, -600,
    -300, -600
};

// Calculated by mirroring KingPosEndgameQueenSide
static int16_t KingPosEndgameKingSide[CBitBoard::SIZE];

int16_t ScaleHalfOpenFilesMine[] = {0, 4, 7, 9, 11};

int16_t ScaleHalfOpenFilesYours[] = {0, 2, 3, 4, 5};

int16_t ScaleOpenFiles[] = {0, 8, 13, 16, 19};

static const int16_t ScaleUp[] = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                                  0,  0,  0,  1,  2,  4,  6,  8,  10, 12, 14,
                                  15, 16, 16, 16, 16, 16, 16, 16, 16, 16};

static const int16_t ScaleDown[] = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
                                    16, 16, 16, 15, 14, 12, 10, 8,  6,  4,  2,
                                    1,  0,  0,  0,  0,  0,  0,  0,  0,  0};

/*
 * MaxPos is the maximum difference between the material balance and
 * a positional evaluation.
 */

int MaxPos;
static const int MaxPosInit = 2000;

/*
 * These scoring parameters will be shared among function calls.
 */

static int RootGamePhase;

/**
 * Masks used in EvaluatePawns.
 */

static const CBitBoard FianchettoMaskWhiteKingSide =
    CBitBoard::SetMask(hf2) | CBitBoard::SetMask(hg3) | CBitBoard::SetMask(hh2);
static const CBitBoard FianchettoMaskBlackKingSide =
    CBitBoard::SetMask(hf7) | CBitBoard::SetMask(hg6) | CBitBoard::SetMask(hh7);
static const CBitBoard FianchettoMaskWhiteQueenSide =
    CBitBoard::SetMask(hc2) | CBitBoard::SetMask(hb3) | CBitBoard::SetMask(ha2);
static const CBitBoard FianchettoMaskBlackQueenSide =
    CBitBoard::SetMask(hc7) | CBitBoard::SetMask(hb6) | CBitBoard::SetMask(ha7);

/**
 * Masks used in EvaluateDevelopment.
 */
static const CBitBoard WKingOpeningMask = CBitBoard::SetMask(he1) | CBitBoard::SetMask(hd1);
static const CBitBoard BKingOpeningMask = CBitBoard::SetMask(he8) | CBitBoard::SetMask(hd8);

static const CBitBoard WKingTrapsRook1 = CBitBoard::SetMask(hf1) | CBitBoard::SetMask(hg1);
static const CBitBoard WRookTrapped1 = CBitBoard::SetMask(hg1) | CBitBoard::SetMask(hh1) | CBitBoard::SetMask(hh2);
static const CBitBoard WKingTrapsRook2 = CBitBoard::SetMask(hc1) | CBitBoard::SetMask(hb1);
static const CBitBoard WRookTrapped2 = CBitBoard::SetMask(hb1) | CBitBoard::SetMask(ha1) | CBitBoard::SetMask(ha2);
static const CBitBoard BKingTrapsRook1 = CBitBoard::SetMask(hf8) | CBitBoard::SetMask(hg8);
static const CBitBoard BRookTrapped1 = CBitBoard::SetMask(hg8) | CBitBoard::SetMask(hh8) | CBitBoard::SetMask(hh7);
static const CBitBoard BKingTrapsRook2 = CBitBoard::SetMask(hc8) | CBitBoard::SetMask(hb8);
static const CBitBoard BRookTrapped2 = CBitBoard::SetMask(hb8) | CBitBoard::SetMask(ha8) | CBitBoard::SetMask(ha7);

static void create_mirrored_piece_square_table(int16_t *, int16_t *);
static bool is_edge(CSCoord);

/**
 * Evaluate the pawn structure.
 *
 * This includes all scoring terms which are independent of piece placement
 * (i.e. only depend on pawn placement).
 *
 * Findings about pawnstructure (e.g. fianchetto pattern or king protection)
 * are stored in pawnFacts.
 *
 */

static int EvaluatePawns(const CPosition *p,
                         struct PawnFacts *pPawnFacts) {
    CBitBoard pcs;
    int nScore = 0;
    int nFile = 0;
    int nTmpW, nTmpB;

    int nKsideOpenFiles = 0;
    int nQsideOpenFiles = 0;
    int nKsideHopenFilesW = 0;
    int nKsideHopenFilesB = 0;
    int nQsideHopenFilesW = 0;
    int nQsideHopenFilesB = 0;

    int nKsidePawnsW = 0;
    int nQsidePawnsW = 0;
    int nKsidePawnsB = 0;
    int nQsidePawnsB = 0;

    pPawnFacts->pf_WhitePassers = {};

    pcs = p->GetMask(White, Pawn);
    while (pcs) {
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        const uint16_t wSq = sqCoord.BitOffset();
        pcs.ClearLowestBit();

        nScore += WPawnPos[wSq];

        /*
         * check if passed
         */

        if (!(p->GetMask(Black, Pawn) & PassedMaskW[wSq])) {
            pPawnFacts->pf_WhitePassers.SetBit(wSq);
        }

        /*
         * check if doubled
         */

        if (p->GetMask(White, Pawn) & ForwardRayW[wSq])
            nScore += DoubledPawn;

        /*
         * check if isolated or backward
         */

        if (!(p->GetMask(White, Pawn) & IsoMask[sqCoord.m_nFile])) {
            nScore += IsolatedPawn[sqCoord.m_nFile];
#ifdef DEBUG
            if (DebugWhat & DebugPawnStructure)
                Print(2, "isolated pawn on %c%c\n", SQUARE(wSq));
#endif
        } else if (!(p->GetMask(White, Pawn) & WPawnBackwardMask[wSq]) &&
                   (p->GetAtkFr(wSq + CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]) &
                    p->GetMask(Black, Pawn))) {
            if (p->GetMask(Black, Pawn) & ForwardRayW[wSq]) {
                nScore += HiddenBackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "hidden backward pawn on %c%c\n", SQUARE(wSq));
#endif
            } else {
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "backward pawn on %c%c\n", SQUARE(wSq));
#endif
                nScore += BackwardPawn;
            }
        }

        const uint16_t wLevelWidth =
            static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]);
        const uint16_t wFile = sqCoord.m_nFile;
        if (wFile < (wLevelWidth - 1) &&
            p->GetMask(White, Pawn).TstBit(wSq + 1)) {
            nScore += PawnDuo;
        }
    }

    pPawnFacts->pf_BlackPassers = {};
    pcs = p->GetMask(Black, Pawn);

    while (pcs) {
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        const uint16_t wSq = sqCoord.BitOffset();

        pcs.ClearLowestBit();
        nScore -= BPawnPos[wSq];

        /*
         * check if passed
         */

        if (!(p->GetMask(White, Pawn) & PassedMaskB[wSq])) {
            pPawnFacts->pf_BlackPassers.SetBit(wSq);
        }

        /*
         * check if doubled
         */

        if (p->GetMask(Black, Pawn) & ForwardRayB[wSq])
            nScore -= DoubledPawn;

        /*
         * check if isolated or backward
         */

        if (!(p->GetMask(Black, Pawn) & IsoMask[sqCoord.m_nFile])) {
            nScore -= IsolatedPawn[sqCoord.m_nFile];
#ifdef DEBUG
            if (DebugWhat & DebugPawnStructure)
                Print(2, "isolated pawn on %c%c\n", SQUARE(wSq));
#endif
        } else if (!(p->GetMask(Black, Pawn) & BPawnBackwardMask[wSq]) &&
                   (p->GetAtkFr(wSq - CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]) &
                    p->GetMask(White, Pawn))) {
            if (p->GetMask(White, Pawn) & ForwardRayB[wSq]) {
                nScore -= HiddenBackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "hidden backward pawn on %c%c\n", SQUARE(wSq));
#endif
            } else {
                nScore -= BackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "backward pawn on %c%c\n", SQUARE(wSq));
#endif
            }
        }

        const unsigned int dwLevelWidth =
            CBitBoard::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)];
        const unsigned int dwFile = static_cast<unsigned int>(sqCoord.m_nFile);
        if (dwFile < (dwLevelWidth - 1) &&
            p->GetMask(Black, Pawn).TstBit(wSq + 1)) {
            nScore -= PawnDuo;
        }
    }

    /*
     * Check for pawn majorities. We only count 'real' majorities, i.e.
     * without doubled pawns.
     */

    nTmpW = (p->GetMask(White, Pawn) & KingSideMask).CountBits();
    nTmpB = (p->GetMask(Black, Pawn) & KingSideMask).CountBits();

    if (nTmpW != nTmpB) {
        nTmpW = nTmpB = 0;
        for (nFile = 0; nFile < 4; nFile++) {
            if (p->GetMask(White, Pawn) & FileMask[nFile])
                nTmpW++;
            if (p->GetMask(Black, Pawn) & FileMask[nFile])
                nTmpB++;
        }

        if (nTmpW > nTmpB) {
            nScore += PawnMajority;
        } else if (nTmpB > nTmpW) {
            nScore -= PawnMajority;
        }
    }

    nTmpW = (p->GetMask(White, Pawn) & QueenSideMask).CountBits();
    nTmpB = (p->GetMask(Black, Pawn) & QueenSideMask).CountBits();

    if (nTmpW != nTmpB) {
        nTmpW = nTmpB = 0;
        for (unsigned int dwFileIndex = CBitBoard::MAX_LEVEL_WIDTH / 2;
             dwFileIndex < CBitBoard::MAX_LEVEL_WIDTH;
             dwFileIndex++) {
            if (p->GetMask(White, Pawn) & FileMask[dwFileIndex])
                nTmpW++;
            if (p->GetMask(Black, Pawn) & FileMask[dwFileIndex])
                nTmpB++;
        }

        if (nTmpW > nTmpB) {
            nScore += PawnMajority;
        } else if (nTmpB > nTmpW) {
            nScore -= PawnMajority;
        }
    }

    for (nFile = 0; nFile < 3; nFile++) {
        int nOpenW = !(p->GetMask(White, Pawn) & FileMask[nFile]);
        int nOpenB = !(p->GetMask(Black, Pawn) & FileMask[nFile]);

        /*
         * Check the queen side
         */

        if (nOpenW && nOpenB) {
            nQsideOpenFiles++;
        } else {
            if (nOpenW) {
                nQsideHopenFilesW++;
            } else {
                if (!p->GetMask(White, Pawn).TstBit(ha2 + nFile)) {
                    nQsidePawnsW++;
                    if (!p->GetMask(White, Pawn).TstBit(ha3 + nFile)) {
                        nQsidePawnsW++;
                    }
                }
            }
            if (nOpenB) {
                nQsideHopenFilesB++;
            } else {
                if (!p->GetMask(Black, Pawn).TstBit(ha7 + nFile)) {
                    nQsidePawnsB++;
                    if (!p->GetMask(Black, Pawn).TstBit(ha6 + nFile)) {
                        nQsidePawnsB++;
                    }
                }
            }
        }

        /*
         * Check the king side
         */

        nOpenW = !(p->GetMask(White, Pawn) & FileMask[7 - nFile]);
        nOpenB = !(p->GetMask(Black, Pawn) & FileMask[7 - nFile]);

        if (nOpenW && nOpenB) {
            nKsideOpenFiles++;
        } else {
            if (nOpenW) {
                nKsideHopenFilesW++;
            } else {
                if (!p->GetMask(White, Pawn).TstBit(hh2 - nFile)) {
                    nKsidePawnsW++;
                    if (!p->GetMask(White, Pawn).TstBit(hh3 - nFile)) {
                        nKsidePawnsW++;
                    }
                }
            }

            if (nOpenB) {
                nKsideHopenFilesB++;
            } else {
                if (!p->GetMask(Black, Pawn).TstBit(hh7 - nFile)) {
                    nKsidePawnsB++;
                    if (!p->GetMask(Black, Pawn).TstBit(hh6 - nFile)) {
                        nKsidePawnsB++;
                    }
                }
            }
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(0, "open : %d %d hopen k: %d %d hopen q: %d %d\n",
              nKsideOpenFiles, nQsideOpenFiles, nKsideHopenFilesW,
              nKsideHopenFilesB, nQsideHopenFilesW, nQsideHopenFilesB);
        Print(0, "pawns w: %d %d pawns b: %d %d\n", nKsidePawnsW,
              nQsidePawnsW, nKsidePawnsB, nQsidePawnsB);
    }
#endif /* DEBUG */

    pPawnFacts->pf_WhiteKingSide =
        (char)(nKsidePawnsW + ScaleHalfOpenFilesMine[nKsideHopenFilesW] +
               ScaleHalfOpenFilesYours[nKsideHopenFilesB] +
               ScaleOpenFiles[nKsideOpenFiles]);

    pPawnFacts->pf_BlackKingSide =
        (char)(nKsidePawnsB + ScaleHalfOpenFilesMine[nKsideHopenFilesB] +
               ScaleHalfOpenFilesYours[nKsideHopenFilesW] +
               ScaleOpenFiles[nKsideOpenFiles]);

    pPawnFacts->pf_WhiteQueenSide =
        (char)(nQsidePawnsW + ScaleHalfOpenFilesMine[nQsideHopenFilesW] +
               ScaleHalfOpenFilesYours[nQsideHopenFilesB] +
               ScaleOpenFiles[nQsideOpenFiles]);

    pPawnFacts->pf_BlackQueenSide =
        (char)(nQsidePawnsB + ScaleHalfOpenFilesMine[nQsideHopenFilesB] +
               ScaleHalfOpenFilesYours[nQsideHopenFilesW] +
               ScaleOpenFiles[nQsideOpenFiles]);

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(0, "king safety white: %d %d\n", pPawnFacts->pf_WhiteKingSide,
              pPawnFacts->pf_WhiteQueenSide);

        Print(0, "king safety black: %d %d\n", pPawnFacts->pf_BlackKingSide,
              pPawnFacts->pf_BlackQueenSide);
    }
#endif /* DEBUG */

    pPawnFacts->pf_Flags = 0;

    pcs = p->GetMask(White, Pawn) | p->GetMask(Black, Pawn);

    if (pcs & KingSideMask) {
        pPawnFacts->pf_Flags |= PawnsOnKingSide;
    }

    if (pcs & QueenSideMask) {
        pPawnFacts->pf_Flags |= PawnsOnQueenSide;
    }

    if ((p->GetMask(White, Pawn) & FianchettoMaskWhiteKingSide) ==
        FianchettoMaskWhiteKingSide) {
        pPawnFacts->pf_Flags |= FianchettoWhiteKingSide;
    }

    if ((p->GetMask(Black, Pawn) & FianchettoMaskBlackKingSide) ==
        FianchettoMaskBlackKingSide) {
        pPawnFacts->pf_Flags |= FianchettoBlackKingSide;
    }

    if ((p->GetMask(White, Pawn) & FianchettoMaskWhiteQueenSide) ==
        FianchettoMaskWhiteQueenSide) {
        pPawnFacts->pf_Flags |= FianchettoWhiteQueenSide;
    }

    if ((p->GetMask(Black, Pawn) & FianchettoMaskBlackQueenSide) ==
        FianchettoMaskBlackQueenSide) {
        pPawnFacts->pf_Flags |= FianchettoBlackQueenSide;
    }

    if (p->GetMask(White, Pawn).TstBit(hd4) && p->GetMask(Black, Pawn).TstBit(hd5)) {
        pPawnFacts->pf_Flags |= QueensPawnOpening;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(2, "EvaluatePawns returns %d\n", nScore);
    }
#endif

    return nScore;
}

/**
 * Look up current pawn structure in pawn hashtable. If not present,
 * use EvaluatePawns() to score the pawn structure. Store result in the
 * pawn hashtable.
 *
 */

static int EvaluatePawnsHashed(const CPosition *p,
                               struct PawnFacts *pPawnFacts) {
    int nScore;

    PTry++;
    if (ProbePT(p->GetPawnKey(), &nScore, pPawnFacts) != Useful) {
        nScore = EvaluatePawns(p, pPawnFacts);
        StorePT(p->GetPawnKey(), nScore, pPawnFacts);
    } else {
        PHit++;
    }

    return nScore;
}

/**
 * Evaluate passed pawns
 */

static int EvaluatePassedPawns(const CPosition *p, int nWhitePhase, int nBlackPhase,
                               struct PawnFacts *pPawnFacts) {
    int nScore = 0;
    int nWhiteDistant = 0;
    int nBlackDistant = 0;

    CBitBoard pcs;
    CBitBoard tmp;
    CBitBoard allpawns = p->GetMask(White, Pawn) | p->GetMask(Black, Pawn);

    CBitBoard WhiteRunner;
    CBitBoard BlackRunner;

    pcs = pPawnFacts->pf_WhitePassers;

    while (pcs) {
#ifdef DEBUG
        int nScoreAtStart = nScore;
#endif
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        const uint16_t wSq = sqCoord.BitOffset();
        const uint16_t wLevelWidth =
            static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]);
        int nRank = sqCoord.m_nRank;
        const uint16_t wFile = sqCoord.m_nFile;

        pcs.ClearLowestBit();

        /* Basic score */

        if (!p->GetMask(Black, 0).TstBit(wSq + wLevelWidth)) {
            nScore += ScaleDown[nWhitePhase] * PassedPawn[nRank] / 16;
        } else {
            nScore += ScaleDown[nWhitePhase] * PassedPawnBlocked[nRank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Evaluate covered passed pawns. */

        if (p->GetAtkFr(wSq) & p->GetMask(White, Pawn)) {
            if (nRank == 5) {
                nScore += CoveredPassedPawn6th;
            }
            if (nRank == 6) {
                nScore += CoveredPassedPawn7th;
            }
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        if (ConnectedMask[wSq] & pPawnFacts->pf_WhitePassers) {
            int nMaxRank = nRank;
            CBitBoard tmp2 = ConnectedMask[wSq] & pPawnFacts->pf_WhitePassers;
            while (tmp2) {
                CSCoord sq2Coord = (tmp2).FindSetBitCoord();
                tmp2.ClearLowestBit();
                int nRank2 = sq2Coord.m_nRank;
                nMaxRank = MAX(nRank2, nMaxRank);
            }
            nScore += ScaleDown[nWhitePhase] * PassedPawnConnected[nMaxRank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Check for rook attacks 'from behind' */

        tmp = p->GetAtkFr(wSq) & ForwardRayB[wSq];

        if (tmp & p->GetMask(White, Rook)) {
            nScore += ScaleDown[nWhitePhase] * RookBehindPasser;
        } else if (tmp & p->GetMask(Black, Rook)) {
            nScore -= ScaleDown[nWhitePhase] * RookBehindPasser;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Check if pawn is out of the king's square */
        if (p->GetNonPawn(Black) == 0) {
            int nSq2 = (p->GetTurn() == White) ? wSq : wSq - wLevelWidth;
            if (!(p->GetMask(Black, King) & KingSquareW[nSq2])) {
                WhiteRunner.SetBit(wSq);
#ifdef DEBUG
                if (DebugWhat & DebugPassedPawns) {
                    Print(0, "white runner on %c%c\n", SQUARE(wSq));
                }
#endif
            }
        }

        /* Check if 'distant' passed pawn */
        if (wFile < (CBitBoard::MAX_LEVEL_WIDTH / 2) && !(allpawns & LeftOf[wFile]) &&
            (allpawns & RightOf[wFile]) &&
            !(p->GetMask(Black, Pawn) & LeftOf[wFile + 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "white outside passed pawn on %c%c\n", SQUARE(wSq));
            }
#endif

            nWhiteDistant = MAX(nWhiteDistant, nRank);
        }

        if (wFile > ((CBitBoard::MAX_LEVEL_WIDTH / 2) - 1) &&
            !(allpawns & RightOf[wFile]) &&
            (allpawns & LeftOf[wFile]) &&
            !(p->GetMask(Black, Pawn) & RightOf[wFile - 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "white outside passed pawn on %c%c\n", SQUARE(wSq));
            }
#endif

            nWhiteDistant = MAX(nWhiteDistant, nRank);
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After white pawn basic scoring: %d\n", nScore);
    }
#endif

    pcs = pPawnFacts->pf_BlackPassers;

    while (pcs) {
#ifdef DEBUG
        int nScoreAtStart = nScore;
#endif
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        const uint16_t wSq = sqCoord.BitOffset();
        const uint16_t wLevelWidth =
            static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[sqCoord.m_nLevel]);
        int nRank = (wLevelWidth - 1) - sqCoord.m_nRank;
        const uint16_t wFile = sqCoord.m_nFile;

        pcs.ClearLowestBit(); //(pcs, sq);

        /* Basic score */

        if (!p->GetMask(White, 0).TstBit(wSq - wLevelWidth)) {
            nScore -= ScaleDown[nBlackPhase] * PassedPawn[nRank] / 16;
        } else {
            nScore -= ScaleDown[nBlackPhase] * PassedPawnBlocked[nRank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Evaluate covered passed pawns. */

        if (p->GetAtkFr(wSq) & p->GetMask(Black, Pawn)) {
            if (nRank == 5) {
                nScore -= CoveredPassedPawn6th;
            }
            if (nRank == 6) {
                nScore -= CoveredPassedPawn7th;
            }
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        if (ConnectedMask[wSq] & pPawnFacts->pf_BlackPassers) {
            int nMaxRank = nRank;
            CBitBoard tmp2 = ConnectedMask[wSq] & pPawnFacts->pf_BlackPassers;
            while (tmp2) {
                CSCoord sq2Coord = (tmp2).FindSetBitCoord();
                tmp2.ClearLowestBit();
                int nRank2 = (wLevelWidth - 1) - sq2Coord.m_nRank;
                nMaxRank = MAX(nRank2, nMaxRank);
            }
            nScore -= ScaleDown[nBlackPhase] * PassedPawnConnected[nMaxRank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Check for rook attacks 'from behind' */

        tmp = p->GetAtkFr(wSq) & ForwardRayW[wSq];

        if (tmp & p->GetMask(White, Rook)) {
            nScore += ScaleDown[nBlackPhase] * RookBehindPasser;
        } else if (tmp & p->GetMask(Black, Rook)) {
            nScore -= ScaleDown[nBlackPhase] * RookBehindPasser;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int nIncrement = nScore - nScoreAtStart;
            Print(0, "score increment from %c%c: %d\n", SQUARE(wSq), nIncrement);
        }
#endif

        /* Check if pawn is out of the king's square */
        if (p->GetNonPawn(White) == 0) {
            int nSq2 = (p->GetTurn() == Black) ? wSq : wSq + wLevelWidth;
            if (!(p->GetMask(White, King) & KingSquareB[nSq2])) {
                BlackRunner.SetBit(wSq);
#ifdef DEBUG
                if (DebugWhat & DebugPassedPawns) {
                    Print(0, "black runner on %c%c\n", SQUARE(wSq));
                }
#endif
            }
        }

        /* Check if 'distant' passed pawn */
        if (wFile < (CBitBoard::MAX_LEVEL_WIDTH / 2) && !(allpawns & LeftOf[wFile]) &&
            (allpawns & RightOf[wFile]) &&
            !(p->GetMask(White, Pawn) & LeftOf[wFile + 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "black outside passed pawn on %c%c\n", SQUARE(wSq));
            }
#endif

            nBlackDistant = MAX(nBlackDistant, nRank);
        }

        if (wFile > ((CBitBoard::MAX_LEVEL_WIDTH / 2) - 1) &&
            !(allpawns & RightOf[wFile]) &&
            (allpawns & LeftOf[wFile]) &&
            !(p->GetMask(White, Pawn) & RightOf[wFile - 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "black outside passed pawn on %c%c\n", SQUARE(wSq));
            }
#endif

            nBlackDistant = MAX(nBlackDistant, nRank);
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After black pawn basic scoring: %d\n", nScore);
    }
#endif

    /*
     * Evaluate pawns that can outrun the king.
     */

    if (WhiteRunner) {
        if (!BlackRunner) {
            nScore += PawnOutrunsKing;
        } else {

            /*
             * Both sides have pawns that can outrun the king.
             * Check who comes first.
             */

            int nWhiteDistance = 5;
            int nBlackDistance = 5;

            while (WhiteRunner) {
                CSCoord sqCoord = (WhiteRunner).FindSetBitCoord();
                int nDistance = static_cast<int>(
                               CBitBoard::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)]) -
                           1 - sqCoord.m_nRank;
                WhiteRunner.ClearLowestBit();

                if (nDistance < nWhiteDistance) {
                    nWhiteDistance = nDistance;
                }
            }

            while (BlackRunner) {
                CSCoord sqCoord = (BlackRunner).FindSetBitCoord();
                int nDistance = sqCoord.m_nRank;
                BlackRunner.ClearLowestBit();

                if (nDistance < nBlackDistance) {
                    nBlackDistance = nDistance;
                }
            }

            if (p->GetTurn() == White) {
                if (nWhiteDistance < nBlackDistance) {
                    nScore += PawnOutrunsKing;
                } else if (nBlackDistance <= (nWhiteDistance - 2)) {
                    nScore -= PawnOutrunsKing;
                }
            } else {
                if (nBlackDistance < nWhiteDistance) {
                    nScore -= PawnOutrunsKing;
                } else if (nWhiteDistance <= (nBlackDistance - 2)) {
                    nScore += PawnOutrunsKing;
                }
            }
        }
    } else if (BlackRunner) {
        nScore -= PawnOutrunsKing;
    }

    /*
     * Evaluate distant passed pawns
     */

    if (nWhiteDistant && nBlackDistant) {
        if (nWhiteDistant > nBlackDistant) {
            nBlackDistant = 0;
        } else if (nBlackDistant > nWhiteDistant) {
            nWhiteDistant = 0;
        } else {
            nWhiteDistant = nBlackDistant = 0;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "Before runner scoring: wdistant: %d bdistant: %d\n", nWhiteDistant,
              nBlackDistant);
    }
#endif

    if (nWhiteDistant && !nBlackDistant) {
        nScore += DistantPassedPawn[nWhitePhase];
    } else if (nBlackDistant && !nWhiteDistant) {
        nScore -= DistantPassedPawn[nBlackPhase];
    } else if (nWhiteDistant && nBlackDistant) {
        if (nWhiteDistant > nBlackDistant) {
            nScore += DistantPassedPawn[nWhitePhase];
        } else if (nBlackDistant > nWhiteDistant) {
            nScore -= DistantPassedPawn[nBlackPhase];
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After runner scoring: %d\n", nScore);
    }
#endif

    return nScore;
}

/**
 * Evaluate the king safety
 */

static int EvaluateKingSafety(const CPosition *p, int nWhitePhase, int nBlackPhase,
                              struct PawnFacts *pPawnFacts) {
    int nScore = 0;

    int nKingSafetyW = 0;
    int nKingSafetyB = 0;

    /*
     * white king safety
     */

    if (p->GetKingSq(White).m_nFile >= 4) {

        /* king side */

        nKingSafetyW = pPawnFacts->pf_WhiteKingSide;

        /* test for fianchetto */

        if (pPawnFacts->pf_Flags & FianchettoWhiteKingSide) {
            if (p->GetMask(White, Bishop).TstBit(hg2)) {
                nKingSafetyW -= 1;
            } else if (!(p->GetMask(White, Bishop) & WhiteSquaresMask) &&
                       (p->GetMask(Black, Bishop) & WhiteSquaresMask)) {
                nKingSafetyW += 2;
            }
        }

    } else {

        /* queen side */

        nKingSafetyW = pPawnFacts->pf_WhiteQueenSide;

        /* test for fianchetto */

        if (pPawnFacts->pf_Flags & FianchettoWhiteQueenSide) {
            if (p->GetMask(White, Bishop).TstBit(hb2)) {
                nKingSafetyW -= 1;
            } else if (!(p->GetMask(White, Bishop) & BlackSquaresMask) &&
                       (p->GetMask(Black, Bishop) & BlackSquaresMask)) {
                nKingSafetyW += 2;
            }
        }
    }

    if (p->GetMask(Black, Queen)) {
        nKingSafetyW *= 2;
    }

    /*
     * black king safety
     */

    if (p->GetKingSq(Black).m_nFile >= 4) {

        /* king side */

        nKingSafetyB = pPawnFacts->pf_BlackKingSide;

        /* test for fianchetto, which is ok */

        if (pPawnFacts->pf_Flags & FianchettoBlackKingSide) {
            if (p->GetMask(Black, Bishop).TstBit(hg7)) {
                nKingSafetyB -= 1;
            } else if (!(p->GetMask(Black, Bishop) & BlackSquaresMask) &&
                       (p->GetMask(White, Bishop) & BlackSquaresMask)) {
                nKingSafetyB += 2;
            }
        }

    } else {

        /* queen side */

        nKingSafetyB = pPawnFacts->pf_BlackQueenSide;

        /* test for fianchetto, which is ok */

        if (pPawnFacts->pf_Flags & FianchettoBlackQueenSide) {
            if (p->GetMask(Black, Bishop).TstBit(hb7)) {
                nKingSafetyB -= 1;
            } else if (!(p->GetMask(Black, Bishop) & WhiteSquaresMask) &&
                       (p->GetMask(White, Bishop) & WhiteSquaresMask)) {
                nKingSafetyB += 2;
            }
        }
    }

    if (p->GetMask(White, Queen)) {
        nKingSafetyB *= 2;
    }

    nScore = -ScaleUp[nWhitePhase] * nKingSafetyW + ScaleUp[nBlackPhase] * nKingSafetyB;

#ifdef DEBUG
    if (DebugWhat & DebugKingSafety) {
        Print(0, "king safety w: %d b: %d total: %d\n", nKingSafetyW,
              nKingSafetyB, nScore);
    }
#endif

    return (KingSafetyScale * nScore) / 256;
}

/**
 * Evaluate the development status
 */

static int EvaluateDevelopment(const CPosition *p) {
    int nScore = 0;
    CBitBoard pcs;

    /*
     * Don't develop pieces to e3/d3 if they block a pawn
     */

    if (p->GetMask(White, Pawn).TstBit(he2) && p->GetMask(White, 0).TstBit(he3)) {
        nScore += PawnDevelopmentBlocked;
    }
    if (p->GetMask(White, Pawn).TstBit(hd2) && p->GetMask(White, 0).TstBit(hd3)) {
        nScore += PawnDevelopmentBlocked;
    }

    /*
     * Don't develop pieces to e6/d6 if they block a pawn
     */

    if (p->GetMask(Black, Pawn).TstBit(he7) && p->GetMask(Black, 0).TstBit(he6)) {
        nScore -= PawnDevelopmentBlocked;
    }
    if (p->GetMask(Black, Pawn).TstBit(hd7) && p->GetMask(Black, 0).TstBit(hd6)) {
        nScore -= PawnDevelopmentBlocked;
    }

    /*
     * Don't leave the king in the center
     */

    if (p->GetMask(White, King) & WKingOpeningMask) {
        nScore += KingInCenter;
    }

    if (p->GetMask(Black, King) & BKingOpeningMask) {
        nScore -= KingInCenter;
    }

    /*
     * Don't make early queen moves
     */

    pcs = p->GetMask(White, Queen);
    while (pcs) {
        CSCoord sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        nScore += QueenPosDevelopment[sq.BitOffset()];
    }

    pcs = p->GetMask(Black, Queen);
    while (pcs) {
        CSCoord sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        nScore -= QueenPosDevelopment[sq.ReflectRank().BitOffset()];
    }

    return nScore;
}

/**
 * Evaluate the material balance.
 */

int MaterialBalance(const CPosition *p) {
    int nScore = p->GetMaterial(White) - p->GetMaterial(Black);

    return nScore;
}

/**
 * Evaluate the position from white points of view.
 */

static int EvaluatePositionForWhite(const CPosition *p) {
    int nScore;

    int nWhitePhase;
    int nBlackPhase;
    const int16_t *pKingPST;
    int nFastScore;
    int nDiff;

    int nTmp;
    CSCoord sq;
    CBitBoard pcs;
    CBitBoard tmpboard;
    struct PawnFacts PawnFactsData;

    /*
     * Lookup the current position in the evaluation hashtable
     */

#ifndef DEBUG
    STry++;
    if (ProbeST(p->GetHashKey(), &nScore) == Useful) {
        SHit++;
        return nScore;
    }
#endif

    nScore = MaterialBalance(p);
    nFastScore = nScore;

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After material balance: %d\n", nScore);
    }
#endif

    nScore += EvaluatePawnsHashed(p, &PawnFactsData);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After hashed pawns: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Kings
     *
     *************************************************************/

    nWhitePhase = MIN(31, p->GetNonPawn(Black) / Value[Pawn]);
    nBlackPhase = MIN(31, p->GetNonPawn(White) / Value[Pawn]);

    nScore += EvaluateKingSafety(p, nWhitePhase, nBlackPhase, &PawnFactsData);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After king safety: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Passed Pawns
     *
     *************************************************************/

    nScore += EvaluatePassedPawns(p, nWhitePhase, nBlackPhase, &PawnFactsData);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After passed pawns: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Trapped Bishops
     *
     *************************************************************/

    if (p->GetMask(White, Bishop).TstBit(ha7) &&
        p->GetMask(Black, Pawn).TstBit(hb6) &&
        (p->GetAtkFr(hb6) & p->GetMask(Black, Pawn))) {
        nScore += BishopTrapped;
    }
    if (p->GetMask(White, Bishop).TstBit(hh7) &&
        p->GetMask(Black, Pawn).TstBit(hg6) &&
        (p->GetAtkFr(hg6) & p->GetMask(Black, Pawn))) {
        nScore += BishopTrapped;
    }
    if (p->GetMask(Black, Bishop).TstBit(ha2) &&
        p->GetMask(White, Pawn).TstBit(hb3) &&
        (p->GetAtkFr(hb3) & p->GetMask(White, Pawn))) {
        nScore -= BishopTrapped;
    }
    if (p->GetMask(Black, Bishop).TstBit(hh2) &&
        p->GetMask(White, Pawn).TstBit(hg3) &&
        (p->GetAtkFr(hg3) & p->GetMask(White, Pawn))) {
        nScore -= BishopTrapped;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After trapped bishops: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Development
     *
     *************************************************************/

    if (RootGamePhase == Opening) {
        nScore += EvaluateDevelopment(p);
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After development: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Kings
     *
     *************************************************************/

    /*
     * Determine which piece/square table to use for kings in the endgame.
     */

    nTmp = PawnFactsData.pf_Flags & (PawnsOnKingSide | PawnsOnQueenSide);

    if (nTmp == PawnsOnKingSide) {
        pKingPST = KingPosEndgameKingSide;
    } else if (nTmp == PawnsOnQueenSide) {
        pKingPST = KingPosEndgameQueenSide;
    } else {
        pKingPST = KingPosEndgame;
    }

    /*
     * Evaluate white king
     */

    nScore += (KingPosMiddlegame[p->GetKingSq(White).BitOffset()] * ScaleUp[nWhitePhase] +
              pKingPST[p->GetKingSq(White).BitOffset()] * ScaleDown[nWhitePhase]) >>
             4;

    /*
     * Check if a king which did not castle blocks a rook in a corner
     */

    if (((p->GetMask(White, King) & WKingTrapsRook1) &&
         (p->GetMask(White, Rook) & WRookTrapped1)) ||
        ((p->GetMask(White, King) & WKingTrapsRook2) &&
         (p->GetMask(White, Rook) & WRookTrapped2))) {
        nScore += KingBlocksRook;
    };

    /*
     * Evaluate black king
     */

    nScore -= (KingPosMiddlegame[p->GetKingSq(Black).ReflectRank().BitOffset()] * ScaleUp[nBlackPhase] +
              pKingPST[p->GetKingSq(Black).ReflectRank().BitOffset()] * ScaleDown[nBlackPhase]) >>
             4;

    /*
     * Check if a king which did not castle blocks a rook in a corner
     */

    if (((p->GetMask(Black, King) & BKingTrapsRook1) &&
         (p->GetMask(Black, Rook) & BRookTrapped1)) ||
        ((p->GetMask(Black, King) & BKingTrapsRook2) &&
         (p->GetMask(Black, Rook) & BRookTrapped2))) {
        nScore -= KingBlocksRook;
    };

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After king: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Knights
     *
     *************************************************************/

    /*
     * Evaluate white knights
     */

    pcs = p->GetMask(White, Knight);
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore += KnightPos[sq.BitOffset()];

        if (is_edge(sq)) {
            nScore += KnightEdgePenalty;
        }

        if (!(p->GetMask(Black, Pawn) & OutpostMaskW[sq.BitOffset()])) {
            nScore += KnightOutpost[sq.BitOffset()];
        }

        nScore += (ScaleUp[nWhitePhase] * KnightKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(Black)))) >>
                 4;

        if (sq.BitOffset() == hc3 && PawnFactsData.pf_Flags & QueensPawnOpening &&
            p->GetMask(White, Pawn).TstBit(hc2)) {
            nScore += KnightBlocksCPawn;
        }
    }

    /*
     * Evaluate black knights
     */

    pcs = p->GetMask(Black, Knight);
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore -= KnightPos[sq.ReflectRank().BitOffset()];

        if (is_edge(sq)) {
            nScore -= KnightEdgePenalty;
        }

        if (!(p->GetMask(White, Pawn) & OutpostMaskB[sq.BitOffset()])) {
            nScore -= KnightOutpost[sq.ReflectRank().BitOffset()];
        }

        nScore -= (ScaleUp[nBlackPhase] * KnightKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(White)))) >>
                 4;

        if (sq.BitOffset() == hc6 && PawnFactsData.pf_Flags & QueensPawnOpening &&
            p->GetMask(Black, Pawn).TstBit(hc7)) {
            nScore -= KnightBlocksCPawn;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After knights: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Bishops
     *
     *************************************************************/

    /*
     * Evaluate white bishops
     */

    pcs = p->GetMask(White, Bishop);

    if ((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask)) {
        nScore += BishopPair[(p->GetMask(White, Pawn)).CountBits()];
    }

    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore += (ScaleUp[nWhitePhase] * BishopPos[sq.BitOffset()]) >> 4;

        nTmp = (p->GetAtkTo(sq.BitOffset()) & ~p->GetMask(White, 0)).CountBits();
        nScore += BishopMobility * (nTmp - 7);

        nScore += (ScaleUp[nWhitePhase] * BishopKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(Black)))) >>
                 4;
    }

    /*
     * Evaluate black bishops
     */

    pcs = p->GetMask(Black, Bishop);

    if ((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask)) {
        nScore -= BishopPair[(p->GetMask(Black, Pawn)).CountBits()];
    }

    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore -= (ScaleUp[nBlackPhase] * BishopPos[sq.ReflectRank().BitOffset()]) >> 4;

        nTmp = (p->GetAtkTo(sq.BitOffset()) & ~p->GetMask(Black, 0)).CountBits();
        nScore -= BishopMobility * (nTmp - 7);

        nScore -= (ScaleUp[nBlackPhase] * BishopKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(White)))) >>
                 4;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After bishops: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Rooks
     *
     *************************************************************/

    /*
     * Evaluate white rooks
     */

    pcs = p->GetMask(White, Rook);
    while (pcs) {
        int nFile;

        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        nFile = sq.m_nFile;

        nScore += (ScaleUp[nWhitePhase] * RookPos[sq.BitOffset()]) >> 4;

        nTmp = (p->GetAtkTo(sq.BitOffset()) & ~p->GetMask(White, 0)).CountBits();
        nScore += RookMobility * (nTmp - 7);

        if (!(FileMask[nFile] & p->GetMask(White, Pawn))) {
            if (!(FileMask[nFile] & p->GetMask(Black, Pawn))) {
                nScore += RookOnOpenFile;
            } else {
                nScore += RookOnSemiOpenFile;
            }
        }

        nScore += (ScaleUp[nWhitePhase] * RookKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(Black)))) >>
                 4;

        tmpboard = p->GetAtkTo(sq.BitOffset()) & ForwardRayW[sq.BitOffset()];
        if (tmpboard & p->GetMask(White, Rook)) {
            nScore += RookConnected;
        }

        if (sq.m_nRank == 6 && p->GetKingSq(Black).m_nRank == 7) {
            nScore += RookOn7thRank;
        }
    }

    /*
     * Evaluate black rooks
     */

    pcs = p->GetMask(Black, Rook);
    while (pcs) {
        int nFile;
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        nFile = sq.m_nFile;

        nScore -= (ScaleUp[nBlackPhase] * RookPos[sq.ReflectRank().BitOffset()]) >> 4;

        nTmp = (p->GetAtkTo(sq.BitOffset()) & ~p->GetMask(Black, 0)).CountBits();
        nScore -= RookMobility * (nTmp - 7);

        if (!(FileMask[nFile] & p->GetMask(Black, Pawn))) {
            if (!(FileMask[nFile] & p->GetMask(White, Pawn))) {
                nScore -= RookOnOpenFile;
            } else {
                nScore -= RookOnSemiOpenFile;
            }
        }

        nScore -= (ScaleUp[nBlackPhase] * RookKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(White)))) >>
                 4;

        tmpboard = p->GetAtkTo(sq.BitOffset()) & ForwardRayB[sq.BitOffset()];
        if (tmpboard & p->GetMask(Black, Rook)) {
            nScore -= RookConnected;
        }

        if (sq.m_nRank == 1 && p->GetKingSq(White).m_nRank == 0) {
            nScore -= RookOn7thRank;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After rooks: %d\n", nScore);
    }
#endif

    /*************************************************************
     *
     * Queens
     *
     *************************************************************/

    /*
     * Evaluate white queens
     */

    pcs = p->GetMask(White, Queen);
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore += QueenPos[sq.BitOffset()];

        nScore += (ScaleUp[nWhitePhase] * QueenKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(Black)))) >>
                 4;
    }

    /*
     * Evaluate black queens
     */

    pcs = p->GetMask(Black, Queen);
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        nScore -= QueenPos[sq.ReflectRank().BitOffset()];

        nScore -= (ScaleUp[nBlackPhase] * QueenKingProximity *
                  (4 - KingDist(sq, p->GetKingSq(White)))) >>
                 4;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After queens: %d\n", nScore);
    }
#endif

    /*
     * Check if both sides have only one bishops. If so, and they are opposite
     * colored, scale the score down.
     */

    if (((p->GetMaterialSignature(White) & 0x1e) == SIGNATURE_BIT(Bishop)) &&
        ((p->GetMaterialSignature(Black) & 0x1e) == SIGNATURE_BIT(Bishop))) {
        bool fWhiteOnWhite =
            (p->GetMask(White, Bishop) & WhiteSquaresMask).IsNotEmpty();
        bool fWhiteOnBlack =
            (p->GetMask(White, Bishop) & BlackSquaresMask).IsNotEmpty();
        bool fBlackOnWhite =
            (p->GetMask(Black, Bishop) & WhiteSquaresMask).IsNotEmpty();
        bool fBlackOnBlack =
            (p->GetMask(Black, Bishop) & BlackSquaresMask).IsNotEmpty();

        bool fWhiteSingleColored = fWhiteOnWhite ^ fWhiteOnBlack;
        bool fBlackSingleColored = fBlackOnWhite ^ fBlackOnBlack;

        if (fWhiteSingleColored && fBlackSingleColored) {
            if ((fWhiteOnWhite && fBlackOnBlack) ||
                (fWhiteOnBlack && fBlackOnWhite)) {
                nScore = 4 * nScore / 5;
            }
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After bishop adjustment: %d\n", nScore);
    }
#endif

    /*
     * Adjust MaxPos if necessary. If the current difference is greater than
     * MaxPos, adjust MaxPos. Otherwise slowly tune MaxPos down to MaxPosInit.
     */

    nDiff = ABS(nScore - nFastScore);
    if (nDiff > MaxPos) {
        MaxPos = MAX(nDiff, MaxPos + 100);
    } else {
        MaxPos = (MaxPosInit + 31 * MaxPos) >> 5;
    }

    if (nScore >= 0) {
        nScore = (nScore + 7) & ~15;
    } else {
        nScore = -((-nScore + 7) & ~15);
    }

    StoreST(p->GetHashKey(), nScore);

    return nScore;
}

/**
 * This is just a convenience routine which handles sign switches
 * if it is not white to move.
 */

int EvaluatePosition(const CPosition *p) {
    if (p->GetTurn() == White)
        return EvaluatePositionForWhite(p);
    else
        return -EvaluatePositionForWhite(p);
}

/**
 * Do the pre-search initialization of evaluation.
 */

void InitEvaluation(const CPosition *p) {
    int nEgThreshold = Value[Queen] + Value[Bishop];

    int nNonPawnMaterial = (p->GetNonPawn(White) + p->GetNonPawn(Black)) / Value[Pawn];

    const unsigned int dwWhiteKingFile = static_cast<unsigned int>(p->GetKingSq(White).m_nFile);
    const unsigned int dwBlackKingFile = static_cast<unsigned int>(p->GetKingSq(Black).m_nFile);
    const unsigned int dwHalfBoardWidth = CBitBoard::MAX_LEVEL_WIDTH / 2;
    int nPawnStorm = 0;

    if (dwWhiteKingFile < (dwHalfBoardWidth - 1) && dwBlackKingFile > dwHalfBoardWidth) {
        nPawnStorm = 1;
    } else if (dwWhiteKingFile > dwHalfBoardWidth && dwBlackKingFile < (dwHalfBoardWidth - 1)) {
        nPawnStorm = 2;
    }


    /*
     * Setup pawn piece/square tables
     */

    for (unsigned int dwLevel = 0; dwLevel < CBitBoard::NUM_LEVELS; dwLevel++) {
        const unsigned int dwLevelWidth = CBitBoard::LEVEL_WIDTH[dwLevel];
        for (unsigned int dwRank = 1; dwRank < dwLevelWidth - 1; dwRank++) {
            for (unsigned int dwFile = 0; dwFile < dwLevelWidth; dwFile++) {
                const uint16_t wSqOffset =
                    CSCoord(static_cast<uint16_t>(dwLevel),
                            static_cast<uint16_t>(dwFile),
                            static_cast<uint16_t>(dwRank))
                        .BitOffset();
                int nWhiteRank = static_cast<int>(dwRank) - 1;
                int nBlackRank = static_cast<int>(dwLevelWidth) - 2 - static_cast<int>(dwRank);
                unsigned int dwWhiteFile = static_cast<unsigned int>(dwFile);
                unsigned int dwBlackFile = static_cast<unsigned int>(dwFile);

                if (dwWhiteKingFile < dwHalfBoardWidth)
                    dwWhiteFile = (CBitBoard::MAX_LEVEL_WIDTH - 1) - dwWhiteFile;
                if (dwBlackKingFile < dwHalfBoardWidth)
                    dwBlackFile = (CBitBoard::MAX_LEVEL_WIDTH - 1) - dwBlackFile;

                if (p->GetNonPawn(Black) < nEgThreshold) {
                    WPawnPos[wSqOffset] = (int16_t)(PawnAdvanceEndgame[dwWhiteFile] * nWhiteRank);
                } else if (p->GetCastle() & 3) {
                    WPawnPos[wSqOffset] = (int16_t)(PawnAdvanceOpening[dwWhiteFile] * nWhiteRank);
                } else {
                    WPawnPos[wSqOffset] = (int16_t)(PawnAdvanceMiddlegame[dwWhiteFile] * nWhiteRank);
                    if (nPawnStorm == 1 && dwWhiteFile > dwHalfBoardWidth) {
                        WPawnPos[wSqOffset] += PawnStorm * nWhiteRank;
                    } else if (nPawnStorm == 2 && dwWhiteFile < (dwHalfBoardWidth - 1)) {
                        WPawnPos[wSqOffset] += PawnStorm * nWhiteRank;
                    }
                }

                if (p->GetNonPawn(White) < nEgThreshold) {
                    BPawnPos[wSqOffset] = (int16_t)(PawnAdvanceEndgame[dwBlackFile] * nBlackRank);
                } else if (p->GetCastle() & 12) {
                    BPawnPos[wSqOffset] = (int16_t)(PawnAdvanceOpening[dwBlackFile] * nBlackRank);
                } else {
                    BPawnPos[wSqOffset] = (int16_t)(PawnAdvanceMiddlegame[dwBlackFile] * nBlackRank);
                    if (nPawnStorm == 1 && dwBlackFile < (dwHalfBoardWidth - 1)) {
                        BPawnPos[wSqOffset] += PawnStorm * nBlackRank;
                    } else if (nPawnStorm == 2 && dwBlackFile > dwHalfBoardWidth) {
                        BPawnPos[wSqOffset] += PawnStorm * nBlackRank;
                    }
                }
            }
        }
    }

    WPawnPos[CSCoord(7, 3, 1).BitOffset()] += CrampingPawn;  /* d2 */
    WPawnPos[CSCoord(7, 4, 1).BitOffset()] += CrampingPawn;  /* e2 */
    BPawnPos[CSCoord(7, 3, 6).BitOffset()] += CrampingPawn;  /* d7 */
    BPawnPos[CSCoord(7, 4, 6).BitOffset()] += CrampingPawn;  /* e7 */

    ClearPawnHashTable();

    /*
     * Set up King piece square table.
     */
    create_mirrored_piece_square_table(KingPosEndgameQueenSide,
                                       KingPosEndgameKingSide);

#ifdef DEBUG
    DebugWhat = 255;
    EvaluatePositionForWhite(p);
    DebugWhat = 0;
#endif

    /*
     * Determine if we are still in the development phase
     */

    RootGamePhase = Middlegame;
    if (nNonPawnMaterial >= 38) {
        bool fDevelopment = (p->GetCastle() != 0);
        int nBackRank =
            ((p->GetMask(White, Knight) | p->GetMask(White, Bishop)) &
                      RankMask[0]).CountBits() +
            ((p->GetMask(Black, Knight) | p->GetMask(Black, Bishop)) &
                      RankMask[7]).CountBits();
        if (nBackRank > 0)
            fDevelopment = true;

        if (fDevelopment)
            RootGamePhase = Opening;
    }

    // Print(2, "GamePhase: %s\n", GamePhaseName[RootGamePhase]);

    MaxPos = MaxPosInit;
}

static bool is_edge(CSCoord coord) {
    const uint16_t wWidth = static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[coord.m_nLevel]);
    return (coord.m_nFile == 0 || coord.m_nFile == (wWidth - 1) || coord.m_nRank == 0 ||
            coord.m_nRank == (wWidth - 1));
}

static void create_mirrored_piece_square_table(int16_t *pSrc, int16_t *pDest) {
    for (unsigned int dwSrcIdx = 0; dwSrcIdx < CBitBoard::SIZE; dwSrcIdx++) {
        const CSCoord source(static_cast<uint16_t>(dwSrcIdx));
        const uint16_t wLevelWidth =
            static_cast<uint16_t>(CBitBoard::LEVEL_WIDTH[source.m_nLevel]);
        const uint16_t wMirroredFile =
            static_cast<uint16_t>(wLevelWidth - 1u - source.m_nFile);
        const CSCoord mirrored(source.m_nLevel, wMirroredFile, source.m_nRank);
        pDest[mirrored.BitOffset()] = pSrc[dwSrcIdx];
    }
}
