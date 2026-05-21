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

static int16_t WPawnPos[CSCoord::SIZE];
static int16_t BPawnPos[CSCoord::SIZE];

/**
 * Knight scoring parameters
 */

int KnightKingProximity = 7;
int KnightBlocksCPawn = -100;
int KnightEdgePenalty = -130;

// clang-format off
int16_t KnightPos[CSCoord::SIZE] = {
     -30,  -30,  -30,  -30,  -30,  -30,  -30,  -30,
     -30,  -30,   60,   60,   60,   60,  -30,  -30,
     -30,   60,  130,  130,  130,  130,   60,  -30,
     -30,  130,  190,  190,  190,  190,  130,  -30,
       0,  130,  190,  250,  250,  190,  130,    0,
       0,  190,  250,  250,  250,  250,  190,    0,
       0,   90,  160,  160,  160,  160,   90,    0,
       0,    0,    0,    0,    0,    0,    0,    0
};

int16_t KnightOutpost[CSCoord::SIZE] = {
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
int16_t BishopPos[CSCoord::SIZE] = {
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
int16_t RookPos[CSCoord::SIZE] = {
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

/**
 * Queen scoring parameters
 */

int QueenKingProximity = 8;

// clang-format off
int16_t QueenPos[CSCoord::SIZE] = {
    0, 0,  0,  0,  0,  0,  0,  0,
    0, 30, 30, 30, 30, 30, 30, 0,
    0, 30, 60, 60, 60, 60, 30, 0,
    0, 30, 60, 90, 90, 60, 30, 0,
    0, 30, 60, 90, 90, 60, 30, 0,
    0, 30, 60, 60, 60, 60, 30, 0,
    0, 30, 30, 60, 60, 30, 30, 0,
    0, 0,  0,  0,  0,  0,  0,  0
};

int16_t QueenPosDevelopment[CSCoord::SIZE] = {
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

/**
 * King scoring parameters
 */

int KingBlocksRook = -300;
int KingInCenter = -100;
int KingSafetyScale = 1024;

// clang-format off
int16_t KingPosMiddlegame[CSCoord::SIZE] = {
    -100, 0,    -200, -300, -300, -200,    0, -100,
    -100, -100, -200, -300, -300, -200, -100, -100,
    -300, -300, -300, -300, -300, -300, -300, -300,
    -400, -400, -400, -400, -400, -400, -400, -400,
    -500, -500, -500, -500, -500, -500, -500, -500,
    -600, -600, -600, -600, -600, -600, -600, -600,
    -700, -700, -700, -700, -700, -700, -700, -700,
    -800, -800, -800, -800, -800, -800, -800, -800};

int16_t KingPosEndgame[CSCoord::SIZE] = {
    -300, -300, -300, -300, -300, -300, -300, -300,
    -300, -200, -100, -100, -100, -100, -200, -300,
    -300, -100,    0,  100,  100,    0, -100, -300,
    -300, -100,  100,  200,  200,  100, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100,  200,  300,  300,  200, -100, -300,
    -300, -100, -100, -100, -100, -100, -100, -300,
    -300, -300, -300, -300, -300, -300, -300, -300};

int16_t KingPosEndgameQueenSide[CSCoord::SIZE] = {
    -300, -300, -300, -300, -300, -400, -500, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
       0,  100,  100,    0, -100, -200, -300, -600,
     100,  200,  200,  100, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
     200,  300,  300,  200, -100, -200, -300, -600,
    -100, -100, -100, -100, -100, -200, -300, -600,
    -300, -300, -300, -300, -300, -400, -500, -600};
// clang-format on

// Calculated by mirroring KingPosEndgameQueenSide
static int16_t KingPosEndgameKingSide[CSCoord::SIZE];

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
    CBitBoard::SetMask(f2) | CBitBoard::SetMask(g3) | CBitBoard::SetMask(h2);
static const CBitBoard FianchettoMaskBlackKingSide =
    CBitBoard::SetMask(f7) | CBitBoard::SetMask(g6) | CBitBoard::SetMask(h7);
static const CBitBoard FianchettoMaskWhiteQueenSide =
    CBitBoard::SetMask(c2) | CBitBoard::SetMask(b3) | CBitBoard::SetMask(a2);
static const CBitBoard FianchettoMaskBlackQueenSide =
    CBitBoard::SetMask(c7) | CBitBoard::SetMask(b6) | CBitBoard::SetMask(a7);

/**
 * Masks used in EvaluateDevelopment.
 */
static const CBitBoard WKingOpeningMask = CBitBoard::SetMask(e1) | CBitBoard::SetMask(d1);
static const CBitBoard BKingOpeningMask = CBitBoard::SetMask(e8) | CBitBoard::SetMask(d8);

static const CBitBoard WKingTrapsRook1 = CBitBoard::SetMask(f1) | CBitBoard::SetMask(g1);
static const CBitBoard WRookTrapped1 = CBitBoard::SetMask(g1) | CBitBoard::SetMask(h1) | CBitBoard::SetMask(h2);
static const CBitBoard WKingTrapsRook2 = CBitBoard::SetMask(c1) | CBitBoard::SetMask(b1);
static const CBitBoard WRookTrapped2 = CBitBoard::SetMask(b1) | CBitBoard::SetMask(a1) | CBitBoard::SetMask(a2);
static const CBitBoard BKingTrapsRook1 = CBitBoard::SetMask(f8) | CBitBoard::SetMask(g8);
static const CBitBoard BRookTrapped1 = CBitBoard::SetMask(g8) | CBitBoard::SetMask(h8) | CBitBoard::SetMask(h7);
static const CBitBoard BKingTrapsRook2 = CBitBoard::SetMask(c8) | CBitBoard::SetMask(b8);
static const CBitBoard BRookTrapped2 = CBitBoard::SetMask(b8) | CBitBoard::SetMask(a8) | CBitBoard::SetMask(a7);

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
                         struct PawnFacts *pawnFacts) {
    CBitBoard pcs = 0;
    int score = 0;
    int file = 0;
    int tmp_w, tmp_b;

    int kside_open_files = 0;
    int qside_open_files = 0;
    int kside_hopen_files_w = 0;
    int kside_hopen_files_b = 0;
    int qside_hopen_files_w = 0;
    int qside_hopen_files_b = 0;

    int kside_pawns_w = 0;
    int qside_pawns_w = 0;
    int kside_pawns_b = 0;
    int qside_pawns_b = 0;

    pawnFacts->pf_WhitePassers = 0;

    pcs = p->m_rgMask[White][Pawn];
    while (pcs) {
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        int sq = sqCoord.BitOffset();
        pcs.ClearLowestBit();

        score += WPawnPos[sq];

        /*
         * check if passed
         */

        if (!(p->m_rgMask[Black][Pawn] & PassedMaskW[sq])) {
            pawnFacts->pf_WhitePassers.SetBit(sq);
        }

        /*
         * check if doubled
         */

        if (p->m_rgMask[White][Pawn] & ForwardRayW[sq])
            score += DoubledPawn;

        /*
         * check if isolated or backward
         */

        if (!(p->m_rgMask[White][Pawn] & IsoMask[sqCoord.m_nFile])) {
            score += IsolatedPawn[sqCoord.m_nFile];
#ifdef DEBUG
            if (DebugWhat & DebugPawnStructure)
                Print(2, "isolated pawn on %c%c\n", SQUARE(sq));
#endif
        } else if (!(p->m_rgMask[White][Pawn] & WPawnBackwardMask[sq]) &&
                   (p->m_rgAtkFr[sq + 8] & p->m_rgMask[Black][Pawn])) {
            if (p->m_rgMask[Black][Pawn] & ForwardRayW[sq]) {
                score += HiddenBackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "hidden backward pawn on %c%c\n", SQUARE(sq));
#endif
            } else {
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "backward pawn on %c%c\n", SQUARE(sq));
#endif
                score += BackwardPawn;
            }
        }

        if (sqCoord.m_nFile <
                (static_cast<int>(CSCoord::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)]) - 1) &&
            p->m_rgMask[White][Pawn].TstBit(sq + 1)) {
            score += PawnDuo;
        }
    }

    pawnFacts->pf_BlackPassers = 0;
    pcs = p->m_rgMask[Black][Pawn];

    while (pcs) {
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        int sq = sqCoord.BitOffset();

        pcs.ClearLowestBit();
        score -= BPawnPos[sq];

        /*
         * check if passed
         */

        if (!(p->m_rgMask[White][Pawn] & PassedMaskB[sq])) {
            pawnFacts->pf_BlackPassers.SetBit(sq);
        }

        /*
         * check if doubled
         */

        if (p->m_rgMask[Black][Pawn] & ForwardRayB[sq])
            score -= DoubledPawn;

        /*
         * check if isolated or backward
         */

        if (!(p->m_rgMask[Black][Pawn] & IsoMask[sqCoord.m_nFile])) {
            score -= IsolatedPawn[sqCoord.m_nFile];
#ifdef DEBUG
            if (DebugWhat & DebugPawnStructure)
                Print(2, "isolated pawn on %c%c\n", SQUARE(sq));
#endif
        } else if (!(p->m_rgMask[Black][Pawn] & BPawnBackwardMask[sq]) &&
                   (p->m_rgAtkFr[sq - 8] & p->m_rgMask[White][Pawn])) {
            if (p->m_rgMask[White][Pawn] & ForwardRayB[sq]) {
                score -= HiddenBackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "hidden backward pawn on %c%c\n", SQUARE(sq));
#endif
            } else {
                score -= BackwardPawn;
#ifdef DEBUG
                if (DebugWhat & DebugPawnStructure)
                    Print(2, "backward pawn on %c%c\n", SQUARE(sq));
#endif
            }
        }

        if (sqCoord.m_nFile <
                (static_cast<int>(CSCoord::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)]) - 1) &&
            p->m_rgMask[Black][Pawn].TstBit(sq + 1)) {
            score -= PawnDuo;
        }
    }

    /*
     * Check for pawn majorities. We only count 'real' majorities, i.e.
     * without doubled pawns.
     */

    tmp_w = (p->m_rgMask[White][Pawn] & KingSideMask).CountBits();
    tmp_b = (p->m_rgMask[Black][Pawn] & KingSideMask).CountBits();

    if (tmp_w != tmp_b) {
        tmp_w = tmp_b = 0;
        for (file = 0; file < 4; file++) {
            if (p->m_rgMask[White][Pawn] & FileMask[file])
                tmp_w++;
            if (p->m_rgMask[Black][Pawn] & FileMask[file])
                tmp_b++;
        }

        if (tmp_w > tmp_b) {
            score += PawnMajority;
        } else if (tmp_b > tmp_w) {
            score -= PawnMajority;
        }
    }

    tmp_w = (p->m_rgMask[White][Pawn] & QueenSideMask).CountBits();
    tmp_b = (p->m_rgMask[Black][Pawn] & QueenSideMask).CountBits();

    if (tmp_w != tmp_b) {
        tmp_w = tmp_b = 0;
        for (file = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2);
             file < static_cast<int>(CSCoord::MAX_LEVEL_WIDTH);
             file++) {
            if (p->m_rgMask[White][Pawn] & FileMask[file])
                tmp_w++;
            if (p->m_rgMask[Black][Pawn] & FileMask[file])
                tmp_b++;
        }

        if (tmp_w > tmp_b) {
            score += PawnMajority;
        } else if (tmp_b > tmp_w) {
            score -= PawnMajority;
        }
    }

    for (file = 0; file < 3; file++) {
        int open_w = !(p->m_rgMask[White][Pawn] & FileMask[file]);
        int open_b = !(p->m_rgMask[Black][Pawn] & FileMask[file]);

        /*
         * Check the queen side
         */

        if (open_w && open_b) {
            qside_open_files++;
        } else {
            if (open_w) {
                qside_hopen_files_w++;
            } else {
                if (!p->m_rgMask[White][Pawn].TstBit(a2 + file)) {
                    qside_pawns_w++;
                    if (!p->m_rgMask[White][Pawn].TstBit(a3 + file)) {
                        qside_pawns_w++;
                    }
                }
            }
            if (open_b) {
                qside_hopen_files_b++;
            } else {
                if (!p->m_rgMask[Black][Pawn].TstBit(a7 + file)) {
                    qside_pawns_b++;
                    if (!p->m_rgMask[Black][Pawn].TstBit(a6 + file)) {
                        qside_pawns_b++;
                    }
                }
            }
        }

        /*
         * Check the king side
         */

        open_w = !(p->m_rgMask[White][Pawn] & FileMask[7 - file]);
        open_b = !(p->m_rgMask[Black][Pawn] & FileMask[7 - file]);

        if (open_w && open_b) {
            kside_open_files++;
        } else {
            if (open_w) {
                kside_hopen_files_w++;
            } else {
                if (!p->m_rgMask[White][Pawn].TstBit(h2 - file)) {
                    kside_pawns_w++;
                    if (!p->m_rgMask[White][Pawn].TstBit(h3 - file)) {
                        kside_pawns_w++;
                    }
                }
            }

            if (open_b) {
                kside_hopen_files_b++;
            } else {
                if (!p->m_rgMask[Black][Pawn].TstBit(h7 - file)) {
                    kside_pawns_b++;
                    if (!p->m_rgMask[Black][Pawn].TstBit(h6 - file)) {
                        kside_pawns_b++;
                    }
                }
            }
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(0, "open : %d %d hopen k: %d %d hopen q: %d %d\n",
              kside_open_files, qside_open_files, kside_hopen_files_w,
              kside_hopen_files_b, qside_hopen_files_w, qside_hopen_files_b);
        Print(0, "pawns w: %d %d pawns b: %d %d\n", kside_pawns_w,
              qside_pawns_w, kside_pawns_b, qside_pawns_b);
    }
#endif /* DEBUG */

    pawnFacts->pf_WhiteKingSide =
        (char)(kside_pawns_w + ScaleHalfOpenFilesMine[kside_hopen_files_w] +
               ScaleHalfOpenFilesYours[kside_hopen_files_b] +
               ScaleOpenFiles[kside_open_files]);

    pawnFacts->pf_BlackKingSide =
        (char)(kside_pawns_b + ScaleHalfOpenFilesMine[kside_hopen_files_b] +
               ScaleHalfOpenFilesYours[kside_hopen_files_w] +
               ScaleOpenFiles[kside_open_files]);

    pawnFacts->pf_WhiteQueenSide =
        (char)(qside_pawns_w + ScaleHalfOpenFilesMine[qside_hopen_files_w] +
               ScaleHalfOpenFilesYours[qside_hopen_files_b] +
               ScaleOpenFiles[qside_open_files]);

    pawnFacts->pf_BlackQueenSide =
        (char)(qside_pawns_b + ScaleHalfOpenFilesMine[qside_hopen_files_b] +
               ScaleHalfOpenFilesYours[qside_hopen_files_w] +
               ScaleOpenFiles[qside_open_files]);

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(0, "king safety white: %d %d\n", pawnFacts->pf_WhiteKingSide,
              pawnFacts->pf_WhiteQueenSide);

        Print(0, "king safety black: %d %d\n", pawnFacts->pf_BlackKingSide,
              pawnFacts->pf_BlackQueenSide);
    }
