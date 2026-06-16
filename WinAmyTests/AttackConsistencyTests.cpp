#include "TestHelpers.h"
#include "heap.h"
#include <vector>
#include <string>
#include <sstream>

namespace WinAmyTests {

// Regression tests guarding these engine invariants:
//   1. The incremental gain/lose-attack updates performed inside DoMove keep
//      the attack tables identical to a full RecalcAttacks() recompute.
//   2. The incremental Zobrist hash key maintained by DoMove (including the
//      en-passant component) stays identical to a full RecalcAttacks() recompute.
//   3. Engine self-play never removes a king from the board.
TEST_CLASS(AttackConsistencyTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
        AllocateHT();
    }

    static uint32_t s_rng;
    static uint32_t Rand() {
        s_rng = s_rng * 1664525u + 1013904223u;
        return s_rng;
    }

    static bool AttacksMatch(const CPosition *a, const CPosition *b, std::string &msg) {
        for (unsigned int i = 0; i < CBitBoard::SIZE; i++) {
            if (!(a->GetAtkTo(i) == b->GetAtkTo(i))) {
                std::ostringstream os; os << "AtkTo mismatch at sq " << i;
                msg = os.str(); return false;
            }
            if (!(a->GetAtkFr(i) == b->GetAtkFr(i))) {
                std::ostringstream os; os << "AtkFr mismatch at sq " << i;
                msg = os.str(); return false;
            }
        }
        if (!(a->GetSlidingPieces() == b->GetSlidingPieces())) { msg = "SlidingPieces mismatch"; return false; }
        return true;
    }

    // Play many random legal games; after every move, compare the incrementally
    // maintained attack tables against an independent full recompute.
    TEST_METHOD(IncrementalAttacksMatchRecomputeOverRandomGames) {
        for (int game = 0; game < 40; game++) {
            s_rng = 0x12345 + game * 2654435761u;
            PositionGuard pos(CPosition::Initial());
            for (int ply = 0; ply < 80; ply++) {
                heap_t heap = allocate_heap();
                push_section(heap);
                pos.get()->LegalMoves(heap);
                std::vector<CMove> moves;
                for (unsigned i = heap->current_section->start; i < heap->current_section->end; ++i)
                    moves.push_back(heap->data[i]);
                free_heap(heap);
                if (moves.empty()) break;
                CMove mv = moves[Rand() % moves.size()];
                pos.get()->DoMove(mv);

                PositionGuard clone(CPosition::Clone(pos.get()));
                clone.get()->RecalcAttacks();
                std::string msg;
                if (pos.get()->GetHashKey() != clone.get()->GetHashKey()) {
                    std::ostringstream os;
                    os << "game " << game << " ply " << ply << ": HKey mismatch incr="
                       << pos.get()->GetHashKey() << " recalc=" << clone.get()->GetHashKey()
                       << " from=" << (int)mv.GetFromCoord().BitOffset()
                       << " to=" << (int)mv.GetToCoord().BitOffset()
                       << " cap=" << (int)mv.IsCapture()
                       << " ep=" << (int)mv.IsEnPassant()
                       << " castle=" << (int)mv.IsCastle()
                       << " promo=" << (int)mv.HasPromotion();
                    std::string s = os.str();
                    std::wstring w(s.begin(), s.end());
                    Assert::Fail(w.c_str());
                }
                if (!AttacksMatch(pos.get(), clone.get(), msg)) {
                    std::ostringstream os;
                    os << "game " << game << " ply " << ply << ": " << msg
                       << " from=" << (int)mv.GetFromCoord().BitOffset()
                       << " to=" << (int)mv.GetToCoord().BitOffset()
                       << " cap=" << (int)mv.IsCapture()
                       << " ep=" << (int)mv.IsEnPassant()
                       << " castle=" << (int)mv.IsCastle()
                       << " promo=" << (int)mv.HasPromotion();
                    std::wstring w(os.str().begin(), os.str().end());
                    Assert::Fail(w.c_str());
                }
            }
        }
    }

    // The engine must never produce a move that captures (or otherwise removes)
    // a king; both kings must remain on the board throughout a self-play game.
    //
    // This runs a full 160-ply self-play game at search depth 3, which is slow,
    // so it is tagged TestCategory "LongRunning" (for grouping in Visual Studio
    // Test Explorer) and excluded from the base regression pass. The native C++
    // adapter does not filter on TestCategory from the command line, so run it
    // by name for deeper scenarios:
    //   vstest.console.exe ... /Tests:EngineSelfPlayKeepsBothKings
    BEGIN_TEST_METHOD_ATTRIBUTE(EngineSelfPlayKeepsBothKings)
        TEST_METHOD_ATTRIBUTE(L"TestCategory", L"LongRunning")
    END_TEST_METHOD_ATTRIBUTE()
    TEST_METHOD(EngineSelfPlayKeepsBothKings) {
        SetMaxSearchDepth(3);
        PositionGuard pos(CPosition::Initial());
        for (int ply = 0; ply < 160; ply++) {
            const char *end = pos.get()->GameEnd();
            if (end != nullptr) break;
            int score = 0, alt = 0;
            CMove mv = pos.get()->Iterate(&score, M_NONE, &alt);
            if (mv == M_NONE) break;
            pos.get()->DoMove(mv);

            int wk = 0, bk = 0;
            for (unsigned int i = 0; i < CBitBoard::SIZE; i++) {
                if (pos.get()->GetPiece(i) == King) wk++;
                if (pos.get()->GetPiece(i) == -King) bk++;
            }
            if (wk != 1 || bk != 1) {
                std::ostringstream os;
                os << "ply " << ply << ": king count wk=" << wk << " bk=" << bk;
                std::wstring w(os.str().begin(), os.str().end());
                Assert::Fail(w.c_str());
            }
        }
    }
};

uint32_t AttackConsistencyTests::s_rng = 1;

} // namespace WinAmyTests
