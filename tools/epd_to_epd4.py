#!/usr/bin/env python3
"""
Convert a standard EPD file to 3D EPD4 format by adding level 'a' to all
square references in SAN move notations.

Standard SAN square notation:  [a-h][1-8]       e.g. "e4", "f3"
3D EPD4 square notation:       [a-z][a-h][1-8]  e.g. "ae4", "af3"

Usage:
    python epd_to_epd4.py input.epd [output.epd4]

If output is not specified, replaces .epd extension with .epd4.
"""

import re
import sys
import os


# Regex to match a SAN move (non-castling)
# Groups: piece, disambiguation, capture, target_file, target_rank, promotion, check
SAN_RE = re.compile(
    r'^([KQRBN]?)'          # 1: optional piece letter
    r'([a-h]?[1-8]?)'      # 2: optional disambiguation (file, rank, or both)
    r'(x?)'                 # 3: optional capture
    r'([a-h])'              # 4: target file (required)
    r'([1-8])'              # 5: target rank (required)
    r'(=[QRBN])?'           # 6: optional promotion
    r'([+#]?)$'             # 7: optional check/mate
)

CASTLING_RE = re.compile(r'^(O-O(?:-O)?)([\+#]?)$')


def convert_san_move(san: str) -> str:
    """Convert a single SAN move to 3D format by adding level 'a' to squares."""
    # Handle castling — unchanged
    m = CASTLING_RE.match(san)
    if m:
        return san

    # Handle normal moves
    m = SAN_RE.match(san)
    if m:
        piece, disambig, capture, target_file, target_rank, promo, check = m.groups()
        promo = promo or ''
        check = check or ''

        # Add level 'a' before the target square
        # If disambiguation contains a file letter, add level 'a' before it too
        disambig_3d = ''
        if disambig:
            # Disambiguation could be: file (a-h), rank (1-8), or file+rank
            if len(disambig) == 2:
                # file+rank = full square reference, add level
                disambig_3d = 'a' + disambig
            elif disambig[0] in 'abcdefgh':
                # file disambiguation, add level
                disambig_3d = 'a' + disambig
            else:
                # rank-only disambiguation, keep as-is
                disambig_3d = disambig

        return f"{piece}{disambig_3d}{capture}a{target_file}{target_rank}{promo}{check}"

    # If we can't parse it, return unchanged (e.g. null move "--")
    return san


def parse_epd_operations(ops_str: str) -> list:
    """
    Parse EPD operation fields into a list of (opcode, values).
    Operations are separated by ';'. Values may be quoted strings or SAN moves.
    """
    operations = []
    remaining = ops_str.strip()

    while remaining:
        # Find the opcode (first word)
        match = re.match(r'(\S+)\s*', remaining)
        if not match:
            break
        opcode = match.group(1)
        remaining = remaining[match.end():]

        # Find the value up to the semicolon
        # Handle quoted strings carefully
        value_parts = []
        while remaining and remaining[0] != ';':
            if remaining[0] == '"':
                # Quoted string — find closing quote
                end_idx = remaining.find('"', 1)
                end = (end_idx + 1) if end_idx >= 0 else len(remaining)
                value_parts.append(remaining[:end])
                remaining = remaining[end:].lstrip()
            else:
                # Non-quoted token — stop at semicolons and whitespace
                match = re.match(r'([^;\s]+)\s*', remaining)
                if match:
                    value_parts.append(match.group(1))
                    remaining = remaining[match.end():]
                else:
                    break

        # Skip the semicolon
        if remaining and remaining[0] == ';':
            remaining = remaining[1:].lstrip()

        operations.append((opcode, value_parts))

    return operations


# Operations whose values contain SAN moves
MOVE_OPCODES = {'bm', 'am', 'sm', 'pv'}


def convert_epd_line(line: str) -> str:
    """Convert a single EPD line to EPD4 format."""
    line = line.strip()
    if not line or line.startswith('#'):
        return line

    # EPD format: <position> <side> <castling> <ep> [operations...]
    # Split into FEN fields and operations
    parts = line.split()
    if len(parts) < 4:
        return line

    # First 4 fields are the position description (unchanged)
    position_fields = parts[:4]

    # Everything after the 4th field is operations
    ops_str = ' '.join(parts[4:])

    if not ops_str:
        return line

    # Parse and convert operations
    operations = parse_epd_operations(ops_str)
    converted_ops = []

    for opcode, values in operations:
        if opcode in MOVE_OPCODES:
            # Convert SAN moves (may be comma-separated or space-separated)
            converted_values = []
            for val in values:
                # Handle comma-separated move lists (e.g., "e4,d4")
                moves = val.rstrip(',').split(',')
                converted_moves = [convert_san_move(m) for m in moves if m]
                # Rejoin with commas if originally comma-separated
                if ',' in val:
                    converted_values.append(','.join(converted_moves) + (',' if val.endswith(',') else ''))
                else:
                    converted_values.append(converted_moves[0] if converted_moves else val)
            converted_ops.append(f"{opcode} {' '.join(converted_values)};")
        else:
            # Non-move operations (id, etc.) — keep unchanged
            converted_ops.append(f"{opcode} {' '.join(values)};")

    return ' '.join(position_fields) + ' ' + ' '.join(converted_ops)


def convert_file(input_path: str, output_path: str = None):
    """Convert an EPD file to EPD4 format."""
    if output_path is None:
        base, _ = os.path.splitext(input_path)
        output_path = base + '.epd4'

    with open(input_path, 'r') as f:
        lines = f.readlines()

    converted = [convert_epd_line(line) for line in lines]

    with open(output_path, 'w') as f:
        for line in converted:
            f.write(line + '\n')

    print(f"Converted {len(lines)} lines: {input_path} -> {output_path}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python epd_to_epd4.py input.epd [output.epd4]")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None

    if not os.path.isfile(input_path):
        print(f"Error: File not found: {input_path}")
        sys.exit(1)

    convert_file(input_path, output_path)


if __name__ == '__main__':
    main()
