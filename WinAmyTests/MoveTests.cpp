#include "TestHelpers.h"

namespace WinAmyTests {

TEST_CLASS(MoveTests) {
  public:
    TEST_CLASS_INITIALIZE(InitializeEngine) {
        InitMoves();
        InitAll();
        HashInit();
    }

    TEST_METHOD(DoMoveAndUndoMoveRestorePosition) {
        PositionGuard position(CPosition::Initial());
        PositionGuard snapshot(CPosition::Clone(position.get()));

        const CMove move = MakeMainBoardMove(e2, e4, M_PAWND);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->m_rgPiece[MainBoardOffset(e4)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(e2)]);
        Assert::AreEqual((int)Black, (int)position.get()->m_nTurn);
        Assert::AreEqual(1, (int)position.get()->m_wPly);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on d6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
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
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        for (unsigned int i = 0; i < CBitBoard::SIZE; i++) {
            position.get()->m_rgAtkTo[i] = {};
            position.get()->m_rgAtkFr[i] = {};
        }

        position.get()->RecalcAttacks();

        CBitBoard occupied = position.get()->m_rgMask[White][0] | position.get()->m_rgMask[Black][0];
        CBitBoard expectedBishopAttacks = ComputeSlidingAttacks(MainBoardCoord(d5), Bishop, occupied);

        Assert::IsTrue(position.get()->m_rgAtkTo[MainBoardOffset(d5)] == expectedBishopAttacks);
        Assert::IsTrue((position.get()->m_rgAtkFr[MainBoardOffset(e6)] &
                        CBitBoard::SetMask(MainBoardOffset(d5)))
                           .IsNotEmpty());
        Assert::IsTrue((position.get()->m_rgAtkFr[MainBoardOffset(c4)] &
                        CBitBoard::SetMask(MainBoardOffset(d5)))
                           .IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(d4, e6, M_CAPTURE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Knight, (int)position.get()->m_rgPiece[MainBoardOffset(e6)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(d4)]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on e5, black pawn just moved d7-d5, en passant on d6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(e5, d6, M_ENPASSANT);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->m_rgPiece[MainBoardOffset(d6)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(e5)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(d5)]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
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
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_C1, M_LCASTLE);
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
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(
            e7, e8,
            static_cast<int>(static_cast<uint32_t>(Queen) << M_PROMOTION_OFFSET));
        position.get()->DoMove(move);

        Assert::AreEqual((int)Queen, (int)position.get()->m_rgPiece[MainBoardOffset(e8)]);
        Assert::AreEqual((int)Neutral, (int)position.get()->m_rgPiece[MainBoardOffset(e7)]);

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(CMoveFlagQueriesReflectConstructorFlags) {
        const CMove move = MakeMainBoardPromotion(
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
        CMove move = MakeMainBoardMove(a2, a3, 0);

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
        CMove move = MakeMainBoardMove(h7, h8, 0);
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
        const CMove moveA = MakeMainBoardMove(b1, c3, M_CAPTURE);
        const CMove moveB = MakeMainBoardMove(b1, c3, M_CAPTURE);
        const CMove moveDifferentFlags = MakeMainBoardMove(b1, c3, 0);
        const CMove moveDifferentTo = MakeMainBoardMove(b1, a3, M_CAPTURE);

        Assert::IsTrue(moveA == moveB);
        Assert::IsFalse(moveA != moveB);
        Assert::IsTrue(moveA != moveDifferentFlags);
        Assert::IsTrue(moveA != moveDifferentTo);
    }

    TEST_METHOD(CMoveFromToIndexMatchesSquareEncoding) {
        const CMove move = MakeMainBoardMove(c2, g7, 0);
        const SFromToIndex expected(MainBoardOffset(c2), MainBoardOffset(g7));
        Assert::AreEqual(expected.nFromOffset, move.GetFromCoord().BitOffset());
        Assert::AreEqual(expected.nToOffset, move.GetToCoord().BitOffset());
    }

    TEST_METHOD(MFromAndMToDecodeFromScooordBitfields) {
        const CSCoord fromSquare(MAIN_LEVEL, 4, 1); // e2 on main board
        const CSCoord toSquare(MAIN_LEVEL, 4, 3);   // e4 on main board
        const CMove move = MakeMainBoardMove(static_cast<int>(fromSquare), static_cast<int>(toSquare), M_PAWND);

        Assert::AreEqual(fromSquare.m_nLevel, move.GetFromCoord().m_nLevel);
        Assert::AreEqual(fromSquare.m_nFile, move.GetFromCoord().m_nFile);
        Assert::AreEqual(fromSquare.m_nRank, move.GetFromCoord().m_nRank);
        Assert::AreEqual(toSquare.m_nLevel, move.GetToCoord().m_nLevel);
        Assert::AreEqual(toSquare.m_nFile, move.GetToCoord().m_nFile);
        Assert::AreEqual(toSquare.m_nRank, move.GetToCoord().m_nRank);
    }
};

} // namespace WinAmyTests
