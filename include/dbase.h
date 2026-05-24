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

#ifndef DBASE_H
#define DBASE_H

#include "bitboard.h"
#include "config.h"
#include "heap.h"
#include "scoord.h"
#include "types.h"
#include "ucoord.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SQUARE(x) 'a' + CSCoord(x).m_nFile, '1' + CSCoord(x).m_nRank

#define OPP(x) (1 ^ (x))

#define TYPE(x) (((x) >= 0) ? (x) : -(x))
#define COLOR(x) (((x) == 0) ? Neutral : (((x) > 0) ? White : Black))
#define SAME_COLOR(p, c)                                                       \
    (((c) == White && (p) > 0) || ((c) == Black && (p) < 0))
#define PIECEID(p, c) (((c) == White) ? (p) : -(p))

#define M_CAPTURE CMove::FLAG_CAPTURE
#define M_SCASTLE CMove::FLAG_SCASTLE
#define M_LCASTLE CMove::FLAG_LCASTLE
#define M_PAWND CMove::FLAG_PAWND
#define M_ENPASSANT CMove::FLAG_ENPASSANT
#define M_PROMOTION_OFFSET CMove::PROMOTION_OFFSET
#define M_PROMOTION_MASK CMove::PROMOTION_MASK
#define M_NULL CMove(CSCoord(0), CSCoord(0), M_SCASTLE | M_LCASTLE | M_ENPASSANT)
#define M_HASHED CMove(CSCoord(0), CSCoord(0), \
                       M_CAPTURE | M_SCASTLE | M_LCASTLE | M_ENPASSANT)

#define M_CANY CMove::FLAG_CANY

#define M_TACTICAL CMove::FLAG_TACTICAL

#define M_NONE CMove()

/* Maximum number of good/bad moves we attempt to parse */
#define MAX_EPD_MOVES 64

/*
 * Constants for piece types and colors.
 */
typedef enum {
    Neutral = 0,
    Pawn,
    Knight,
    Bishop,
    Rook,
    Queen,
    King,
    BPawn
} Piece;
typedef enum { White = 0, Black = 1 } Color;

static inline CSCoord InvalidSquareCoord(void) {
    CSCoord coord;
    coord.m_nLevel = -1;
    return coord;
}

/*
 * Named constants for squares on the main 8x8 board (level 'h', level 7).
 * Values equal 140 (= CBitBoard::LEVEL_OFFSET[7]) + rank*8 + file, so they
 * map directly to 3D BitBoard offsets.  ha1 is white's bottom-left corner
 * (offset 140), hh8 is black's top-right corner (offset 203).
 *
 * aa1 and oa1 are the single squares of the bottom-most (level 'a', level 0)
 * and top-most (level 'o', level 14) boards respectively.
 */
// clang-format off
typedef enum {
    ha1 = 140, hb1, hc1, hd1, he1, hf1, hg1, hh1,
    ha2, hb2, hc2, hd2, he2, hf2, hg2, hh2,
    ha3, hb3, hc3, hd3, he3, hf3, hg3, hh3,
    ha4, hb4, hc4, hd4, he4, hf4, hg4, hh4,
    ha5, hb5, hc5, hd5, he5, hf5, hg5, hh5,
    ha6, hb6, hc6, hd6, he6, hf6, hg6, hh6,
    ha7, hb7, hc7, hd7, he7, hf7, hg7, hh7,
    ha8, hb8, hc8, hd8, he8, hf8, hg8, hh8,
    aa1 = 0,   // level 'a' (bottom 1x1 board) -- only square
    oa1 = 343  // level 'o' (top 1x1 board)    -- only square
} Square;
// clang-format on

/*
 * Castling is only valid on the main 8x8 board (level 7).
 * These constants provide proper CSCoord-compatible offsets for castling squares.
 */
static constexpr uint16_t MAIN_LEVEL = 7;
static constexpr uint16_t MAIN_LEVEL_OFFSET = 140; // CBitBoard::LEVEL_OFFSET[7]

// King home squares on level 7
static constexpr int CASTLE_E1 = MAIN_LEVEL_OFFSET + 0 * 8 + 4; // 144
static constexpr int CASTLE_E8 = MAIN_LEVEL_OFFSET + 7 * 8 + 4; // 200

// Short castle target squares
static constexpr int CASTLE_G1 = MAIN_LEVEL_OFFSET + 0 * 8 + 6; // 146
static constexpr int CASTLE_G8 = MAIN_LEVEL_OFFSET + 7 * 8 + 6; // 202

// Long castle target squares
static constexpr int CASTLE_C1 = MAIN_LEVEL_OFFSET + 0 * 8 + 2; // 142
static constexpr int CASTLE_C8 = MAIN_LEVEL_OFFSET + 7 * 8 + 2; // 198

// Squares checked during castling legality (must be empty/not attacked)
static constexpr int CASTLE_F1 = MAIN_LEVEL_OFFSET + 0 * 8 + 5; // 145
static constexpr int CASTLE_F8 = MAIN_LEVEL_OFFSET + 7 * 8 + 5; // 201
static constexpr int CASTLE_D1 = MAIN_LEVEL_OFFSET + 0 * 8 + 3; // 143
static constexpr int CASTLE_D8 = MAIN_LEVEL_OFFSET + 7 * 8 + 3; // 199
static constexpr int CASTLE_B1 = MAIN_LEVEL_OFFSET + 0 * 8 + 1; // 141
static constexpr int CASTLE_B8 = MAIN_LEVEL_OFFSET + 7 * 8 + 1; // 197

