#include "TestHelpers.h"

#include <set>
#include <utility>

namespace WinAmyTests {

TEST_CLASS(PositionTests) {
    static void AssertCheckingMoveMatchesResultingCheck(CPosition *position, CMove move) {
        const bool reportsCheck = position->IsCheckingMove(move);
        position->DoMove(move);
        const bool expected = position->InCheck(position->m_nTurn);
        position->UndoMove(move);
        Assert::AreEqual(expected, reportsCheck);
    }

  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // --- IsCheckingMove tests ---

    TEST_METHOD(IsCheckingMoveDirectKnightCheck) {
        // White knight on hd4, black king on hf3. Nd4-he2 checks king on hf3 (knight on he2 attacks hf4,hd4,hc3,hc1,hg1,hg3 ? no)
        // Knight on hd4 attacks: hb3,hb5,hc2,hc6,he2,he6,hf3,hf5. King on hf3: Nd4-he2 attacks hd4,hf4,hc1,hc3,hg1,hg3 ? no.
        // Let's use: knight hd4, king on hc6. Knight hd4-hb5 attacks hc7,ha7,hd6,ha3,hc3,hd4 ? no.
        // Simplest: knight on hd4, king on hf5. hd4 attacks hf5? Yes! But knight is MOVING TO a square.
        // Knight on hb1, king on he4. Nb1-hc3 attacks he4? Knight on hc3 attacks: ha2,ha4,hb1,hb5,hd1,hd5,he2,he4. Yes!
        char epd[] = "8/8/8/8/4k3/8/8/1N2K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hb1, hc3, 0);
        Assert::IsFalse(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectKnightNoCheck) {
        // White knight on hd4, black king on he6. Nc2 doesn't give check.
        char epd[] = "8/8/4k3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hd4, hc2, 0);
        Assert::IsFalse(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectRookCheck) {
        // White rook on hh1, black king on he8. Rh8 gives check (same rank).
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hh1, hh8, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectBishopCheck) {
        // White bishop on ha1, king on hh8. Bishop to hd4 gives check on diagonal.
        char epd[] = "7k/8/8/8/8/8/8/B3K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        // ha1 to hd4: bishop attacks along ha1-hh8 diagonal, hd4 attacks hh8
        CMove move = MakeMainBoardMove(ha1, hd4, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectPawnCheck) {
        // White pawn on hd5, black king on he7. hd5-hd6 doesn't check.
        // Pawn on hf6, king on he8: not a legal scenario for pawn check detection.
        // White pawn on hd6, if it could move to hd7 ? no, pawn checks from attack squares.
        // White to move, pawn on hd5. King on hc7. Pawn hd5-hd6 checks king on hc7 via hd6 attacks hc7.
        char epd[] = "8/2k5/8/3P4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hd5, hd6, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDiscoveredCheck) {
        // White rook on he1, white bishop on he4 blocking rook's attack on he8 king.
        // Moving bishop off e-file discovers check from rook.
        char epd[] = "4k3/8/8/8/4B3/8/8/4K1R1 w - -";
        // Actually need rook on e-file. Let's set up properly:
        // Rook on he1, bishop on he4, king on he8. Bishop moves to hc2 (off e-file) -> discovered check.
        char epd2[] = "4k3/8/8/8/4B3/8/8/4KR2 w - -";
        // Hmm, rook needs to be on e-file. Let me be more careful:
        // White: King hg1, Rook he1, Bishop he4. Black: King he8.
        char epd3[] = "4k3/8/8/8/4B3/8/8/4R1K1 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd3));

        // Bishop he4 to hc2: off the e-file, rook on he1 now has open line to he8
        CMove move = MakeMainBoardMove(he4, hc2, 0);
        AssertCheckingMoveMatchesResultingCheck(position.get(), move);
    }

    TEST_METHOD(IsCheckingMoveNoDiscoveredCheckWhenBlocked) {
        // White rook on he1, white bishop on he4, white pawn on he5 also on the file.
        // Moving bishop off e-file still blocked by pawn on he5.
        char epd[] = "4k3/8/8/4P3/4B3/8/8/4R1K1 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(he4, hc2, 0);
        Assert::IsFalse(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMovePromotionToQueenCheck) {
        // Pawn on hh7, king on he8. Promote to queen on hh8 checks via rank.
        char epd[] = "4k3/7P/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardPromotion(hh7, hh8, Queen, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    // --- LegalMove tests ---

    TEST_METHOD(LegalMoveAcceptsValidKnightCapture) {
        // White knight on hd4, black pawn on he6
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hd4, he6, M_CAPTURE);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsNoneMove) {
        char epd[] = "4k3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsFalse(position.get()->LegalMove(M_NONE));
    }

    TEST_METHOD(LegalMoveRejectsWrongColorPiece) {
        // It's white's turn but we try to move a black pawn
        char epd[] = "4k3/3p4/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(hd7, hd6, 0);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsValidPawnPush) {
        PositionGuard position(CPosition::Initial());

        CMove move = MakeMainBoardMove(he2, he3, 0);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsValidPawnDoublePush) {
        PositionGuard position(CPosition::Initial());

        CMove move = MakeMainBoardMove(he2, he4, M_PAWND);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsPawnDoublePushWhenBlocked) {
        // Pawn on he2, piece on he3 blocks double push
        char epd[] = "4k3/8/8/8/8/4n3/4P3/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(he2, he4, M_PAWND);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsCaptureOnEmptySquare) {
        PositionGuard position(CPosition::Initial());

        CMove move = MakeMainBoardMove(hb1, hc3, M_CAPTURE);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsEnPassant) {
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(he5, hd6, M_ENPASSANT);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsEnPassantWhenNoneAvailable) {
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(he5, hd6, M_ENPASSANT);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    // --- MayCastle tests ---

    TEST_METHOD(MayCastleAllowsKingsideCastling) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        Assert::IsTrue(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleAllowsQueensideCastling) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_C1, M_LCASTLE);
        Assert::IsTrue(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenInCheck) {
        // King on he1 in check from black rook on he8
        char epd[] = "4r3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenPathBlocked) {
        // Knight on hf1 blocks short castle
        char epd[] = "4k3/8/8/8/8/8/8/4KN1R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenPathAttacked) {
        // Black rook on hf8 attacks hf1
        char epd[] = "4kr2/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWithoutCastlingRights) {
        // Position has no castling rights
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    // --- IsPassed tests ---

    TEST_METHOD(IsPassedReturnsTrueForPassedPawn) {
        // White pawn on he5, no black pawns on d/e/f files ahead
        char epd[] = "4k3/8/8/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsTrue(position.get()->IsPassed(MainBoardCoord(he5), White));
    }

    TEST_METHOD(IsPassedReturnsFalseWhenBlockedByEnemyPawn) {
        // White pawn on he5, black pawn on he6 blocks
        char epd[] = "4k3/8/4p3/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(MainBoardCoord(he5), White));
    }

    TEST_METHOD(IsPassedReturnsFalseWhenEnemyPawnOnAdjacentFile) {
        // White pawn on he5, black pawn on hd6 guards
        char epd[] = "4k3/8/3p4/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(MainBoardCoord(he5), White));
    }

    TEST_METHOD(IsPassedBlackPassedPawn) {
        // Black pawn on hd4, no white pawns on c/d/e files below
        char epd[] = "4k3/8/8/8/3p4/8/8/4K3 b - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsTrue(position.get()->IsPassed(MainBoardCoord(hd4), Black));
    }

    TEST_METHOD(IsPassedBlackNotPassedPawn) {
        // Black pawn on hd4, white pawn on hc3
        char epd[] = "4k3/8/8/8/3p4/2P5/8/4K3 b - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(MainBoardCoord(hd4), Black));
    }

    // --- LegalMoves count tests ---

    TEST_METHOD(LegalMovesInitialPositionIs20) {
        PositionGuard position(CPosition::Initial());

        int count = position.get()->LegalMoves(NULL);
        Assert::AreEqual(63, count);
    }

    TEST_METHOD(LegalMovesInitialPositionAllPieceMovesPresent) {
        // Verify every expected legal move from the 4D opening position is present
        // in the output of LegalMoves.  The 63 moves break down as:
        //   Main board (h, 8x8)  : 8 pawns × 2 (push+double) + 2 knights × 3 = 22
        //   Level g (6, 7x7)     : 7 pawns × 1 (single push, not at homeRank) = 7
        //   Level i (8, 7x7)     : 7 pawns × 2 + 2 knights × 7 (incl. cross-level) = 28
        //   Level j (9, 6x6)     : 6 pawns × 1 (single push, not at homeRank) = 6
        PositionGuard position(CPosition::Initial());

        // Collect all fully-legal moves into a heap, then index by (from, to) offsets.
        heap_t heap = allocate_heap();
        const int count = position.get()->LegalMoves(heap);
        Assert::AreEqual(63, count);

        std::set<std::pair<uint16_t, uint16_t>> legalSet;
        for (unsigned int i = heap->current_section->start;
             i < heap->current_section->end; i++) {
            legalSet.insert({heap->data[i].GetFromCoord().BitOffset(),
                             heap->data[i].GetToCoord().BitOffset()});
        }
        free_heap(heap);

        auto AssertPresent = [&](CSCoord from, CSCoord to) {
            Assert::IsTrue(
                legalSet.count({from.BitOffset(), to.BitOffset()}) > 0,
                L"Expected legal move not found in LegalMoves output");
        };

        // --- Main board (level h = 7): 22 moves ---

        // 8 white pawns on rank 1 (ha2–hh2): single push to rank 2, double push to rank 3.
        for (uint16_t file = 0; file < 8; file++) {
            CSCoord from(MAIN_LEVEL, file, 1);
            AssertPresent(from, CSCoord(MAIN_LEVEL, file, 2)); // single push
            AssertPresent(from, CSCoord(MAIN_LEVEL, file, 3)); // double push (M_PAWND)
        }
        // Knight hb1 (file=1, rank=0): ha3, hc3 on main board + gc2 on level g.
        CSCoord hb1(MAIN_LEVEL, 1, 0);
        AssertPresent(hb1, CSCoord(MAIN_LEVEL, 0, 2)); // ha3
        AssertPresent(hb1, CSCoord(MAIN_LEVEL, 2, 2)); // hc3
        AssertPresent(hb1, CSCoord(6, 2, 1));           // gc2

        // Knight hg1 (file=6, rank=0): hf3, hh3 on main board + ge2 on level g.
        CSCoord hg1(MAIN_LEVEL, 6, 0);
        AssertPresent(hg1, CSCoord(MAIN_LEVEL, 5, 2)); // hf3
        AssertPresent(hg1, CSCoord(MAIN_LEVEL, 7, 2)); // hh3
        AssertPresent(hg1, CSCoord(6, 4, 1));           // ge2

        // --- Level g (index 6, 7x7): 7 moves ---
        // 7 white pawns on rank 0 (ga1–gg1).
        // Rank 0 ≠ homeRank (1) so no double push is available; single push only.
        for (uint16_t file = 0; file < 7; file++) {
            AssertPresent(CSCoord(6, file, 0), CSCoord(6, file, 1));
        }

        // --- Level i (index 8, 7x7): 28 moves ---
        // 7 white pawns on rank 1 (ia2–ig2).
        // Rank 1 == homeRank so both single and double push are available.
        for (uint16_t file = 0; file < 7; file++) {
            CSCoord from(8, file, 1);
            AssertPresent(from, CSCoord(8, file, 2)); // single push to rank 2
            AssertPresent(from, CSCoord(8, file, 3)); // double push to rank 3 (M_PAWND)
        }
        // Knight ib1 (file=1, rank=0): 2 same-level + 5 cross-level jumps.
        CSCoord ib1(8, 1, 0);
        AssertPresent(ib1, CSCoord(8, 0, 2));          // ia3
        AssertPresent(ib1, CSCoord(8, 2, 2));          // ic3
        AssertPresent(ib1, CSCoord(9, 2, 1));          // jc2 (level j)
        AssertPresent(ib1, CSCoord(5, 0, 0));          // fa1 (level f)
        AssertPresent(ib1, CSCoord(5, 1, 0));          // fb1 (level f)
        AssertPresent(ib1, CSCoord(MAIN_LEVEL, 0, 2)); // ha3 (main board)
        AssertPresent(ib1, CSCoord(MAIN_LEVEL, 3, 2)); // hd3 (main board)

        // Knight if1 (file=5, rank=0): 2 same-level + 5 cross-level jumps.
        CSCoord if1(8, 5, 0);
        AssertPresent(if1, CSCoord(8, 4, 2));          // ie3
        AssertPresent(if1, CSCoord(8, 6, 2));          // ig3
        AssertPresent(if1, CSCoord(9, 3, 1));          // jd2 (level j)
        AssertPresent(if1, CSCoord(5, 4, 0));          // fe1 (level f)
        AssertPresent(if1, CSCoord(5, 5, 0));          // ff1 (level f)
        AssertPresent(if1, CSCoord(MAIN_LEVEL, 4, 2)); // he3 (main board)
        AssertPresent(if1, CSCoord(MAIN_LEVEL, 7, 2)); // hh3 (main board)

        // --- Level j (index 9, 6x6): 6 moves ---
        // 6 white pawns on rank 0 (ja1–jf1).
        // Rank 0 ≠ homeRank (1) so single push only.
        for (uint16_t file = 0; file < 6; file++) {
            AssertPresent(CSCoord(9, file, 0), CSCoord(9, file, 1));
        }
    }

    TEST_METHOD(LegalMovesKingAloneInCorner) {
        // King on ha1, enemy king far away
        char epd[] = "4k3/8/8/8/8/8/8/K7 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        int count = position.get()->LegalMoves(NULL);
        Assert::AreEqual(5, count);
    }

    // --- Repeated tests ---

    TEST_METHOD(RepeatedReturnsFalseForNewPosition) {
        PositionGuard position(CPosition::Initial());

        int result = position.get()->Repeated(0);
        Assert::AreEqual(0, result);
    }

    TEST_METHOD(RepeatedDetectsRepetition) {
        PositionGuard position(CPosition::Initial());

        // Play Nf3, Nf6, Ng1, Ng8 to return to start position
        CMove nf3 = MakeMainBoardMove(hg1, hf3, 0);
        CMove nf6 = MakeMainBoardMove(hg8, hf6, 0);
        CMove ng1 = MakeMainBoardMove(hf3, hg1, 0);
        CMove ng8 = MakeMainBoardMove(hf6, hg8, 0);

        position.get()->DoMove(nf3);
        position.get()->DoMove(nf6);
        position.get()->DoMove(ng1);
        position.get()->DoMove(ng8);

        // Position has now repeated
        int result = position.get()->Repeated(0);
        Assert::IsTrue(result != 0);
    }

    // --- CheckDraw tests ---

    TEST_METHOD(CheckDrawReturnsFalseForNormalPosition) {
        PositionGuard position(CPosition::Initial());

        Assert::IsFalse(position.get()->CheckDraw());
    }

    TEST_METHOD(CheckDrawReturnsTrueForKPvsKWithWrongCorner) {
        // White: King he1, Pawn ha6. Black: King ha8. 
        // Black king in corner of a-file rook pawn
        char epd[] = "k7/8/P7/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsTrue(position.get()->CheckDraw());
    }

    // --- InCheck tests ---

    TEST_METHOD(InCheckReturnsFalseInInitialPosition) {
        PositionGuard position(CPosition::Initial());

        Assert::IsFalse(position.get()->InCheck(White));
        Assert::IsFalse(position.get()->InCheck(Black));
    }

    TEST_METHOD(InCheckReturnsTrueWhenKingAttacked) {
        // White king on he1, black rook on he8 (open file)
        char epd[] = "4r3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsTrue(position.get()->InCheck(White));
    }

    TEST_METHOD(InCheckReturnsFalseWhenKingNotAttacked) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        Assert::IsFalse(position.get()->InCheck(White));
    }
};

} // namespace WinAmyTests
