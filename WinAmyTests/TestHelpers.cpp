#include "TestHelpers.h"
#include "scoord.h"

namespace WinAmyTests {

uint64_t ReferenceRookAttacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    const CSCoord sourceSquare(sq);
    const int level = sourceSquare.m_nLevel;
    const int width = CBitBoard::LEVEL_WIDTH[level];
    const int file = sourceSquare.m_nFile;
    const int rank = sourceSquare.m_nRank;

    for (int r = rank + 1; r < width; r++) {
        const int target = static_cast<int>(CSCoord(level, file, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int r = rank - 1; r >= 0; r--) {
        const int target = static_cast<int>(CSCoord(level, file, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int f = file + 1; f < width; f++) {
        const int target = static_cast<int>(CSCoord(level, f, rank));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int f = file - 1; f >= 0; f--) {
        const int target = static_cast<int>(CSCoord(level, f, rank));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    return attacks;
}

uint64_t ReferenceBishopAttacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    const CSCoord sourceSquare(sq);
    const int level = sourceSquare.m_nLevel;
    const int width = CBitBoard::LEVEL_WIDTH[level];
    const int file = sourceSquare.m_nFile;
    const int rank = sourceSquare.m_nRank;

    for (int f = file + 1, r = rank + 1; f < width && r < width; f++, r++) {
        const int target = static_cast<int>(CSCoord(level, f, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int f = file - 1, r = rank + 1; f >= 0 && r < width; f--, r++) {
        const int target = static_cast<int>(CSCoord(level, f, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int f = file + 1, r = rank - 1; f < width && r >= 0; f++, r--) {
        const int target = static_cast<int>(CSCoord(level, f, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    for (int f = file - 1, r = rank - 1; f >= 0 && r >= 0; f--, r--) {
        const int target = static_cast<int>(CSCoord(level, f, r));
        attacks |= CBitBoard::SetMask(target).GetBits();
        if (occupied & CBitBoard::SetMask(target).GetBits())
            break;
    }

    return attacks;
}

void AssertPositionsEqual(const CPosition *lhs, const CPosition *rhs) {
    for (int i = 0; i < 64; i++) {
        Assert::IsTrue(CBitBoard(lhs->m_rgAtkTo[i]) == CBitBoard(rhs->m_rgAtkTo[i]));
        Assert::IsTrue(CBitBoard(lhs->m_rgAtkFr[i]) == CBitBoard(rhs->m_rgAtkFr[i]));
        Assert::AreEqual((int)lhs->m_rgPiece[i], (int)rhs->m_rgPiece[i]);
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::IsTrue(CBitBoard(lhs->m_rgMask[c][p]) ==
                           CBitBoard(rhs->m_rgMask[c][p]));
        }

        Assert::AreEqual(lhs->m_rgnMaterial[c], rhs->m_rgnMaterial[c]);
        Assert::AreEqual(lhs->m_rgnNonPawn[c], rhs->m_rgnNonPawn[c]);
        Assert::AreEqual((int)lhs->m_rgKingSq[c], (int)rhs->m_rgKingSq[c]);
        Assert::AreEqual((int)lhs->m_rgbMaterialSignature[c],
                         (int)rhs->m_rgbMaterialSignature[c]);
    }

    Assert::IsTrue(CBitBoard(lhs->m_SlidingPieces) ==
                   CBitBoard(rhs->m_SlidingPieces));
    Assert::AreEqual((unsigned long long)lhs->m_ullHKey, (unsigned long long)rhs->m_ullHKey);
    Assert::AreEqual((unsigned long long)lhs->m_ullPKey, (unsigned long long)rhs->m_ullPKey);
    Assert::AreEqual((int)lhs->m_bCastle, (int)rhs->m_bCastle);
    Assert::AreEqual(lhs->m_EnPassant.IsValid(), rhs->m_EnPassant.IsValid());
    if (lhs->m_EnPassant.IsValid() && rhs->m_EnPassant.IsValid()) {
        Assert::AreEqual(lhs->m_EnPassant.BitOffset(), rhs->m_EnPassant.BitOffset());
    }
    Assert::AreEqual((int)lhs->m_nTurn, (int)rhs->m_nTurn);
    Assert::AreEqual((int)lhs->m_wPly, (int)rhs->m_wPly);
}

} // namespace WinAmyTests
