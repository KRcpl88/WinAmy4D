#include "TestHelpers.h"
#include "heap.h"
#include "time_ctl.h"
#include <vector>
#include <string>
#include <sstream>

namespace WinAmyTests {

TEST_CLASS(ReproTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
        AllocateHT();
    }

    TEST_METHOD(EngineCorruptedRepro) {
        const char *epd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|pppppp1/4n1p/7/7/7/7/"
            "PPPPPPP|rnbqkb1r/ppp1pppp/8/4N3/3P4/1P6/4PPPP/RNBQ1B1R|r1bq1b1/"
            "ppppppp/7/7/2P4/PPP1PnP/2BQKBR|pppppp/6/6/6/2N3/PPPPPP|5/5/5/5/5|"
            "4/4/4/4|3/3/3|2/2|1 b q -";

        CPosition *p = CPosition::CreateFromEPD(epd);
        Assert::IsNotNull(p, L"EPD did not parse");
        PositionGuard pos(p);

        std::ostringstream os;
        os << "turn=" << pos.get()->GetTurn() << "\n";

        // Snapshot pieces before search.
        std::vector<int> before(CBitBoard::SIZE);
        for (unsigned int i = 0; i < CBitBoard::SIZE; i++)
            before[i] = pos.get()->GetPiece(i);

        SetMaxSearchDepth(7);
        bool fFailed = false;
        for (int iter = 0; iter < 60 && !fFailed; iter++) {
            CPosition *pClone = CPosition::Clone(pos.get());
            int score = 0, alt = 0;
            CMove mv = pClone->Iterate(&score, M_NONE, &alt);
            bool fLegal = pos.get()->LegalMove(mv);
            if (!fLegal) {
                os << "ITER " << iter << " ILLEGAL best from="
                   << (int)mv.GetFromCoord().BitOffset()
                   << " (L" << mv.GetFromCoord().m_nLevel << "F"
                   << mv.GetFromCoord().m_nFile << "R" << mv.GetFromCoord().m_nRank
                   << ") to=" << (int)mv.GetToCoord().BitOffset()
                   << " (L" << mv.GetToCoord().m_nLevel << "F"
                   << mv.GetToCoord().m_nFile << "R" << mv.GetToCoord().m_nRank
                   << ")\n";
                fFailed = true;
            }
            CPosition::Free(pClone);
        }
        (void)before;

        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        Assert::IsFalse(fFailed, w.c_str());
    }
    TEST_METHOD(CountRootMovesCorruptedPosition) {
        const char *epd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|pppppp1/4n1p/7/7/7/7/"
            "PPPPPPP|rnbqkb1r/ppp1pppp/8/4N3/3P4/1P6/4PPPP/RNBQ1B1R|r1bq1b1/"
            "ppppppp/7/7/2P4/PPP1PnP/2BQKBR|pppppp/6/6/6/2N3/PPPPPP|5/5/5/5/5|"
            "4/4/4/4|3/3/3|2/2|1 b q -";

        CPosition *p = CPosition::CreateFromEPD(epd);
        Assert::IsNotNull(p, L"EPD did not parse");
        PositionGuard pos(p);

        heap_t heap = allocate_heap();
        push_section(heap);
        int n = (int)pos.get()->LegalMoves(heap);
        free_heap(heap);

        std::ostringstream os;
        os << "root legal moves = " << n << "\n";
        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        // The search keeps per-root-move bookkeeping in a fixed 256-entry stack
        // array (nodes[256] in IterateInt); more than 256 root moves overflows it.
        Assert::IsTrue(n <= 256, w.c_str());
    }

