#include "ucoordFloat.h"

CUCoordFloat::CUCoordFloat(double x, double y, double z) {
    setX(x);
    setY(y);
    setZ(z);
}

CUCoordFloat::CUCoordFloat(const CUCoord& coord) {
    setX(static_cast<double>(coord.getX()));
    setY(static_cast<double>(coord.getY()));
    setZ(static_cast<double>(coord.getZ()));
}

double CUCoordFloat::getX() const {
    return m_rgdData[0];
}

void CUCoordFloat::setX(double value) {
    m_rgdData[0] = value;
}

double CUCoordFloat::getY() const {
    return m_rgdData[1];
}

void CUCoordFloat::setY(double value) {
    m_rgdData[1] = value;
}

double CUCoordFloat::getZ() const {
    return m_rgdData[2];
}

void CUCoordFloat::setZ(double value) {
    m_rgdData[2] = value;
}

double& CUCoordFloat::operator[](int index) {
    return m_rgdData[index];
}

const double& CUCoordFloat::operator[](int index) const {
    return m_rgdData[index];
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
