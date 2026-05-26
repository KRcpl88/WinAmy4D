#include "ucoordRotate.h"

CUCoordRotate::CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                             double rotation0, double rotation1) {
    m_rgAxes[0] = axis0;
    m_rgAxes[1] = axis1;
    m_rgdwRotation[0] = rotation0;
    m_rgdwRotation[1] = rotation1;
}
