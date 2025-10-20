#include "Application.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    Application app(hInstance);
    if (!app.Initialize(nCmdShow)) {
        return -1;
    }
    return app.Run();
}
