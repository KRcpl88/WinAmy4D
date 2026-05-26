#include "ucoordFloat.h"

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

double& CUCoordFloat::operator[](uint16_t i) {
    return m_rgdData[i];
}

const double& CUCoordFloat::operator[](uint16_t i) const {
    return m_rgdData[i];
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
