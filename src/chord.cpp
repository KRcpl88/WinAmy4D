#include "chord.h"

CChord::CChord(const CUCoord& start, const CUCoord& end) 
{
    m_rgData[0] = start;
    m_rgData[1] = end; 
}

CChord::CChord(const CUCoord rgData[2]) 
{
    m_rgData[0] = rgData[0];
    m_rgData[1] = rgData[1]; 
}

bool CChord::operator==(const CChord& other) const {
    return (m_rgData[0] == other.m_rgData[0]) && (m_rgData[1] == other.m_rgData[1]);
}
