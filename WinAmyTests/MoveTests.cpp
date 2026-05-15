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
        PositionGuard position(InitialPosition());
        PositionGuard snapshot(ClonePosition(position.get()));

        const move_t move = make_move(e2, e4, M_PAWND);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[e4]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e2]);
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(1, (int)position.get()->ply);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoNullAndUndoNullRestorePositionFields) {
        // Position with en passant available on d6.
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        DoNull(position.get());
        Assert::AreEqual((int)Black, (int)position.get()->turn);
        Assert::AreEqual(0, (int)position.get()->enPassant);

        UndoNull(position.get());
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(RecalcAttacksRebuildsAtkSetDerivedData) {
        // Position with a white bishop on d5.
        char epd[] = "4k3/8/8/3B4/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));

        for (int i = 0; i < 64; i++) {
            position.get()->atkTo[i] = 0;
            position.get()->atkFr[i] = 0;
        }

        RecalcAttacks(position.get());

        const BitBoardBits occupied = position.get()->mask[White][0] | position.get()->mask[Black][0];
        CBitboard expectedBishopAttacks(bishop_attacks(d5, occupied));

        Assert::IsTrue(CBitboard(position.get()->atkTo[d5]) == expectedBishopAttacks);
        Assert::IsTrue((CBitboard(position.get()->atkFr[e6]) & CBitboard(SetMask(d5))).IsNotEmpty());
        Assert::IsTrue((CBitboard(position.get()->atkFr[c4]) & CBitboard(SetMask(d5))).IsNotEmpty());
    }

    TEST_METHOD(DoMoveCaptureRemovesCapturedPiece) {
        // White knight captures black pawn
        char epd[] = "4k3/8/4p3/8/3N4/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(d4, e6, M_CAPTURE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Knight, (int)position.get()->piece[e6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d4]);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveEnPassantCapturesCorrectly) {
        // White pawn on e5, black pawn just moved d7-d5, en passant on d6
        char epd[] = "4k3/8/8/3pP3/8/8/8/4K3 w - d6";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e5, d6, M_ENPASSANT);
        DoMove(position.get(), move);

        Assert::AreEqual((int)Pawn, (int)position.get()->piece[d6]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e5]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[d5]);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveShortCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/4K2R w K -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e1, g1, M_SCASTLE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)King, (int)position.get()->piece[g1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[f1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[h1]);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMoveLongCastleMovesKingAndRook) {
        char epd[] = "4k3/8/8/8/8/8/8/R3K3 w Q -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e1, c1, M_LCASTLE);
        DoMove(position.get(), move);

        Assert::AreEqual((int)King, (int)position.get()->piece[c1]);
        Assert::AreEqual((int)Rook, (int)position.get()->piece[d1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e1]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[a1]);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }

    TEST_METHOD(DoMovePromotionChangesType) {
        char epd[] = "7k/4P3/8/8/8/8/8/4K3 w - -";
        PositionGuard position(CreatePositionFromEPD(epd));
        PositionGuard snapshot(ClonePosition(position.get()));

        move_t move = make_move(e7, e8, (Queen << M_PROMOTION_OFFSET));
        DoMove(position.get(), move);

        Assert::AreEqual((int)Queen, (int)position.get()->piece[e8]);
        Assert::AreEqual((int)Neutral, (int)position.get()->piece[e7]);

        UndoMove(position.get(), move);
        AssertPositionsEqual(position.get(), snapshot.get());
    }
};

} // namespace WinAmyTests
