#pragma once

#include "CppUnitTest.h"

#include "bitboard.h"
#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "magic.h"
#include "movedata.h"

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace WinAmyTests {

class PositionGuard {
  public:
    explicit PositionGuard(CPosition *position) : p(position) {}
    ~PositionGuard() { CPosition::Free(p); }
    CPosition *get() const { return p; }

  private:
    CPosition *p;
};

uint64_t ReferenceRookAttacks(int sq, uint64_t occupied);
uint64_t ReferenceBishopAttacks(int sq, uint64_t occupied);
void AssertPositionsEqual(const CPosition *lhs, const CPosition *rhs);

} // namespace WinAmyTests
