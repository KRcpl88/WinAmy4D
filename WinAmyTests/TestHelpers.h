#pragma once

#include "CppUnitTest.h"

#include "bitboard.h"
#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "movedata.h"
#include "search.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace WinAmyTests {

class PositionGuard {
  public:
    explicit PositionGuard(CPosition *position) : m_pPosition(position) {}
    ~PositionGuard() { CPosition::Free(m_pPosition); }
    CPosition *get() const { return m_pPosition; }

  private:
    CPosition *m_pPosition;
};

CBitBoard ReferenceRookAttacks(int sq, CBitBoard occupied);
CBitBoard ReferenceBishopAttacks(int sq, CBitBoard occupied);
void AssertPositionsEqual(const CPosition *lhs, const CPosition *rhs);

} // namespace WinAmyTests
