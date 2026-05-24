#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(AttackTests) {
    static void AssertAttackFromIsReflectedInAttackTo(const CPosition *position) {
        for (unsigned int from = 0; from < CBitBoard::SIZE; from++) {
            CBitBoard attacks = position->m_rgAtkTo[from];
            while (attacks.IsNotEmpty()) {
                const uint16_t to = attacks.FindSetBit();
                Assert::IsTrue(position->m_rgAtkFr[to].TstBit(static_cast<uint16_t>(from)));
                attacks.ClrBit(to);
            }
        }
    }

  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(AtkSetPawnWhiteAttacksCorrectSquares) {
        char epd[] = "4k3/8/8/3P4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d5);
        const CBitBoard expected = CBitBoard::SetMask(MainBoardOffset(c6)) | CBitBoard::SetMask(MainBoardOffset(e6));

        Assert::IsTrue(position.get()->m_rgAtkTo[source] == expected);
    }

    TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) {
        char epd[] = "4k3/8/8/8/4p3/8/8/4K3 b - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(e4);
        const CBitBoard expected = CBitBoard::SetMask(MainBoardOffset(d3)) | CBitBoard::SetMask(MainBoardOffset(f3));

        Assert::IsTrue(position.get()->m_rgAtkTo[source] == expected);
    }

    TEST_METHOD(AtkSetKnightAttacksAllEightSquares) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d4);
        const CBitBoard expected = ComputeLeapAttacks(MainBoardCoord(d4), Knight);

        Assert::IsTrue(position.get()->m_rgAtkTo[source] == expected);
        Assert::AreEqual(8, expected.CountBits());
    }

    TEST_METHOD(AtkSetKingAttacksAllEightSquares) {
        char epd[] = "4k3/8/8/8/4K3/8/8/8 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(e4);
        const CBitBoard expected = ComputeLeapAttacks(MainBoardCoord(e4), King);

        Assert::IsTrue(position.get()->m_rgAtkTo[source] == expected);
        Assert::AreEqual(8, expected.CountBits());
    }

    TEST_METHOD(AtkSetRookAttacksStopAtBlockers) {
        char epd[] = "4k3/8/3p4/8/1p1R1p2/8/3P4/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d4);
        const CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];

        Assert::IsTrue(position.get()->m_rgAtkTo[source] ==
                       ReferenceRookAttacks(source, occupied));
    }

    TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) {
        char epd[] = "4k3/8/2p2p2/8/3B4/8/2p2p2/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d4);
        const CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];

        Assert::IsTrue(position.get()->m_rgAtkTo[source] ==
                       ReferenceBishopAttacks(source, occupied));
    }

    TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) {
        char epd[] = "4k3/8/2p1p3/8/3Q4/8/2p1p3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d4);
        const CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];
        const CBitBoard expected = ReferenceRookAttacks(source, occupied) | ReferenceBishopAttacks(source, occupied);

        Assert::IsTrue(position.get()->m_rgAtkTo[source] == expected);
    }

    TEST_METHOD(AtkFrReflectsAtkTo) {
        PositionGuard position(CPosition::Initial());
        AssertAttackFromIsReflectedInAttackTo(position.get());
    }

    TEST_METHOD(AtkClrViaDoMoveClearsOldSquareAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t from = MainBoardOffset(d4);
        Assert::IsTrue(position.get()->m_rgAtkTo[from].IsNotEmpty());

        const CMove move = MakeMainBoardMove(d4, f5, 0);
        position.get()->DoMove(move);

        Assert::IsTrue(position.get()->m_rgAtkTo[from].IsEmpty());
    }

    TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        const CMove move = MakeMainBoardMove(d4, f5, 0);
        position.get()->DoMove(move);
        position.get()->UndoMove(move);

        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksMatchesIncrementalAttacks) {
        PositionGuard position(CPosition::Initial());
        PositionGuard snapshot(CPosition::Clone(position.get()));

        position.get()->RecalcAttacks();

        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(CaptureUpdatesAttacksCorrectly) {
        char epd[] = "4k3/8/3p4/8/3R4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const CMove capture = MakeMainBoardMove(d4, d6, M_CAPTURE);

        position.get()->DoMove(capture);

        Assert::AreEqual((int)Rook, (int)position.get()->m_rgPiece[MainBoardOffset(d6)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(d4)]);
        Assert::IsTrue(position.get()->m_rgAtkTo[MainBoardOffset(d4)].IsEmpty());
        AssertAttackFromIsReflectedInAttackTo(position.get());
    }

    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        const uint16_t e2Main = MainBoardOffset(e2);
        const uint16_t e7Main = MainBoardOffset(e7);
        const uint16_t g1Main = MainBoardOffset(g1);

        CBitBoard whitePawnExpected =
            CBitBoard::SetMask(MainBoardOffset(d3)) | CBitBoard::SetMask(MainBoardOffset(f3));
        CBitBoard blackPawnExpected =
            CBitBoard::SetMask(MainBoardOffset(d6)) | CBitBoard::SetMask(MainBoardOffset(f6));
        CBitBoard knightExpected =
            CBitBoard::SetMask(MainBoardOffset(e2)) | CBitBoard::SetMask(MainBoardOffset(f3)) |
            CBitBoard::SetMask(MainBoardOffset(h3));

        Assert::IsTrue(PawnEPM[White][e2Main] == whitePawnExpected);
        Assert::IsTrue(PawnEPM[Black][e7Main] == blackPawnExpected);
        Assert::IsTrue(KnightEPM[g1Main] == knightExpected);
    }

    TEST_METHOD(RookAndBishopAttacksMatchNaiveAttacks) {
        const uint16_t d4Main = MainBoardOffset(d4);
        CBitBoard occupiedBB = CBitBoard::SetMask(d4Main) | CBitBoard::SetMask(MainBoardOffset(d6)) |
                               CBitBoard::SetMask(MainBoardOffset(f4)) | CBitBoard::SetMask(MainBoardOffset(d2)) |
                               CBitBoard::SetMask(MainBoardOffset(b4)) | CBitBoard::SetMask(MainBoardOffset(f6)) |
                               CBitBoard::SetMask(MainBoardOffset(b6)) | CBitBoard::SetMask(MainBoardOffset(f2)) |
                               CBitBoard::SetMask(MainBoardOffset(b2));

        Assert::IsTrue(ReferenceRookAttacks(d4Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(d4), Rook, occupiedBB));

        Assert::IsTrue(ReferenceBishopAttacks(d4Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(d4), Bishop, occupiedBB));
    }

    TEST_METHOD(RookAttacksOnEdgeSquares) {
        const uint16_t a1Main = MainBoardOffset(a1);
        const uint16_t h8Main = MainBoardOffset(h8);

        CBitBoard occupiedBB = CBitBoard::SetMask(a1Main);
        Assert::IsTrue(ReferenceRookAttacks(a1Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(a1), Rook, occupiedBB));

        occupiedBB = CBitBoard::SetMask(h8Main);
        Assert::IsTrue(ReferenceRookAttacks(h8Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(h8), Rook, occupiedBB));
    }

    TEST_METHOD(BishopAttacksOnCornerSquares) {
        const uint16_t a1Main = MainBoardOffset(a1);
        const uint16_t h8Main = MainBoardOffset(h8);

        CBitBoard occupiedBB = CBitBoard::SetMask(a1Main);
        Assert::IsTrue(ReferenceBishopAttacks(a1Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(a1), Bishop, occupiedBB));

        occupiedBB = CBitBoard::SetMask(h8Main);
        Assert::IsTrue(ReferenceBishopAttacks(h8Main, occupiedBB) ==
                       ComputeSlidingAttacks(MainBoardCoord(h8), Bishop, occupiedBB));
    }
};

} // namespace WinAmyTests
