#pragma once

#include "CppUnitTest.h"

extern "C" {
#include "bitboard.h"
#include "dbase.h"
#include "hashtable.h"
#include "init.h"
#include "inline.h"
#include "magic.h"
#include "movedata.h"
}

#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace WinAmyTests {

class PositionGuard {
  public:
    explicit PositionGuard(Position *position) : p(position) {}
    ~PositionGuard() { FreePosition(p); }
    Position *get() const { return p; }

  private:
    Position *p;
};

uint64_t ReferenceRookAttacks(int sq, uint64_t occupied);
uint64_t ReferenceBishopAttacks(int sq, uint64_t occupied);
void AssertPositionsEqual(const Position *lhs, const Position *rhs);

} // namespace WinAmyTests