#endif /* DEBUG */

    pawnFacts->pf_Flags = 0;

    pcs = p->m_rgMask[White][Pawn] | p->m_rgMask[Black][Pawn];

    if (pcs & KingSideMask) {
        pawnFacts->pf_Flags |= PawnsOnKingSide;
    }

    if (pcs & QueenSideMask) {
        pawnFacts->pf_Flags |= PawnsOnQueenSide;
    }

    if ((p->m_rgMask[White][Pawn] & FianchettoMaskWhiteKingSide) ==
        FianchettoMaskWhiteKingSide) {
        pawnFacts->pf_Flags |= FianchettoWhiteKingSide;
    }

    if ((p->m_rgMask[Black][Pawn] & FianchettoMaskBlackKingSide) ==
        FianchettoMaskBlackKingSide) {
        pawnFacts->pf_Flags |= FianchettoBlackKingSide;
    }

    if ((p->m_rgMask[White][Pawn] & FianchettoMaskWhiteQueenSide) ==
        FianchettoMaskWhiteQueenSide) {
        pawnFacts->pf_Flags |= FianchettoWhiteQueenSide;
    }

    if ((p->m_rgMask[Black][Pawn] & FianchettoMaskBlackQueenSide) ==
        FianchettoMaskBlackQueenSide) {
        pawnFacts->pf_Flags |= FianchettoBlackQueenSide;
    }

    if (p->m_rgMask[White][Pawn].TstBit(d4) && p->m_rgMask[Black][Pawn].TstBit(d5)) {
        pawnFacts->pf_Flags |= QueensPawnOpening;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPawnStructure) {
        Print(2, "EvaluatePawns returns %d\n", score);
    }
