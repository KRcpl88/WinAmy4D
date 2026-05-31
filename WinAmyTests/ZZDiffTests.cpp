#include "TestHelpers.h"
#include "heap.h"
#include <vector>
#include <string>
#include <sstream>

namespace WinAmyTests {

TEST_CLASS(ZZDiffTests) {
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
            if (!(a->m_rgAtkTo[i] == b->m_rgAtkTo[i])) {
                std::ostringstream os; os << "AtkTo mismatch at sq " << i;
                msg = os.str(); return false;
            }
            if (!(a->m_rgAtkFr[i] == b->m_rgAtkFr[i])) {
                std::ostringstream os; os << "AtkFr mismatch at sq " << i;
                msg = os.str(); return false;
            }
        }
        if (!(a->m_SlidingPieces == b->m_SlidingPieces)) { msg = "SlidingPieces mismatch"; return false; }
        return true;
    }

    TEST_METHOD(IncrementalAttacksMatchRecomputeOverRandomGames) {
        for (int game = 0; game < 200; game++) {
            s_rng = 0x12345 + game * 2654435761u;
            PositionGuard pos(CPosition::Initial());
            std::vector<std::string> hist;
            for (int ply = 0; ply < 120; ply++) {
                heap_t heap = allocate_heap();
                push_section(heap);
                // collect legal moves
                pos.get()->LegalMoves(heap);
                std::vector<CMove> moves;
                for (unsigned i = heap->current_section->start; i < heap->current_section->end; ++i)
                    moves.push_back(heap->data[i]);
                free_heap(heap);
                if (moves.empty()) break;
                CMove mv = moves[Rand() % moves.size()];
                pos.get()->DoMove(mv);

                // compare incremental vs recompute
                PositionGuard clone(CPosition::Clone(pos.get()));
                clone.get()->RecalcAttacks();
                std::string msg;
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

    TEST_METHOD(EngineSelfPlayKeepsBothKings) {
        setMaxSearchDepth(3);
        PositionGuard pos(CPosition::Initial());
        for (int ply = 0; ply < 200; ply++) {
            const char *end = pos.get()->GameEnd();
            if (end != nullptr) break;
            int score = 0, alt = 0;
            CMove mv = pos.get()->Iterate(&score, M_NONE, &alt);
            if (mv == M_NONE) break;
            pos.get()->DoMove(mv);

            int wk = 0, bk = 0;
            for (unsigned int i = 0; i < CBitBoard::SIZE; i++) {
                if (pos.get()->m_rgPiece[i] == King) wk++;
                if (pos.get()->m_rgPiece[i] == -King) bk++;
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

uint32_t ZZDiffTests::s_rng = 1;

} // namespace WinAmyTests