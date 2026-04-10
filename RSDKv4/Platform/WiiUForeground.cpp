// Platform-specific foreground handlers for Wii U.
// These implementations attempt to free/recreate foreground-only resources
// (MEM1, foreground gfx heap) when the system requests the app release
// the foreground. All calls are guarded via weak symbols so this file is
// non-invasive and safe on builds that don't provide the helper symbols.

#include "WiiUProc.hpp"
#include "../RetroEngine.hpp"

#if RETRO_PLATFORM == RETRO_WIIU

#if defined(__WUT__)
#include <proc_ui/procui.h>
#endif

// Weak declarations for optional engine helpers. If these symbols are not
// present in the link, the pointers will be NULL and the calls will be
// skipped.
// Note: InitRenderDevice / ReleaseRenderDevice have C++ linkage (declared
// in Drawing.hpp), so they must NOT be inside an extern "C" block.
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
    // Avoid releasing graphics resources before the engine has fully
    // initialised. Some WUHB launch paths call ProcUI handlers very early
    // which can result in the render device being torn down before it is
    // created, producing a blank screen.
    if (!Engine.initialised) return;

    // Best-effort: destroy foreground/MEM1 resources if available.
    if (ReleaseRenderDevice)
        ReleaseRenderDevice();

    if (GfxHeapDestroyForeground)
        GfxHeapDestroyForeground();

    if (GfxHeapDestroyMEM1)
        GfxHeapDestroyMEM1();

    // Mark that we no longer have a foreground (if the symbol exists).
    if (&sGfxHasForeground)
        sGfxHasForeground = 0;

    // Notify ProcUI that the draw is done and the system can reclaim memory.
#if defined(__WUT__)
    ProcUIDrawDoneRelease();
#endif
}

extern "C" void WiiU_OnAcquireForeground()
{
    // Best-effort: recreate MEM1 and foreground heaps and reinit render device.
    if (GfxHeapInitMEM1)
        GfxHeapInitMEM1();

    if (GfxHeapInitForeground)
        GfxHeapInitForeground();

    if (InitRenderDevice)
        InitRenderDevice();

    // Mark that we have a foreground again (if the symbol exists).
    if (&sGfxHasForeground)
        sGfxHasForeground = 1;
}

#endif