    TEST_METHOD(GameReplayMaskConsistency) {
        const char *moves[] = {
            "Phd2hd4", "Phd7hd5", "Phc2hc4", "Phd5xhc4", "Nhg1hf3", "Nie7id5",
            "Phb2hb3", "Phc4xhb3", "Pha2xhb3", "Nhg8ge6", "Ria1bb2", "Nib7hd6",
            "Nib1ic3", "Nid5xic3", "Pid2xic3", "Pgg7gg6", "Nhf3he5", "Nhd6ie4",
            "Rbb2xhh8", "Rig7xhh8", "Nie1jc2", "Nie4xif2", "Khe1ie1"};
        const int nMoves = sizeof(moves) / sizeof(moves[0]);

        PositionGuard pos(CPosition::Initial());
        std::ostringstream os;
        bool fFailed = false;

        for (int m = 0; m < nMoves && !fFailed; m++) {
            CMove mv = pos.get()->ParseSAN(moves[m]);
            if (mv == M_NONE) {
                os << "move " << m << " '" << moves[m] << "' did not parse\n";
                fFailed = true;
                break;
            }
            pos.get()->DoMove(mv);
            // Mirror the GUI: full recompute after every applied move.
            pos.get()->RecalcAttacks();

            // The occupancy mask [color][0] must equal the set of squares that
            // actually hold a piece of that color in m_rgPiece.
            for (int color = 0; color < 2; color++) {
                for (unsigned int sq = 0; sq < CBitBoard::SIZE; sq++) {
                    int pc = pos.get()->GetPiece(sq);
                    bool fHasColorPiece =
                        (color == 0) ? (pc > 0) : (pc < 0);
                    bool fMaskBit = pos.get()->GetMask((uint16_t)color, 0).TstBit(sq);
                    if (fHasColorPiece != fMaskBit) {
                        os << "after move " << m << " '" << moves[m]
                           << "': occupancy mismatch color=" << color
                           << " sq=" << sq << " piece=" << pc
                           << " maskBit=" << (int)fMaskBit << "\n";
                        fFailed = true;
                    }
                }
            }
        }

        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        Assert::IsFalse(fFailed, w.c_str());
    }

    TEST_METHOD(GameReplayThenSearchLegal) {
        const char *moves[] = {
            "Phd2hd4", "Phd7hd5", "Phc2hc4", "Phd5xhc4", "Nhg1hf3", "Nie7id5",
            "Phb2hb3", "Phc4xhb3", "Pha2xhb3", "Nhg8ge6", "Ria1bb2", "Nib7hd6",
            "Nib1ic3", "Nid5xic3", "Pid2xic3", "Pgg7gg6", "Nhf3he5", "Nhd6ie4",
            "Rbb2xhh8", "Rig7xhh8", "Nie1jc2", "Nie4xif2", "Khe1ie1"};
        const int nMoves = sizeof(moves) / sizeof(moves[0]);

        PositionGuard pos(CPosition::Initial());
        std::ostringstream os;
        for (int m = 0; m < nMoves; m++) {
            CMove mv = pos.get()->ParseSAN(moves[m]);
            Assert::IsTrue(mv != M_NONE, L"move did not parse");
            pos.get()->DoMove(mv);
            pos.get()->RecalcAttacks();
        }

        SetMaxSearchDepth(60);
        SetFixedTimePerMove(5);
        bool fFailed = false;
        for (int iter = 0; iter < 20 && !fFailed; iter++) {
            CPosition *pClone = CPosition::Clone(pos.get());
            int score = 0, alt = 0;
            CMove mv = pClone->Iterate(&score, M_NONE, &alt);
            if (!pos.get()->LegalMove(mv)) {
                os << "ITER " << iter << " ILLEGAL best L"
                   << mv.GetFromCoord().m_nLevel << "F" << mv.GetFromCoord().m_nFile
                   << "R" << mv.GetFromCoord().m_nRank << " -> L"
                   << mv.GetToCoord().m_nLevel << "F" << mv.GetToCoord().m_nFile
                   << "R" << mv.GetToCoord().m_nRank << "\n";
                fFailed = true;
            }
            CPosition::Free(pClone);
        }

        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        Assert::IsFalse(fFailed, w.c_str());
    }

