#include "WiiUProc.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(RETRO_WIIU_CF_AROMA)
// Provide compat stubs
void WiiU_ProcInit() { }
void WiiU_ProcShutdown() { }
bool WiiU_ProcIsRunning() { return true; }


#else
// Prefer WUT/ProcUI when available; fall back to WHB helpers.
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
// For graceful exit under WHB/Tiramisu, signal the proc loop to stop
// rather than forcibly shutting down low-level subsystems.
void WiiU_ProcShutdown() { WHBProcStopRunning(); }
bool WiiU_ProcIsRunning() { return WHBProcIsRunning(); }

#endif


#if RETRO_PLATFORM == RETRO_WIIU
#if defined(ENABLE_WIIU_FOREGROUND_HANDLERS)
// If handlers are enabled, call the optional render device helpers if
// they're present. Mark the default implementations as weak so other
// translation units (e.g. `WiiUForeground.cpp`) can provide strong
// overrides without creating multiple-definition link errors.
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
// Provide weak no-op defaults so platform-specific modules can override
// them with strong definitions when necessary.
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
