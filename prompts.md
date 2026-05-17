Please refactor Bitboard.c as a C++ class.

1. Rename the file Bitboard.cpp, and the class name will be CBitboard
2. Create class accessor methods for all the bitboard functions in bitboard.h, including SetBit, TestBit, etc.  as well as the functions in botboard.c
3. Add static operator overrides for CBitboard for ==, !=. bitwise &, ~, |.  The operators chould be declared as static and should take too CBitboard objects as operands and return the result as a new CBitboard object.
4. There should be a cast constructor CBitboard (uint64_t )  that will create a CBitboard from a uint64_t bitboard bits.
5. Rename typedef uint64_t BitBoard to BitBoardBits, but otherwise keep that typedef for the bitboard data (which is a uint64_t)
6. Update all unit tests and other code to use CBitboard instead of uint64_t, BitBoard to BitBoardBits when possible.  In some cases that rely on direct bit manipulation, we can continue to use uint64_t directly (or the typedef BitBoardBits), but if there can be a 1 to 1 conversion to use CBitboard and/or the new CBitboard bitwise operators &, |, ~, == and !=, use those.


Please refactor Position to C++

1. Refactor the Position struct as a C++ class CPosition
2. Refactor  DoMove, UndoMove, and all other functions which either take struct Position * as a first parameter or create a new Position struct and return a pointer to it, and m,ake sure to remove the old global function versions, they should all be converted to C++ functions
3. IF necessary, also refactor move_t as a C++ class CMove.  But, if the Position struct can be refactored without also refactoring move_t, then we can do that later.
4. Rename CBitboard to CBitBoard
5. Remove the #define for SetMask, ClrMask, SetBit, ClrBit, TstBit, and remove the golbal scope functions CountBits and FindSetBit.  All those typedefs and functions should instead be using the same functions in CBitBoard
6. Update all unit tests and other code to use CPosition instead of Position when possible.


The CSCoord class was designed to convert a chess square position rank and file into a bit offset for the CBitBoard class.

1. The CSCoord Level member is for a multidimensional chessboard, for traditional 2 dimiensional chess on an 8x8 board, it will always be 0.
2. Please update any code which converts a bitboard offset too or from a rank and file to use the SCoord class to do that.
3. Please update any code whihc enumerates the rank and file in a chess board position to enumerate using a CSCoord class using the Rank and File members
4. Please add any new unit tests to verify any logic changes created by using CSCoord instead of directly using level and rank variables inline or locally.


To prepare to expand the logic to a 3D chess board, we need to include level with rank and file.  Because NUM_LEVELS is 1 and LEVEL_WIDTH[0] is 8, these changes will have no affect on the logic.  But, in future commits we will be expanding NUM_LEVELS and LEVEL_WIDTH will be different for each level.

1. In each place where we are enumerating rank and file, add an outer for loop to enumerate level from 0 to NUM_LEVELS
2. each place where we enumerate rank and file form 0 to 7, use LEVEL_WIDTH[level] as the upper limit instead of 8





Please fix all build warnings





Please SAN parsing (including parse_san_with_heap) to parse the level, file and rank for each square instead of just file and rank, such that the SAN notation for square level 1, file a, and rank 1 would be "aa1" and level 1, file g, rank 8 would be "ag8".  In this notation, the move Nac3 would be to move the Knight piece to square "c3" on level 1.  In CSCoord, level, file and rank are 0 based, so that CSCoord(0,0,0) would be level 1 or a, file a, rank 1.  The SAN notation will support levels 1-15, or a - 0

