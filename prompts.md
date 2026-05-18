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



move_t is defined as a 16 bit bitfield with the rank and file of a from and to square.  We need to expand this to accomodate a level, file and rank.  

1. Modify the move_t typedef to be a 32 bit DWORD
2. Define a new typedef scoord_bitfield_t which is a 16 bit WORD bitfield contsining s level, file and rank.  bits 0-3 are the file, bits 4-7 are the rank, and bits 8-11 are the level
3. Add a new CSCoord contructor whihc takes a scoord_bitfield_t and intiialized LEvel, Rank and File from the bitfield
4. Add a new const CSCoord helper function GetBitField which returns a scoord_bitfield_t bitfield
5. Modify M_FROM to construct a CSCoord from the low order 16 bit scoord_bitfield_t of the move_t type, an M_TO to use the high order scoord_bitfield_t




The original C implementation of Amy chess used a 64 bit int bitboard as a bit representation of a chess board, which relied on the specific geometry of the chessboard being exactly 8x8 squares, whcih is exactly 64 squares and fits in a single 64 bit int as a bitfield.  The original impmentation also made extensive use of convierting a square's file and rank position into a bit offest, and assumed the low order 3 bits of the offset were the file and the high order was the rank.  However, the goal of WinAmy4D is to expand this chessboard into 3 dimensions, with many more than the original 64 sqaures.  So all the assumptions of the bit offests and the rank and file of the chessboard being in certain bit positions in the bit offset are all invalid.  This is why the CSCoord class has been introduced, to work with the rank, file, and level position of each square artihemtically instead of making assumptions about the relationship of the rank and file to the bit offsets.  In order to fix this, we need to find every place in the code where the original code makes assumptions about the rank and file in the bit offset ansd instead convert the bit offset into a CSCoord object and then use level, rank and file of the CSCoord instead of meaking any assumptions about the bit offset.

1. Anyplace we use a bitwise mask operation & 0x7 against a bit offset to obtaain the file position of a chess square, use CSCoord instead to convert the bit offset to a CSCoord and compute the rank using CSCoord.File
2. Anyplace we use a bitwise shift operation >> 3 to obtaain the rank position of a chess square, use CSCoord instead to convert the bit offset to a CSCoord and compute the rank using CSCoord.Rank
3. Any place we are using a bit offset and making assumptions about which bits in the bit offset encode the rank or file of an actual chess square on the board, use CSCoord instead.
4. Avoid using BitOffset, GetBitOffset, GetFromBitOffset or GetToBitOffset, or converting a CSCoord to or from an int bit offset, if the code is using the bit offset to enumerate, manipulate or compute the file and rank location of a square on a chessboard, convert the code to using a CSCoord to convert the bit offset into a rank and file location of the square.
5. If after these changes, BitOffset, GetBitOffset, GetFromBitOffset or GetToBitOffset are not longer used, remove them from the code.
6. GetBitOffset returns an int, not a int8_t or uint8_t.  Converstion from int to an 8 bit interger type will cause data loss from trunctation.  Anyplace we are storing a bit offset as an 8 bit interger type, make sure the interger is at list 16 bits of more.
7. BitOffset and GetBitOffset are redundant functions, remove GetBitOffset and just use BitOffset, if it is still needed, but only if the bit offset is NOT being used to measure the rank or file location of a square on the chess board.
8. In many places we convert a suare position to an offset for the prupsoe or enumerating squares, this is OK as long as we convert back to a CSCoord to compute the rank and file location of the chess square.  However, the chessboard square or bit offset must be stored at least as 16 bits, because the final 3D chess board will have 344 squares and will require bit offsets > 255