#endif

    return score;
}

/**
 * Look up current pawn structure in pawn hashtable. If not present,
 * use EvaluatePawns() to score the pawn structure. Store result in the
 * pawn hashtable.
 *
 */

static int EvaluatePawnsHashed(const CPosition *p,
                               struct PawnFacts *pawnFacts) {
    int score;

    PTry++;
    if (ProbePT(p->m_ullPKey, &score, pawnFacts) != Useful) {
        score = EvaluatePawns(p, pawnFacts);
        StorePT(p->m_ullPKey, score, pawnFacts);
    } else {
        PHit++;
    }

    return score;
}

/**
 * Evaluate passed pawns
 */

static int EvaluatePassedPawns(const CPosition *p, int wphase, int bphase,
                               struct PawnFacts *pawnFacts) {
    int score = 0;
    int wdistant = 0;
    int bdistant = 0;

    CBitBoard pcs;
    CBitBoard tmp;
    CBitBoard allpawns = p->m_rgMask[White][Pawn] | p->m_rgMask[Black][Pawn];

    CBitBoard wrunner = 0;
    CBitBoard brunner = 0;

    pcs = pawnFacts->pf_WhitePassers;

    while (pcs) {
#ifdef DEBUG
        int score_at_start = score;
#endif
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        int sq = sqCoord.BitOffset();
        const int levelWidth = CSCoord::LEVEL_WIDTH[sqCoord.m_nLevel];
        int rank = sqCoord.m_nRank;
        int file = sqCoord.m_nFile;

        pcs.ClearLowestBit();

        /* Basic score */

        if (!p->m_rgMask[Black][0].TstBit(sq + 8)) {
            score += ScaleDown[wphase] * PassedPawn[rank] / 16;
        } else {
            score += ScaleDown[wphase] * PassedPawnBlocked[rank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Evaluate covered passed pawns. */

        if (p->m_rgAtkFr[sq] & p->m_rgMask[White][Pawn]) {
            if (rank == 5) {
                score += CoveredPassedPawn6th;
            }
            if (rank == 6) {
                score += CoveredPassedPawn7th;
            }
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        if (ConnectedMask[sq] & pawnFacts->pf_WhitePassers) {
            int max_rank = rank;
            CBitBoard tmp2 = ConnectedMask[sq] & pawnFacts->pf_WhitePassers;
            while (tmp2) {
                CSCoord sq2Coord = (tmp2).FindSetBitCoord();
                tmp2.ClearLowestBit();
                int rank2 = sq2Coord.m_nRank;
                max_rank = MAX(rank2, max_rank);
            }
            score += ScaleDown[wphase] * PassedPawnConnected[max_rank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Check for rook attacks 'from behind' */

        tmp = p->m_rgAtkFr[sq] & ForwardRayB[sq];

        if (tmp & p->m_rgMask[White][Rook]) {
            score += ScaleDown[wphase] * RookBehindPasser;
        } else if (tmp & p->m_rgMask[Black][Rook]) {
            score -= ScaleDown[wphase] * RookBehindPasser;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Check if pawn is out of the king's square */
        if (p->m_rgnNonPawn[Black] == 0) {
            int sq2 = (p->m_nTurn == White) ? sq : sq - 8;
            if (!(p->m_rgMask[Black][King] & KingSquareW[sq2])) {
                wrunner.SetBit(sq);
#ifdef DEBUG
                if (DebugWhat & DebugPassedPawns) {
                    Print(0, "white runner on %c%c\n", SQUARE(sq));
                }
#endif
            }
        }

        /* Check if 'distant' passed pawn */
        if (file < static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) && !(allpawns & LeftOf[file]) &&
            (allpawns & RightOf[file]) &&
            !(p->m_rgMask[Black][Pawn] & LeftOf[file + 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "white outside passed pawn on %c%c\n", SQUARE(sq));
            }
#endif

            wdistant = MAX(wdistant, rank);
        }

        if (file > (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1) &&
            !(allpawns & RightOf[file]) &&
            (allpawns & LeftOf[file]) &&
            !(p->m_rgMask[Black][Pawn] & RightOf[file - 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "white outside passed pawn on %c%c\n", SQUARE(sq));
            }
#endif

            wdistant = MAX(wdistant, rank);
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After white pawn basic scoring: %d\n", score);
    }
#endif

    pcs = pawnFacts->pf_BlackPassers;

    while (pcs) {
#ifdef DEBUG
        int score_at_start = score;
#endif
        CSCoord sqCoord = (pcs).FindSetBitCoord();
        int sq = sqCoord.BitOffset();
        const int levelWidth = CSCoord::LEVEL_WIDTH[sqCoord.m_nLevel];
        int rank = (levelWidth - 1) - sqCoord.m_nRank;
        int file = sqCoord.m_nFile;

        pcs.ClearLowestBit(); //(pcs, sq);

        /* Basic score */

        if (!p->m_rgMask[White][0].TstBit(sq - 8)) {
            score -= ScaleDown[bphase] * PassedPawn[rank] / 16;
        } else {
            score -= ScaleDown[bphase] * PassedPawnBlocked[rank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Evaluate covered passed pawns. */

        if (p->m_rgAtkFr[sq] & p->m_rgMask[Black][Pawn]) {
            if (rank == 5) {
                score -= CoveredPassedPawn6th;
            }
            if (rank == 6) {
                score -= CoveredPassedPawn7th;
            }
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        if (ConnectedMask[sq] & pawnFacts->pf_BlackPassers) {
            int max_rank = rank;
            CBitBoard tmp2 = ConnectedMask[sq] & pawnFacts->pf_BlackPassers;
            while (tmp2) {
                CSCoord sq2Coord = (tmp2).FindSetBitCoord();
                tmp2.ClearLowestBit();
                int rank2 = (levelWidth - 1) - sq2Coord.m_nRank;
                max_rank = MAX(rank2, max_rank);
            }
            score -= ScaleDown[bphase] * PassedPawnConnected[max_rank] / 16;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Check for rook attacks 'from behind' */

        tmp = p->m_rgAtkFr[sq] & ForwardRayW[sq];

        if (tmp & p->m_rgMask[White][Rook]) {
            score += ScaleDown[bphase] * RookBehindPasser;
        } else if (tmp & p->m_rgMask[Black][Rook]) {
            score -= ScaleDown[bphase] * RookBehindPasser;
        }

#ifdef DEBUG
        if (DebugWhat & DebugPassedPawns) {
            int increment = score - score_at_start;
            Print(0, "score increment from %c%c: %d\n", SQUARE(sq), increment);
        }
#endif

        /* Check if pawn is out of the king's square */
        if (p->m_rgnNonPawn[White] == 0) {
            int sq2 = (p->m_nTurn == Black) ? sq : sq + 8;
            if (!(p->m_rgMask[White][King] & KingSquareB[sq2])) {
                brunner.SetBit(sq);
#ifdef DEBUG
                if (DebugWhat & DebugPassedPawns) {
                    Print(0, "black runner on %c%c\n", SQUARE(sq));
                }
#endif
            }
        }

        /* Check if 'distant' passed pawn */
        if (file < static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) && !(allpawns & LeftOf[file]) &&
            (allpawns & RightOf[file]) &&
            !(p->m_rgMask[White][Pawn] & LeftOf[file + 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "black outside passed pawn on %c%c\n", SQUARE(sq));
            }
#endif

            bdistant = MAX(bdistant, rank);
        }

        if (file > (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1) &&
            !(allpawns & RightOf[file]) &&
            (allpawns & LeftOf[file]) &&
            !(p->m_rgMask[White][Pawn] & RightOf[file - 2])) {
#ifdef DEBUG
            if (DebugWhat & DebugPassedPawns) {
                Print(0, "black outside passed pawn on %c%c\n", SQUARE(sq));
            }
#endif

            bdistant = MAX(bdistant, rank);
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After black pawn basic scoring: %d\n", score);
    }
#endif

    /*
     * Evaluate pawns that can outrun the king.
     */

    if (wrunner) {
        if (!brunner) {
            score += PawnOutrunsKing;
        } else {

            /*
             * Both sides have pawns that can outrun the king.
             * Check who comes first.
             */

            int wdist = 5;
            int bdist = 5;

            while (wrunner) {
                CSCoord sqCoord = (wrunner).FindSetBitCoord();
                int dist = static_cast<int>(
                               CSCoord::LEVEL_WIDTH[static_cast<unsigned int>(sqCoord.m_nLevel)]) -
                           1 - sqCoord.m_nRank;
                wrunner.ClearLowestBit();

                if (dist < wdist) {
                    wdist = dist;
                }
            }

            while (brunner) {
                CSCoord sqCoord = (brunner).FindSetBitCoord();
                int dist = sqCoord.m_nRank;
                brunner.ClearLowestBit();

                if (dist < bdist) {
                    bdist = dist;
                }
            }

            if (p->m_nTurn == White) {
                if (wdist < bdist) {
                    score += PawnOutrunsKing;
                } else if (bdist <= (wdist - 2)) {
                    score -= PawnOutrunsKing;
                }
            } else {
                if (bdist < wdist) {
                    score -= PawnOutrunsKing;
                } else if (wdist <= (bdist - 2)) {
                    score += PawnOutrunsKing;
                }
            }
        }
    } else if (brunner) {
        score -= PawnOutrunsKing;
    }

    /*
     * Evaluate distant passed pawns
     */

    if (wdistant && bdistant) {
        if (wdistant > bdistant) {
            bdistant = 0;
        } else if (bdistant > wdistant) {
            wdistant = 0;
        } else {
            wdistant = bdistant = 0;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "Before runner scoring: wdistant: %d bdistant: %d\n", wdistant,
              bdistant);
    }
#endif

    if (wdistant && !bdistant) {
        score += DistantPassedPawn[wphase];
    } else if (bdistant && !wdistant) {
        score -= DistantPassedPawn[bphase];
    } else if (wdistant && bdistant) {
        if (wdistant > bdistant) {
            score += DistantPassedPawn[wphase];
        } else if (bdistant > wdistant) {
            score -= DistantPassedPawn[bphase];
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPassedPawns) {
        Print(0, "After runner scoring: %d\n", score);
    }
#endif

    return score;
}

/**
 * Evaluate the king safety
 */

static int EvaluateKingSafety(const CPosition *p, int wphase, int bphase,
                              struct PawnFacts *pawnFacts) {
    int score = 0;

    int king_safety_w = 0;
    int king_safety_b = 0;

    /*
     * white king safety
     */

    if (p->m_rgKingSq[White].m_nFile >= 4) {

        /* king side */

        king_safety_w = pawnFacts->pf_WhiteKingSide;

        /* test for fianchetto */

        if (pawnFacts->pf_Flags & FianchettoWhiteKingSide) {
            if (p->m_rgMask[White][Bishop].TstBit(g2)) {
                king_safety_w -= 1;
            } else if (!(p->m_rgMask[White][Bishop] & WhiteSquaresMask) &&
                       (p->m_rgMask[Black][Bishop] & WhiteSquaresMask)) {
                king_safety_w += 2;
            }
        }

    } else {

        /* queen side */

        king_safety_w = pawnFacts->pf_WhiteQueenSide;

        /* test for fianchetto */

        if (pawnFacts->pf_Flags & FianchettoWhiteQueenSide) {
            if (p->m_rgMask[White][Bishop].TstBit(b2)) {
                king_safety_w -= 1;
            } else if (!(p->m_rgMask[White][Bishop] & BlackSquaresMask) &&
                       (p->m_rgMask[Black][Bishop] & BlackSquaresMask)) {
                king_safety_w += 2;
            }
        }
    }

    if (p->m_rgMask[Black][Queen]) {
        king_safety_w *= 2;
    }

    /*
     * black king safety
     */

    if (p->m_rgKingSq[Black].m_nFile >= 4) {

        /* king side */

        king_safety_b = pawnFacts->pf_BlackKingSide;

        /* test for fianchetto, which is ok */

        if (pawnFacts->pf_Flags & FianchettoBlackKingSide) {
            if (p->m_rgMask[Black][Bishop].TstBit(g7)) {
                king_safety_b -= 1;
            } else if (!(p->m_rgMask[Black][Bishop] & BlackSquaresMask) &&
                       (p->m_rgMask[White][Bishop] & BlackSquaresMask)) {
                king_safety_b += 2;
            }
        }

    } else {

        /* queen side */

        king_safety_b = pawnFacts->pf_BlackQueenSide;

        /* test for fianchetto, which is ok */

        if (pawnFacts->pf_Flags & FianchettoBlackQueenSide) {
            if (p->m_rgMask[Black][Bishop].TstBit(b7)) {
                king_safety_b -= 1;
            } else if (!(p->m_rgMask[Black][Bishop] & WhiteSquaresMask) &&
                       (p->m_rgMask[White][Bishop] & WhiteSquaresMask)) {
                king_safety_b += 2;
            }
        }
    }

    if (p->m_rgMask[White][Queen]) {
        king_safety_b *= 2;
    }

    score = -ScaleUp[wphase] * king_safety_w + ScaleUp[bphase] * king_safety_b;

#ifdef DEBUG
    if (DebugWhat & DebugKingSafety) {
        Print(0, "king safety w: %d b: %d total: %d\n", king_safety_w,
              king_safety_b, score);
    }
#endif

    return (KingSafetyScale * score) / 256;
}

/**
 * Evaluate the development status
 */

static int EvaluateDevelopment(const CPosition *p) {
    int score = 0;
    CBitBoard pcs;

    /*
     * Don't develop pieces to e3/d3 if they block a pawn
     */

    if (p->m_rgMask[White][Pawn].TstBit(e2) && p->m_rgMask[White][0].TstBit(e3)) {
        score += PawnDevelopmentBlocked;
    }
    if (p->m_rgMask[White][Pawn].TstBit(d2) && p->m_rgMask[White][0].TstBit(d3)) {
        score += PawnDevelopmentBlocked;
    }

    /*
     * Don't develop pieces to e6/d6 if they block a pawn
     */

    if (p->m_rgMask[Black][Pawn].TstBit(e7) && p->m_rgMask[Black][0].TstBit(e6)) {
        score -= PawnDevelopmentBlocked;
    }
    if (p->m_rgMask[Black][Pawn].TstBit(d7) && p->m_rgMask[Black][0].TstBit(d6)) {
        score -= PawnDevelopmentBlocked;
    }

    /*
     * Don't leave the king in the center
     */

    if (p->m_rgMask[White][King] & WKingOpeningMask) {
        score += KingInCenter;
    }

    if (p->m_rgMask[Black][King] & BKingOpeningMask) {
        score -= KingInCenter;
    }

    /*
     * Don't make early queen moves
     */

    pcs = p->m_rgMask[White][Queen];
    while (pcs) {
        CSCoord sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        score += QueenPosDevelopment[sq.BitOffset()];
    }

    pcs = p->m_rgMask[Black][Queen];
    while (pcs) {
        CSCoord sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        score -= QueenPosDevelopment[sq.ReflectRank().BitOffset()];
    }

    return score;
}

/**
 * Evaluate the material balance.
 */

int MaterialBalance(const CPosition *p) {
    int score = p->m_rgnMaterial[White] - p->m_rgnMaterial[Black];

    return score;
}

/**
 * Evaluate the position from white points of view.
 */

static int EvaluatePositionForWhite(const CPosition *p) {
    int score;

    int wphase;
    int bphase;
    const int16_t *kingPST;
    int fastscore;
    int diff;

    int tmp;
    CSCoord sq;
    CBitBoard pcs;
    CBitBoard tmpboard;
    struct PawnFacts pawnFacts;

    /*
     * Lookup the current position in the evaluation hashtable
     */

#ifndef DEBUG
    STry++;
    if (ProbeST(p->m_ullHKey, &score) == Useful) {
        SHit++;
        return score;
    }
#endif

    score = MaterialBalance(p);
    fastscore = score;

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After material balance: %d\n", score);
    }
#endif

    score += EvaluatePawnsHashed(p, &pawnFacts);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After hashed pawns: %d\n", score);
    }
#endif

    /*************************************************************
     *
     * Kings
     *
     *************************************************************/

    wphase = MIN(31, p->m_rgnNonPawn[Black] / Value[Pawn]);
    bphase = MIN(31, p->m_rgnNonPawn[White] / Value[Pawn]);

    score += EvaluateKingSafety(p, wphase, bphase, &pawnFacts);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After king safety: %d\n", score);
    }
#endif

    /*************************************************************
     *
     * Passed Pawns
     *
     *************************************************************/

    score += EvaluatePassedPawns(p, wphase, bphase, &pawnFacts);

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After passed pawns: %d\n", score);
    }
#endif

    /*************************************************************
     *
     * Trapped Bishops
     *
     *************************************************************/

    if (p->m_rgMask[White][Bishop].TstBit(a7) &&
        p->m_rgMask[Black][Pawn].TstBit(b6) &&
        (p->m_rgAtkFr[b6] & p->m_rgMask[Black][Pawn])) {
        score += BishopTrapped;
    }
    if (p->m_rgMask[White][Bishop].TstBit(h7) &&
        p->m_rgMask[Black][Pawn].TstBit(g6) &&
        (p->m_rgAtkFr[g6] & p->m_rgMask[Black][Pawn])) {
        score += BishopTrapped;
    }
    if (p->m_rgMask[Black][Bishop].TstBit(a2) &&
        p->m_rgMask[White][Pawn].TstBit(b3) &&
        (p->m_rgAtkFr[b3] & p->m_rgMask[White][Pawn])) {
        score -= BishopTrapped;
    }
    if (p->m_rgMask[Black][Bishop].TstBit(h2) &&
        p->m_rgMask[White][Pawn].TstBit(g3) &&
        (p->m_rgAtkFr[g3] & p->m_rgMask[White][Pawn])) {
        score -= BishopTrapped;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After trapped bishops: %d\n", score);
    }
#endif

    /*************************************************************
     *
     * Development
     *
     *************************************************************/

    if (RootGamePhase == Opening) {
        score += EvaluateDevelopment(p);
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After development: %d\n", score);
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

    tmp = pawnFacts.pf_Flags & (PawnsOnKingSide | PawnsOnQueenSide);

    if (tmp == PawnsOnKingSide) {
        kingPST = KingPosEndgameKingSide;
    } else if (tmp == PawnsOnQueenSide) {
        kingPST = KingPosEndgameQueenSide;
    } else {
        kingPST = KingPosEndgame;
    }

    /*
     * Evaluate white king
     */

    score += (KingPosMiddlegame[p->m_rgKingSq[White].BitOffset()] * ScaleUp[wphase] +
              kingPST[p->m_rgKingSq[White].BitOffset()] * ScaleDown[wphase]) >>
             4;

    /*
     * Check if a king which did not castle blocks a rook in a corner
     */

    if (((p->m_rgMask[White][King] & WKingTrapsRook1) &&
         (p->m_rgMask[White][Rook] & WRookTrapped1)) ||
        ((p->m_rgMask[White][King] & WKingTrapsRook2) &&
         (p->m_rgMask[White][Rook] & WRookTrapped2))) {
        score += KingBlocksRook;
    };

    /*
     * Evaluate black king
     */

    score -= (KingPosMiddlegame[p->m_rgKingSq[Black].ReflectRank().BitOffset()] * ScaleUp[bphase] +
              kingPST[p->m_rgKingSq[Black].ReflectRank().BitOffset()] * ScaleDown[bphase]) >>
             4;

    /*
     * Check if a king which did not castle blocks a rook in a corner
     */

    if (((p->m_rgMask[Black][King] & BKingTrapsRook1) &&
         (p->m_rgMask[Black][Rook] & BRookTrapped1)) ||
        ((p->m_rgMask[Black][King] & BKingTrapsRook2) &&
         (p->m_rgMask[Black][Rook] & BRookTrapped2))) {
        score -= KingBlocksRook;
    };

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After king: %d\n", score);
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

    pcs = p->m_rgMask[White][Knight];
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score += KnightPos[sq.BitOffset()];

        if (is_edge(sq)) {
            score += KnightEdgePenalty;
        }

        if (!(p->m_rgMask[Black][Pawn] & OutpostMaskW[sq.BitOffset()])) {
            score += KnightOutpost[sq.BitOffset()];
        }

        score += (ScaleUp[wphase] * KnightKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[Black]))) >>
                 4;

        if (sq.BitOffset() == c3 && pawnFacts.pf_Flags & QueensPawnOpening &&
            p->m_rgMask[White][Pawn].TstBit(c2)) {
            score += KnightBlocksCPawn;
        }
    }

    /*
     * Evaluate black knights
     */

    pcs = p->m_rgMask[Black][Knight];
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score -= KnightPos[sq.ReflectRank().BitOffset()];

        if (is_edge(sq)) {
            score -= KnightEdgePenalty;
        }

        if (!(p->m_rgMask[White][Pawn] & OutpostMaskB[sq.BitOffset()])) {
            score -= KnightOutpost[sq.ReflectRank().BitOffset()];
        }

        score -= (ScaleUp[bphase] * KnightKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[White]))) >>
                 4;

        if (sq.BitOffset() == c6 && pawnFacts.pf_Flags & QueensPawnOpening &&
            p->m_rgMask[Black][Pawn].TstBit(c7)) {
            score -= KnightBlocksCPawn;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After knights: %d\n", score);
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

    pcs = p->m_rgMask[White][Bishop];

    if ((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask)) {
        score += BishopPair[(p->m_rgMask[White][Pawn]).CountBits()];
    }

    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score += (ScaleUp[wphase] * BishopPos[sq.BitOffset()]) >> 4;

        tmp = (p->m_rgAtkTo[sq.BitOffset()] & ~p->m_rgMask[White][0]).CountBits();
        score += BishopMobility * (tmp - 7);

        score += (ScaleUp[wphase] * BishopKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[Black]))) >>
                 4;
    }

    /*
     * Evaluate black bishops
     */

    pcs = p->m_rgMask[Black][Bishop];

    if ((pcs & WhiteSquaresMask) && (pcs & BlackSquaresMask)) {
        score -= BishopPair[(p->m_rgMask[Black][Pawn]).CountBits()];
    }

    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score -= (ScaleUp[bphase] * BishopPos[sq.ReflectRank().BitOffset()]) >> 4;

        tmp = (p->m_rgAtkTo[sq.BitOffset()] & ~p->m_rgMask[Black][0]).CountBits();
        score -= BishopMobility * (tmp - 7);

        score -= (ScaleUp[bphase] * BishopKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[White]))) >>
                 4;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After bishops: %d\n", score);
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

    pcs = p->m_rgMask[White][Rook];
    while (pcs) {
        int file;

        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        file = sq.m_nFile;

        score += (ScaleUp[wphase] * RookPos[sq.BitOffset()]) >> 4;

        tmp = (p->m_rgAtkTo[sq.BitOffset()] & ~p->m_rgMask[White][0]).CountBits();
        score += RookMobility * (tmp - 7);

        if (!(FileMask[file] & p->m_rgMask[White][Pawn])) {
            if (!(FileMask[file] & p->m_rgMask[Black][Pawn])) {
                score += RookOnOpenFile;
            } else {
                score += RookOnSemiOpenFile;
            }
        }

        score += (ScaleUp[wphase] * RookKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[Black]))) >>
                 4;

        tmpboard = p->m_rgAtkTo[sq.BitOffset()] & ForwardRayW[sq.BitOffset()];
        if (tmpboard & p->m_rgMask[White][Rook]) {
            score += RookConnected;
        }

        if (sq.m_nRank == 6 && p->m_rgKingSq[Black].m_nRank == 7) {
            score += RookOn7thRank;
        }
    }

    /*
     * Evaluate black rooks
     */

    pcs = p->m_rgMask[Black][Rook];
    while (pcs) {
        int file;
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();
        file = sq.m_nFile;

        score -= (ScaleUp[bphase] * RookPos[sq.ReflectRank().BitOffset()]) >> 4;

        tmp = (p->m_rgAtkTo[sq.BitOffset()] & ~p->m_rgMask[Black][0]).CountBits();
        score -= RookMobility * (tmp - 7);

        if (!(FileMask[file] & p->m_rgMask[Black][Pawn])) {
            if (!(FileMask[file] & p->m_rgMask[White][Pawn])) {
                score -= RookOnOpenFile;
            } else {
                score -= RookOnSemiOpenFile;
            }
        }

        score -= (ScaleUp[bphase] * RookKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[White]))) >>
                 4;

        tmpboard = p->m_rgAtkTo[sq.BitOffset()] & ForwardRayB[sq.BitOffset()];
        if (tmpboard & p->m_rgMask[Black][Rook]) {
            score -= RookConnected;
        }

        if (sq.m_nRank == 1 && p->m_rgKingSq[White].m_nRank == 0) {
            score -= RookOn7thRank;
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After rooks: %d\n", score);
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

    pcs = p->m_rgMask[White][Queen];
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score += QueenPos[sq.BitOffset()];

        score += (ScaleUp[wphase] * QueenKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[Black]))) >>
                 4;
    }

    /*
     * Evaluate black queens
     */

    pcs = p->m_rgMask[Black][Queen];
    while (pcs) {
        sq = (pcs).FindSetBitCoord();
        pcs.ClearLowestBit();

        score -= QueenPos[sq.ReflectRank().BitOffset()];

        score -= (ScaleUp[bphase] * QueenKingProximity *
                  (4 - KingDist(sq, p->m_rgKingSq[White]))) >>
                 4;
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After queens: %d\n", score);
    }