// Rook home squares for castling
static constexpr int CASTLE_H1 = MAIN_LEVEL_OFFSET + 0 * 8 + 7; // 147
static constexpr int CASTLE_H8 = MAIN_LEVEL_OFFSET + 7 * 8 + 7; // 203
static constexpr int CASTLE_A1 = MAIN_LEVEL_OFFSET + 0 * 8 + 0; // 140
static constexpr int CASTLE_A8 = MAIN_LEVEL_OFFSET + 7 * 8 + 0; // 196

struct SGameLog {
    CMove gl_Move;        /* the move that has been made in the position */
    int8_t gl_Piece;       /* the piece that was captured (if any) */
    int8_t gl_Castle;      /* the castling rights */
    CSCoord gl_EnPassant;  /* the enpassant target square (if any) */
    uint8_t gl_IrrevCount; /* number of moves since last irreversible move */
    hash_t gl_HashKey;     /* used to detect repetitions */
    hash_t gl_PawnKey;
};

class CPosition {
  public:
    // Data members (public for direct access from engine code)
    CBitBoard m_rgAtkTo[CBitBoard::SIZE];
    CBitBoard m_rgAtkFr[CBitBoard::SIZE];
    CBitBoard m_rgMask[2][7];
    CBitBoard m_SlidingPieces;
    hash_t m_ullHKey;
    hash_t m_ullPKey;
    SGameLog *m_pGameLog;
    SGameLog *m_pActLog;
    unsigned int m_cGameLog;
    int m_rgnMaterial[2], m_rgnNonPawn[2];
    uint16_t m_rgwOutOfBookCnt[2];
    uint16_t m_wPly;
    int8_t m_rgPiece[CBitBoard::SIZE];
    int8_t m_bCastle;
    CSCoord m_EnPassant;
    int8_t m_nTurn; /* 0 == white, 1 == black */
    CSCoord m_rgKingSq[2];
    int8_t m_rgbMaterialSignature[2];

    // Move making/unmaking
    void DoMove(CMove move);
    void UndoMove(CMove move);
    void DoNull();
    void UndoNull();

    // Move generation
    void GenTo(const CSCoord& square, heap_t heap);
    void GenEnpas(heap_t heap);
    void GenFrom(const CSCoord& square, heap_t heap);
    void GenChecks(heap_t heap);
    bool MayCastle(CMove move);
    bool LegalMove(CMove move);
    bool IsCheckingMove(CMove move);
    int LegalMoves(heap_t heap);
    void PLegalMoves(heap_t heap);

    // Position queries
    int Repeated(int mode);
    bool InCheck(int side) const;
    void RecalcAttacks();
    const char *GameEnd();
    bool CheckDraw() const;
    bool IsPassed(const CSCoord& sq, int color) const;

    // Attack map maintenance
    void AtkSet(int piece, int side, const CSCoord& sq);
    void AtkClr(const CSCoord& sq);
    void GainAttack(const CSCoord& from, const CSCoord& to);
    void LooseAttack(const CSCoord& from, const CSCoord& to);
    void GainAttacks(const CSCoord& to);
    void LooseAttacks(const CSCoord& to);

    // Notation
    char *SAN(CMove move, char *buffer);
    CMove ParseSAN(const char *san);
    CMove ParseGSAN(char *san);
    char *MakeEPD();

    // Display
    void ShowPosition();
    void ShowMoves();
    void TestNextGenerators();

    // Search entry points
    CMove Iterate(int *score_ptr, CMove alternate_move, int *alternate_score_ptr);
    void SearchRoot();
    void AnalysisMode();
    int PermanentBrain();
    int QuiescenceSearch();
    int CheckExtend();
    int ScoreMove(CMove move);
    char *NumberedSAN(CMove move, char *buffer, size_t len);
    void AnaLoop(int depth);
    void AnalyzeHT(CMove move);
#if MP
    void StartHelpers();
#endif

    // Static factory methods
    static CPosition *CreateFromEPD(const char *epd);
    static CPosition *Initial();
    static CPosition *Clone(const CPosition *src);
    static void Free(CPosition *p);
};

// Backward compatibility typedef
typedef CPosition Position;

extern int Value[];
extern CMove goodmove[MAX_EPD_MOVES];
extern CMove badmove[MAX_EPD_MOVES];
extern char PieceName[];
extern const int8_t CastleMask[2][2];

// Attack delta table for piece movement directions
static const int ATTACK_DELTA_MAX = 24;
extern const CUCoord ATTACK_DELTA[BPawn + 1][ATTACK_DELTA_MAX + 1];
extern const int ATTACK_DELTA_COUNT[BPawn + 1];

// Runtime attack computation using ATTACK_DELTA ray-walk
CBitBoard ComputeSlidingAttacks(const CSCoord &sq, int pieceType,
                                const CBitBoard &occupied);
CBitBoard ComputeLeapAttacks(const CSCoord &sq, int pieceType);

// Free functions that don't operate on a position
char *ICS_SAN(CMove move);
void GenRest(CMove *moves);
int GenCaps(CMove *moves, int good);
int GenContactChecks(CMove *moves);
CMove ParseSANList(char *san, Color side, CMove *mvs, int cnt, int *pmap);
CMove ParseGSANList(char *san, Color side, CMove *mvs, int cnt);

#endif

