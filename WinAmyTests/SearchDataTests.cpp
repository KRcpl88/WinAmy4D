#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(SearchDataTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(ConstructorInitializesSearchState) {
        PositionGuard position(CPosition::Initial());

        CSearchData searchData(position.get());

        Assert::IsTrue(searchData.m_pPosition == position.get());
        Assert::IsTrue(searchData.m_pStatusTable != nullptr);
        Assert::IsTrue(searchData.m_pCurrent == searchData.m_pStatusTable);
        Assert::IsTrue(searchData.m_pKillerTable != nullptr);
        Assert::IsTrue(searchData.m_pKiller == searchData.m_pKillerTable);
        Assert::IsTrue(searchData.m_hHeap != nullptr);
        Assert::AreEqual(0, (int)searchData.m_wPly);
    }

    TEST_METHOD(EnterNodeAndLeaveNodeUpdatePlyAndPointers) {
        PositionGuard position(CPosition::Initial());
        CSearchData searchData(position.get());

        SSearchStatus *initialStatus = searchData.m_pCurrent;
        SKillerEntry *initialKiller = searchData.m_pKiller;

        searchData.EnterNode();
        Assert::AreEqual(1, (int)searchData.m_wPly);
        Assert::IsTrue(searchData.m_pCurrent == initialStatus + 1);
        Assert::IsTrue(searchData.m_pKiller == initialKiller + 1);

        searchData.LeaveNode();
        Assert::AreEqual(0, (int)searchData.m_wPly);
        Assert::IsTrue(searchData.m_pCurrent == initialStatus);
        Assert::IsTrue(searchData.m_pKiller == initialKiller);
    }

    TEST_METHOD(NextMoveReturnsLegalMove) {
        PositionGuard position(CPosition::Initial());
        CSearchData searchData(position.get());
        CMove expected = MakeMainBoardMove(e2, e4, M_PAWND);

        searchData.EnterNode();
        searchData.m_pCurrent->st_hashmove = expected;
        CMove move = searchData.NextMove();
        searchData.LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextEvasionReturnsLegalMoveWhenInCheck) {
        char epd[] = "4r3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        CSearchData searchData(position.get());
        CMove expected = MakeMainBoardMove(e1, d1, 0);

        searchData.EnterNode();
        searchData.m_pCurrent->st_hashmove = expected;
        CMove move = searchData.NextEvasion();
        searchData.LeaveNode();

        Assert::IsTrue(move == expected);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(NextMoveQReturnsCaptureWhenCaptureExists) {
        char epd[] = "4k3/8/8/8/4p3/8/4Q3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        CSearchData searchData(position.get());

        searchData.EnterNode();
        CMove move = searchData.NextMoveQ(-500000);
        searchData.LeaveNode();

        Assert::IsTrue(move != M_NONE);
        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(PutKillerTracksAndPromotesByHitCount) {
        PositionGuard position(CPosition::Initial());
        CSearchData searchData(position.get());

        CMove moveOne = MakeMainBoardMove(e2, e4, M_PAWND);
        CMove moveTwo = MakeMainBoardMove(d2, d4, M_PAWND);

        searchData.PutKiller(moveOne);
        Assert::IsTrue(searchData.m_pKiller->killer1 == moveOne);
        Assert::AreEqual((uint32_t)1, searchData.m_pKiller->kcount1);

        searchData.PutKiller(moveTwo);
        Assert::IsTrue(searchData.m_pKiller->killer2 == moveTwo);
        Assert::AreEqual((uint32_t)1, searchData.m_pKiller->kcount2);

        searchData.PutKiller(moveTwo);
        Assert::IsTrue(searchData.m_pKiller->killer1 == moveTwo);
        Assert::IsTrue(searchData.m_pKiller->killer2 == moveOne);
        Assert::AreEqual((uint32_t)2, searchData.m_pKiller->kcount1);
        Assert::AreEqual((uint32_t)1, searchData.m_pKiller->kcount2);
    }
};

} // namespace WinAmyTests
