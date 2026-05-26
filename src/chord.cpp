#include "chord.h"

CChord::CChord(const CUCoord& start, const CUCoord& end) : m_Start(start), m_End(end) {}

bool CChord::operator==(const CChord& other) const {
    return (m_Start == other.m_Start) && (m_End == other.m_End);
}
