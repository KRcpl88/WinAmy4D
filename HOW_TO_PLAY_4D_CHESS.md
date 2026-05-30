# How to Play 4D Chess

Welcome to **WinAmy4D**, a four-dimensional variant of chess.

If you already know standard chess, you know almost everything you need
to play. 4D chess uses **all the same pieces, all the same special
rules, and the same overall objective** — checkmate your opponent's king.
What changes is the *playing field*: instead of a single 8×8 board, you
play on a stack of fifteen boards of varying sizes that together form a
single, three-dimensional playing space. This guide walks you through the
board, explains why it's called "4D," shows you the starting position,
and describes how each piece moves.

> **One-sentence summary**: 4D chess is regular chess on a stepped pyramid
> of fifteen boards, where every piece keeps the move it has in regular
> chess and gains natural extensions of that move into the third (and
> fourth) dimensions.

---

## 1. The Playing Board

### Fifteen levels, stacked

The playing area is made up of **fifteen levels**, labelled with letters
**`a`** through **`o`** (the first fifteen letters of the alphabet).

Each level is its own square board, but the boards are not all the same
size. They form a **stepped pyramid** that grows from a single square at
the very bottom, up to a full 8×8 board in the middle, and then tapers
back down to a single square at the very top:

| Level | Size  | Notes                                               |
|:-----:|:-----:|-----------------------------------------------------|
|  `a`  | 1 × 1 | the bottom point of the pyramid                     |
|  `b`  | 2 × 2 |                                                     |
|  `c`  | 3 × 3 |                                                     |
|  `d`  | 4 × 4 |                                                     |
|  `e`  | 5 × 5 |                                                     |
|  `f`  | 6 × 6 |                                                     |
|  `g`  | 7 × 7 |                                                     |
| **`h`** | **8 × 8** | **the main board — standard chess board**       |
|  `i`  | 7 × 7 |                                                     |
|  `j`  | 6 × 6 |                                                     |
|  `k`  | 5 × 5 |                                                     |
|  `l`  | 4 × 4 |                                                     |
|  `m`  | 3 × 3 |                                                     |
|  `n`  | 2 × 2 |                                                     |
|  `o`  | 1 × 1 | the top point of the pyramid                        |

That's **344 squares** in total. The widths of the levels are:

```
   level: a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
   width: 1  2  3  4  5  6  7  8  7  6  5  4  3  2  1
                              ^^^
                              main 8x8 board
```

A side-on silhouette of the stack:

```
                  o   (1)
                 nnn
                mmmmm
               lllllll
              kkkkkkkkk
             jjjjjjjjjjj
            ggggggggggggg
           hhhhhhhhhhhhhhh   <-- standard 8x8 board
            iiiiiiiiiiiii
             ffffffffffff
              eeeeeeeeee
               dddddddd
                cccccc
                 bbbb
                  a    (1)
```

### Relationship to standard 2D chess

**Level `h` is the standard chessboard.** If you played a game that used
only level `h`, you would be playing regular chess: the same eight files
(a–h), the same eight ranks (1–8), the same starting position, the same
moves, the same rules.

The other fourteen levels add space above and below the main board, into
which pieces can move when their movement carries them off the main
board. Think of them as the "rooms upstairs and downstairs" of a normal
chess game.

### Viewing the board

The app offers two views of the board:

- **2D view**: shows all fifteen levels laid out side-by-side, arranged
  in three rows so they fit on your screen. Easiest for reading exact
  positions.
- **3D view**: shows the levels as a true stack in three-dimensional
  space, which makes vertical (cross-level) relationships much easier
  to see. Several "grid" orientations are available — see *Why is it
  called 4D chess?* below for what those are.

A toolbar button toggles between the two views. **Most players find the
3D view essential once pieces start moving between levels.**

---

## 2. Why is it called 4D chess?

The straightforward answer is: the 344 squares are arranged as the
**three-dimensional shadow of a four-dimensional shape**. The stepped
pyramid you see on screen is what that 4D shape looks like when it's
projected down into the 3D world where we can actually look at it — much
the same way that the shadow of a 3D cube on a 2D wall is a flat
hexagon.

You don't have to think in four dimensions to play, but two clues that
4D structure is "hiding behind" the board are useful to know:

1. **The pyramid is symmetrical in more than one way.** It isn't just
   symmetric top-to-bottom; if you turn it in 3D space, the same kind
   of stepped pyramid appears looking down each of several different
   directions. That extra symmetry is a fingerprint of the 4D shape it
   came from.

