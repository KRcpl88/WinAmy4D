#include "TestHelpers.h"

#include "ucoord.h"
#include "ucoordFloat.h"

namespace WinAmyTests {

TEST_CLASS(UCoordFloatTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordFloat coord;
        Assert::AreEqual(0.0, coord.getX());
        Assert::AreEqual(0.0, coord.getY());
        Assert::AreEqual(0.0, coord.getZ());
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CUCoordFloat coord(1.5, 2.5, 3.5);
        Assert::AreEqual(1.5, coord.getX());
        Assert::AreEqual(2.5, coord.getY());
        Assert::AreEqual(3.5, coord.getZ());
    }

    TEST_METHOD(CopyConstructorFromCUCoordSetsFields) {
        CUCoord icoord(2, 5, 1);
        CUCoordFloat fcoord(icoord);
        Assert::AreEqual(2.0, fcoord.getX());
        Assert::AreEqual(5.0, fcoord.getY());
        Assert::AreEqual(1.0, fcoord.getZ());
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat original(1.1, 2.2, 3.3);
        CUCoordFloat copy(original);
        Assert::AreEqual(original.getX(), copy.getX());
        Assert::AreEqual(original.getY(), copy.getY());
        Assert::AreEqual(original.getZ(), copy.getZ());
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b;
        b = a;
        Assert::AreEqual(1.0, b.getX());
        Assert::AreEqual(2.0, b.getY());
        Assert::AreEqual(3.0, b.getZ());
    }

    TEST_METHOD(EqualityOperatorReturnsTrueForIdentical) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(1.0, 2.0, 3.0);
        Assert::IsTrue(a == b);
    }

    TEST_METHOD(EqualityOperatorReturnsFalseForDifferent) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(1.0, 2.0, 4.0);
        Assert::IsFalse(a == b);
    }

    TEST_METHOD(AdditionOperatorAddsComponentWise) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(0.5, 1.5, 2.5);
        CUCoordFloat result = a + b;
        Assert::AreEqual(1.5, result.getX());
        Assert::AreEqual(3.5, result.getY());
        Assert::AreEqual(5.5, result.getZ());
    }
};

} // namespace WinAmyTests
