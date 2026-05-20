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
        PositionGuard position(CPosition::CreateFromEPD(epd));
        CBitBoard expected = CBitBoard::SetMask(d5) | CBitBoard::SetMask(f5);
        Assert::IsTrue(position.get()->m_rgAtkTo[e4] == expected);
    }

    TEST_METHOD(AtkSetPawnBlackAttacksCorrectSquares) {
        // Black pawn on e5 should attack d4 and f4
        char epd[] = "4k3/8/8/4p3/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        CBitBoard expected = CBitBoard::SetMask(d4) | CBitBoard::SetMask(f4);
        Assert::IsTrue(position.get()->m_rgAtkTo[e5] == expected);
    }

    TEST_METHOD(AtkSetKnightAttacksAllEightSquares) {
        // Knight on d4 attacks 8 squares
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        CBitBoard expected = CBitBoard::SetMask(c2) | CBitBoard::SetMask(e2) |
                             CBitBoard::SetMask(b3) | CBitBoard::SetMask(f3) |
                             CBitBoard::SetMask(b5) | CBitBoard::SetMask(f5) |
                             CBitBoard::SetMask(c6) | CBitBoard::SetMask(e6);
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == expected);
    }

    TEST_METHOD(AtkSetKingAttacksAllEightSquares) {
        // King on e4 attacks 8 surrounding squares
        char epd[] = "4k3/8/8/8/4K3/8/8/8 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        CBitBoard expected = CBitBoard::SetMask(d3) | CBitBoard::SetMask(e3) |
                             CBitBoard::SetMask(f3) | CBitBoard::SetMask(d4) |
                             CBitBoard::SetMask(f4) | CBitBoard::SetMask(d5) |
                             CBitBoard::SetMask(e5) | CBitBoard::SetMask(f5);
        Assert::IsTrue(position.get()->m_rgAtkTo[e4] == expected);
    }

    TEST_METHOD(AtkSetRookAttacksStopAtBlockers) {
        // Rook on d4 with blockers
        char epd[] = "4k3/8/8/8/1P1R1p2/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        BitBoardBits occupied = (position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0]).GetBits();
        CBitBoard expected(ReferenceRookAttacks(d4, occupied));
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == expected);
    }

    TEST_METHOD(AtkSetBishopAttacksStopAtBlockers) {
        // Bishop on d4 with a blocker on f6
        char epd[] = "4k3/8/5p2/8/3B4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        BitBoardBits occupied = (position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0]).GetBits();
        CBitBoard expected(ReferenceBishopAttacks(d4, occupied));
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == expected);
    }

    TEST_METHOD(AtkSetQueenCombinesRookAndBishopAttacks) {
        // Queen on d4
        char epd[] = "4k3/8/8/8/3Q4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        BitBoardBits occupied = (position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0]).GetBits();
        CBitBoard expected(ReferenceRookAttacks(d4, occupied) |
                           ReferenceBishopAttacks(d4, occupied));
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == expected);
    }

    TEST_METHOD(AtkFrReflectsAtkTo) {
        // Verify atkFr is consistent with atkTo for all squares
        char epd[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        for (int sq = 0; sq < 64; sq++) {
            BitBoardBits atkTo = position.get()->m_rgAtkTo[sq].GetBits();
            while (atkTo) {
                int target = CBitBoard(atkTo).FindSetBit();
                atkTo &= atkTo - 1;
                Assert::IsTrue((position.get()->m_rgAtkFr[target] & CBitBoard::SetMask(sq)) != 0,
                    L"atkFr must reflect atkTo");
            }
        }
    }

    TEST_METHOD(AtkClrViaDoMoveClearsOldSquareAttacks) {
        // After moving a knight, its old square should have no attacks
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] != 0);

        CMove move = make_move(d4, e6, 0);
        position.get()->DoMove(move);

        // d4 is now empty, so atkTo[d4] should be 0
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == CBitBoard(0));
        // e6 should have knight attacks
        Assert::IsTrue(position.get()->m_rgAtkTo[e6].IsNotEmpty());
    }

    TEST_METHOD(AtkClrViaUndoMoveRestoresAttacks) {
        char epd[] = "4k3/8/8/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        CBitBoard originalAtkTo(position.get()->m_rgAtkTo[d4]);

        CMove move = make_move(d4, e6, 0);
        position.get()->DoMove(move);
        position.get()->UndoMove(move);

        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == originalAtkTo);
    }

    TEST_METHOD(RecalcAttacksMatchesIncrementalAttacks) {
        // Set up a complex position, compare RecalcAttacks result with
        // the incrementally maintained data
        char epd[] = "r1bqkb1r/pppppppp/2n2n2/8/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard recalced(CPosition::Clone(position.get()));

        // Clear and recalculate
        for (int i = 0; i < 64; i++) {
            recalced.get()->m_rgAtkTo[i] = 0;
            recalced.get()->m_rgAtkFr[i] = 0;
        }
        recalced.get()->RecalcAttacks();

        for (int i = 0; i < 64; i++) {
            Assert::IsTrue(position.get()->m_rgAtkTo[i] ==
                           CBitBoard(recalced.get()->m_rgAtkTo[i]));
            Assert::IsTrue(position.get()->m_rgAtkFr[i] ==
                           CBitBoard(recalced.get()->m_rgAtkFr[i]));
        }
    }

    TEST_METHOD(CaptureUpdatesAttacksCorrectly) {
        // White knight on d4 captures black pawn on e6
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(d4, e6, M_CAPTURE);
        position.get()->DoMove(move);

        // Knight now on e6, no piece on d4
        Assert::AreEqual((int)Knight, (int)position.get()->m_rgPiece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[d4]);
        Assert::IsTrue(position.get()->m_rgAtkTo[d4] == CBitBoard(0));
        Assert::IsTrue(position.get()->m_rgAtkTo[e6].IsNotEmpty());
    }

    TEST_METHOD(PawnAndKnightAttackTablesMatchExpectedSquares) {
        CBitBoard whitePawnExpected =
            CBitBoard::SetMask(d3) | CBitBoard::SetMask(f3);
        CBitBoard blackPawnExpected =
            CBitBoard::SetMask(d6) | CBitBoard::SetMask(f6);
        CBitBoard knightExpected =
            CBitBoard::SetMask(e2) | CBitBoard::SetMask(f3) | CBitBoard::SetMask(h3);

        Assert::IsTrue(CBitBoard(PawnEPM[White][e2]) == whitePawnExpected);
        Assert::IsTrue(CBitBoard(PawnEPM[Black][e7]) == blackPawnExpected);
        Assert::IsTrue(CBitBoard(KnightEPM[g1]) == knightExpected);
    }

    TEST_METHOD(RookAndBishopAttacksMatchNaiveAttacks) {
        CBitBoard occupiedBB = CBitBoard::SetMask(d4) | CBitBoard::SetMask(d6) | CBitBoard::SetMask(f4) |
                                CBitBoard::SetMask(d2) | CBitBoard::SetMask(b4) | CBitBoard::SetMask(f6) |
                                CBitBoard::SetMask(b6) | CBitBoard::SetMask(f2) | CBitBoard::SetMask(b2);
        BitBoardBits occupied = occupiedBB.GetBits();

        Assert::IsTrue(CBitBoard(ReferenceRookAttacks(d4, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(d4), Rook, occupiedBB));

        Assert::IsTrue(CBitBoard(ReferenceBishopAttacks(d4, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(d4), Bishop, occupiedBB));
    }

    TEST_METHOD(RookAttacksOnEdgeSquares) {
        // Rook on a1 with no blockers besides edges
        CBitBoard occupiedBB = CBitBoard::SetMask(a1);
        BitBoardBits occupied = occupiedBB.GetBits();
        Assert::IsTrue(CBitBoard(ReferenceRookAttacks(a1, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(a1), Rook, occupiedBB));

        // Rook on h8
        occupiedBB = CBitBoard::SetMask(h8);
        occupied = occupiedBB.GetBits();
        Assert::IsTrue(CBitBoard(ReferenceRookAttacks(h8, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(h8), Rook, occupiedBB));
    }

    TEST_METHOD(BishopAttacksOnCornerSquares) {
        CBitBoard occupiedBB = CBitBoard::SetMask(a1);
        BitBoardBits occupied = occupiedBB.GetBits();
        Assert::IsTrue(CBitBoard(ReferenceBishopAttacks(a1, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(a1), Bishop, occupiedBB));

        occupiedBB = CBitBoard::SetMask(h8);
        occupied = occupiedBB.GetBits();
        Assert::IsTrue(CBitBoard(ReferenceBishopAttacks(h8, occupied)) ==
                       ComputeSlidingAttacks(CSCoord(h8), Bishop, occupiedBB));
    }
};

} // namespace WinAmyTests
