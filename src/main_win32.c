#ifdef _WIN32

#include <windows.h>
#include "starter.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    cgfs_start();
}

#endif
