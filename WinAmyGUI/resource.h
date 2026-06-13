#pragma once

// Menu IDs
#define IDM_FILE_NEW        1001
#define IDM_FILE_EXIT       1002
#define IDM_FILE_LOAD_EPD   1003
#define IDM_FILE_SAVE_EPD   1004
#define IDM_FILE_LOAD_PGN   1005
#define IDM_FILE_SAVE_PGN   1006
// Search-time menu items — each maps to a fixed per-move search time in seconds.
#define IDM_TIME_FIRST      1011
#define IDM_TIME_5          1011
#define IDM_TIME_15         1012
#define IDM_TIME_30         1013
#define IDM_TIME_60         1014
#define IDM_TIME_120        1015
#define IDM_TIME_180        1016
#define IDM_TIME_LAST       1016
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
#define IDM_SUGGEST_MOVE    1059
#define IDM_VIEW_GRIDLINES  1060
#define IDM_VIEW_RESET      1061

// Highlight all legal moves for one side. These are checkboxes, not radio
// items, so the active side can be deselected.
#define IDM_HIGHLIGHT_WHITE 1057
#define IDM_HIGHLIGHT_BLACK 1058

// The fixed per-move search time (in seconds) is selected from the Options ->
// Search Time menu (IDM_TIME_5 .. IDM_TIME_180). The engine searches as deeply
// as it can within the chosen interval; a longer interval yields a stronger
// (deeper) move at the cost of a longer wait.

// Button / control IDs
#define IDC_BTN_NEW_GAME    2001
#define IDC_BTN_HINT        2002
#define IDC_BTN_OUTLINES    2008
#define IDC_BTN_RESET_VIEW  2009
#define IDC_BTN_ZOOM_IN     2010
#define IDC_BTN_ZOOM_OUT    2011
#define IDC_BTN_VIEW_TOGGLE 2012
#define IDC_CB_GRID_TYPE    2013
#define IDC_CHK_PRESERVE_VIEW 2014
#define IDC_CB_SWAP_AXES    2017

// Custom window messages
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

// Menus
#define IDR_MAINMENU        100