    // Reproduce the exact game recorded in engine-corrupted.pgn4: replay its
    // move sequence from the initial position, then run a single timed search
    // at 5 seconds per move - the time control that triggered the original
    // "engine produced an illegal/corrupt move" repro. The returned best move
    // must be legal in the resulting position. Engine diagnostics emitted via
    // Print/PrintDebug during the search are captured to the module log file.
    TEST_METHOD(EngineCorruptedPgnTimedSearchProducesLegalMove) {
        // Moves transcribed from engine-corrupted.pgn4 (check annotation on
        // Nie4xif2+ dropped, matching the SAN the parser accepts).
        const char *moves[] = {
            "Phd2hd4", "Phd7hd5", "Phc2hc4", "Phd5xhc4", "Nhg1hf3", "Nie7id5",
            "Phb2hb3", "Phc4xhb3", "Pha2xhb3", "Nhg8ge6", "Ria1bb2", "Nib7hd6",
            "Nib1ic3", "Nid5xic3", "Pid2xic3", "Pgg7gg6", "Nhf3he5", "Nhd6ie4",
            "Rbb2xhh8", "Rig7xhh8", "Nie1jc2", "Nie4xif2", "Khe1ie1"};
        const int nMoves = sizeof(moves) / sizeof(moves[0]);

        PositionGuard pos(CPosition::Initial());
        for (int m = 0; m < nMoves; m++) {
            CMove mv = pos.get()->ParseSAN(moves[m]);
            Assert::IsTrue(mv != M_NONE, L"move did not parse");
            pos.get()->DoMove(mv);
            pos.get()->RecalcAttacks();
        }

        // Start a timed search at 5 seconds, as used for the repro.
        SetMaxSearchDepth(60);
        SetFixedTimePerMove(5);

        CPosition *pClone = CPosition::Clone(pos.get());
        int score = 0, alt = 0;
        CMove BestMove = pClone->Iterate(&score, M_NONE, &alt);
        bool fLegal = pos.get()->LegalMove(BestMove);
        CPosition::Free(pClone);

        std::ostringstream os;
        os << "best move L" << BestMove.GetFromCoord().m_nLevel << "F"
           << BestMove.GetFromCoord().m_nFile << "R"
           << BestMove.GetFromCoord().m_nRank << " -> L"
           << BestMove.GetToCoord().m_nLevel << "F"
           << BestMove.GetToCoord().m_nFile << "R"
           << BestMove.GetToCoord().m_nRank << " legal=" << (int)fLegal << "\n";
        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        Assert::IsTrue(fLegal, w.c_str());
    }

    // Recursively make/unmake every legal move to a fixed depth, verifying that
    // UndoMove fully restores the position - specifically that the occupancy
    // mask m_rgMask[color][0] still matches m_rgPiece, the per-piece masks
    // match, attacks match a fresh recompute, and the hash key is restored.
    // This catches DoMove/UndoMove imbalances that the attack-consistency test
    // misses, because RecalcAttacks trusts m_rgMask[*][0].
    static bool VerifyMaskState(CPosition *p, std::ostringstream &os, const char *where) {
        bool fOk = true;
        for (int color = 0; color < 2 && fOk; color++) {
            for (unsigned int sq = 0; sq < CBitBoard::SIZE; sq++) {
                int pc = p->GetPiece(sq);
                bool fHas = (color == 0) ? (pc > 0) : (pc < 0);
                bool fBit = p->GetMask((uint16_t)color, 0).TstBit(sq);
                if (fHas != fBit) {
                    os << where << ": occupancy mismatch color=" << color
                       << " sq=" << sq << " piece=" << pc
                       << " maskBit=" << (int)fBit << "\n";
                    fOk = false;
                    break;
                }
            }
        }
        return fOk;
    }

