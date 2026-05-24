#include "TestHelpers.h"

#include "scoord.h"
#include "ucoord.h"

namespace WinAmyTests {

TEST_CLASS(AttackDeltaTests) {
    static CBitBoard TraceSlidingRays(const CSCoord &origin, int pieceType, CBitBoard occupied = {}) {
        CBitBoard result;
        for (int d = 0; d < ATTACK_DELTA_COUNT[pieceType]; d++) {
            const CUCoord dir = ATTACK_DELTA[pieceType][d];
            CSCoord sq = origin.Step(dir);
            while (sq.IsValid()) {
                result.SetBit(sq.BitOffset());
                if (occupied.TstBit(sq.BitOffset())) {
                    break;
                }
                sq = sq.Step(dir);
            }
        }
        return result;
    }

    static CBitBoard TraceLeaps(const CSCoord &origin, int pieceType) {
        CBitBoard result;
        for (int d = 0; d < ATTACK_DELTA_COUNT[pieceType]; d++) {
            const CUCoord dir = ATTACK_DELTA[pieceType][d];
            const CSCoord sq = origin.Step(dir);
            if (sq.IsValid()) {
                result.SetBit(sq.BitOffset());
            }
        }
        return result;
    }

  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(WhitePawnAttacksFromD4) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        Assert::IsTrue(ComputeLeapAttacks(d4, Pawn) == TraceLeaps(d4, Pawn));
    }

    TEST_METHOD(BlackPawnAttacksFromE5) {
        const CSCoord e5(MAIN_LEVEL, 4, 4);
        Assert::IsTrue(ComputeLeapAttacks(e5, BPawn) == TraceLeaps(e5, BPawn));
    }

    TEST_METHOD(KnightAttacksFromE4Center) {
        const CSCoord e4(MAIN_LEVEL, 4, 3);
        Assert::IsTrue(ComputeLeapAttacks(e4, Knight) == TraceLeaps(e4, Knight));
    }

    TEST_METHOD(KingAttacksFromE4Center) {
        const CSCoord e4(MAIN_LEVEL, 4, 3);
        Assert::IsTrue(ComputeLeapAttacks(e4, King) == TraceLeaps(e4, King));
    }

