#include "TestHelpers.h"
#include "heap.h"
#include "searchdata.h"

#include <memory>
#include <sstream>
#include <vector>

namespace WinAmyTests {

TEST_CLASS(SearchDataTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    static uint32_t s_rng;
    static uint32_t Rand() {
        s_rng = s_rng * 1664525u + 1013904223u;
        return s_rng;
    }

    // For a pawn move, promotion must be decided by the destination square:
    // a pawn landing on a promotion square must promote, and a pawn landing on
    // any other square must not.  This relationship must hold for every move
    // produced by the engine, regardless of which generator created it.
    static bool PawnPromotionInvariantHolds(CPosition *p, CMove move) {
        const uint16_t from = move.GetFromCoord().BitOffset();
        if (TYPE(p->GetPiece(from)) != Pawn)
            return true;
        const bool promoSquare = is_promo_square(move.GetToCoord());
        return promoSquare == move.HasPromotion();
    }

    TEST_METHOD(ConstructorInitializesSearchState) {
        PositionGuard position(CPosition::Initial());

        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        Assert::IsTrue(searchData->m_pPosition == position.get());
        Assert::IsTrue(searchData->m_pStatusTable != nullptr);
        Assert::IsTrue(searchData->m_pCurrent == searchData->m_pStatusTable);
        Assert::IsTrue(searchData->m_pKillerTable != nullptr);
        Assert::IsTrue(searchData->m_pKiller == searchData->m_pKillerTable);
        Assert::IsTrue(searchData->m_hHeap != nullptr);
        Assert::AreEqual(0, (int)searchData->m_wPly);
    }

    TEST_METHOD(EnterNodeAndLeaveNodeUpdatePlyAndPointers) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        SSearchStatus *initialStatus = searchData->m_pCurrent;
        SKillerEntry *initialKiller = searchData->m_pKiller;

        searchData->EnterNode();
        Assert::AreEqual(1, (int)searchData->m_wPly);
        Assert::IsTrue(searchData->m_pCurrent == initialStatus + 1);
        Assert::IsTrue(searchData->m_pKiller == initialKiller + 1);

        searchData->LeaveNode();
        Assert::AreEqual(0, (int)searchData->m_wPly);
        Assert::IsTrue(searchData->m_pCurrent == initialStatus);
        Assert::IsTrue(searchData->m_pKiller == initialKiller);
    }

