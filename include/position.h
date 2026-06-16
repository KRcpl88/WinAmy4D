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

#ifndef POSITION_H
#define POSITION_H

#include "dbase.h"

#include "bitboard.h"
#include "heap.h"
#include "scoord.h"
#include "types.h"
#include "ucoord.h"
#include <stddef.h>
#include <stdint.h>

class CSearchData;

class CPosition {
  public:
    // Move making/unmaking
    void DoMove(CMove move);
    void UndoMove(CMove move);
    bool Undo();
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
    bool IsKingCapture(CMove move) const;
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
    CMove ProbeBestMove();
#if MP
    void StartHelpers();
#endif

    // Static factory methods
    static CPosition *CreateFromEPD(const char *epd);
    static bool IsValidEPD(const char *epd);
    static CPosition *Initial();
    static CPosition *Clone(const CPosition *src);
    static void Free(CPosition *p);

    // ----------------------------------------------------------------------
    // Accessors for the (private) data members.  Only the accessors that are
    // actually used outside the class are provided.  Index parameters use an
    // unsigned type (negative array offsets are never valid here) and every
    // accessor matches the signed/unsigned attribute of the member it exposes.
    // ----------------------------------------------------------------------

    // Attack tables.  These are only ever populated from inside the class, so
    // no setters are required; callers just read the bitboard at an offset.
    const CBitBoard &GetAtkTo(uint16_t wOffset) const { return m_rgAtkTo[wOffset]; }
    const CBitBoard &GetAtkFr(uint16_t wOffset) const { return m_rgAtkFr[wOffset]; }

    // Piece / occupancy bitboards.  These are mutated in place (SetBit/ClrBit)
    // during board setup and move making, so a reference accessor is provided.
    CBitBoard &GetMask(uint16_t wSide, uint16_t wPiece) {
        return m_rgMask[wSide][wPiece];
    }
    const CBitBoard &GetMask(uint16_t wSide, uint16_t wPiece) const {
        return m_rgMask[wSide][wPiece];
    }

    CBitBoard &GetSlidingPieces() { return m_SlidingPieces; }
    const CBitBoard &GetSlidingPieces() const { return m_SlidingPieces; }

    hash_t GetHashKey() const { return m_ullHKey; }
    void SetHashKey(hash_t ullHashKey) { m_ullHKey = ullHashKey; }

    hash_t GetPawnKey() const { return m_ullPKey; }

    SGameLog *GetGameLog() const { return m_pGameLog; }
    SGameLog *GetActLog() const { return m_pActLog; }

    int GetMaterial(uint16_t wSide) const { return m_rgnMaterial[wSide]; }
    int GetNonPawn(uint16_t wSide) const { return m_rgnNonPawn[wSide]; }

    uint16_t GetPly() const { return m_wPly; }
    void SetPly(uint16_t wPly) { m_wPly = wPly; }

    int8_t GetPiece(uint16_t wOffset) const { return m_rgPiece[wOffset]; }
    void SetPiece(uint16_t wOffset, int8_t nPiece) { m_rgPiece[wOffset] = nPiece; }

    int8_t GetCastle() const { return m_bCastle; }
    void SetCastle(int8_t bCastle) { m_bCastle = bCastle; }

    const CSCoord &GetEnPassant() const { return m_EnPassant; }
    void SetEnPassant(const CSCoord &enPassant) { m_EnPassant = enPassant; }

    int8_t GetTurn() const { return m_nTurn; }
    void SetTurn(int8_t nTurn) { m_nTurn = nTurn; }

    const CSCoord &GetKingSq(uint16_t wSide) const { return m_rgKingSq[wSide]; }
    void SetKingSq(uint16_t wSide, const CSCoord &kingSq) {
        m_rgKingSq[wSide] = kingSq;
    }

    int8_t GetMaterialSignature(uint16_t wSide) const {
        return m_rgbMaterialSignature[wSide];
    }

    // Search progress. While Iterate() is running, m_pSearchData points at the
    // live CSearchData driving that search; the GUI status bar polls it (on
    // another thread) to report progress as a fraction of the root moves
    // searched. It is null whenever no search is in flight on this position. A
    // momentarily torn read only perturbs a progress percentage, which is
    // harmless.
    const CSearchData *GetSearchData() const { return m_pSearchData; }

  private:
    // Data members
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

    // Non-owning pointer to the CSearchData driving the in-flight Iterate() on
    // this position, or null when no search is active. Set/cleared by Iterate()
    // and read by GetSearchData(). Zero-initialised by safe_calloc.
    CSearchData *m_pSearchData;
};

// Backward compatibility typedef
typedef CPosition Position;

#endif
