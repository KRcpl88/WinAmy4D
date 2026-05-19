#include "TestHelpers.h"
#include "scoord.h"

namespace WinAmyTests {

uint64_t ReferenceRookAttacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    const CSCoord sourceSquare(sq);
    const int level = sourceSquare.m_nLevel;
    const int width = CSCoord::LEVEL_WIDTH[level];
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
    const int width = CSCoord::LEVEL_WIDTH[level];
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
        Assert::IsTrue(CBitBoard(lhs->atkTo[i]) == CBitBoard(rhs->atkTo[i]));
        Assert::IsTrue(CBitBoard(lhs->atkFr[i]) == CBitBoard(rhs->atkFr[i]));
        Assert::AreEqual((int)lhs->piece[i], (int)rhs->piece[i]);
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::IsTrue(CBitBoard(lhs->mask[c][p]) ==
                           CBitBoard(rhs->mask[c][p]));
        }

        Assert::AreEqual(lhs->material[c], rhs->material[c]);
        Assert::AreEqual(lhs->nonPawn[c], rhs->nonPawn[c]);
        Assert::AreEqual((int)lhs->kingSq[c], (int)rhs->kingSq[c]);
        Assert::AreEqual((int)lhs->material_signature[c],
                         (int)rhs->material_signature[c]);
    }

    Assert::IsTrue(CBitBoard(lhs->slidingPieces) ==
                   CBitBoard(rhs->slidingPieces));
    Assert::AreEqual((unsigned long long)lhs->hkey, (unsigned long long)rhs->hkey);
    Assert::AreEqual((unsigned long long)lhs->pkey, (unsigned long long)rhs->pkey);
    Assert::AreEqual((int)lhs->castle, (int)rhs->castle);
    Assert::AreEqual(lhs->enPassant.IsValid(), rhs->enPassant.IsValid());
    if (lhs->enPassant.IsValid() && rhs->enPassant.IsValid()) {
        Assert::AreEqual(lhs->enPassant.BitOffset(), rhs->enPassant.BitOffset());
    }
    Assert::AreEqual((int)lhs->turn, (int)rhs->turn);
    Assert::AreEqual((int)lhs->ply, (int)rhs->ply);
}

} // namespace WinAmyTests
