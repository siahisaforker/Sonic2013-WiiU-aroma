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

#else
#include <whb/proc.h>

void WiiU_ProcInit() { WHBProcInit(); }
void WiiU_ProcShutdown() { WHBProcStopRunning(); }
bool WiiU_ProcIsRunning() { return WHBProcIsRunning(); }

#endif

extern "C" __attribute__((weak)) void WiiU_OnReleaseForeground() { }
extern "C" __attribute__((weak)) bool WiiU_OnAcquireForeground() { return true; }

#else
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }
#endif