    TEST_METHOD(NextMoveReturnsLegalMove) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));
        CMove expected = MakeMainBoardMove(he2, he4, M_PAWND);

        searchData->EnterNode();
        searchData->m_pCurrent->st_hashmove = expected;
        CMove move = searchData->NextMove();
        searchData->LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextEvasionReturnsLegalMoveWhenInCheck) {
        char epd[] = "4r3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));
        CMove expected = MakeMainBoardMove(he1, hd1, 0);

        searchData->EnterNode();
        searchData->m_pCurrent->st_hashmove = expected;
        CMove move = searchData->NextEvasion();
        searchData->LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextMoveQReturnsCaptureWhenCaptureExists) {
        char epd[] = "4k3/8/8/8/4p3/8/4Q3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        searchData->EnterNode();
        CMove move = searchData->NextMoveQ(-500000);
        searchData->LeaveNode();

        Assert::IsTrue(move != M_NONE);
        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    // Regression for the 3D pawn-promotion bug: the quiescence capture
    // generator used to decide promotion from the pawn's source rank
    // (PrePromoRank), an 8x8-board assumption.  In 3D a pawn can capture across
    // levels onto a promotion square from a square that is not on the
    // pre-promotion rank (e.g. capturing onto bit offset 97 = level g/file
    // g/rank 0), and a pre-promotion pawn can capture across levels onto a
    // non-promotion square.  The generator must therefore decide promotion from
    // the destination square.  Play random legal games and, at every position,
    // verify every quiescence move is pseudo-legal and respects the
    // pawn-promotion invariant.
    TEST_METHOD(QuiescenceMovesRespectPromotionInvariant) {
        for (int game = 0; game < 25; game++) {
            s_rng = 0xC0FFEE + game * 2654435761u;
            PositionGuard pos(CPosition::Initial());
            for (int ply = 0; ply < 80; ply++) {
                // Collect the strictly-legal moves to pick a random one.
                heap_t legal = allocate_heap();
                push_section(legal);
                pos.get()->LegalMoves(legal);
                std::vector<CMove> moves;
                for (unsigned i = legal->current_section->start;
                     i < legal->current_section->end; ++i)
                    moves.push_back(legal->data[i]);
                free_heap(legal);
                if (moves.empty())
                    break;

                // Every legal move must respect the promotion invariant.
                for (CMove m : moves) {
                    if (!PawnPromotionInvariantHolds(pos.get(), m)) {
                        std::wstringstream w;
                        w << L"legal move violates promotion invariant: game "
                          << game << L" ply " << ply << L" from "
                          << m.GetFromCoord().BitOffset() << L" to "
                          << m.GetToCoord().BitOffset() << L" promo "
                          << (int)m.HasPromotion();
                        Assert::Fail(w.str().c_str());
                    }
                }

                // Every quiescence move must be pseudo-legal and respect the
                // promotion invariant.
                std::unique_ptr<CSearchData> sd(new CSearchData(pos.get()));
                sd->EnterNode();
                CMove qm;
                int guard = 0;
                while ((qm = sd->NextMoveQ(-1000000)) != M_NONE && guard++ < 4096) {
                    if (!pos.get()->LegalMove(qm)) {
                        std::wstringstream w;
                        w << L"quiescence move is not legal: game " << game
                          << L" ply " << ply << L" from "
                          << qm.GetFromCoord().BitOffset() << L" to "
                          << qm.GetToCoord().BitOffset() << L" promo "
                          << (int)qm.HasPromotion() << L" cap "
                          << (int)qm.IsCapture();
                        sd->LeaveNode();
                        Assert::Fail(w.str().c_str());
                    }
                    if (!PawnPromotionInvariantHolds(pos.get(), qm)) {
                        std::wstringstream w;
                        w << L"quiescence move violates promotion invariant: game "
                          << game << L" ply " << ply << L" from "
                          << qm.GetFromCoord().BitOffset() << L" to "
                          << qm.GetToCoord().BitOffset() << L" promo "
                          << (int)qm.HasPromotion();
                        sd->LeaveNode();
                        Assert::Fail(w.str().c_str());
                    }
                }
                sd->LeaveNode();

                pos.get()->DoMove(moves[Rand() % moves.size()]);
            }
        }
    }

    // LegalMove must reject a promotion whose destination is not a promotion
    // square (such a move can arrive from a stale hash/killer/countermove
    // entry).  Here a white pawn on e2 "promotes" to e4 — e4 is not a promotion
    // square, so the move must be rejected.
    TEST_METHOD(LegalMoveRejectsPromotionToNonPromotionSquare) {
        PositionGuard position(CPosition::Initial());
        CMove bogus = MakeMainBoardPromotion(he2, he4, Queen, 0);
        Assert::IsFalse(position.get()->LegalMove(bogus));
    }

    TEST_METHOD(PutKillerTracksAndPromotesByHitCount) {
        PositionGuard position(CPosition::Initial());
        std::unique_ptr<CSearchData> searchData(new CSearchData(position.get()));

        CMove moveOne = MakeMainBoardMove(he2, he4, M_PAWND);
        CMove moveTwo = MakeMainBoardMove(hd2, hd4, M_PAWND);

        searchData->PutKiller(moveOne);
        Assert::IsTrue(searchData->m_pKiller->killer1 == moveOne);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount1);

        searchData->PutKiller(moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer2 == moveTwo);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount2);

        searchData->PutKiller(moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer1 == moveTwo);
        Assert::IsTrue(searchData->m_pKiller->killer2 == moveOne);
        Assert::AreEqual((uint32_t)2, searchData->m_pKiller->kcount1);
        Assert::AreEqual((uint32_t)1, searchData->m_pKiller->kcount2);
    }
};

uint32_t SearchDataTests::s_rng = 1;

} // namespace WinAmyTests
