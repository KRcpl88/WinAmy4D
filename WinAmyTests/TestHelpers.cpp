#include "TestHelpers.h"
#include "scoord.h"

#include <sstream>

namespace WinAmyTests {

CBitBoard ReferenceRookAttacks(int sq, CBitBoard occupied) {
    CBitBoard attacks;
    const CSCoord sourceSquare(static_cast<uint16_t>(sq));
    for (int d = 0; d < ATTACK_DELTA_COUNT[Rook]; d++) {
        CSCoord target = sourceSquare.Step(ATTACK_DELTA[Rook][d]);
        while (target.IsValid()) {
            attacks.SetBit(target.BitOffset());
            if (occupied.TstBit(target.BitOffset())) {
                break;
            }
            target = target.Step(ATTACK_DELTA[Rook][d]);
        }
    }

    return attacks;
}

CBitBoard ReferenceBishopAttacks(int sq, CBitBoard occupied) {
    CBitBoard attacks;
    const CSCoord sourceSquare(static_cast<uint16_t>(sq));
    for (int d = 0; d < ATTACK_DELTA_COUNT[Bishop]; d++) {
        CSCoord target = sourceSquare.Step(ATTACK_DELTA[Bishop][d]);
        while (target.IsValid()) {
            attacks.SetBit(target.BitOffset());
            if (occupied.TstBit(target.BitOffset())) {
                break;
            }
            target = target.Step(ATTACK_DELTA[Bishop][d]);
        }
    }

    return attacks;
}

void AssertPositionsEqual(const CPosition *lhs, const CPosition *rhs) {
    for (unsigned int i = 0; i < CBitBoard::SIZE; i++) {
        Assert::IsTrue(lhs->GetAtkTo(i) == rhs->GetAtkTo(i));
        Assert::IsTrue(lhs->GetAtkFr(i) == rhs->GetAtkFr(i));
        Assert::AreEqual((int)lhs->GetPiece(i), (int)rhs->GetPiece(i));
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::IsTrue(lhs->GetMask(c, p) == rhs->GetMask(c, p));
        }

        Assert::AreEqual(lhs->GetMaterial(c), rhs->GetMaterial(c));
        Assert::AreEqual(lhs->GetNonPawn(c), rhs->GetNonPawn(c));
        Assert::AreEqual((int)lhs->GetKingSq(c), (int)rhs->GetKingSq(c));
        Assert::AreEqual((int)lhs->GetMaterialSignature(c),
                         (int)rhs->GetMaterialSignature(c));
    }

    Assert::IsTrue(lhs->GetSlidingPieces() == rhs->GetSlidingPieces());
    Assert::AreEqual((unsigned long long)lhs->GetHashKey(), (unsigned long long)rhs->GetHashKey());
    Assert::AreEqual((unsigned long long)lhs->GetPawnKey(), (unsigned long long)rhs->GetPawnKey());
    Assert::AreEqual((int)lhs->GetCastle(), (int)rhs->GetCastle());
    Assert::AreEqual(lhs->GetEnPassant().IsValid(), rhs->GetEnPassant().IsValid());
    if (lhs->GetEnPassant().IsValid() && rhs->GetEnPassant().IsValid()) {
        Assert::AreEqual(lhs->GetEnPassant().BitOffset(), rhs->GetEnPassant().BitOffset());
    }
    Assert::AreEqual((int)lhs->GetTurn(), (int)rhs->GetTurn());
    Assert::AreEqual((int)lhs->GetPly(), (int)rhs->GetPly());
}

uint16_t MainBoardOffset(int square) {
    if (square >= 0 && square < 64) {
        const int file = square % 8;
        const int rank = square / 8;
        return static_cast<uint16_t>(CSCoord(MAIN_LEVEL, file, rank).BitOffset());
    }
    return static_cast<uint16_t>(square);
}

CSCoord MainBoardCoord(int square) {
    return CSCoord(MainBoardOffset(square));
}

CMove MakeMainBoardMove(int from, int to, int flags) {
    return make_move(MainBoardCoord(from), MainBoardCoord(to), flags);
}

CMove MakeMainBoardPromotion(int from, int to, int promotionType, int flags) {
    return make_promotion(MainBoardCoord(from), MainBoardCoord(to), promotionType, flags);
}

std::string BuildMainBoardEPD(const std::string &mainBoardPlacement, const std::string &sideToMove,
                              const std::string &castleRights, const std::string &enPassant) {
    static const char *kLevelPrefix =
        "1|2/2|3/3/3|4/4/4/4|5/5/5/5/5|6/6/6/6/6/6|7/7/7/7/7/7/7|";
    return std::string(kLevelPrefix) + mainBoardPlacement + " " + sideToMove + " " + castleRights +
           " " + enPassant;
}

CPosition *CreatePositionFromLegacyMainEPD(const char *legacyMainBoardEpd) {
    std::istringstream parser(legacyMainBoardEpd ? legacyMainBoardEpd : "");
    std::string board;
    std::string side = "w";
    std::string castle = "-";
    std::string ep = "-";
    parser >> board >> side >> castle >> ep;
    const bool hasEnPassant = ep.size() == 2 && ep != "-";
    std::string epd = BuildMainBoardEPD(board, side, castle, hasEnPassant ? "-" : ep);
    CPosition *position = CPosition::CreateFromEPD(epd.c_str());
    if (hasEnPassant) {
        const int file = ep[0] - 'a';
        const int rank = ep[1] - '1';
        position->SetEnPassant(CSCoord(MAIN_LEVEL, file, rank));
    }
    return position;
}

} // namespace WinAmyTests
