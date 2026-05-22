#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(MoveTests) {
    BEGIN_TEST_CLASS_ATTRIBUTE()
    TEST_CLASS_ATTRIBUTE(L"Ignore", L"true")
    END_TEST_CLASS_ATTRIBUTE()
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(DoMoveAndUndoMoveRestorePosition) {
        PositionGuard position(CPosition::Initial());
        PositionGuard snapshot(CPosition::Clone(position.get()));

        const CMove move = make_move(e2, e4, M_PAWND);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->m_rgPiece[e4]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[e2]);
        Assert::AreEqual((int)Black, (int)position.get()->m_nTurn);
        Assert::AreEqual(1, (int)position.get()->m_wPly);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on d6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        position.get()->DoNull();
        Assert::AreEqual((int)Black, (int)position.get()->m_nTurn);
        Assert::IsFalse(position.get()->m_EnPassant.IsValid());

        position.get()->UndoNull();
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on d5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));

        for (int i = 0; i < 64; i++) {
            position.get()->m_rgAtkTo[i] = {};
            position.get()->m_rgAtkFr[i] = {};
        }

        position.get()->RecalcAttacks();

        CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];
        CBitBoard expectedBishopAttacks = ComputeSlidingAttacks(CSCoord(d5), Bishop, occupied);

        Assert::IsTrue(position.get()->m_rgAtkTo[d5] == expectedBishopAttacks);
        Assert::IsTrue((position.get()->m_rgAtkFr[e6] & CBitBoard::SetMask(d5)).IsNotEmpty());
        Assert::IsTrue((position.get()->m_rgAtkFr[c4] & CBitBoard::SetMask(d5)).IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(d4, e6, M_CAPTURE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Knight, (int)position.get()->m_rgPiece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[d4]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on e5, black pawn just moved d7-d5, en passant on d6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(e5, d6, M_ENPASSANT);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->m_rgPiece[d6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[e5]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[d5]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->m_rgPiece[CASTLE_G1]);
        Assert::AreEqual((int)Rook, (int)position.get()->m_rgPiece[CASTLE_F1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[CASTLE_E1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[CASTLE_H1]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(CASTLE_E1, CASTLE_C1, M_LCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->m_rgPiece[CASTLE_C1]);
        Assert::AreEqual((int)Rook, (int)position.get()->m_rgPiece[CASTLE_D1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[CASTLE_E1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[CASTLE_A1]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CPosition::CreateFromEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = make_move(
            e7, e8,
            static_cast<int>(static_cast<uint32_t>(Queen) << M_PROMOTION_OFFSET));
        position.get()->DoMove(move);

        Assert::AreEqual((int)Queen, (int)position.get()->m_rgPiece[e8]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[e7]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(CMoveFlagQueriesReflectConstructorFlags) {
        const CMove move = make_promotion(
            e2, e4, Queen, M_CAPTURE | M_PAWND | M_ENPASSANT | M_SCASTLE | M_LCASTLE);

        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(move.IsPawnDoublePush());
        Assert::IsTrue(move.IsEnPassant());
        Assert::IsTrue(move.IsShortCastle());
        Assert::IsTrue(move.IsLongCastle());
        Assert::IsTrue(move.IsCastle());
        Assert::IsTrue(move.HasPromotion());
        Assert::AreEqual((int)Queen, move.GetPromotionType());
        Assert::IsTrue(move.IsTactical());
    }

    TEST_METHOD(CMoveMutatorsSetAndClearFlags) {
        CMove move = make_move(a2, a3, 0);

        Assert::IsFalse(move.IsCapture());
        Assert::IsFalse(move.IsShortCastle());
        Assert::IsFalse(move.IsLongCastle());
        Assert::IsFalse(move.IsCastle());
        Assert::IsFalse(move.IsPawnDoublePush());
        Assert::IsFalse(move.IsEnPassant());
        Assert::IsFalse(move.IsTactical());

        move.SetCapture();
        move.SetShortCastle();
        move.SetLongCastle();
        move.SetPawnDoublePush();
        move.SetEnPassant();
        Assert::IsTrue(move.IsCapture());
        Assert::IsTrue(move.IsShortCastle());
        Assert::IsTrue(move.IsLongCastle());
        Assert::IsTrue(move.IsCastle());
        Assert::IsTrue(move.IsPawnDoublePush());
        Assert::IsTrue(move.IsEnPassant());
        Assert::IsTrue(move.IsTactical());

        move.SetCapture(false);
        move.SetShortCastle(false);
        move.SetLongCastle(false);
        move.SetPawnDoublePush(false);
        move.SetEnPassant(false);
        Assert::IsFalse(move.IsCapture());
        Assert::IsFalse(move.IsShortCastle());
        Assert::IsFalse(move.IsLongCastle());
        Assert::IsFalse(move.IsCastle());
        Assert::IsFalse(move.IsPawnDoublePush());
        Assert::IsFalse(move.IsEnPassant());
        Assert::IsFalse(move.IsTactical());
    }

    TEST_METHOD(CMovePromotionMutatorsControlPromotionFlags) {
        CMove move = make_move(h7, h8, 0);
        Assert::IsFalse(move.HasPromotion());
        Assert::AreEqual(0, move.GetPromotionType());
        Assert::IsFalse(move.IsTactical());

        move.SetPromotionType(Rook);
        Assert::IsTrue(move.HasPromotion());
        Assert::AreEqual((int)Rook, move.GetPromotionType());
        Assert::IsTrue(move.IsTactical());

        move.ClearPromotion();
        Assert::IsFalse(move.HasPromotion());
        Assert::AreEqual(0, move.GetPromotionType());
        Assert::IsFalse(move.IsTactical());
    }

    TEST_METHOD(CMoveEqualityAndInequalityCompareMoveBits) {
        const CMove moveA = make_move(b1, c3, M_CAPTURE);
        const CMove moveB = make_move(b1, c3, M_CAPTURE);
        const CMove moveDifferentFlags = make_move(b1, c3, 0);
        const CMove moveDifferentTo = make_move(b1, a3, M_CAPTURE);

        Assert::IsTrue(moveA == moveB);
        Assert::IsFalse(moveA != moveB);
        Assert::IsTrue(moveA != moveDifferentFlags);
        Assert::IsTrue(moveA != moveDifferentTo);
    }

    TEST_METHOD(CMoveFromToIndexMatchesSquareEncoding) {
        const CMove move = make_move(CSCoord(c2), CSCoord(g7), 0);
        const int expected = c2 + (g7 <<CBitBoard::BITBOARD_SIZE_BITS);
        Assert::AreEqual(expected, move.GetFromToIndex());
    }

    TEST_METHOD(MFromAndMToDecodeFromScooordBitfields) {
        const CSCoord fromSquare(0, 4, 1); // e2
        const CSCoord toSquare(0, 4, 3);   // e4
        const CMove move = make_move(static_cast<int>(fromSquare), static_cast<int>(toSquare), M_PAWND);

        Assert::AreEqual(fromSquare.m_nLevel, move.GetFromCoord().m_nLevel);
        Assert::AreEqual(fromSquare.m_nFile, move.GetFromCoord().m_nFile);
        Assert::AreEqual(fromSquare.m_nRank, move.GetFromCoord().m_nRank);
        Assert::AreEqual(toSquare.m_nLevel, move.GetToCoord().m_nLevel);
        Assert::AreEqual(toSquare.m_nFile, move.GetToCoord().m_nFile);
        Assert::AreEqual(toSquare.m_nRank, move.GetToCoord().m_nRank);
    }
};

} // namespace WinAmyTests