2. **The 3D view offers four "Hex" grid orientations.** In the 3D view's
   grid menu you'll see **Hex 1**, **Hex 2**, **Hex 3**, and **Hex 4** as
   alternatives to the square grid. Each Hex view lines the squares up
   along one of the **four main diagonals** of the underlying 4D shape.
   Each gives you a different "honest" way to see the cells lined up.
   Together, they're the easiest way to *feel* the fourth dimension
   without having to do any math.

By way of analogy: 3D chess takes flat (2D) chess and stacks boards into
a third dimension. 4D chess goes one step further — it adds another
diagonal direction that ties cells together across the pyramid, which is
why pieces can move between levels in more ways than a single "up/down"
axis would allow.

For purposes of actually playing, you only need to remember:

- Pieces still move in straight lines, L-shapes, and so on — just like
  regular chess.
- Those lines now include directions that pass between levels.
- The 3D view's Hex grids let you see those between-level lines clearly.

---

## 3. Setting Up the Pieces

Both sides start with a familiar standard-chess army (eight pawns and the
usual king, queen, two rooks, two bishops, two knights) **plus** a number
of additional pieces and pawns spread over the middle levels of the
pyramid.

The initial setup occupies only **four of the fifteen levels**, with
levels `a`–`f` and `k`–`o` starting empty:

| Level | Size  | White's back rank → forward                       | What's on it                                |
|:-----:|:-----:|--------------------------------------------------|---------------------------------------------|
|  `g`  | 7 × 7 | (none — pawns only)                              | One row of **7 pawns** for each side       |
| **`h`** | **8 × 8** | **R N B Q K B N R**                            | **Full standard chess starting position**   |
|  `i`  | 7 × 7 | **R N B Q B N R** (no king)                      | Pieces plus one row of 7 pawns each side    |
|  `j`  | 6 × 6 | (none — pawns only)                              | One row of **6 pawns** for each side       |

A few important things to notice:

- The **kings live only on level `h`**, and there is exactly one king per
  side, just like in standard chess. There are no extra kings on any
  other level.
- White's pieces begin on the side of each level closest to White (the
  "first rank" of that level); Black's pieces begin on the opposite
  side, mirroring White.
- White moves first.

If you ignore everything except level `h`, the starting position **is
the starting position of regular chess**. The other three levels (g, i,
j) are extra forces stacked above and below the main board.

---

## 4. How Each Piece Moves

Every piece keeps the move it has in regular chess. In 4D chess, each of
those moves is **extended** in a natural way so that the piece can also
travel between levels.

A useful rule of thumb: **if a move would be legal in regular chess on
level `h`, it is also legal here.** The new options are *additions*, not
replacements.

When you click on a piece in the app, every square it can legally move to
lights up. This is the fastest way to learn the new directions — but
here is the conceptual description for each piece.

### The Pawn ♙ ♟

