#include "TestHelpers.h"

namespace WinAmyTests {

uint64_t ReferenceRookAttacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    const int file = sq & 7;
    const int rank = sq >> 3;

    for (int r = rank + 1; r < 8; r++) {
        const int target = r * 8 + file;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int r = rank - 1; r >= 0; r--) {
        const int target = r * 8 + file;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file + 1; f < 8; f++) {
        const int target = rank * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1; f >= 0; f--) {
        const int target = rank * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    return attacks;
}

uint64_t ReferenceBishopAttacks(int sq, uint64_t occupied) {
    uint64_t attacks = 0;
    int file = sq & 7;
    int rank = sq >> 3;

    for (int f = file + 1, r = rank + 1; f < 8 && r < 8; f++, r++) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1, r = rank + 1; f >= 0 && r < 8; f--, r++) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file + 1, r = rank - 1; f < 8 && r >= 0; f++, r--) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    for (int f = file - 1, r = rank - 1; f >= 0 && r >= 0; f--, r--) {
        const int target = r * 8 + f;
        attacks |= SetMask(target);
        if (TstBit(occupied, target))
            break;
    }

    return attacks;
}

void AssertPositionsEqual(const Position *lhs, const Position *rhs) {
    for (int i = 0; i < 64; i++) {
        Assert::AreEqual((unsigned long long)lhs->atkTo[i],
                         (unsigned long long)rhs->atkTo[i]);
        Assert::AreEqual((unsigned long long)lhs->atkFr[i],
                         (unsigned long long)rhs->atkFr[i]);
        Assert::AreEqual((int)lhs->piece[i], (int)rhs->piece[i]);
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::AreEqual((unsigned long long)lhs->mask[c][p],
                             (unsigned long long)rhs->mask[c][p]);
        }

        Assert::AreEqual(lhs->material[c], rhs->material[c]);
        Assert::AreEqual(lhs->nonPawn[c], rhs->nonPawn[c]);
        Assert::AreEqual((int)lhs->kingSq[c], (int)rhs->kingSq[c]);
        Assert::AreEqual((int)lhs->material_signature[c],
                         (int)rhs->material_signature[c]);
    }

    Assert::AreEqual((unsigned long long)lhs->slidingPieces,
                     (unsigned long long)rhs->slidingPieces);
    Assert::AreEqual((unsigned long long)lhs->hkey, (unsigned long long)rhs->hkey);
    Assert::AreEqual((unsigned long long)lhs->pkey, (unsigned long long)rhs->pkey);
    Assert::AreEqual((int)lhs->castle, (int)rhs->castle);
    Assert::AreEqual((int)lhs->enPassant, (int)rhs->enPassant);
    Assert::AreEqual((int)lhs->turn, (int)rhs->turn);
    Assert::AreEqual((int)lhs->ply, (int)rhs->ply);
}

} // namespace WinAmyTests
