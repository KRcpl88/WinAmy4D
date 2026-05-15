#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(AttackTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(AtkSetPawnWhiteAttacksCorrectSquares) {
        // White pawn on e4 should attack d5 and f5
        char epd[] = "4k3/8/8/8/4P3/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        CBitboard expected = CBitboard(SetMask(d5)) | CBitboard(SetMask(f5));
        Assert::IsTrue(CBitboard(position.get()->atkTo[e4]) == expected);
    }

    TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) {
        // Black pawn on e5 should attack d4 and f4
        char epd[] = "4k3/8/8/4p3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        CBitboard expected = CBitboard(SetMask(d4)) | CBitboard(SetMask(f4));
        Assert::IsTrue(CBitboard(position.get()->atkTo[e5]) == expected);
    }

    TEST_METHOD(AtkSetKnightAttacksAllEightSquares) {
        // Knight on d4 attacks 8 squares
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        CBitboard expected = CBitboard(SetMask(c2)) | CBitboard(SetMask(e2)) |
                             CBitboard(SetMask(b3)) | CBitboard(SetMask(f3)) |
                             CBitboard(SetMask(b5)) | CBitboard(SetMask(f5)) |
                             CBitboard(SetMask(c6)) | CBitboard(SetMask(e6));
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == expected);
    }

    TEST_METHOD(AtkSetKingAttacksAllEightSquares) {
        // King on e4 attacks 8 surrounding squares
        char epd[] = "4k3/8/8/8/4K3/8/8/8 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        CBitboard expected = CBitboard(SetMask(d3)) | CBitboard(SetMask(e3)) |
                             CBitboard(SetMask(f3)) | CBitboard(SetMask(d4)) |
                             CBitboard(SetMask(f4)) | CBitboard(SetMask(d5)) |
                             CBitboard(SetMask(e5)) | CBitboard(SetMask(f5));
        Assert::IsTrue(CBitboard(position.get()->atkTo[e4]) == expected);
    }

    TEST_METHOD(AtkSetRookAttacksStopAtBlockers) {
        // Rook on d4 with blockers
        char epd[] = "4k3/8/8/8/1P1R1p2/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        BitBoardBits occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        CBitboard expected(ReferenceRookAttacks(d4, occupied));
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == expected);
    }

    TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) {
        // Bishop on d4 with a blocker on f6
        char epd[] = "4k3/8/5p2/8/3B4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        BitBoardBits occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        CBitboard expected(ReferenceBishopAttacks(d4, occupied));
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == expected);
    }

    TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) {
        // Queen on d4
        char epd[] = "4k3/8/8/8/3Q4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        BitBoardBits occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        CBitboard expected(ReferenceRookAttacks(d4, occupied) |
                           ReferenceBishopAttacks(d4, occupied));
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == expected);
    }

    TEST_METHOD(AtkFrReflectsAtkTo) {
        // Verify atkFr is consistent with atkTo for all squares
        char epd[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
        PositionGuard position(CreatePositionFromEPD(epd));
        for (int sq = 0; sq < 64; sq++) {
            BitBoardBits atkTo = position.get()->atkTo[sq];
            while (atkTo) {
                int target = FindSetBit(atkTo);
                atkTo &= atkTo - 1;
                Assert::IsTrue((position.get()->atkFr[target] & SetMask(sq)) != 0,
                    L"atkFr must reflect atkTo");
            }
        }
    }

    TEST_METHOD(AtkClrViaDoMoveClearsOldSquareAttacks) {
        // After moving a knight, its old square should have no attacks
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        Assert::IsTrue(position.get()->atkTo[d4] != 0);

        move_t move = make_move(d4, e6, 0);
        DoMove(position.get(), move);

        // d4 is now empty, so atkTo[d4] should be 0
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == CBitboard(0));
        // e6 should have knight attacks
        Assert::IsTrue(CBitboard(position.get()->atkTo[e6]).IsNotEmpty());
    }

    TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        CBitboard originalAtkTo(position.get()->atkTo[d4]);

        move_t move = make_move(d4, e6, 0);
        DoMove(position.get(), move);
        UndoMove(position.get(), move);

        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == originalAtkTo);
    }

    TEST_METHOD(RecalcAttacksMatchesIncrementalAttacks) {
        // Set up a complex position, compare RecalcAttacks result with
        // the incrementally maintained data
        char epd[] = "r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard recalced(ClonePosition(position.get()));

        // Clear and recalculate
        for (int i = 0; i < 64; i++) {
            recalced.get()->atkTo[i] = 0;
            recalced.get()->atkFr[i] = 0;
        }
        RecalcAttacks(recalced.get());

        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(CBitboard(position.get()->atkTo[i]) ==
                           CBitboard(recalced.get()->atkTo[i]));
            Assert::IsTrue(CBitboard(position.get()->atkFr[i]) ==
                           CBitboard(recalced.get()->atkFr[i]));
        }
    }

    TEST_METHOD(CaptureUpdatesAttacksCorrectly) {
        // White knight on d4 captures black pawn on e6
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));

        move_t move = make_move(d4, e6, M_CAPTURE);
        DoMove(position.get(), move);

        // Knight now on e6, no piece on d4
        Assert::AreEqual((int)Knight, (int)position.get()->piece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d4]);
        Assert::IsTrue(CBitboard(position.get()->atkTo[d4]) == CBitboard(0));
        Assert::IsTrue(CBitboard(position.get()->atkTo[e6]).IsNotEmpty());
    }

    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        CBitboard whitePawnExpected =
            CBitboard(SetMask(d3)) | CBitboard(SetMask(f3));
        CBitboard blackPawnExpected =
            CBitboard(SetMask(d6)) | CBitboard(SetMask(f6));
        CBitboard knightExpected =
            CBitboard(SetMask(e2)) | CBitboard(SetMask(f3)) | CBitboard(SetMask(h3));

        Assert::IsTrue(CBitboard(PawnEPM[White][e2]) == whitePawnExpected);
        Assert::IsTrue(CBitboard(PawnEPM[Black][e7]) == blackPawnExpected);
        Assert::IsTrue(CBitboard(KnightEPM[g1]) == knightExpected);
    }

    TEST_METHOD(RookAndBishopMagicAttacksMatchNaiveAttacks) {
        BitBoardBits occupied = SetMask(d4) | SetMask(d6) | SetMask(f4) |
                                SetMask(d2) | SetMask(b4) | SetMask(f6) |
                                SetMask(b6) | SetMask(f2) | SetMask(b2);

        Assert::IsTrue(CBitboard(ReferenceRookAttacks(d4, occupied)) ==
                       CBitboard(rook_attacks(d4, occupied)));

        Assert::IsTrue(CBitboard(ReferenceBishopAttacks(d4, occupied)) ==
                       CBitboard(bishop_attacks(d4, occupied)));
    }

    TEST_METHOD(RookMagicAttacksOnEdgeSquares) {
        // Rook on a1 with no blockers besides edges
        BitBoardBits occupied = SetMask(a1);
        Assert::IsTrue(CBitboard(ReferenceRookAttacks(a1, occupied)) ==
                       CBitboard(rook_attacks(a1, occupied)));

        // Rook on h8
        occupied = SetMask(h8);
        Assert::IsTrue(CBitboard(ReferenceRookAttacks(h8, occupied)) ==
                       CBitboard(rook_attacks(h8, occupied)));
    }

    TEST_METHOD(BishopMagicAttacksOnCornerSquares) {
        BitBoardBits occupied = SetMask(a1);
        Assert::IsTrue(CBitboard(ReferenceBishopAttacks(a1, occupied)) ==
                       CBitboard(bishop_attacks(a1, occupied)));

        occupied = SetMask(h8);
        Assert::IsTrue(CBitboard(ReferenceBishopAttacks(h8, occupied)) ==
                       CBitboard(bishop_attacks(h8, occupied)));
    }
};

} // namespace WinAmyTests