    static bool Perft(CPosition *p, int depth, std::ostringstream &os) {
        if (depth == 0)
            return true;
        heap_t heap = allocate_heap();
        push_section(heap);
        p->LegalMoves(heap);
        std::vector<CMove> moves;
        for (unsigned i = heap->current_section->start;
             i < heap->current_section->end; ++i)
            moves.push_back(heap->data[i]);
        free_heap(heap);

        for (size_t k = 0; k < moves.size(); k++) {
            CMove mv = moves[k];
            // Full snapshot of pieces and occupancy masks before the move.
            std::vector<int8_t> pcBefore(CBitBoard::SIZE);
            for (unsigned int sq = 0; sq < CBitBoard::SIZE; sq++)
                pcBefore[sq] = p->GetPiece(sq);
            CBitBoard wMaskBefore = p->GetMask(0, 0);
            CBitBoard bMaskBefore = p->GetMask(1, 0);
            uint64_t hkBefore = p->GetHashKey();

            p->DoMove(mv);
            bool fDeeper = Perft(p, depth - 1, os);
            p->UndoMove(mv);

            if (!fDeeper)
                return false;

            bool fOk = true;
            for (unsigned int sq = 0; sq < CBitBoard::SIZE && fOk; sq++) {
                if (pcBefore[sq] != p->GetPiece(sq)) {
                    os << "UndoMove did not restore piece at sq=" << sq
                       << " expected=" << (int)pcBefore[sq]
                       << " got=" << (int)p->GetPiece(sq) << "\n";
                    fOk = false;
                }
            }
            if (fOk && !(wMaskBefore == p->GetMask(0, 0))) {
                os << "UndoMove did not restore White occupancy mask\n";
                fOk = false;
            }
            if (fOk && !(bMaskBefore == p->GetMask(1, 0))) {
                os << "UndoMove did not restore Black occupancy mask\n";
                fOk = false;
            }
            if (fOk && p->GetHashKey() != hkBefore) {
                os << "UndoMove did not restore hash key\n";
                fOk = false;
            }
            if (!fOk) {
                os << "  move L" << mv.GetFromCoord().m_nLevel << "F"
                   << mv.GetFromCoord().m_nFile << "R" << mv.GetFromCoord().m_nRank
                   << " -> L" << mv.GetToCoord().m_nLevel << "F"
                   << mv.GetToCoord().m_nFile << "R" << mv.GetToCoord().m_nRank
                   << " cap=" << (int)mv.IsCapture() << " ep=" << (int)mv.IsEnPassant()
                   << " castle=" << (int)mv.IsCastle()
                   << " promo=" << (int)mv.HasPromotion() << "\n";
                return false;
            }
        }
        return true;
    }

    TEST_METHOD(GamePositionPerftUnmakeRestores) {
        const char *moves[] = {
            "Phd2hd4", "Phd7hd5", "Phc2hc4", "Phd5xhc4", "Nhg1hf3", "Nie7id5",
            "Phb2hb3", "Phc4xhb3", "Pha2xhb3", "Nhg8ge6", "Ria1bb2", "Nib7hd6",
            "Nib1ic3", "Nid5xic3", "Pid2xic3", "Pgg7gg6", "Nhf3he5", "Nhd6ie4",
            "Rbb2xhh8", "Rig7xhh8", "Nie1jc2", "Nie4xif2", "Khe1ie1"};
        const int nMoves = sizeof(moves) / sizeof(moves[0]);

        PositionGuard pos(CPosition::Initial());
        for (int m = 0; m < nMoves; m++) {
            CMove mv = pos.get()->ParseSAN(moves[m]);
            Assert::IsTrue(mv != M_NONE, L"move did not parse");
            pos.get()->DoMove(mv);
            pos.get()->RecalcAttacks();
        }

        std::ostringstream os;
        bool fOk = Perft(pos.get(), 3, os);
        std::string s = os.str();
        std::wstring w(s.begin(), s.end());
        Logger::WriteMessage(w.c_str());
        Assert::IsTrue(fOk, w.c_str());
    }
};

} // namespace WinAmyTests
