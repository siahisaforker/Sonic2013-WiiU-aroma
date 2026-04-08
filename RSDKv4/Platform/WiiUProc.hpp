#pragma once

#include "RetroEngine.hpp"

#if RETRO_PLATFORM == RETRO_WIIU
// Define `RETRO_WIIU_CF_AROMA` at build time to compile Aroma-compatible stubs
void WiiU_ProcInit();
void WiiU_ProcShutdown();
bool WiiU_ProcIsRunning();
#ifdef __cplusplus
extern "C" {
#endif

void WiiU_OnReleaseForeground();
void WiiU_OnAcquireForeground();

#ifdef __cplusplus
}
#endif
#else
static inline void WiiU_ProcInit() {}
static inline void WiiU_ProcShutdown() {}
static inline bool WiiU_ProcIsRunning() { return true; }
#endif