#endif

    /*
     * Check if both sides have only one bishops. If so, and they are opposite
     * colored, scale the score down.
     */

    if (((p->m_rgbMaterialSignature[White] & 0x1e) == SIGNATURE_BIT(Bishop)) &&
        ((p->m_rgbMaterialSignature[Black] & 0x1e) == SIGNATURE_BIT(Bishop))) {
        bool white_on_white =
            (p->m_rgMask[White][Bishop] & WhiteSquaresMask) != 0ULL;
        bool white_on_black =
            (p->m_rgMask[White][Bishop] & BlackSquaresMask) != 0ULL;
        bool black_on_white =
            (p->m_rgMask[Black][Bishop] & WhiteSquaresMask) != 0ULL;
        bool black_on_black =
            (p->m_rgMask[Black][Bishop] & BlackSquaresMask) != 0ULL;

        bool white_single_colored = white_on_white ^ white_on_black;
        bool black_single_colored = black_on_white ^ black_on_black;

        if (white_single_colored && black_single_colored) {
            if ((white_on_white && black_on_black) ||
                (white_on_black && black_on_white)) {
                score = 4 * score / 5;
            }
        }
    }

#ifdef DEBUG
    if (DebugWhat & DebugPieces) {
        Print(0, "After bishop adjustment: %d\n", score);
    }
