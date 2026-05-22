#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(PositionTests) {
    BEGIN_TEST_CLASS_ATTRIBUTE()
    TEST_CLASS_ATTRIBUTE(L"Ignore", L"true")
    END_TEST_CLASS_ATTRIBUTE()
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    // --- IsCheckingMove tests ---

    TEST_METHOD(IsCheckingMoveDirectKnightCheck) {
        // White knight on d4, black king on f3. Nd4-e2 checks king on f3 (knight on e2 attacks f4,d4,c3,c1,g1,g3 — no)
        // Knight on d4 attacks: b3,b5,c2,c6,e2,e6,f3,f5. King on f3: Nd4-e2 attacks d4,f4,c1,c3,g1,g3 — no.
        // Let's use: knight d4, king on c6. Knight d4-b5 attacks c7,a7,d6,a3,c3,d4 — no.
        // Simplest: knight on d4, king on f5. d4 attacks f5? Yes! But knight is MOVING TO a square.
        // Knight on b1, king on e4. Nb1-c3 attacks e4? Knight on c3 attacks: a2,a4,b1,b5,d1,d5,e2,e4. Yes!
        char epd[] = "8/8/8/8/4k3/8/8/1N2K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(b1, c3, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectKnightNoCheck) {
        // White knight on d4, black king on e6. Nc2 doesn't give check.
        char epd[] = "8/8/4k3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(d4, c2, 0);
        Assert::IsFalse(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectRookCheck) {
        // White rook on a1, black king on a8. Ra1-a5 doesn't check (blocked by nothing, but a5 not a8)
        // Actually use: rook on h1, king on e8. Rh8 gives check.
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(h1, h8, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectBishopCheck) {
        // White bishop on c1, black king on h6. Bd2 doesn't check, but Bf4 might not...
        // Use: bishop on a1, king on h8. Bishop to d4 gives check on diagonal.
        char epd[] = "7k/8/8/8/8/8/8/B3K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        // a1 to d4: bishop attacks along a1-h8 diagonal, d4 attacks h8
        CMove move = make_move(a1, d4, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDirectPawnCheck) {
        // White pawn on d5, black king on e7. d5-d6 doesn't check.
        // Pawn on f6, king on e8: not a legal scenario for pawn check detection.
        // White pawn on d6, if it could move to d7 — no, pawn checks from attack squares.
        // White to move, pawn on d5. King on c7. Pawn d5-d6 checks king on c7 via d6 attacks c7.
        char epd[] = "8/2k5/8/3P4/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(d5, d6, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveDiscoveredCheck) {
        // White rook on e1, white bishop on e4 blocking rook's attack on e8 king.
        // Moving bishop off e-file discovers check from rook.
        char epd[] = "4k3/8/8/8/4B3/8/8/4K1R1 w - -";
        // Actually need rook on e-file. Let's set up properly:
        // Rook on e1, bishop on e4, king on e8. Bishop moves to c2 (off e-file) -> discovered check.
        char epd2[] = "4k3/8/8/8/4B3/8/8/4KR2 w - -";
        // Hmm, rook needs to be on e-file. Let me be more careful:
        // White: King g1, Rook e1, Bishop e4. Black: King e8.
        char epd3[] = "4k3/8/8/8/4B3/8/8/4R1K1 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd3));

        // Bishop e4 to c2: off the e-file, rook on e1 now has open line to e8
        CMove move = make_move(e4, c2, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMoveNoDiscoveredCheckWhenBlocked) {
        // White rook on e1, white bishop on e4, white pawn on e5 also on the file.
        // Moving bishop off e-file still blocked by pawn on e5.
        char epd[] = "4k3/8/8/4P3/4B3/8/8/4R1K1 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e4, c2, 0);
        Assert::IsFalse(position.get()->IsCheckingMove(move));
    }

    TEST_METHOD(IsCheckingMovePromotionToQueenCheck) {
        // White pawn on e7, black king on a3 (won't get direct queen check from e8 to a3 easily)
        // Use: pawn on d7, king on d8... no, king can't be on promo rank for white pawn promo.
        // Pawn on h7, king on e8. Promote to queen on h8 checks via rank.
        char epd[] = "4k3/7P/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_promotion(h7, h8, Queen, 0);
        Assert::IsTrue(position.get()->IsCheckingMove(move));
    }

    // --- LegalMove tests ---

    TEST_METHOD(LegalMoveAcceptsValidKnightCapture) {
        // White knight on d4, black pawn on e6
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(d4, e6, M_CAPTURE);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsNoneMove) {
        char epd[] = "4k3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsFalse(position.get()->LegalMove(M_NONE));
    }

    TEST_METHOD(LegalMoveRejectsWrongColorPiece) {
        // It's white's turn but we try to move a black pawn
        char epd[] = "4k3/3p4/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(d7, d6, 0);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsValidPawnPush) {
        PositionGuard position(CPosition::Initial());

        CMove move = make_move(e2, e3, 0);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsValidPawnDoublePush) {
        PositionGuard position(CPosition::Initial());

        CMove move = make_move(e2, e4, M_PAWND);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsPawnDoublePushWhenBlocked) {
        // Pawn on e2, piece on e3 blocks double push
        char epd[] = "4k3/8/8/8/8/4n3/4P3/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e2, e4, M_PAWND);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsCaptureOnEmptySquare) {
        PositionGuard position(CPosition::Initial());

        CMove move = make_move(b1, c3, M_CAPTURE);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveAcceptsEnPassant) {
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e5, d6, M_ENPASSANT);
        Assert::IsTrue(position.get()->LegalMove(move));
    }

    TEST_METHOD(LegalMoveRejectsEnPassantWhenNoneAvailable) {
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e5, d6, M_ENPASSANT);
        Assert::IsFalse(position.get()->LegalMove(move));
    }

    // --- MayCastle tests ---

    TEST_METHOD(MayCastleAllowsKingsideCastling) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, g1, M_SCASTLE);
        Assert::IsTrue(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleAllowsQueensideCastling) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, c1, M_LCASTLE);
        Assert::IsTrue(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenInCheck) {
        // King on e1 in check from black rook on e8
        char epd[] = "4r3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, g1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenPathBlocked) {
        // Knight on f1 blocks short castle
        char epd[] = "4k3/8/8/8/8/8/8/4KN1R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, g1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWhenPathAttacked) {
        // Black rook on f8 attacks f1
        char epd[] = "4kr2/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, g1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    TEST_METHOD(MayCastleRejectsWithoutCastlingRights) {
        // Position has no castling rights
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        CMove move = make_move(e1, g1, M_SCASTLE);
        Assert::IsFalse(position.get()->MayCastle(move));
    }

    // --- IsPassed tests ---

    TEST_METHOD(IsPassedReturnsTrueForPassedPawn) {
        // White pawn on e5, no black pawns on d/e/f files ahead
        char epd[] = "4k3/8/8/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsTrue(position.get()->IsPassed(CSCoord(e5), White));
    }

    TEST_METHOD(IsPassedReturnsFalseWhenBlockedByEnemyPawn) {
        // White pawn on e5, black pawn on e6 blocks
        char epd[] = "4k3/8/4p3/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(CSCoord(e5), White));
    }

    TEST_METHOD(IsPassedReturnsFalseWhenEnemyPawnOnAdjacentFile) {
        // White pawn on e5, black pawn on d6 guards
        char epd[] = "4k3/8/3p4/4P3/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(CSCoord(e5), White));
    }

    TEST_METHOD(IsPassedBlackPassedPawn) {
        // Black pawn on d4, no white pawns on c/d/e files below
        char epd[] = "4k3/8/8/8/3p4/8/8/4K3 b - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsTrue(position.get()->IsPassed(CSCoord(d4), Black));
    }

    TEST_METHOD(IsPassedBlackNotPassedPawn) {
        // Black pawn on d4, white pawn on c3
        char epd[] = "4k3/8/8/8/3p4/2P5/8/4K3 b - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsFalse(position.get()->IsPassed(CSCoord(d4), Black));
    }

    // --- LegalMoves count tests ---

    TEST_METHOD(LegalMovesInitialPositionIs20) {
        PositionGuard position(CPosition::Initial());

        int count = position.get()->LegalMoves(NULL);
        Assert::AreEqual(20, count);
    }

    TEST_METHOD(LegalMovesKingAloneInCorner) {
        // King on a1, enemy king far away
        char epd[] = "4k3/8/8/8/8/8/8/K7 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        int count = position.get()->LegalMoves(NULL);
        Assert::AreEqual(3, count); // b1, a2, b2
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
        CMove nf3 = make_move(g1, f3, 0);
        CMove nf6 = make_move(g8, f6, 0);
        CMove ng1 = make_move(f3, g1, 0);
        CMove ng8 = make_move(f6, g8, 0);

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
        // White: King e1, Pawn a6. Black: King a8. 
        // Black king in corner of a-file rook pawn
        char epd[] = "k7/8/P7/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsTrue(position.get()->CheckDraw());
    }

    // --- InCheck tests ---

    TEST_METHOD(InCheckReturnsFalseInInitialPosition) {
        PositionGuard position(CPosition::Initial());

        Assert::IsFalse(position.get()->InCheck(White));
        Assert::IsFalse(position.get()->InCheck(Black));
    }

    TEST_METHOD(InCheckReturnsTrueWhenKingAttacked) {
        // White king on e1, black rook on e8 (open file)
        char epd[] = "4r3/8/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsTrue(position.get()->InCheck(White));
    }

    TEST_METHOD(InCheckReturnsFalseWhenKingNotAttacked) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        Assert::IsFalse(position.get()->InCheck(White));
    }
};

} // namespace WinAmyTests
