#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(AttackTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // TEST_METHOD(AtkSetPawnWhiteAttacksCorrectSquares) - disabled: uses EPD (8x8 board not yet supported)
    // TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) - disabled: uses EPD
    // TEST_METHOD(AtkSetKnightAttacksAllEightSquares) - disabled: uses EPD
    // TEST_METHOD(AtkSetKingAttacksAllEightSquares) - disabled: uses EPD

    // TEST_METHOD(AtkSetRookAttacksStopAtBlockers) - disabled: uses EPD (8x8 board not yet supported)
    // TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) - disabled: uses EPD
    // TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) - disabled: uses EPD
    // TEST_METHOD(AtkFrReflectsAtkTo) - disabled: uses EPD + 64-square loop

    // TEST_METHOD(AtkClrViaDoMoveClearsOldSquareAttacks) - disabled: uses EPD

    // TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) - disabled: uses EPD

    // TEST_METHOD(RecalcAttacksMatchesIncrementalAttacks) - disabled: uses EPD

    // TEST_METHOD(CaptureUpdatesAttacksCorrectly) - disabled: uses EPD

    // TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) - disabled: expects 2D chess EPM layout
    /* Temporarily re-enabled for regression */
    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        CBitBoard whitePawnExpected =
            CBitBoard::SetMask(d3) | CBitBoard::SetMask(f3);
        CBitBoard blackPawnExpected =
            CBitBoard::SetMask(d6) | CBitBoard::SetMask(f6);
        CBitBoard knightExpected =
            CBitBoard::SetMask(e2) | CBitBoard::SetMask(f3) | CBitBoard::SetMask(h3);

        Assert::IsTrue(PawnEPM[White][e2] == whitePawnExpected);
        Assert::IsTrue(PawnEPM[Black][e7] == blackPawnExpected);
        Assert::IsTrue(KnightEPM[g1] == knightExpected);
    }

    TEST_METHOD(RookAndBishopAttacksMatchNaiveAttacks) {
        CBitBoard occupiedBB = CBitBoard::SetMask(d4) | CBitBoard::SetMask(d6) | CBitBoard::SetMask(f4) |
                                CBitBoard::SetMask(d2) | CBitBoard::SetMask(b4) | CBitBoard::SetMask(f6) |
                                CBitBoard::SetMask(b6) | CBitBoard::SetMask(f2) | CBitBoard::SetMask(b2);

        Assert::IsTrue(ReferenceRookAttacks(d4, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(d4), Rook, occupiedBB));

        Assert::IsTrue(ReferenceBishopAttacks(d4, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(d4), Bishop, occupiedBB));
    }

    TEST_METHOD(RookAttacksOnEdgeSquares) {
        CBitBoard occupiedBB = CBitBoard::SetMask(a1);
        Assert::IsTrue(ReferenceRookAttacks(a1, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(a1), Rook, occupiedBB));

        occupiedBB = CBitBoard::SetMask(h8);
        Assert::IsTrue(ReferenceRookAttacks(h8, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(h8), Rook, occupiedBB));
    }

    TEST_METHOD(BishopAttacksOnCornerSquares) {
        CBitBoard occupiedBB = CBitBoard::SetMask(a1);
        Assert::IsTrue(ReferenceBishopAttacks(a1, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(a1), Bishop, occupiedBB));

        occupiedBB = CBitBoard::SetMask(h8);
        Assert::IsTrue(ReferenceBishopAttacks(h8, occupiedBB) ==
                       ComputeSlidingAttacks(CSCoord(h8), Bishop, occupiedBB));
    }
};

} // namespace WinAmyTests
