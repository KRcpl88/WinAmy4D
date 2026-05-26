#include "TestHelpers.h"

#include "ucoord.h"
#include "ucoordFloat.h"

#include <cmath>

namespace WinAmyTests {

static constexpr double k_dEps = 1e-9;
static const double k_dPi = std::acos(-1.0);

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
        Assert::AreEqual(4.0, coord.getX());
        Assert::AreEqual(5.0, coord.getY());
        Assert::AreEqual(6.0, coord.getZ());
    }

    TEST_METHOD(ConstSubscriptOperatorReadsComponents) {
        const CUCoordFloat coord(7.0, 8.0, 9.0);
        Assert::AreEqual(7.0, coord[0]);
        Assert::AreEqual(8.0, coord[1]);
        Assert::AreEqual(9.0, coord[2]);
    }

    // --- Vector length ---

    TEST_METHOD(LengthOfUnitXVectorIsOne) {
        CUCoordFloat v(1.0, 0.0, 0.0);
        Assert::AreEqual(1.0, v.Length(), k_dEps);
    }

    TEST_METHOD(LengthOfKnownVector) {
        CUCoordFloat v(3.0, 4.0, 0.0);
        Assert::AreEqual(5.0, v.Length(), k_dEps);
    }

    TEST_METHOD(LengthOfZeroVectorIsZero) {
        CUCoordFloat v;
        Assert::AreEqual(0.0, v.Length(), k_dEps);
    }

    // --- Normalize ---

    TEST_METHOD(NormalizeProducesUnitVector) {
        CUCoordFloat v(3.0, 4.0, 0.0);
        CUCoordFloat n = v.Normalize();
        Assert::AreEqual(1.0, n.Length(), k_dEps);
    }

    TEST_METHOD(NormalizePreservesDirection) {
        CUCoordFloat v(0.0, 0.0, 5.0);
        CUCoordFloat n = v.Normalize();
        Assert::AreEqual(0.0, n.getX(), k_dEps);
        Assert::AreEqual(0.0, n.getY(), k_dEps);
        Assert::AreEqual(1.0, n.getZ(), k_dEps);
    }

    // --- Dot product ---

    TEST_METHOD(DotProductOfParallelVectorsEqualsLengthProduct) {
        CUCoordFloat a(2.0, 0.0, 0.0);
        CUCoordFloat b(5.0, 0.0, 0.0);
        Assert::AreEqual(10.0, a.Dot(b), k_dEps);
    }

    TEST_METHOD(DotProductOfPerpendicularVectorsIsZero) {
        CUCoordFloat a(1.0, 0.0, 0.0);
        CUCoordFloat b(0.0, 1.0, 0.0);
        Assert::AreEqual(0.0, a.Dot(b), k_dEps);
    }

    TEST_METHOD(DotProductIsCommutative) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(4.0, 5.0, 6.0);
        Assert::AreEqual(a.Dot(b), b.Dot(a), k_dEps);
    }

    // --- Cross product ---

    TEST_METHOD(CrossProductXCrossYEqualsZ) {
        CUCoordFloat x(1.0, 0.0, 0.0);
        CUCoordFloat y(0.0, 1.0, 0.0);
        CUCoordFloat z = x.Cross(y);
        Assert::AreEqual(0.0, z.getX(), k_dEps);
        Assert::AreEqual(0.0, z.getY(), k_dEps);
        Assert::AreEqual(1.0, z.getZ(), k_dEps);
    }

    TEST_METHOD(CrossProductIsAnticommutative) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(4.0, 5.0, 6.0);
        CUCoordFloat ab = a.Cross(b);
        CUCoordFloat ba = b.Cross(a);
        Assert::AreEqual(-ab.getX(), ba.getX(), k_dEps);
        Assert::AreEqual(-ab.getY(), ba.getY(), k_dEps);
        Assert::AreEqual(-ab.getZ(), ba.getZ(), k_dEps);
    }

    TEST_METHOD(CrossProductOfParallelVectorsIsZero) {
        CUCoordFloat a(1.0, 0.0, 0.0);
        CUCoordFloat b(3.0, 0.0, 0.0);
        CUCoordFloat z = a.Cross(b);
        Assert::AreEqual(0.0, z.getX(), k_dEps);
        Assert::AreEqual(0.0, z.getY(), k_dEps);
        Assert::AreEqual(0.0, z.getZ(), k_dEps);
    }

    // --- Multiplication operator (cross product) ---

    TEST_METHOD(MultiplicationOperatorMatchesCrossProduct) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(4.0, 5.0, 6.0);
        CUCoordFloat expected = a.Cross(b);
        CUCoordFloat actual = a * b;
        Assert::AreEqual(expected.getX(), actual.getX(), k_dEps);
        Assert::AreEqual(expected.getY(), actual.getY(), k_dEps);
        Assert::AreEqual(expected.getZ(), actual.getZ(), k_dEps);
    }

    // --- Scalar product ---

    TEST_METHOD(ScaleDoublesVector) {
        CUCoordFloat v(1.0, 2.0, 3.0);
        CUCoordFloat scaled = v.Scale(2.0);
        Assert::AreEqual(2.0, scaled.getX(), k_dEps);
        Assert::AreEqual(4.0, scaled.getY(), k_dEps);
        Assert::AreEqual(6.0, scaled.getZ(), k_dEps);
    }

    TEST_METHOD(ScaleByZeroGivesZeroVector) {
        CUCoordFloat v(1.0, 2.0, 3.0);
        CUCoordFloat scaled = v.Scale(0.0);
        Assert::AreEqual(0.0, scaled.getX(), k_dEps);
        Assert::AreEqual(0.0, scaled.getY(), k_dEps);
        Assert::AreEqual(0.0, scaled.getZ(), k_dEps);
    }

    TEST_METHOD(ScaleByNegativeNegatesVector) {
        CUCoordFloat v(1.0, -2.0, 3.0);
        CUCoordFloat scaled = v.Scale(-1.0);
        Assert::AreEqual(-1.0, scaled.getX(), k_dEps);
        Assert::AreEqual(2.0, scaled.getY(), k_dEps);
        Assert::AreEqual(-3.0, scaled.getZ(), k_dEps);
    }

    // --- Rotation about axis ---

    TEST_METHOD(RotateXAxisAboutZBy90DegreesGivesYAxis) {
        CUCoordFloat vX(1.0, 0.0, 0.0);
        CUCoordFloat axisZ(0.0, 0.0, 1.0);
        CUCoordFloat result = vX.Rotate(axisZ, k_dPi / 2.0);
        Assert::AreEqual(0.0, result.getX(), k_dEps);
        Assert::AreEqual(1.0, result.getY(), k_dEps);
        Assert::AreEqual(0.0, result.getZ(), k_dEps);
    }

    TEST_METHOD(RotateBy360DegreesReturnsOriginalVector) {
        CUCoordFloat v(1.0, 2.0, 0.0);
        CUCoordFloat axisZ(0.0, 0.0, 1.0);
        CUCoordFloat result = v.Rotate(axisZ, 2.0 * k_dPi);
        Assert::AreEqual(v.getX(), result.getX(), k_dEps);
        Assert::AreEqual(v.getY(), result.getY(), k_dEps);
        Assert::AreEqual(v.getZ(), result.getZ(), k_dEps);
    }

    TEST_METHOD(RotateVectorAboutItsOwnAxisIsUnchanged) {
        CUCoordFloat v(0.0, 0.0, 3.0);
        CUCoordFloat axisZ(0.0, 0.0, 1.0);
        CUCoordFloat result = v.Rotate(axisZ, k_dPi / 3.0);
        Assert::AreEqual(v.getX(), result.getX(), k_dEps);
        Assert::AreEqual(v.getY(), result.getY(), k_dEps);
        Assert::AreEqual(v.getZ(), result.getZ(), k_dEps);
    }

    TEST_METHOD(RotatePreservesVectorLength) {
        CUCoordFloat v(1.0, 2.0, 3.0);
        CUCoordFloat axis(1.0, 1.0, 0.0);
        CUCoordFloat result = v.Rotate(axis, k_dPi / 4.0);
        Assert::AreEqual(v.Length(), result.Length(), k_dEps);
    }

    // --- Angle between vectors ---

    TEST_METHOD(AngleBetweenParallelVectorsIsZero) {
        CUCoordFloat a(1.0, 0.0, 0.0);
        CUCoordFloat b(5.0, 0.0, 0.0);
        Assert::AreEqual(0.0, a.AngleTo(b), k_dEps);
    }

    TEST_METHOD(AngleBetweenPerpendicularVectorsIs90Degrees) {
        CUCoordFloat a(1.0, 0.0, 0.0);
        CUCoordFloat b(0.0, 1.0, 0.0);
        Assert::AreEqual(k_dPi / 2.0, a.AngleTo(b), k_dEps);
    }

    TEST_METHOD(AngleBetweenOppositeVectorsIs180Degrees) {
        CUCoordFloat a(1.0, 0.0, 0.0);
        CUCoordFloat b(-1.0, 0.0, 0.0);
        Assert::AreEqual(k_dPi, a.AngleTo(b), k_dEps);
    }

    TEST_METHOD(AngleBetweenIsSymmetric) {
        CUCoordFloat a(1.0, 2.0, 3.0);
        CUCoordFloat b(4.0, 5.0, 6.0);
        Assert::AreEqual(a.AngleTo(b), b.AngleTo(a), k_dEps);
    }
};

} // namespace WinAmyTests
