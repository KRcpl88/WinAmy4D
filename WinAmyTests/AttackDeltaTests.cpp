#include "TestHelpers.h"

#include "scoord.h"
#include "ucoord.h"

namespace WinAmyTests {

TEST_CLASS(AttackDeltaTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // Helper: trace rays for a sliding piece from a starting square.
    // For each direction in ATTACK_DELTA[pieceType], step repeatedly until
    // CSCoord::Step returns an invalid coord.
    static CBitBoard TraceSlidingRays(const CSCoord &origin, int pieceType) {
        CBitBoard result;
        for (int d = 0; d < ATTACK_DELTA_COUNT[pieceType]; d++) {
            CUCoord dir = ATTACK_DELTA[pieceType][d];
            CSCoord sq = origin.Step(dir);
            while (sq.IsValid()) {
                result.SetBit(sq.BitOffset());
                sq = sq.Step(dir);
            }
        }
        return result;
    }

    // Helper: trace single-step moves for a leaping piece from a starting square.
    static CBitBoard TraceLeaps(const CSCoord &origin, int pieceType) {
        CBitBoard result;
        for (int d = 0; d < ATTACK_DELTA_COUNT[pieceType]; d++) {
            CUCoord dir = ATTACK_DELTA[pieceType][d];
            CSCoord sq = origin.Step(dir);
            if (sq.IsValid()) {
                result.SetBit(sq.BitOffset());
            }
        }
        return result;
    }

    // ---------------------------------------------------------------
    // White Pawn tests
    // ---------------------------------------------------------------

    TEST_METHOD(WhitePawnAttacksFromD4) {
        // White pawn on d4 (file=3, rank=3) attacks c5 and e5
        CSCoord d4(0, 3, 3);
        CBitBoard attacks = TraceLeaps(d4, Pawn);

        // On 2D board, only 2 of 4 directions produce valid same-level squares
        Assert::AreEqual(2, attacks.CountBits());

        // c5 = file 2, rank 4
        CSCoord c5(0, 2, 4);
        Assert::IsTrue(attacks.TstBit(c5.BitOffset()));

        // e5 = file 4, rank 4
        CSCoord e5(0, 4, 4);
        Assert::IsTrue(attacks.TstBit(e5.BitOffset()));
    }

    TEST_METHOD(WhitePawnAttacksFromA1Corner) {
        // White pawn on a1 (file=0, rank=0) attacks b2 only
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceLeaps(a1, Pawn);

        // Only one diagonal is on the board
        Assert::AreEqual(1, attacks.CountBits());

        CSCoord b2(0, 1, 1);
        Assert::IsTrue(attacks.TstBit(b2.BitOffset()));
    }

    TEST_METHOD(WhitePawnAttacksFromH8TopEdge) {
        // White pawn on h8 (file=7, rank=7) — no forward attacks possible
        CSCoord h8(0, 7, 7);
        CBitBoard attacks = TraceLeaps(h8, Pawn);
        Assert::AreEqual(0, attacks.CountBits());
    }

    // ---------------------------------------------------------------
    // Black Pawn tests
    // ---------------------------------------------------------------

    TEST_METHOD(BlackPawnAttacksFromE5) {
        // Black pawn on e5 (file=4, rank=4) attacks d4 and f4
        CSCoord e5(0, 4, 4);
        CBitBoard attacks = TraceLeaps(e5, BPawn);

        Assert::AreEqual(2, attacks.CountBits());

        CSCoord d4(0, 3, 3);
        Assert::IsTrue(attacks.TstBit(d4.BitOffset()));

        CSCoord f4(0, 5, 3);
        Assert::IsTrue(attacks.TstBit(f4.BitOffset()));
    }

    TEST_METHOD(BlackPawnAttacksFromA1Corner) {
        // Black pawn on a1 (file=0, rank=0) — no backward attacks possible
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceLeaps(a1, BPawn);
        Assert::AreEqual(0, attacks.CountBits());
    }

    // ---------------------------------------------------------------
    // Knight tests
    // ---------------------------------------------------------------

    TEST_METHOD(KnightAttacksFromE4Center) {
        // Knight on e4 (file=4, rank=3) should reach 8 squares
        CSCoord e4(0, 4, 3);
        CBitBoard attacks = TraceLeaps(e4, Knight);

        Assert::AreEqual(8, attacks.CountBits());

        // All 8 standard L-shape destinations from e4
        CSCoord expected[] = {
            CSCoord(0, 5, 5), // f6 (file+1, rank+2)
            CSCoord(0, 3, 5), // d6 (file-1, rank+2)
            CSCoord(0, 6, 4), // g5 (file+2, rank+1)
            CSCoord(0, 2, 4), // c5 (file-2, rank+1)
            CSCoord(0, 6, 2), // g3 (file+2, rank-1)
            CSCoord(0, 2, 2), // c3 (file-2, rank-1)
            CSCoord(0, 5, 1), // f2 (file+1, rank-2)
            CSCoord(0, 3, 1), // d2 (file-1, rank-2)
        };

        for (const auto &sq : expected) {
            Assert::IsTrue(attacks.TstBit(sq.BitOffset()),
                L"Knight should attack expected square");
        }
    }

    TEST_METHOD(KnightAttacksFromA1Corner) {
        // Knight on a1 (file=0, rank=0) should reach only 2 squares
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceLeaps(a1, Knight);

        Assert::AreEqual(2, attacks.CountBits());

        CSCoord b3(0, 1, 2);
        Assert::IsTrue(attacks.TstBit(b3.BitOffset()));

        CSCoord c2(0, 2, 1);
        Assert::IsTrue(attacks.TstBit(c2.BitOffset()));
    }

    TEST_METHOD(KnightAttacksFromH8Corner) {
        // Knight on h8 (file=7, rank=7) should reach only 2 squares
        CSCoord h8(0, 7, 7);
        CBitBoard attacks = TraceLeaps(h8, Knight);

        Assert::AreEqual(2, attacks.CountBits());

        CSCoord g6(0, 6, 5);
        Assert::IsTrue(attacks.TstBit(g6.BitOffset()));

        CSCoord f7(0, 5, 6);
        Assert::IsTrue(attacks.TstBit(f7.BitOffset()));
    }

    // ---------------------------------------------------------------
    // Bishop tests (sliding)
    // ---------------------------------------------------------------

    TEST_METHOD(BishopAttacksFromD4Center) {
        // Bishop on d4 (file=3, rank=3) on empty board: 13 squares
        CSCoord d4(0, 3, 3);
        CBitBoard attacks = TraceSlidingRays(d4, Bishop);

        Assert::AreEqual(13, attacks.CountBits());

        // Verify one square in each diagonal direction
        CSCoord e5(0, 4, 4); // NE
        CSCoord b6(0, 1, 5); // NW (continuing further)
        CSCoord e3(0, 4, 2); // SE
        CSCoord a1(0, 0, 0); // SW (end of ray)

        Assert::IsTrue(attacks.TstBit(e5.BitOffset()));
        Assert::IsTrue(attacks.TstBit(b6.BitOffset()));
        Assert::IsTrue(attacks.TstBit(e3.BitOffset()));
        Assert::IsTrue(attacks.TstBit(a1.BitOffset()));
    }

    TEST_METHOD(BishopAttacksFromA1Corner) {
        // Bishop on a1 (file=0, rank=0): only the a1-h8 diagonal = 7 squares
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceSlidingRays(a1, Bishop);

        Assert::AreEqual(7, attacks.CountBits());

        // Check end of the diagonal
        CSCoord h8(0, 7, 7);
        Assert::IsTrue(attacks.TstBit(h8.BitOffset()));
    }

    TEST_METHOD(BishopRaysStopAtBoardEdge) {
        // Bishop on h1 (file=7, rank=0): only one diagonal goes inward
        CSCoord h1(0, 7, 0);
        CBitBoard attacks = TraceSlidingRays(h1, Bishop);

        Assert::AreEqual(7, attacks.CountBits());

        // Should reach a8
        CSCoord a8(0, 0, 7);
        Assert::IsTrue(attacks.TstBit(a8.BitOffset()));
    }

    // ---------------------------------------------------------------
    // Rook tests (sliding)
    // ---------------------------------------------------------------

    TEST_METHOD(RookAttacksFromD4Center) {
        // Rook on d4 (file=3, rank=3) on empty board: 14 squares
        CSCoord d4(0, 3, 3);
        CBitBoard attacks = TraceSlidingRays(d4, Rook);

        Assert::AreEqual(14, attacks.CountBits());

        // Check one square in each direction
        CSCoord d8(0, 3, 7); // North
        CSCoord d1(0, 3, 0); // South
        CSCoord h4(0, 7, 3); // East
        CSCoord a4(0, 0, 3); // West

        Assert::IsTrue(attacks.TstBit(d8.BitOffset()));
        Assert::IsTrue(attacks.TstBit(d1.BitOffset()));
        Assert::IsTrue(attacks.TstBit(h4.BitOffset()));
        Assert::IsTrue(attacks.TstBit(a4.BitOffset()));
    }

    TEST_METHOD(RookAttacksFromA1Corner) {
        // Rook on a1 (file=0, rank=0): 14 squares
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceSlidingRays(a1, Rook);

        Assert::AreEqual(14, attacks.CountBits());
    }

    TEST_METHOD(RookRaysStopAtBoardEdge) {
        // All squares reached by rook from d4 must be on the same file or rank
        CSCoord d4(0, 3, 3);
        CBitBoard attacks = TraceSlidingRays(d4, Rook);

        CBitBoard temp = attacks;
        while (temp) {
            int bit = temp.FindSetBit();
            temp.ClearLowestBit();
            CSCoord sq(bit);
            bool sameFile = (sq.m_nFile == 3);
            bool sameRank = (sq.m_nRank == 3);
            Assert::IsTrue(sameFile || sameRank,
                L"Rook ray square must share file or rank with origin");
        }
    }

    // ---------------------------------------------------------------
    // Queen tests (sliding)
    // ---------------------------------------------------------------

    TEST_METHOD(QueenAttacksFromD4Center) {
        // Queen on d4 = bishop + rook = 13 + 14 = 27 squares
        CSCoord d4(0, 3, 3);
        CBitBoard attacks = TraceSlidingRays(d4, Queen);

        Assert::AreEqual(27, attacks.CountBits());
    }

    TEST_METHOD(QueenAttacksEqualBishopPlusRook) {
        CSCoord e4(0, 4, 3);
        CBitBoard queenAtk = TraceSlidingRays(e4, Queen);
        CBitBoard bishopAtk = TraceSlidingRays(e4, Bishop);
        CBitBoard rookAtk = TraceSlidingRays(e4, Rook);

        CBitBoard combined = bishopAtk | rookAtk;
        Assert::IsTrue(queenAtk == combined);
    }

    // ---------------------------------------------------------------
    // King tests (single step)
    // ---------------------------------------------------------------

    TEST_METHOD(KingAttacksFromE4Center) {
        // King on e4 (file=4, rank=3): 8 adjacent squares
        CSCoord e4(0, 4, 3);
        CBitBoard attacks = TraceLeaps(e4, King);

        Assert::AreEqual(8, attacks.CountBits());

        // Check all 8 adjacent squares
        CSCoord expected[] = {
            CSCoord(0, 4, 4), // N
            CSCoord(0, 4, 2), // S
            CSCoord(0, 5, 3), // E
            CSCoord(0, 3, 3), // W
            CSCoord(0, 5, 4), // NE
            CSCoord(0, 3, 4), // NW
            CSCoord(0, 5, 2), // SE
            CSCoord(0, 3, 2), // SW
        };

        for (const auto &sq : expected) {
            Assert::IsTrue(attacks.TstBit(sq.BitOffset()),
                L"King should attack adjacent square");
        }
    }

    TEST_METHOD(KingAttacksFromA1Corner) {
        // King on a1 (file=0, rank=0): only 3 squares
        CSCoord a1(0, 0, 0);
        CBitBoard attacks = TraceLeaps(a1, King);

        Assert::AreEqual(3, attacks.CountBits());

        CSCoord a2(0, 0, 1);
        CSCoord b1(0, 1, 0);
        CSCoord b2(0, 1, 1);

        Assert::IsTrue(attacks.TstBit(a2.BitOffset()));
        Assert::IsTrue(attacks.TstBit(b1.BitOffset()));
        Assert::IsTrue(attacks.TstBit(b2.BitOffset()));
    }

    TEST_METHOD(KingAttacksFromH8Corner) {
        // King on h8 (file=7, rank=7): only 3 squares
        CSCoord h8(0, 7, 7);
        CBitBoard attacks = TraceLeaps(h8, King);

        Assert::AreEqual(3, attacks.CountBits());
    }

    // ---------------------------------------------------------------
    // General: rays stop at board edge
    // ---------------------------------------------------------------

    TEST_METHOD(AllSlidingRaysTerminateAtBoardEdge) {
        // For every starting square and every sliding piece,
        // verify all reached squares are valid board coordinates
        int slidingPieces[] = { Bishop, Rook, Queen };

        for (int piece : slidingPieces) {
            for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
                CSCoord origin(static_cast<int>(offset));
                CBitBoard attacks = TraceSlidingRays(origin, piece);

                CBitBoard temp = attacks;
                while (temp) {
                    int bit = temp.FindSetBit();
                    temp.ClearLowestBit();
                    Assert::IsTrue(CSCoord::IsValid(bit),
                        L"Ray square must be a valid board offset");
                }
            }
        }
    }

