#include "poly.h"

CPoly::CPoly(const CUCoordFloat& p0, const CUCoordFloat& p1, const CUCoordFloat& p2) {
    m_rfPoints[0] = p0;
    m_rfPoints[1] = p1;
    m_rfPoints[2] = p2;
}

bool CPoly::operator==(const CPoly& other) const {
    if (!(m_rfPoints[0] == other.m_rfPoints[0])) {
        return false;
    }
    if (!(m_rfPoints[1] == other.m_rfPoints[1])) {
        return false;
    }
    if (!(m_rfPoints[2] == other.m_rfPoints[2])) {
        return false;
    }
    return true;
}
