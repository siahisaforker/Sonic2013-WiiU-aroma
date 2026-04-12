#include "WiiUProc.hpp"
#include "../RetroEngine.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(__WUT__)
#include <proc_ui/procui.h>
#endif

extern int InitRenderDevice() __attribute__((weak));
extern void ReleaseRenderDevice() __attribute__((weak));

extern "C" {
extern void GfxHeapDestroyForeground() __attribute__((weak));
extern void GfxHeapDestroyMEM1() __attribute__((weak));
extern void GfxHeapInitMEM1() __attribute__((weak));
extern void GfxHeapInitForeground() __attribute__((weak));
extern unsigned char sGfxHasForeground __attribute__((weak));
}

extern "C" void WiiU_OnReleaseForeground()
{
    if (!Engine.initialised) return;

    if (ReleaseRenderDevice)
        ReleaseRenderDevice();

    if (GfxHeapDestroyForeground)
        GfxHeapDestroyForeground();

    if (GfxHeapDestroyMEM1)
        GfxHeapDestroyMEM1();

    if (&sGfxHasForeground)
        sGfxHasForeground = 0;

#if defined(__WUT__)
    ProcUIDrawDoneRelease();
#endif
}

extern "C" void WiiU_OnAcquireForeground()
{
    if (GfxHeapInitMEM1)
        GfxHeapInitMEM1();

    if (GfxHeapInitForeground)
        GfxHeapInitForeground();

    if (InitRenderDevice)
        InitRenderDevice();

    if (&sGfxHasForeground)
        sGfxHasForeground = 1;
}

#endif