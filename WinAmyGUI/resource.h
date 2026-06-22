#pragma once

// Menu IDs
#define IDM_FILE_NEW        1001
#define IDM_FILE_EXIT       1002
#define IDM_FILE_LOAD_EPD   1003
#define IDM_FILE_SAVE_EPD   1004
#define IDM_FILE_LOAD_PGN   1005
#define IDM_FILE_SAVE_PGN   1006
// Search-depth menu items — each maps to a fixed iterative-deepening depth.
#define IDM_DEPTH_FIRST     1011
#define IDM_DEPTH_1         1011
#define IDM_DEPTH_2         1012
#define IDM_DEPTH_3         1013
#define IDM_DEPTH_4         1014
#define IDM_DEPTH_5         1015
#define IDM_DEPTH_6         1016
#define IDM_DEPTH_7         1017
#define IDM_DEPTH_8         1018
#define IDM_DEPTH_9         1019
#define IDM_DEPTH_LAST      1019
#define IDM_VIEW_2D         1031
#define IDM_VIEW_3D         1032

// Grid (cell outline) type — contiguous range so CheckMenuRadioItem works.
#define IDM_GRID_FIRST      1040
#define IDM_GRID_FULL       1040
#define IDM_GRID_SQUARE_Z   1041
#define IDM_GRID_SQUARE_Y   1042
#define IDM_GRID_SQUARE_X   1043
#define IDM_GRID_HEX_1      1044
#define IDM_GRID_HEX_2      1045
#define IDM_GRID_HEX_3      1046
#define IDM_GRID_HEX_4      1047
#define IDM_GRID_LAST       1047

// Player-count menu items — contiguous range so CheckMenuRadioItem works.
#define IDM_PLAYERS_FIRST   1051
#define IDM_PLAYERS_0       1051
#define IDM_PLAYERS_1       1052
#define IDM_PLAYERS_2       1053
#define IDM_PLAYERS_LAST    1053

// Pause / resume the engine self-play.
#define IDM_PAUSE           1054

// Undo the last full move (engine reply + human move) — 1-player mode only.
#define IDM_UNDO            1055

// Compute and display a short strategy (top 3 moves with replies) for the
// current player.
#define IDM_STRATEGY        1056

// Highlight all legal moves for one side. These are checkboxes, not radio
// items, so the active side can be deselected.
#define IDM_HIGHLIGHT_WHITE 1057
#define IDM_HIGHLIGHT_BLACK 1058

// Ask the engine to suggest a move for the current human player (highlights the
// recommended from/to squares without applying the move).
#define IDM_SUGGEST         1059

// View menu — 3D-only items (enabled only while in 3D mode).
#define IDM_VIEW_SHOW_GRIDLINES 1060
#define IDM_VIEW_RESET_VIEW     1061

// The fixed search depth is selected from the Options -> Search Depth menu
// (IDM_DEPTH_1 .. IDM_DEPTH_9). The engine searches to the chosen depth via
// iterative deepening; a greater depth yields a stronger move at the cost of a
// longer wait.

// Button / control IDs
#define IDC_BTN_NEW_GAME    2001
#define IDC_BTN_HINT        2002
#define IDC_BTN_OUTLINES    2008
#define IDC_BTN_RESET_VIEW  2009
#define IDC_BTN_ZOOM_IN     2010
#define IDC_BTN_ZOOM_OUT    2011
#define IDC_BTN_VIEW_TOGGLE 2012
#define IDC_CB_GRID_TYPE    2013
#define IDC_BTN_ENTER_MOVE  2014
#define IDC_CB_SWAP_AXES    2017
#define IDC_BTN_ROTATE_GRID 2018

// Custom window messages
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

// Menus
#define IDR_MAINMENU        100

// Dialogs
#define IDD_ENTER_MOVE      200

// Dialog controls
#define IDC_EDIT_MOVE       2100

#ifndef IDC_STATIC
#define IDC_STATIC          (-1)
#endif
