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
        uint64_t expected = SetMask(d5) | SetMask(f5);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e4]);
    }

    TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) {
        // Black pawn on e5 should attack d4 and f4
        char epd[] = "4k3/8/8/4p3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(d4) | SetMask(f4);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e5]);
    }

    TEST_METHOD(AtkSetKnightAttacksAllEightSquares) {
        // Knight on d4 attacks 8 squares
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(c2) | SetMask(e2) | SetMask(b3) |
                            SetMask(f3) | SetMask(b5) | SetMask(f5) |
                            SetMask(c6) | SetMask(e6);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetKingAttacksAllEightSquares) {
        // King on e4 attacks 8 surrounding squares
        char epd[] = "4k3/8/8/8/4K3/8/8/8 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t expected = SetMask(d3) | SetMask(e3) | SetMask(f3) |
                            SetMask(d4) | SetMask(f4) | SetMask(d5) |
                            SetMask(e5) | SetMask(f5);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[e4]);
    }

    TEST_METHOD(AtkSetRookAttacksStopAtBlockers) {
        // Rook on d4 with blockers
        char epd[] = "4k3/8/8/8/1P1R1p2/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expected = ReferenceRookAttacks(d4, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) {
        // Bishop on d4 with a blocker on f6
        char epd[] = "4k3/8/5p2/8/3B4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expected = ReferenceBishopAttacks(d4, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) {
        // Queen on d4
        char epd[] = "4k3/8/8/8/3Q4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        uint64_t expectedRook = ReferenceRookAttacks(d4, occupied);
        uint64_t expectedBishop = ReferenceBishopAttacks(d4, occupied);
        uint64_t expected = expectedRook | expectedBishop;
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)position.get()->atkTo[d4]);
    }

    TEST_METHOD(AtkFrReflectsAtkTo) {
        // Verify atkFr is consistent with atkTo for all squares
        char epd[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
        PositionGuard position(CreatePositionFromEPD(epd));
        for (int sq = 0; sq < 64; sq++) {
            BitBoard atkTo = position.get()->atkTo[sq];
            while (atkTo) {
                int target = FindSetBit(atkTo);
                atkTo &= atkTo - 1;
                Assert::IsTrue(TstBit(position.get()->atkFr[target], sq) != 0,
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
        Assert::AreEqual(0ULL, (uint64_t)position.get()->atkTo[d4]);
        // e6 should have knight attacks
        Assert::IsTrue(position.get()->atkTo[e6] != 0);
    }

    TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        uint64_t originalAtkTo = position.get()->atkTo[d4];

        move_t move = make_move(d4, e6, 0);
        DoMove(position.get(), move);
        UndoMove(position.get(), move);

        Assert::AreEqual((unsigned long long)originalAtkTo,
                         (unsigned long long)position.get()->atkTo[d4]);
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
            Assert::AreEqual((unsigned long long)position.get()->atkTo[i],
                             (unsigned long long)recalced.get()->atkTo[i]);
            Assert::AreEqual((unsigned long long)position.get()->atkFr[i],
                             (unsigned long long)recalced.get()->atkFr[i]);
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
        Assert::AreEqual(0ULL, (uint64_t)position.get()->atkTo[d4]);
        Assert::IsTrue(position.get()->atkTo[e6] != 0);
    }

    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        const uint64_t whitePawnExpected = SetMask(d3) | SetMask(f3);
        const uint64_t blackPawnExpected = SetMask(d6) | SetMask(f6);
        const uint64_t knightExpected = SetMask(e2) | SetMask(f3) | SetMask(h3);

        Assert::AreEqual((unsigned long long)whitePawnExpected,
                         (unsigned long long)PawnEPM[White][e2]);
        Assert::AreEqual((unsigned long long)blackPawnExpected,
                         (unsigned long long)PawnEPM[Black][e7]);
        Assert::AreEqual((unsigned long long)knightExpected,
                         (unsigned long long)KnightEPM[g1]);
    }

    TEST_METHOD(RookAndBishopMagicAttacksMatchNaiveAttacks) {
        const uint64_t occupied = SetMask(d4) | SetMask(d6) | SetMask(f4) |
                                  SetMask(d2) | SetMask(b4) | SetMask(f6) |
                                  SetMask(b6) | SetMask(f2) | SetMask(b2);

        Assert::AreEqual((unsigned long long)ReferenceRookAttacks(d4, occupied),
                         (unsigned long long)rook_attacks(d4, occupied));

        Assert::AreEqual((unsigned long long)ReferenceBishopAttacks(d4, occupied),
                         (unsigned long long)bishop_attacks(d4, occupied));
    }

    TEST_METHOD(RookMagicAttacksOnEdgeSquares) {
        // Rook on a1 with no blockers besides edges
        uint64_t occupied = SetMask(a1);
        uint64_t expected = ReferenceRookAttacks(a1, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)rook_attacks(a1, occupied));

        // Rook on h8
        occupied = SetMask(h8);
        expected = ReferenceRookAttacks(h8, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)rook_attacks(h8, occupied));
    }

    TEST_METHOD(BishopMagicAttacksOnCornerSquares) {
        uint64_t occupied = SetMask(a1);
        uint64_t expected = ReferenceBishopAttacks(a1, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)bishop_attacks(a1, occupied));

        occupied = SetMask(h8);
        expected = ReferenceBishopAttacks(h8, occupied);
        Assert::AreEqual((unsigned long long)expected,
                         (unsigned long long)bishop_attacks(h8, occupied));
    }
};

} // namespace WinAmyTests