- **Forward move.** A pawn pushes one square *forward* (toward the
  opponent's side) on its own level. As in regular chess, it never moves
  backward and never captures with this forward push.
- **Double first move.** From its starting rank, a pawn may push two
  squares forward on its own level instead of one — provided both
  squares are empty. The "starting rank" is the second rank of the
  pawn's side on whichever level it's on (so on a 7-wide level, Black's
  starting rank is rank 5; on the main 8-wide level it's rank 6, just
  like normal).
- **Diagonal capture.** A pawn captures diagonally forward — and it has
  *two kinds* of diagonal:
  1. The standard forward-left / forward-right diagonal **on its own
     level** (exactly as in regular chess).
  2. A diagonal that carries it forward into a square on a **neighbouring
     level** above or below.

  In both cases the destination must contain an enemy piece (or be the
  en-passant square — see below).
- **En passant.** Just as in regular chess, if a pawn double-pushes past
  a square that an enemy pawn could have captured it on, that enemy pawn
  may capture *en passant* on its very next move.
- **Promotion.** When a pawn reaches the **far rank** of one of the
  central levels (levels `f`, `g`, `h`, `i`, or `j`), it must promote
  to a Queen, Rook, Bishop, or Knight, as in regular chess.

> On level `h` only, the pawn behaves *exactly* like a regular chess pawn.

### The Knight ♘ ♞

- **Standard L-shape.** Within level `h`, the knight moves to the same
  eight squares it always has (two squares in one direction and one in
  a perpendicular direction).
- **Extended L-shape.** When other levels are in play, the knight also
  jumps to L-shaped destinations that involve travelling between levels
  — for example, jumping "two levels up and one square sideways," or
  "one level up and two squares forward." There are several such
  destinations in 3D space.
- **Jumps over pieces.** Just like in regular chess, the knight is the
  only piece that can leap over intervening pieces of either colour.

> On level `h` only, the knight behaves exactly like a regular knight.

### The Bishop ♗ ♝

- **On its own level.** Slides any distance along the diagonals, just as
  in regular chess: blocked by the first piece it meets, capturing if
  that piece is an enemy.
- **Between levels.** Also slides straight "up" or "down" through the
  levels of the pyramid in a direction that is diagonal in the 4D sense.
  A bishop's slide can carry it across many levels in a single move,
  again stopping at the first piece in its path.

> On level `h` only, the bishop behaves exactly like a regular bishop —
> alternating-colour bishops included.

### The Rook ♖ ♜

- **On its own level.** Slides any distance along a rank or a file, just
  as in regular chess.
- **Between levels.** Also slides in directions that combine a step
  along the level with a step into the next level above or below. As
  with the bishop, the slide continues until it meets a piece or runs
  off the edge of the playing space.

> On level `h` only, the rook behaves exactly like a regular rook.

### The Queen ♕ ♛

- **All of the above.** The queen combines the moves of the bishop and
  the rook, with no exceptions. Whatever the bishop or rook can do, the
  queen can do.

> On level `h` only, the queen behaves exactly like a regular queen.

### The King ♔ ♚

- **One square in any direction.** The king moves exactly one square in
  any direction the queen would move — that is, any direction along
  which a bishop, rook, or queen could slide. This includes one-square
  moves *between levels*.
- **Castling.** See *Special Rules* below.

> On level `h` only, the king behaves exactly like a regular king (eight
> possible single-square moves, plus castling).

---

## 5. Special Rules (Same as Regular Chess)

The special rules of regular chess all apply, with one small extension
already noted (pawn promotion) and one small restriction (where
castling can happen).

- **Promotion.** A pawn that reaches the far rank of any of the central
  levels (`f`, `g`, `h`, `i`, `j`) **must** promote to a Queen, Rook,
  Bishop, or Knight on the same move. Promotion is identical to regular
  chess in every other respect.
- **Castling.** Castling is exactly the regular-chess move: king and
  rook trade places under the well-known conditions (neither piece has
  moved, no pieces between them, the king is not currently in check,
  and the king does not pass through or land on a square attacked by
  the opponent). **Castling is only available on the main board (level
  `h`)** — that is, between the king and his original level-`h` rooks.
- **En passant.** Identical to regular chess. If a pawn makes a
  two-square push past a square that an enemy pawn could have captured
  it on, the enemy pawn may capture it *en passant* — but only on the
  very next move.
- **Check and checkmate.** A king is **in check** when any enemy piece
  attacks the square it stands on. The player on move must always
  respond to check (by moving the king, blocking the attack, or
  capturing the attacker). If there is no legal response, it is
  **checkmate** and that player loses. Since both kings live on level
  `h`, all checks ultimately resolve there — but the *attacking* piece
  may be on any level whose movement reaches the king.
- **Stalemate.** If the player on move has no legal moves *and* is not
  in check, the game is a **draw** by stalemate, exactly as in regular
  chess.
- **Turn order.** White moves first, then Black, alternating.

---

## 6. How to Win, How to Draw

The objective is **checkmate the opposing king**, exactly as in regular
chess. The game also ends in a draw under all the familiar conditions:
stalemate, insufficient material, agreed draw, and so on.

---

## 7. Tips for New Players

- **Start with level `h`.** Until you are comfortable spotting threats
  across multiple levels, treat level `h` as your "main theatre" and
  play it like a regular game of chess. The other levels will start to
  matter as pieces drift into them.
- **Turn on the 3D view.** The 2D view is fine for reading exact
  positions, but the 3D view is much better for spotting threats that
  cross between levels — and those are the threats that catch beginners
  out.
- **Try each Hex grid.** The four Hex orientations in the 3D view's
  grid menu each show you cells aligned along a different one of the
  underlying 4D diagonals. Trying all four is the quickest way to
  develop an intuition for cross-level lines.
- **Watch above and below your king.** A piece on level `g` or level
  `i` (or further) can deliver check to your king on level `h`. If you
  only watch level `h`, you will miss those threats.
- **Click your pieces.** When you click on any of your pieces, every
  legal destination lights up. Use this freely — there is no penalty
  for clicking around to learn the moves.

---

## 8. Quick Reference

| | |
|---|---|
| Number of levels | 15 (`a`–`o`) |
| Total squares | 344 |
| Main board | level `h` (8 × 8) — standard chessboard |
| Starting position occupies levels | `g`, `h`, `i`, `j` |
| Kings | one per side, on level `h` only |
| Pieces | same as standard chess (pawn, knight, bishop, rook, queen, king) |
| Special rules | promotion, castling (level `h` only), en passant — all identical to regular chess |
| Objective | checkmate the opposing king |

Enjoy the game!
