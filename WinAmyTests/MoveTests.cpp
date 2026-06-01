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

        const CMove move = MakeMainBoardMove(he2, he4, M_PAWND);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->GetPiece(MainBoardOffset(he4)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he2)));
        Assert::AreEqual((int)Black, (int)position.get()->GetTurn());
        Assert::AreEqual(1, (int)position.get()->GetPly());

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on hd6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        position.get()->DoNull();
        Assert::AreEqual((int)Black, (int)position.get()->GetTurn());
        Assert::IsFalse(position.get()->GetEnPassant().IsValid());

        position.get()->UndoNull();
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on hd5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));

        position.get()->RecalcAttacks();

        CBitBoard occupied = position.get()->GetMask(White, 0) | position.get()->GetMask(Black, 0);
        CBitBoard expectedBishopAttacks = ComputeSlidingAttacks(MainBoardCoord(hd5), Bishop, occupied);

        Assert::IsTrue(position.get()->GetAtkTo(MainBoardOffset(hd5)) == expectedBishopAttacks);
        Assert::IsTrue((position.get()->GetAtkFr(MainBoardOffset(he6)) &
                        CBitBoard::SetMask(MainBoardOffset(hd5)))
                           .IsNotEmpty());
        Assert::IsTrue((position.get()->GetAtkFr(MainBoardOffset(hc4)) &
                        CBitBoard::SetMask(MainBoardOffset(hd5)))
                           .IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(hd4, he6, M_CAPTURE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Knight, (int)position.get()->GetPiece(MainBoardOffset(he6)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(hd4)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on he5, black pawn just moved hd7-hd5, en passant on hd6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(he5, hd6, M_ENPASSANT);
        position.get()->DoMove(move);

        Assert::AreEqual((int)Pawn, (int)position.get()->GetPiece(MainBoardOffset(hd6)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he5)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(hd5)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_G1, M_SCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->GetPiece(CASTLE_G1));
        Assert::AreEqual((int)Rook, (int)position.get()->GetPiece(CASTLE_F1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_E1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_H1));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(CASTLE_E1, CASTLE_C1, M_LCASTLE);
        position.get()->DoMove(move);

        Assert::AreEqual((int)King, (int)position.get()->GetPiece(CASTLE_C1));
        Assert::AreEqual((int)Rook, (int)position.get()->GetPiece(CASTLE_D1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_E1));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(CASTLE_A1));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromLegacyMainEPD(epd));
        PositionGuard snapshot(CPosition::Clone(position.get()));

        CMove move = MakeMainBoardMove(
            he7, he8,
            static_cast<int>(static_cast<uint32_t>(Queen) << M_PROMOTION_OFFSET));
        position.get()->DoMove(move);

        Assert::AreEqual((int)Queen, (int)position.get()->GetPiece(MainBoardOffset(he8)));
        Assert::AreEqual((int)Neutral, (int)position.get()->GetPiece(MainBoardOffset(he7)));

        position.get()->UndoMove(move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(CMoveFlagQueriesReflectConstructorFlags) {
        const CMove move = MakeMainBoardPromotion(
            he2, he4, Queen, M_CAPTURE | M_PAWND | M_ENPASSANT | M_SCASTLE | M_LCASTLE);

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
        CMove move = MakeMainBoardMove(ha2, ha3, 0);

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
        CMove move = MakeMainBoardMove(hh7, hh8, 0);
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
        const CMove moveA = MakeMainBoardMove(hb1, hc3, M_CAPTURE);
        const CMove moveB = MakeMainBoardMove(hb1, hc3, M_CAPTURE);
        const CMove moveDifferentFlags = MakeMainBoardMove(hb1, hc3, 0);
        const CMove moveDifferentTo = MakeMainBoardMove(hb1, ha3, M_CAPTURE);

        Assert::IsTrue(moveA == moveB);
        Assert::IsFalse(moveA != moveB);
        Assert::IsTrue(moveA != moveDifferentFlags);
        Assert::IsTrue(moveA != moveDifferentTo);
    }

    TEST_METHOD(CMoveFromToIndexMatchesSquareEncoding) {
        const CMove move = MakeMainBoardMove(hc2, hg7, 0);
        const SFromToIndex expected(MainBoardOffset(hc2), MainBoardOffset(hg7));
        Assert::AreEqual(expected.nFromOffset, move.GetFromCoord().BitOffset());
        Assert::AreEqual(expected.nToOffset, move.GetToCoord().BitOffset());
    }

    TEST_METHOD(MFromAndMToDecodeFromScooordBitfields) {
        const CSCoord fromSquare(MAIN_LEVEL, 4, 1); // he2 on main board
        const CSCoord toSquare(MAIN_LEVEL, 4, 3);   // he4 on main board
        const CMove move = MakeMainBoardMove(static_cast<int>(fromSquare), static_cast<int>(toSquare), M_PAWND);

        Assert::AreEqual(fromSquare.m_nLevel, move.GetFromCoord().m_nLevel);
        Assert::AreEqual(fromSquare.m_nFile, move.GetFromCoord().m_nFile);
        Assert::AreEqual(fromSquare.m_nRank, move.GetFromCoord().m_nRank);
        Assert::AreEqual(toSquare.m_nLevel, move.GetToCoord().m_nLevel);
        Assert::AreEqual(toSquare.m_nFile, move.GetToCoord().m_nFile);
        Assert::AreEqual(toSquare.m_nRank, move.GetToCoord().m_nRank);
    }

    // Regression: a pawn sitting on the last rank of a narrow (non-main) level
    // must not push forward, because the square one rank ahead does not exist
    // on that level.  Previously GenFrom constructed an out-of-range CSCoord
    // (e.g. level 6 / file 0 / rank 7 on a 7-wide level), throwing
    // std::out_of_range from CSCoord::Validate.
    TEST_METHOD(GenFromPawnOnLastRankOfNarrowLevelDoesNotThrow) {
        // White pawn on level g (level 6, 7 wide) at file 0 / rank 6 (top rank).
        const char *pszEpd =
            "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|P6/7/7/7/7/7/7|"
            "4k3/8/8/8/8/8/8/4K3| w - -";
        PositionGuard position(CPosition::CreateFromEPD(pszEpd));

        const CSCoord pawnCoord(6, 0, 6);
        Assert::AreEqual((int)Pawn,
                         (int)position.get()->GetPiece(pawnCoord.BitOffset()));

        // Generating all pseudo-legal moves must not throw and must not produce
        // any move originating from the stranded pawn.
        heap_t heap = allocate_heap();
        position.get()->PLegalMoves(heap);

        for (unsigned int i = heap->current_section->start;
             i < heap->current_section->end; i++) {
            Assert::AreNotEqual(pawnCoord.BitOffset(),
                                heap->data[i].GetFromCoord().BitOffset(),
                                L"Pawn on last rank of narrow level must not move forward");
        }
        free_heap(heap);
    }
};

} // namespace WinAmyTests