    TEST_METHOD(BishopAttacksFromD4Center) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        Assert::IsTrue(ComputeSlidingAttacks(d4, Bishop, {}) == TraceSlidingRays(d4, Bishop));
    }

    TEST_METHOD(RookAttacksFromD4Center) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        Assert::IsTrue(ComputeSlidingAttacks(d4, Rook, {}) == TraceSlidingRays(d4, Rook));
    }

    TEST_METHOD(QueenAttacksEqualBishopPlusRook) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        const CBitBoard queen = ComputeSlidingAttacks(d4, Queen, {});
        const CBitBoard parts = ComputeSlidingAttacks(d4, Bishop, {}) | ComputeSlidingAttacks(d4, Rook, {});
        Assert::IsTrue(queen == parts);
    }

    TEST_METHOD(AllSlidingRaysTerminateAtBoardEdge) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            const CSCoord origin(static_cast<uint16_t>(offset));
            for (int pieceType : {Bishop, Rook, Queen}) {
                const CBitBoard attacks = TraceSlidingRays(origin, pieceType);
                CBitBoard scan = attacks;
                while (scan.IsNotEmpty()) {
                    const uint16_t sq = scan.FindSetBit();
                    Assert::IsTrue(CSCoord(sq).IsValid());
                    scan.ClrBit(sq);
                }
            }
        }
    }

    TEST_METHOD(AllLeapingMovesProduceValidSquares) {
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            const CSCoord origin(static_cast<uint16_t>(offset));
            for (int pieceType : {Pawn, BPawn, Knight, King}) {
                const CBitBoard attacks = TraceLeaps(origin, pieceType);
                CBitBoard scan = attacks;
                while (scan.IsNotEmpty()) {
                    const uint16_t sq = scan.FindSetBit();
                    Assert::IsTrue(CSCoord(sq).IsValid());
                    scan.ClrBit(sq);
                }
            }
        }
    }

    TEST_METHOD(RookBlockedByPieceOnSameFile) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        const CSCoord d6(MAIN_LEVEL, 3, 5);
        CBitBoard occupied = CBitBoard::SetMask(d6.BitOffset());

        const CBitBoard expected = TraceSlidingRays(d4, Rook, occupied);
        const CBitBoard computed = ComputeSlidingAttacks(d4, Rook, occupied);
        Assert::IsTrue(computed == expected);
        Assert::IsTrue(computed.TstBit(d6.BitOffset()));
    }

    TEST_METHOD(BishopBlockedByPieceOnDiagonal) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        const CSCoord f6(MAIN_LEVEL, 5, 5);
        CBitBoard occupied = CBitBoard::SetMask(f6.BitOffset());

        const CBitBoard expected = TraceSlidingRays(d4, Bishop, occupied);
        const CBitBoard computed = ComputeSlidingAttacks(d4, Bishop, occupied);
        Assert::IsTrue(computed == expected);
        Assert::IsTrue(computed.TstBit(f6.BitOffset()));
    }

    TEST_METHOD(QueenBlockedMultipleDirections) {
        const CSCoord d4(MAIN_LEVEL, 3, 3);
        CBitBoard occupied = CBitBoard::SetMask(CSCoord(MAIN_LEVEL, 3, 5).BitOffset()) |
                             CBitBoard::SetMask(CSCoord(MAIN_LEVEL, 5, 3).BitOffset()) |
                             CBitBoard::SetMask(CSCoord(MAIN_LEVEL, 5, 5).BitOffset());

        const CBitBoard expected = TraceSlidingRays(d4, Queen, occupied);
        const CBitBoard computed = ComputeSlidingAttacks(d4, Queen, occupied);
        Assert::IsTrue(computed == expected);
    }

    TEST_METHOD(ComputeSlidingAttacksEmptyBoardMatchesUnblockedRays) {
        CBitBoard empty;
        for (unsigned int offset = 0; offset < CBitBoard::SIZE; offset++) {
            const CSCoord sq(static_cast<uint16_t>(offset));
            Assert::IsTrue(ComputeSlidingAttacks(sq, Rook, empty) == TraceSlidingRays(sq, Rook));
            Assert::IsTrue(ComputeSlidingAttacks(sq, Bishop, empty) == TraceSlidingRays(sq, Bishop));
            Assert::IsTrue(ComputeSlidingAttacks(sq, Queen, empty) == TraceSlidingRays(sq, Queen));
        }
    }

    TEST_METHOD(ComputeSlidingAttacksMatchesReferenceWithBlockers) {
        char epd[] = "4k3/8/3p1p2/8/3Q4/8/3P1P2/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const uint16_t source = MainBoardOffset(d4);
        const CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];

        const CBitBoard rookExpected = ReferenceRookAttacks(source, occupied);
        const CBitBoard bishopExpected = ReferenceBishopAttacks(source, occupied);
        const CBitBoard queenExpected = rookExpected | bishopExpected;

        Assert::IsTrue(ComputeSlidingAttacks(MainBoardCoord(d4), Rook, occupied) == rookExpected);
        Assert::IsTrue(ComputeSlidingAttacks(MainBoardCoord(d4), Bishop, occupied) == bishopExpected);
        Assert::IsTrue(ComputeSlidingAttacks(MainBoardCoord(d4), Queen, occupied) == queenExpected);
    }

    TEST_METHOD(ComputeSlidingAttacksMatchesReferenceMidgamePosition) {
        char epd[] = "r3k2r/ppp2ppp/2npbn2/3qp3/3P4/2N1PN2/PPP2PPP/R2QKB1R w KQkq -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        const CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];
        const uint16_t whiteQueen = MainBoardOffset(d1);
        const uint16_t blackQueen = MainBoardOffset(d5);

        Assert::IsTrue(ComputeSlidingAttacks(MainBoardCoord(d1), Queen, occupied) ==
                       (ReferenceRookAttacks(whiteQueen, occupied) |
                        ReferenceBishopAttacks(whiteQueen, occupied)));
        Assert::IsTrue(ComputeSlidingAttacks(MainBoardCoord(d5), Queen, occupied) ==
                       (ReferenceRookAttacks(blackQueen, occupied) |
                        ReferenceBishopAttacks(blackQueen, occupied)));
    }
};

} // namespace WinAmyTests
