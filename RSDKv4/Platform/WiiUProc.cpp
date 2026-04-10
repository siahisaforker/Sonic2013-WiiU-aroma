#include "WiiUProc.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(__WUT__)
#include <proc_ui/procui.h>
#include <coreinit/foreground.h>
#include <coreinit/systeminfo.h>

static void WiiU_ProcSaveCallback(void) { OSSavesDone_ReadyToRelease(); }

void WiiU_ProcInit() {
    ProcUIInit(WiiU_ProcSaveCallback);
    OSEnableHomeButtonMenu(TRUE);
}
void WiiU_ProcShutdown() { ProcUIShutdown(); }
bool WiiU_ProcIsRunning() { return ProcUIIsRunning() && !ProcUIInShutdown(); }

#if 0 // unified above
#endif // 0

#else
// WHB fallback (older toolchains)
#include <whb/proc.h>

void WiiU_ProcInit() { WHBProcInit(); }
void WiiU_ProcShutdown() { WHBProcStopRunning(); }
bool WiiU_ProcIsRunning() { return WHBProcIsRunning(); }

#endif

// Weak foreground handler stubs – overridden by WiiUForeground.cpp when it
// is part of the build.  Kept here so the link succeeds even without that
// translation unit.
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak));
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak)) { }
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak));
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak)) { }

#else
// Non-WiiU platforms: no-op
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }
#endif