    TEST_METHOD(AllLeapingMovesProduceValidSquares) {
        int leapingPieces[] = { Pawn, Knight, King, BPawn };

        for (int piece : leapingPieces) {
            for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
                CSCoord origin(static_cast<int>(offset));
                CBitBoard attacks = TraceLeaps(origin, piece);

                CBitBoard temp = attacks;
                while (temp) {
                    int bit = temp.FindSetBit();
                    temp.ClearLowestBit();
                    Assert::IsTrue(CSCoord::IsValid(bit),
                        L"Leap square must be a valid board offset");
                }
            }
        }
    }

    // ---------------------------------------------------------------
    // ComputeSlidingAttacks with blockers
    // ---------------------------------------------------------------

    TEST_METHOD(RookBlockedByPieceOnSameFile) {
        // Rook on d1 (file=3, rank=0), blocker on d4 (file=3, rank=3)
        CSCoord d1(0, 3, 0);
        CSCoord d4(0, 3, 3);
        CBitBoard occupied;
        occupied.SetBit(d1.BitOffset());
        occupied.SetBit(d4.BitOffset());

        CBitBoard attacks = ComputeSlidingAttacks(d1, Rook, occupied);

        // Should include d2, d3, d4 (blocker) but not d5-d8
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 3, 1).BitOffset()));  // d2
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 3, 2).BitOffset()));  // d3
        Assert::IsTrue(attacks.TstBit(d4.BitOffset()));                 // d4 (blocker included)
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 3, 4).BitOffset())); // d5 blocked
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 3, 5).BitOffset())); // d6 blocked

        // Horizontal rays should still reach board edges
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 0, 0).BitOffset()));  // a1
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 7, 0).BitOffset()));  // h1
    }

    TEST_METHOD(BishopBlockedByPieceOnDiagonal) {
        // Bishop on c1 (file=2, rank=0), blocker on e3 (file=4, rank=2)
        CSCoord c1(0, 2, 0);
        CSCoord e3(0, 4, 2);
        CBitBoard occupied;
        occupied.SetBit(c1.BitOffset());
        occupied.SetBit(e3.BitOffset());

        CBitBoard attacks = ComputeSlidingAttacks(c1, Bishop, occupied);

        // NE diagonal: d2, e3 (blocker) but not f4, g5, h6
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 3, 1).BitOffset()));  // d2
        Assert::IsTrue(attacks.TstBit(e3.BitOffset()));                 // e3
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 5, 3).BitOffset())); // f4 blocked

        // NW diagonal: b2, a3
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 1, 1).BitOffset()));  // b2
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 0, 2).BitOffset()));  // a3
    }

    TEST_METHOD(QueenBlockedMultipleDirections) {
        // Queen on d4, blockers on d6, f4, b4, b6
        CSCoord d4(0, 3, 3);
        CBitBoard occupied;
        occupied.SetBit(d4.BitOffset());
        occupied.SetBit(CSCoord(0, 3, 5).BitOffset()); // d6
        occupied.SetBit(CSCoord(0, 5, 3).BitOffset()); // f4
        occupied.SetBit(CSCoord(0, 1, 3).BitOffset()); // b4
        occupied.SetBit(CSCoord(0, 1, 5).BitOffset()); // b6

        CBitBoard attacks = ComputeSlidingAttacks(d4, Queen, occupied);

        // North: d5, d6 (blocked), not d7, d8
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 3, 4).BitOffset()));  // d5
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 3, 5).BitOffset()));  // d6
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 3, 6).BitOffset())); // d7

        // East: e4, f4 (blocked), not g4, h4
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 4, 3).BitOffset()));  // e4
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 5, 3).BitOffset()));  // f4
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 6, 3).BitOffset())); // g4

        // NW diagonal: c5, b6 (blocked), not a7
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 2, 4).BitOffset()));  // c5
        Assert::IsTrue(attacks.TstBit(CSCoord(0, 1, 5).BitOffset()));  // b6
        Assert::IsFalse(attacks.TstBit(CSCoord(0, 0, 6).BitOffset())); // a7
    }

    TEST_METHOD(ComputeSlidingAttacksEmptyBoardMatchesUnblockedRays) {
        // With no blockers, ComputeSlidingAttacks should equal TraceSlidingRays
        CSCoord e4(0, 4, 3);
        CBitBoard empty;

        CBitBoard rookAtk = ComputeSlidingAttacks(e4, Rook, empty);
        CBitBoard rookRays = TraceSlidingRays(e4, Rook);
        Assert::IsTrue(rookAtk == rookRays);

        CBitBoard bishopAtk = ComputeSlidingAttacks(e4, Bishop, empty);
        CBitBoard bishopRays = TraceSlidingRays(e4, Bishop);
        Assert::IsTrue(bishopAtk == bishopRays);
    }

    // ---------------------------------------------------------------
    // ComputeLeapAttacks matches EPM tables
    // ---------------------------------------------------------------

    TEST_METHOD(ComputeLeapAttacksKnightMatchesEPM) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeLeapAttacks(sq, Knight);
            Assert::IsTrue(computed == KnightEPM[offset],
                L"Knight ComputeLeapAttacks must match KnightEPM");
        }
    }

    TEST_METHOD(ComputeLeapAttacksKingMatchesEPM) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeLeapAttacks(sq, King);
            Assert::IsTrue(computed == KingEPM[offset],
                L"King ComputeLeapAttacks must match KingEPM");
        }
    }

    TEST_METHOD(ComputeLeapAttacksWhitePawnMatchesEPM) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeLeapAttacks(sq, Pawn);
            Assert::IsTrue(computed == PawnEPM[White][offset],
                L"White Pawn ComputeLeapAttacks must match PawnEPM[White]");
        }
    }

    TEST_METHOD(ComputeLeapAttacksBlackPawnMatchesEPM) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeLeapAttacks(sq, BPawn);
            Assert::IsTrue(computed == PawnEPM[Black][offset],
                L"Black Pawn ComputeLeapAttacks must match PawnEPM[Black]");
        }
    }

    // ---------------------------------------------------------------
    // ComputeSlidingAttacks matches old magic tables
    // ---------------------------------------------------------------

    TEST_METHOD(ComputeSlidingAttacksRookMatchesMagicForAllSquares) {
        // Verify that for every square on the empty board,
        // ComputeSlidingAttacks produces the same result as RookEPM
        CBitBoard empty;
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeSlidingAttacks(sq, Rook, empty);
            Assert::IsTrue(computed == RookEPM[offset],
                L"Rook on empty board must match RookEPM");
        }
    }

    TEST_METHOD(ComputeSlidingAttacksBishopMatchesMagicForAllSquares) {
        CBitBoard empty;
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeSlidingAttacks(sq, Bishop, empty);
            Assert::IsTrue(computed == BishopEPM[offset],
                L"Bishop on empty board must match BishopEPM");
        }
    }

    TEST_METHOD(ComputeSlidingAttacksQueenMatchesMagicForAllSquares) {
        CBitBoard empty;
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            CSCoord sq(static_cast<int>(offset));
            CBitBoard computed = ComputeSlidingAttacks(sq, Queen, empty);
            Assert::IsTrue(computed == QueenEPM[offset],
                L"Queen on empty board must match QueenEPM");
        }
    }

    // TEST_METHOD(ComputeSlidingAttacksMatchesReferenceWithBlockers) - disabled: uses EPD
    // TEST_METHOD(ComputeSlidingAttacksMatchesReferenceMidgamePosition) - disabled: uses EPD
};

} // namespace WinAmyTests
