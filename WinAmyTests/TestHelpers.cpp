#include "TestHelpers.h"
#include "scoord.h"

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
        Assert::IsTrue(lhs->m_rgAtkTo[i] == rhs->m_rgAtkTo[i]);
        Assert::IsTrue(lhs->m_rgAtkFr[i] == rhs->m_rgAtkFr[i]);
        Assert::AreEqual((int)lhs->m_rgPiece[i], (int)rhs->m_rgPiece[i]);
    }

    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 7; p++) {
            Assert::IsTrue(lhs->m_rgMask[c][p] == rhs->m_rgMask[c][p]);
        }

        Assert::AreEqual(lhs->m_rgnMaterial[c], rhs->m_rgnMaterial[c]);
        Assert::AreEqual(lhs->m_rgnNonPawn[c], rhs->m_rgnNonPawn[c]);
        Assert::AreEqual((int)lhs->m_rgKingSq[c], (int)rhs->m_rgKingSq[c]);
        Assert::AreEqual((int)lhs->m_rgbMaterialSignature[c],
                         (int)rhs->m_rgbMaterialSignature[c]);
    }

    Assert::IsTrue(lhs->m_SlidingPieces == rhs->m_SlidingPieces);
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
