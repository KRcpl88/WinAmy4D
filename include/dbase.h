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
 * Constants for chess board squares.
 */
// clang-format off
typedef enum {
    a1 = 0, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
} Square;
// clang-format on

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
    CBitBoard m_rgAtkTo[CSCoord::SIZE];
    CBitBoard m_rgAtkFr[CSCoord::SIZE];
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
    int8_t m_rgPiece[CSCoord::SIZE];
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

// Free functions that don't operate on a position
char *ICS_SAN(CMove move);
void GenRest(CMove *moves);
int GenCaps(CMove *moves, int good);
int GenContactChecks(CMove *moves);
CMove ParseSANList(char *san, Color side, CMove *mvs, int cnt, int *pmap);
CMove ParseGSANList(char *san, Color side, CMove *mvs, int cnt);

#endif

