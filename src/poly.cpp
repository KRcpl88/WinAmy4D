#include "poly.h"

CPoly::CPoly(const CUCoordFloat& p0, const CUCoordFloat& p1, const CUCoordFloat& p2) {
    m_rgPoints[0] = p0;
    m_rgPoints[1] = p1;
    m_rgPoints[2] = p2;
}

bool CPoly::operator==(const CPoly& other) const {
    if (!(m_rgPoints[0] == other.m_rgPoints[0])) {
        return false;
    }
    if (!(m_rgPoints[1] == other.m_rgPoints[1])) {
        return false;
    }
    if (!(m_rgPoints[2] == other.m_rgPoints[2])) {
        return false;
    }
    return true;
}
