#pragma once

// Menu IDs
#define IDM_FILE_NEW        1001
#define IDM_FILE_EXIT       1002
#define IDM_DEPTH_1         1011
#define IDM_DEPTH_2         1012
#define IDM_DEPTH_3         1013
#define IDM_DEPTH_4         1014
#define IDM_DEPTH_5         1015
#define IDM_DEPTH_6         1016
#define IDM_DEPTH_7         1017
#define IDM_DEPTH_8         1018
#define IDM_DEPTH_9         1019
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

// Button / control IDs
#define IDC_BTN_NEW_GAME    2001
#define IDC_BTN_OUTLINES    2008
#define IDC_BTN_RESET_VIEW  2009
#define IDC_BTN_ZOOM_IN     2010
#define IDC_BTN_ZOOM_OUT    2011
#define IDC_BTN_VIEW_TOGGLE 2012
#define IDC_CB_GRID_TYPE    2013

// Custom window messages
#define WM_APP_ENGINE_MOVE  (WM_APP + 1)

// Menus
#define IDR_MAINMENU        100
