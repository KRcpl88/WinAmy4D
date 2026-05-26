#include "ucoordFloat.h"

#include <cmath>

CUCoordFloat::CUCoordFloat(double dX, double dY, double dZ) {
    SetX(dX);
    SetY(dY);
    SetZ(dZ);
}

CUCoordFloat::CUCoordFloat(double rgdData[3]) {
    SetX(rgdData[0]);
    SetY(rgdData[1]);
    SetZ(rgdData[2]);
}

CUCoordFloat::CUCoordFloat(const CUCoord& coord) {
    SetX(static_cast<double>(coord.GetX()));
    SetY(static_cast<double>(coord.GetY()));
    SetZ(static_cast<double>(coord.GetZ()));
}

double CUCoordFloat::GetX() const {
    return m_rgdData[0];
}

void CUCoordFloat::SetX(double dValue) {
    m_rgdData[0] = dValue;
}

double CUCoordFloat::GetY() const {
    return m_rgdData[1];
}

void CUCoordFloat::SetY(double dValue) {
    m_rgdData[1] = dValue;
}

double CUCoordFloat::GetZ() const {
    return m_rgdData[2];
}

void CUCoordFloat::SetZ(double dValue) {
    m_rgdData[2] = dValue;
}

double& CUCoordFloat::operator[](int nIndex) {
    return m_rgdData[nIndex];
}

const double& CUCoordFloat::operator[](int nIndex) const {
    return m_rgdData[nIndex];
}

bool CUCoordFloat::operator==(const CUCoordFloat& other) const {
    if (m_rgdData[0] != other.m_rgdData[0]) {
        return false;
    }
    if (m_rgdData[1] != other.m_rgdData[1]) {
        return false;
    }
    if (m_rgdData[2] != other.m_rgdData[2]) {
        return false;
    }
    return true;
}

CUCoordFloat CUCoordFloat::operator+(const CUCoordFloat& other) const {
    CUCoordFloat result;
    result.m_rgdData[0] = m_rgdData[0] + other.m_rgdData[0];
    result.m_rgdData[1] = m_rgdData[1] + other.m_rgdData[1];
    result.m_rgdData[2] = m_rgdData[2] + other.m_rgdData[2];
    return result;
}

CUCoordFloat CUCoordFloat::operator*(const CUCoordFloat& other) const {
    return Cross(other);
}

double CUCoordFloat::Length() const {
    return std::sqrt(m_rgdData[0] * m_rgdData[0] + m_rgdData[1] * m_rgdData[1] +
                     m_rgdData[2] * m_rgdData[2]);
}

CUCoordFloat CUCoordFloat::Normalize() const {
    double dLen = Length();
    return CUCoordFloat(m_rgdData[0] / dLen, m_rgdData[1] / dLen, m_rgdData[2] / dLen);
}

double CUCoordFloat::Dot(const CUCoordFloat& other) const {
    return m_rgdData[0] * other.m_rgdData[0] + m_rgdData[1] * other.m_rgdData[1] +
           m_rgdData[2] * other.m_rgdData[2];
}

CUCoordFloat CUCoordFloat::Cross(const CUCoordFloat& other) const {
    return CUCoordFloat(m_rgdData[1] * other.m_rgdData[2] - m_rgdData[2] * other.m_rgdData[1],
                        m_rgdData[2] * other.m_rgdData[0] - m_rgdData[0] * other.m_rgdData[2],
                        m_rgdData[0] * other.m_rgdData[1] - m_rgdData[1] * other.m_rgdData[0]);
}

CUCoordFloat CUCoordFloat::Scale(double dScalar) const {
    return CUCoordFloat(m_rgdData[0] * dScalar, m_rgdData[1] * dScalar, m_rgdData[2] * dScalar);
}

// Rodrigues' rotation formula: rotates this vector about axisVec by dAngleRad radians.
CUCoordFloat CUCoordFloat::Rotate(const CUCoordFloat& axisVec, double dAngleRad) const {
    CUCoordFloat unit = axisVec.Normalize();
    double dCos = std::cos(dAngleRad);
    double dSin = std::sin(dAngleRad);
    // v*cos + (k x v)*sin + k*(k.v)*(1-cos)
    CUCoordFloat vCos = Scale(dCos);
    CUCoordFloat vCross = unit.Cross(*this).Scale(dSin);
    CUCoordFloat vDot = unit.Scale(unit.Dot(*this) * (1.0 - dCos));
    return CUCoordFloat(vCos.m_rgdData[0] + vCross.m_rgdData[0] + vDot.m_rgdData[0],
                        vCos.m_rgdData[1] + vCross.m_rgdData[1] + vDot.m_rgdData[1],
                        vCos.m_rgdData[2] + vCross.m_rgdData[2] + vDot.m_rgdData[2]);
}

double CUCoordFloat::AngleTo(const CUCoordFloat& other) const {
    double dDot = Dot(other);
    double dLenProd = Length() * other.Length();
    // Clamp to [-1, 1] to guard against floating-point rounding beyond unit range
    double dCosAngle = dDot / dLenProd;
    if (dCosAngle > 1.0) {
        dCosAngle = 1.0;
    } else if (dCosAngle < -1.0) {
        dCosAngle = -1.0;
    }
    return std::acos(dCosAngle);
}
