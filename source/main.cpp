// Entry point.
//
// The original's shape, for the record (level 1): GA's real entry is 0x01d0e310 inside the
// EncryptPE `.bind` section; the decrypted dump's entry is 0x011e11e1, which is MSVC's
// crtStartup: `call 0x11e1588` (guard/init) then `jmp 0x11e0f21` (__tmainCRTStartup), and
// that path is where GetStartupInfoA (slot 0x013cc29c, ref 0x11e0f3b) and the argv walk
// (0x11e1021: `cmp al,0x20` / `cmp al,0x22` - space and quote handling) live. The string
// 'WinMain' exists at 0x0149c998. We do the same job with the modern CRT: a thin WinMain
// that immediately hands over to app::run, whose FIRST act is to stand up logging and the
// crash harness (field guide §4: infrastructure before code).
#include <windows.h>

#include "app.h"
#include "engine/diag.h"

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmdline, int show) {
    // First statement, before anything that can assert. Our own namespace-scope objects are
    // all POD (see log.cpp/diag.cpp), so no C++ static initialiser runs ahead of this point
    // that could raise one - but the CRT's own startup does, and this is the cheapest guard.
    os::diag::early_neutralise();
    return os::app::run(hInst, hPrev, cmdline, show);
}
