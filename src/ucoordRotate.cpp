#include "ucoordRotate.h"

CUCoordRotate::CUCoordRotate(const CUCoordFloat rgAxes[2], const double rgdRotation[2]) {
    m_rgAxes[0] = rgAxes[0];
    m_rgAxes[1] = rgAxes[1];
    m_rgdRotation[0] = rgdRotation[0];
    m_rgdRotation[1] = rgdRotation[1];
}


CUCoordRotate::CUCoordRotate(const CUCoordFloat& axis0, const CUCoordFloat& axis1,
                             double rotation0, double rotation1) {
    m_rgAxes[0] = axis0;
    m_rgAxes[1] = axis1;
    m_rgdRotation[0] = rotation0;
    m_rgdRotation[1] = rotation1;
}