#endif

    /*
     * Adjust MaxPos if necessary. If the current difference is greater than
     * MaxPos, adjust MaxPos. Otherwise slowly tune MaxPos down to MaxPosInit.
     */

    diff = ABS(score - fastscore);
    if (diff > MaxPos) {
        MaxPos = MAX(diff, MaxPos + 100);
    } else {
        MaxPos = (MaxPosInit + 31 * MaxPos) >> 5;
    }

    if (score >= 0) {
        score = (score + 7) & ~15;
    } else {
        score = -((-score + 7) & ~15);
    }

    StoreST(p->m_ullHKey, score);

    return score;
}

/**
 * This is just a convenience routine which handles sign switches
 * if it is not white to move.
 */

int EvaluatePosition(const CPosition *p) {
    if (p->m_nTurn == White)
        return EvaluatePositionForWhite(p);
    else
        return -EvaluatePositionForWhite(p);
}

/**
 * Do the pre-search initialization of evaluation.
 */

void InitEvaluation(const CPosition *p) {
    int sq;

    int eg_threshold = Value[Queen] + Value[Bishop];

    int npmat = (p->m_rgnNonPawn[White] + p->m_rgnNonPawn[Black]) / Value[Pawn];

    int wkfile = p->m_rgKingSq[White].m_nFile;
    int bkfile = p->m_rgKingSq[Black].m_nFile;
    int pawnstorm = 0;

    if (wkfile < (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1) &&
        bkfile > static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2)) {
        pawnstorm = 1;
    } else if (wkfile > static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) &&
               bkfile < (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1)) {
        pawnstorm = 2;
    }

    /*
     * Setup pawn piece/square tables
     */

    for (sq = a2; sq <= h7; sq++) {
        const CSCoord sqCoord(sq);
        const int levelWidth = CSCoord::LEVEL_WIDTH[sqCoord.m_nLevel];
        int wrank = sqCoord.m_nRank - 1;
        int brank = (levelWidth - 2) - sqCoord.m_nRank;
        int wfile = sqCoord.m_nFile;
        int bfile = sqCoord.m_nFile;

        if (p->m_rgKingSq[White].m_nFile < static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2))
            wfile = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH - 1) - wfile;
        if (p->m_rgKingSq[Black].m_nFile < static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2))
            bfile = static_cast<int>(CSCoord::MAX_LEVEL_WIDTH - 1) - bfile;

        if (p->m_rgnNonPawn[Black] < eg_threshold) {
            WPawnPos[sq] = (int16_t)(PawnAdvanceEndgame[wfile] * wrank);
        } else if (p->m_bCastle & 3) {
            WPawnPos[sq] = (int16_t)(PawnAdvanceOpening[wfile] * wrank);
        } else {
            WPawnPos[sq] = (int16_t)(PawnAdvanceMiddlegame[wfile] * wrank);
            if (pawnstorm == 1 && wfile > static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2)) {
                WPawnPos[sq] += PawnStorm * wrank;
            } else if (pawnstorm == 2 &&
                       wfile < (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1)) {
                WPawnPos[sq] += PawnStorm * wrank;
            }
        }

        if (p->m_rgnNonPawn[White] < eg_threshold) {
            BPawnPos[sq] = (int16_t)(PawnAdvanceEndgame[bfile] * brank);
        } else if (p->m_bCastle & 12) {
            BPawnPos[sq] = (int16_t)(PawnAdvanceOpening[bfile] * brank);
        } else {
            BPawnPos[sq] = (int16_t)(PawnAdvanceMiddlegame[bfile] * brank);
            if (pawnstorm == 1 &&
                bfile < (static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2) - 1)) {
                BPawnPos[sq] += PawnStorm * brank;
            } else if (pawnstorm == 2 &&
                       bfile > static_cast<int>(CSCoord::MAX_LEVEL_WIDTH / 2)) {
                BPawnPos[sq] += PawnStorm * brank;
            }
        }
    }

    WPawnPos[d2] += CrampingPawn;
    WPawnPos[e2] += CrampingPawn;
    BPawnPos[d7] += CrampingPawn;
    BPawnPos[e7] += CrampingPawn;

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
    if (npmat >= 38) {
        bool devel = (p->m_bCastle != 0);
        int backrank =
            ((p->m_rgMask[White][Knight] | p->m_rgMask[White][Bishop]) &
                      RankMask[0]).CountBits() +
            ((p->m_rgMask[Black][Knight] | p->m_rgMask[Black][Bishop]) &
                      RankMask[7]).CountBits();
        if (backrank > 0)
            devel = true;

        if (devel)
            RootGamePhase = Opening;
    }

    // Print(2, "GamePhase: %s\n", GamePhaseName[RootGamePhase]);

    MaxPos = MaxPosInit;
}

static bool is_edge(CSCoord coord) {
    const int width = CSCoord::LEVEL_WIDTH[coord.m_nLevel];
    return (coord.m_nFile == 0 || coord.m_nFile == (width - 1) || coord.m_nRank == 0 ||
            coord.m_nRank == (width - 1));
}

static void create_mirrored_piece_square_table(int16_t *src, int16_t *dest) {
    for (int src_idx = 0; src_idx < static_cast<int>(CSCoord::SIZE); src_idx++) {
        const CSCoord source(src_idx);
        const CSCoord mirrored(0, 7 - source.m_nFile, source.m_nRank);
        dest[static_cast<int>(mirrored)] = src[static_cast<int>(source)];
    }
}

