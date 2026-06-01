/*
    WinAmyGUI - main.cpp

    WinMain entry point for the WinAmy 4D chess GUI. All window state and
    behaviour is encapsulated in CWinAmy4dWnd (see WinAmy4dWnd.h/.cpp); this
    file simply owns the single instance and runs it.
*/

#include <windows.h>

#include "WinAmy4dWnd.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CWinAmy4dWnd app;
    return app.Run(hInstance, nCmdShow);
}
