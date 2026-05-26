#include "TestHelpers.h"

#include "ucoord.h"
#include "ucoordFloat.h"

namespace WinAmyTests {

TEST_CLASS(UCoordFloatTests) {
  public:
    TEST_METHOD(DefaultConstructorInitializesToZero) {
        CUCoordFloat coord;
        Assert::AreEqual(0.0, coord.GetX());
        Assert::AreEqual(0.0, coord.GetY());
        Assert::AreEqual(0.0, coord.GetZ());
    }

    TEST_METHOD(ThreeArgConstructorSetsFields) {
        CUCoordFloat coord(1.5, 2.5, 3.5);
        Assert::AreEqual(1.5, coord.GetX());
        Assert::AreEqual(2.5, coord.GetY());
        Assert::AreEqual(3.5, coord.GetZ());
    }

    TEST_METHOD(ArrayArgConstructorSetsFields) {
        CUCoordFloat rgCoord[2] = {{1.5, 2.5, 3.5}, {0.5, 1.5, 2.5}};
        Assert::AreEqual(1.5, rgCoord[0].GetX());
        Assert::AreEqual(2.5, rgCoord[0].GetY());
        Assert::AreEqual(3.5, rgCoord[0].GetZ());

        Assert::AreEqual(0.5, rgCoord[1].GetX());
        Assert::AreEqual(1.5, rgCoord[1].GetY());
        Assert::AreEqual(2.5, rgCoord[1].GetZ());        
    }

    TEST_METHOD(CopyConstructorFromCUCoordSetsFields) {
        CUCoord icoord(2, 5, 1);
        CUCoordFloat fcoord(icoord);
        Assert::AreEqual(2.0, fcoord.GetX());
        Assert::AreEqual(5.0, fcoord.GetY());
        Assert::AreEqual(1.0, fcoord.GetZ());
    }

    TEST_METHOD(CopyConstructorCopiesAllFields) {
        CUCoordFloat original(1.1, 2.2, 3.3);
        CUCoordFloat copy(original);
        Assert::AreEqual(original.GetX(), copy.GetX());
        Assert::AreEqual(original.GetY(), copy.GetY());
        Assert::AreEqual(original.GetZ(), copy.GetZ());
    }

    TEST_METHOD(AssignmentOperatorCopiesAllFields) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b;
        b = a;
        Assert::AreEqual(1.0, b.GetX());
        Assert::AreEqual(2.0, b.GetY());
        Assert::AreEqual(3.0, b.GetZ());
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
        Assert::AreEqual(1.5, result.GetX());
        Assert::AreEqual(3.5, result.GetY());
        Assert::AreEqual(5.5, result.GetZ());
    }

    TEST_METHOD(SubscriptOperatorReadsComponents) {
        CUCoordFloat coord(1.0, 2.0, 3.0);
        Assert::AreEqual(1.0, coord[0]);
        Assert::AreEqual(2.0, coord[1]);
        Assert::AreEqual(3.0, coord[2]);
    }

    TEST_METHOD(SubscriptOperatorWritesComponents) {
        CUCoordFloat coord;
        coord[0] = 4.0;
        coord[1] = 5.0;
        coord[2] = 6.0;
        Assert::AreEqual(4.0, coord.GetX());
        Assert::AreEqual(5.0, coord.GetY());
        Assert::AreEqual(6.0, coord.GetZ());
    }

    TEST_METHOD(ConstSubscriptOperatorReadsComponents) {
        const CUCoordFloat coord(7.0, 8.0, 9.0);
        Assert::AreEqual(7.0, coord[0]);
        Assert::AreEqual(8.0, coord[1]);
        Assert::AreEqual(9.0, coord[2]);
    }
};

} // namespace WinAmyTests
