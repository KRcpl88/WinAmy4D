#define  _CRT_SECURE_NO_WARNINGS

#define NEW
#define XX 128

#define C_PIECES 3

#define SqFindKing(psq) (psq[C_PIECES * (x_pieceKing - 1)])
#define SqFindOne(psq, p) (psq[C_PIECES * (p - 1)])
#define SqFindFirst(psq, p) (psq[C_PIECES * (p - 1)])
#define SqFindSecond(psq, p) (psq[C_PIECES * (p - 1) + 1])
#define SqFindThird(psq, p) (psq[C_PIECES * (p - 1) + 2])

typedef int square;
#define TBINDEX_SQUARE_DECLARED

typedef unsigned int INDEX;
#define TBINDEX_INDEX_DECLARED

#include "tbindex.cpp"
