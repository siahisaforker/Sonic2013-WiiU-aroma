#include "WiiUProc.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(RETRO_WIIU_CF_AROMA)
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }
extern "C" void WiiU_OnReleaseForeground() { }
extern "C" void WiiU_OnAcquireForeground() { }


#else
#if defined(__WUT__)
#include <proc_ui/procui.h>
#include <coreinit/foreground.h>

static void WiiU_ProcSaveCallback(void) { OSSavesDone_ReadyToRelease(); }

void WiiU_ProcInit() { ProcUIInit(WiiU_ProcSaveCallback); }
void WiiU_ProcShutdown() { ProcUIShutdown(); }
bool WiiU_ProcIsRunning() { return ProcUIIsRunning() && !ProcUIInShutdown(); }

#else
// WHB fallback (older toolchains)
#include <whb/proc.h>

void WiiU_ProcInit() { WHBProcInit(); }
void WiiU_ProcShutdown() { WHBProcStopRunning(); }
bool WiiU_ProcIsRunning() { return WHBProcIsRunning(); }

#endif


#if RETRO_PLATFORM == RETRO_WIIU
#if defined(ENABLE_WIIU_FOREGROUND_HANDLERS)

extern int InitRenderDevice() __attribute__((weak));
extern void ReleaseRenderDevice() __attribute__((weak));

extern "C" void WiiU_OnReleaseForeground() __attribute__((weak));
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak))
{
	if (ReleaseRenderDevice)
		ReleaseRenderDevice();
}

extern "C" void WiiU_OnAcquireForeground() __attribute__((weak));
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak))
{
	if (InitRenderDevice)
		InitRenderDevice();
}
#else
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak));
extern "C" void WiiU_OnReleaseForeground() __attribute__((weak)) { }
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak));
extern "C" void WiiU_OnAcquireForeground() __attribute__((weak)) { }
#endif

#endif

#endif // RETRO_WIIU_CF_AROMA

#else
// Non-WiiU platforms: no-op
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }
#endif
