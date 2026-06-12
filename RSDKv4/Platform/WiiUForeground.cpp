#include "WiiUProc.hpp"
#include "../RetroEngine.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(__WUT__)
#include <proc_ui/procui.h>
#endif

extern int InitRenderDevice() __attribute__((weak));
extern void ReleaseRenderDevice() __attribute__((weak));
extern int RecreateRenderDeviceForForeground() __attribute__((weak));
extern void ReleaseRenderDeviceForForeground() __attribute__((weak));

extern "C" {
extern void GfxHeapDestroyForeground() __attribute__((weak));
extern void GfxHeapDestroyMEM1() __attribute__((weak));
extern void GfxHeapInitMEM1() __attribute__((weak));
extern void GfxHeapInitForeground() __attribute__((weak));
extern unsigned char sGfxHasForeground __attribute__((weak));
}

static bool sWiiUForegroundReleased = false;

static void WiiU_MarkForegroundAvailable(bool available)
{
    if (&sGfxHasForeground)
        sGfxHasForeground = available ? 1 : 0;
}

static void WiiU_DestroyForegroundResources()
{
    CloseAudioPlaybackDevice();

    if (ReleaseRenderDeviceForForeground)
        ReleaseRenderDeviceForForeground();
    else if (ReleaseRenderDevice)
        ReleaseRenderDevice();

    if (GfxHeapDestroyForeground)
        GfxHeapDestroyForeground();

    if (GfxHeapDestroyMEM1)
        GfxHeapDestroyMEM1();

    WiiU_MarkForegroundAvailable(false);
}

extern "C" void WiiU_OnReleaseForeground()
{
    if (!sWiiUForegroundReleased) {
        if (Engine.initialised)
            WiiU_DestroyForegroundResources();
        else
            WiiU_MarkForegroundAvailable(false);
        sWiiUForegroundReleased = true;
    }

#if defined(__WUT__)
    ProcUIDrawDoneRelease();
#endif
}

extern "C" bool WiiU_OnAcquireForeground()
{
    if (!sWiiUForegroundReleased) {
        WiiU_MarkForegroundAvailable(true);
        return true;
    }

    if (GfxHeapInitMEM1)
        GfxHeapInitMEM1();

    if (GfxHeapInitForeground)
        GfxHeapInitForeground();

    bool renderReady = true;
    if (RecreateRenderDeviceForForeground)
        renderReady = RecreateRenderDeviceForForeground() != 0;
    else if (InitRenderDevice)
        renderReady = InitRenderDevice() != 0;

    if (!renderReady) {
        if (ReleaseRenderDeviceForForeground)
            ReleaseRenderDeviceForForeground();
        else if (ReleaseRenderDevice)
            ReleaseRenderDevice();

        if (GfxHeapDestroyForeground)
            GfxHeapDestroyForeground();

        if (GfxHeapDestroyMEM1)
            GfxHeapDestroyMEM1();

        WiiU_MarkForegroundAvailable(false);
        return false;
    }

    ReopenAudioPlaybackDevice();

    WiiU_MarkForegroundAvailable(true);
    sWiiUForegroundReleased = false;
    return true;
}

#endif
