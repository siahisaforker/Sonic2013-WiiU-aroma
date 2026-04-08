#include "RetroEngine.hpp"

#if !RETRO_USE_ORIGINAL_CODE
bool usingCWD        = false;
bool engineDebugMode = false;
#endif

#if RETRO_PLATFORM == RETRO_ANDROID
#include <unistd.h>
#endif

#if RETRO_PLATFORM == RETRO_WIIU
#include "Platform/WiiUProc.hpp"
#if defined(__WUT__)
#include <proc_ui/procui.h>
#endif
#endif

RetroEngine Engine = RetroEngine();
char launchPath[0x400] = { 0 };

#if !RETRO_USE_ORIGINAL_CODE
inline int getLowerRate(int intendRate, int targetRate)
{
    int result   = 0;
    int valStore = 0;

    result = targetRate;
    if (intendRate) {
        do {
            valStore   = result % intendRate;
            result     = intendRate;
            intendRate = valStore;
        } while (valStore);
    }
    return result;
}
#endif

bool processEvents()
{
#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    while (SDL_PollEvent(&Engine.sdlEvents)) {
        // Main Events
        switch (Engine.sdlEvents.type) {
#if RETRO_USING_SDL2
            case SDL_WINDOWEVENT:
                switch (Engine.sdlEvents.window.event) {
                    case SDL_WINDOWEVENT_MAXIMIZED: {
                        SDL_RestoreWindow(Engine.window);
                        SDL_SetWindowFullscreen(Engine.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        SDL_ShowCursor(SDL_FALSE);
                        Engine.isFullScreen = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_CLOSE: {
                        SDL_Event ev;
                        SDL_zero(ev);
                        ev.type = SDL_QUIT;
                        SDL_PushEvent(&ev);
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        if (Engine.gameMode == ENGINE_MAINGAME && !disableFocusPause)
                            Engine.gameMode = ENGINE_INITPAUSE;
                        break;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED: controllerInit(Engine.sdlEvents.cdevice.which); break;
            case SDL_CONTROLLERDEVICEREMOVED: controllerClose(Engine.sdlEvents.cdevice.which); break;
            case SDL_APP_WILLENTERBACKGROUND:
                if (Engine.gameMode == ENGINE_MAINGAME && !disableFocusPause)
                    Engine.gameMode = ENGINE_INITPAUSE;
                break;
            case SDL_APP_TERMINATING: {
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = SDL_QUIT;
                SDL_PushEvent(&ev);
                break;
            }
#endif

#if RETRO_USING_SDL2 && defined(RETRO_USING_MOUSE)
            case SDL_MOUSEMOTION:
                if (touches <= 1) { // Touch always takes priority over mouse
                    uint state = SDL_GetMouseState(&touchX[0], &touchY[0]);

                    touchXF[0] = (((touchX[0] - displaySettings.offsetX) / (float)displaySettings.width) * SCREEN_XSIZE_F) - SCREEN_CENTERX_F;
                    touchYF[0] = -(((touchY[0] / (float)displaySettings.height) * SCREEN_YSIZE_F) - SCREEN_CENTERY_F);
                    touchX[0]  = ((touchX[0] - displaySettings.offsetX) / (float)displaySettings.width) * SCREEN_XSIZE;
                    touchY[0]  = (touchY[0] / (float)displaySettings.height) * SCREEN_YSIZE;

                    touchDown[0] = state & SDL_BUTTON_LMASK;
                    if (touchDown[0])
                        touches = 1;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (touches <= 1) { // Touch always takes priority over mouse
                    switch (Engine.sdlEvents.button.button) {
                        case SDL_BUTTON_LEFT: touchDown[0] = true; break;
                    }
                    touches = 1;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (touches <= 1) { // Touch always takes priority over mouse
                    switch (Engine.sdlEvents.button.button) {
                        case SDL_BUTTON_LEFT: touchDown[0] = false; break;
                    }
                    touches = 0;
                }
                break;
#endif

#if defined(RETRO_USING_TOUCH) && RETRO_USING_SDL2
            case SDL_FINGERMOTION:
            case SDL_FINGERDOWN:
            case SDL_FINGERUP: {
                int count = SDL_GetNumTouchFingers(Engine.sdlEvents.tfinger.touchId);
                touches   = 0;
                for (int i = 0; i < count; i++) {
                    SDL_Finger *finger = SDL_GetTouchFinger(Engine.sdlEvents.tfinger.touchId, i);
                    if (finger) {
                        touchDown[touches] = true;
                        touchX[touches]    = finger->x * SCREEN_XSIZE;
                        touchY[touches]    = finger->y * SCREEN_YSIZE;
                        touchXF[touches]   = (finger->x * SCREEN_XSIZE_F) - SCREEN_CENTERX_F;
                        touchYF[touches]   = -((finger->y * SCREEN_YSIZE_F) - SCREEN_CENTERY_F);

                        touchX[touches] -= displaySettings.offsetX;
                        touchXF[touches] -= displaySettings.offsetX;

                        touches++;
                    }
                }
                break;
            }
#endif //! RETRO_USING_SDL2

            case SDL_KEYDOWN:
                switch (Engine.sdlEvents.key.keysym.sym) {
                    default: break;
                    case SDLK_ESCAPE:
                        if (Engine.devMenu) {
#if RETRO_USE_MOD_LOADER
                            // hacky patch because people can escape
                            if (Engine.gameMode == ENGINE_DEVMENU && stageMode == DEVMENU_MODMENU)
                                RefreshEngine();
#endif
                            ClearNativeObjects();
                            CREATE_ENTITY(RetroGameLoop);
                            if (Engine.gameDeviceType == RETRO_MOBILE)
                                CREATE_ENTITY(VirtualDPad);
                            Engine.gameMode = ENGINE_INITDEVMENU;
                        }
                        break;
                    case SDLK_F4:
                        Engine.isFullScreen ^= 1;
                        setFullScreen(Engine.isFullScreen);
                        break;
                    case SDLK_F1:
                        if (Engine.devMenu) {
                            activeStageList   = 0;
                            stageListPosition = 0;
                            stageMode         = STAGEMODE_LOAD;
                            Engine.gameMode   = ENGINE_MAINGAME;
                        }
                        break;
                    case SDLK_F2:
                        if (Engine.devMenu) {
                            stageListPosition--;
                            if (stageListPosition < 0) {
                                activeStageList--;

                                if (activeStageList < 0) {
                                    activeStageList = 3;
                                }
                                stageListPosition = stageListCount[activeStageList] - 1;
                            }
                            stageMode       = STAGEMODE_LOAD;
                            Engine.gameMode = ENGINE_MAINGAME;
                            SetGlobalVariableByName("lampPostID", 0); // For S1
                            SetGlobalVariableByName("starPostID", 0); // For S2
                        }
                        break;
                    case SDLK_F3:
                        if (Engine.devMenu) {
                            stageListPosition++;
                            if (stageListPosition >= stageListCount[activeStageList]) {
                                activeStageList++;

                                stageListPosition = 0;

                                if (activeStageList >= 4) {
                                    activeStageList = 0;
                                }
                            }
                            stageMode       = STAGEMODE_LOAD;
                            Engine.gameMode = ENGINE_MAINGAME;
                            SetGlobalVariableByName("lampPostID", 0); // For S1
                            SetGlobalVariableByName("starPostID", 0); // For S2
                        }
                        break;
                    case SDLK_F9:
                        if (Engine.devMenu)
                            showHitboxes ^= 1;
                        break;
                    case SDLK_F10:
                        if (Engine.devMenu)
                            Engine.showPaletteOverlay ^= 1;
                        break;
                    case SDLK_BACKSPACE:
                        if (Engine.devMenu)
                            Engine.gameSpeed = Engine.fastForwardSpeed;
                        break;
#if RETRO_PLATFORM == RETRO_OSX
                    case SDLK_F6:
                        if (Engine.masterPaused)
                            Engine.frameStep = true;
                        break;
                    case SDLK_F7:
                        if (Engine.devMenu)
                            Engine.masterPaused ^= 1;
                        break;
#else
                    case SDLK_F11:
                    case SDLK_INSERT:
                        if (Engine.masterPaused)
                            Engine.frameStep = true;
                        break;
                    case SDLK_F12:
                    case SDLK_PAUSE:
                        if (Engine.devMenu)
                            Engine.masterPaused ^= 1;
                        break;
#endif
                }

#if RETRO_USING_SDL1
                keyState[Engine.sdlEvents.key.keysym.sym] = 1;
#endif
                break;
            case SDL_KEYUP:
                switch (Engine.sdlEvents.key.keysym.sym) {
                    default: break;
                    case SDLK_BACKSPACE: Engine.gameSpeed = 1; break;
                }
#if RETRO_USING_SDL1
                keyState[Engine.sdlEvents.key.keysym.sym] = 0;
#endif
                break;
            case SDL_QUIT: return false;
        }
    }
#endif
#endif
    return true;
}

#if RETRO_PLATFORM == RETRO_WIIU
#include <whb/log.h>
#include <whb/log_udp.h>
#include <whb/log_cafe.h>
#include <unistd.h>
#include <sys/iosupport.h>
#include <dirent.h>
#include <sys/stat.h>

static ssize_t wiiu_log_write(struct _reent* r, void* fd, const char* ptr, size_t len) {
    WHBLogPrintf("%*.*s", len, len, ptr);
    return len;
}
static const devoptab_t dotab_stdout = {
    .name = "stdout_whb",
    .write_r = wiiu_log_write,
};
#endif

void RetroEngine::Init()
{
#if RETRO_PLATFORM == RETRO_ANDROID
    sleep(1); // wait to initialize the engine
#endif
#if RETRO_PLATFORM == RETRO_WIIU
    WHBLogUdpInit();
    WHBLogCafeInit();
    devoptab_list[STD_OUT] = &dotab_stdout;
    devoptab_list[STD_ERR] = &dotab_stdout;
    WiiU_ProcInit();
#endif

    CalculateTrigAngles();
    GenerateBlendLookupTable();

    CloseRSDKContainers(); // Clears files

    Engine.usingDataFile = false;
    Engine.usingBytecode = false;

#if !RETRO_USE_ORIGINAL_CODE
#if 1
    InitUserdata();
#endif
#if RETRO_USE_NETWORKING
    initNetwork();
#endif

    char dest[0x200];
#if RETRO_PLATFORM == RETRO_UWP
    static char resourcePath[256] = { 0 };

    if (strlen(resourcePath) == 0) {
        auto folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
        auto path   = to_string(folder.Path());

        std::copy(path.begin(), path.end(), resourcePath);
    }

    strcpy(dest, resourcePath);
    strcat(dest, "\\");
    strcat(dest, Engine.dataFile);
#elif RETRO_PLATFORM == RETRO_ANDROID
    StrCopy(dest, gamePath);
    StrAdd(dest, Engine.dataFile[0]);
#else

#if RETRO_PLATFORM == RETRO_WIIU
    // Prefer packaged metadata if present. metadata.txt is added by the packer
    // and may contain a `game_folder=Sonic1` or `game_folder=Sonic2` line.
    char metaBuf[0x400];
    bool metaFound = false;

    // Diagnostic helper: list directory contents for common mount points
    auto LogDirContents = [&](const char *path) {
        DIR *d = opendir(path);
        if (!d) {
            printLog("dir missing: %s", path);
            return;
        }
        printLog("dir listing: %s", path);
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            printLog(" - %s", ent->d_name);
        }
        closedir(d);
    };

    // If the launcher path was recorded (argv[0]), prefer using it to
    // determine which packaged app folder we're running from. The packer
    // writes the RPX and metadata into distinct internal folders
    // (`RSDKv4_Sonic1` / `RSDKv4_Sonic2`), so check for those markers.
    if (launchPath[0]) {
        if (strstr(launchPath, "RSDKv4_Sonic1") != NULL) {
            StrCopy(dest, "/vol/external01/RSDKv4_Sonic1/");
            StrAdd(dest, Engine.dataFile[0]);
            metaFound = true;
            printLog("launchPath indicates RSDKv4_Sonic1 -> %s", dest);
        }
        else if (strstr(launchPath, "RSDKv4_Sonic2") != NULL) {
            StrCopy(dest, "/vol/external01/RSDKv4_Sonic2/");
            StrAdd(dest, Engine.dataFile[0]);
            metaFound = true;
            printLog("launchPath indicates RSDKv4_Sonic2 -> %s", dest);
        }
    }

    // Diagnostic: always log the recorded launchPath so we can see what argv[0] contains on the console
    if (launchPath[0])
        printLog("launchPath='%s'", launchPath);

    // Diagnostic: enumerate likely mount points so we can see where package files are exposed on-device
    printLog("enumerating likely mount points for debugging metadata visibility");
    LogDirContents("/");
    LogDirContents("/content");
    LogDirContents("/content/apps");
    LogDirContents("/content/apps/RSDKv4_Sonic1");
    LogDirContents("/content/apps/RSDKv4_Sonic2");
    LogDirContents("/code");
    LogDirContents("/vol");
    LogDirContents("/vol/external01");
    LogDirContents("/vol/external01/wiiu");
    LogDirContents("/vol/external01/wiiu/apps");
    // Also show BASE_PATH if defined
#if defined(BASE_PATH)
    printLog("BASE_PATH=%s", BASE_PATH);
#endif

    // Compile-time pack flag: if the binary was built specifically for Sonic1/2,
    // prefer that external folder immediately. This covers the case where argv[0]
    // doesn't contain a useful path (eg. 'root.rpx') so launchPath detection fails.
#if defined(PACKAGED_GAME)
    if (!metaFound) {
        int packagedVal = PACKAGED_GAME;
        if (packagedVal == 1) {
            StrCopy(dest, "/vol/external01/Sonic1/");
            StrAdd(dest, Engine.dataFile[0]);
            metaFound = true;
            printLog("compile-time PACKAGED_GAME=%d -> %s", packagedVal, dest);
        }
        else if (packagedVal == 2) {
            StrCopy(dest, "/vol/external01/Sonic2/");
            StrAdd(dest, Engine.dataFile[0]);
            metaFound = true;
            printLog("compile-time PACKAGED_GAME=%d -> %s", packagedVal, dest);
        }
    }
#endif

    // If `launchPath` already selected a `dest`, trust it and skip metadata scanning
    if (!metaFound) {
        // Try a few likely locations for metadata inside a WUHB or next to the RPX.
            FileIO *metaFile = NULL;
            // Also try the per-package internal app folders created by the packer
            const char *metaCandidates[] = {
                "/code/metadata.txt",
                "/vol/external01/wiiu/apps/RSDKv4_Sonic1/metadata.txt",
                "/vol/external01/wiiu/apps/RSDKv4_Sonic2/metadata.txt",
                "/vol/external01/wiiu/apps/RSDKv4/metadata.txt",
                "metadata.txt",
                "/content/apps/RSDKv4/metadata.txt",
                "content/apps/RSDKv4/metadata.txt",
                "/meta/meta.ini",
                NULL,
            };
        const char *metaFoundPath = NULL;

        for (int m = 0; metaCandidates[m]; ++m) {
            // Log each candidate check to help debugging on-device
            printLog("checking metadata candidate: %s", metaCandidates[m]);
            metaFile = fOpen(metaCandidates[m], "rb");
            if (metaFile) {
                metaFoundPath = metaCandidates[m];
                printLog("metadata exists: %s", metaFoundPath);
                break;
            }
            else {
                printLog("metadata missing: %s", metaCandidates[m]);
            }
        }
        if (!metaFile) {
            // Try metadata next to BASE_PATH as a last resort
            char metaPath[0x300];
            StrCopy(metaPath, BASE_PATH);
            StrAdd(metaPath, "metadata.txt");
            metaFile = fOpen(metaPath, "rb");
            if (metaFile)
                metaFoundPath = metaPath;
        }

        if (metaFile) {
            int read = fRead(metaBuf, 1, sizeof(metaBuf) - 1, metaFile);
            metaBuf[read] = '\0';
            fClose(metaFile);

            const char *key = "game_folder=";
            char *pos       = strstr(metaBuf, key);
            if (pos) {
                pos += strlen(key);
                char folder[0x100];
                int i = 0;
                while (*pos && *pos != '\n' && *pos != '\r' && i < (int)sizeof(folder) - 1) {
                    folder[i++] = *pos++;
                }
                folder[i] = '\0';
                if (i > 0) {
                    // Try likely mounted paths inside a WUHB: /vol/external01/<folder>/Data.rsdk
                    char extPath[0x300];
                    sprintf(extPath, "/vol/external01/%s/%s", folder, Engine.dataFile[0]);
                    FileIO *extFile = fOpen(extPath, "rb");
                    if (extFile) {
                        fClose(extFile);
                        StrCopy(dest, "/vol/external01/");
                        StrAdd(dest, folder);
                        StrAdd(dest, "/");
                        StrAdd(dest, Engine.dataFile[0]);
                        metaFound = true;
                        printLog("metadata: found %s -> game_folder=%s; using %s", metaFoundPath ? metaFoundPath : "(unknown)", folder, dest);
                    }
                }
            }
        }

        // Diagnostic: log the recorded launchPath so we can see what argv[0] contains on the console
        if (launchPath[0])
            printLog("launchPath='%s'", launchPath);

        if (!metaFound) {
            // Fall back to explicit Sonic1 / Sonic2 checks so SD users can keep per-game folders
            char extPath[0x200];
            FileIO *extFile = NULL;

            // Prefer Sonic2 if present; check Sonic2 first to avoid Sonic1 shadowing Sonic2
            sprintf(extPath, "/vol/external01/Sonic2/%s", Engine.dataFile[0]);
            extFile = fOpen(extPath, "rb");
            if (extFile) {
                fClose(extFile);
                StrCopy(dest, "/vol/external01/Sonic2/");
                StrAdd(dest, Engine.dataFile[0]);
                printLog("fallback: found /vol/external01/Sonic2/%s", Engine.dataFile[0]);
            }
            else {
                sprintf(extPath, "/vol/external01/Sonic1/%s", Engine.dataFile[0]);
                extFile = fOpen(extPath, "rb");
                if (extFile) {
                    fClose(extFile);
                    StrCopy(dest, "/vol/external01/Sonic1/");
                    StrAdd(dest, Engine.dataFile[0]);
                    printLog("fallback: found /vol/external01/Sonic1/%s", Engine.dataFile[0]);
                }
                else {
                    StrCopy(dest, BASE_PATH);
                    StrAdd(dest, Engine.dataFile[0]);
                    printLog("fallback: no external Data.rsdk found; using BASE_PATH %s", dest);
                }
            }
        }
    }
    else {
        printLog("launchPath preselected dest; skipping metadata/fallback checks -> %s", dest);
    }
#else

    StrCopy(dest, BASE_PATH);
    StrAdd(dest, Engine.dataFile[0]);
#endif
#endif

#if RETRO_PLATFORM == RETRO_WIIU
    // Attempt to locate a `mods/` folder inside the mounted WUHB content
    // Common mount points vary by launcher; check likely locations and pick the first that exists.
    const char *modCandidates[] = {
        // First, check WUHB-wide mods folder exposed at /vol/content/mods/
        "/vol/content/mods/",
        // Common WUHB / content mount points
        "/vol/content/apps/RSDKv4_Sonic1/mods/",
        "/vol/content/apps/RSDKv4_Sonic2/mods/",
        "/vol/content/apps/RSDKv4/mods/",
        "/content/apps/RSDKv4_Sonic1/mods/",
        "/content/apps/RSDKv4_Sonic2/mods/",
        "/content/apps/RSDKv4/mods/",
        // Some launchers expose the WUHB under /vol/external01/wiiu/apps
        "/vol/external01/wiiu/apps/RSDKv4_Sonic1/mods/",
        "/vol/external01/wiiu/apps/RSDKv4_Sonic2/mods/",
        "/vol/external01/wiiu/apps/RSDKv4/mods/",
        // Legacy per-game folders next to mounted Data.rsdk
        "/vol/external01/Sonic1/mods/",
        "/vol/external01/Sonic2/mods/",
        NULL
    };

    for (int mi = 0; modCandidates[mi]; ++mi) {
        DIR *d = opendir(modCandidates[mi]);
        if (d) {
            closedir(d);
            // copy into global `modsPath` so ModAPI will use it
            StrCopy(modsPath, modCandidates[mi]);
            printLog("modsPath set to %s", modsPath);

            // Print the chosen mods directory contents to aid debugging on-device
            {
                DIR *dd = opendir(modsPath);
                if (dd) {
                    struct dirent *e;
                    printLog("modsPath contents (%s):", modsPath);
                    while ((e = readdir(dd)) != NULL) {
                        printLog(" - %s", e->d_name);
                    }
                    closedir(dd);
                }
                else {
                    printLog("modsPath exists but opendir failed: %s", modsPath);
                }
            }
            break;
        }
    }

    // If no modsPath was found by directory, try to detect a modconfig.ini
    // inside the likely mount points (some launchers expose files but not
    // directories). This ensures mods included in a WUHB are detected.
    if (!modsPath[0]) {
        for (int mi = 0; modCandidates[mi]; ++mi) {
            char cfgPath[0x300];
            sprintf(cfgPath, "%smodconfig.ini", modCandidates[mi]);
            FileIO *cfgFile = fOpen(cfgPath, "rb");
            if (cfgFile) {
                fClose(cfgFile);
                StrCopy(modsPath, modCandidates[mi]);
                printLog("modsPath set (via modconfig) to %s", modsPath);
                break;
            }
        }
    }

#if RETRO_USE_MOD_LOADER
    // Initialize mods after we've determined the modsPath so the loader can
    // discover packaged mods exposed by the runtime environment immediately
    // during startup instead of waiting for a later reload.
    initMods();
#endif
#endif

    CheckRSDKFile(dest);
#else
    CheckRSDKFile("Data.rsdk");
#endif

#if !RETRO_USE_ORIGINAL_CODE
    for (int i = 1; i < RETRO_PACK_COUNT; ++i) {
        if (!StrComp(Engine.dataFile[i], "")) {
            StrCopy(dest, BASE_PATH);
            StrAdd(dest, Engine.dataFile[i]);
            CheckRSDKFile(dest);
        }
    }
#endif

    gameMode = ENGINE_MAINGAME;
    running  = false;
#if !RETRO_USE_ORIGINAL_CODE
    bool skipStart = skipStartMenu;
#endif
    SaveGame *saveGame = (SaveGame *)saveRAM;

    if (LoadGameConfig("Data/Game/GameConfig.bin")) {
        if (InitRenderDevice()) {
            if (InitAudioPlayback()) {
                InitFirstStage();
                ClearScriptData();
                initialised = true;
                running     = true;

#if !RETRO_USE_ORIGINAL_CODE
                if ((startList != 0xFF && startList) || (startStage != 0xFF && startStage) || startPlayer != 0xFF) {
                    skipStart = true;
                    InitStartingStage(startList == 0xFF ? STAGELIST_PRESENTATION : startList, startStage == 0xFF ? 0 : startStage,
                                      startPlayer == 0xFF ? 0 : startPlayer);
                }
                else if (startSave != 0xFF && startSave < 4) {
                    if (startSave == 0) {
                        SetGlobalVariableByName("options.saveSlot", 0);
                        SetGlobalVariableByName("options.gameMode", 0);

                        SetGlobalVariableByName("options.stageSelectFlag", 0);
                        SetGlobalVariableByName("player.lives", 3);
                        SetGlobalVariableByName("player.score", 0);
                        SetGlobalVariableByName("player.scoreBonus", 50000);
                        SetGlobalVariableByName("specialStage.emeralds", 0);
                        SetGlobalVariableByName("specialStage.listPos", 0);
                        SetGlobalVariableByName("stage.player2Enabled", 0);
                        SetGlobalVariableByName("lampPostID", 0); // For S1
                        SetGlobalVariableByName("starPostID", 0); // For S2
                        SetGlobalVariableByName("options.vsMode", 0);

                        SetGlobalVariableByName("specialStage.nextZone", 0);
                        InitStartingStage(STAGELIST_REGULAR, 0, 0);
                    }
                    else {
                        SetGlobalVariableByName("options.saveSlot", startSave);
                        SetGlobalVariableByName("options.gameMode", 1);
                        int slot = (startSave - 1) << 3;

                        SetGlobalVariableByName("options.stageSelectFlag", false);
                        SetGlobalVariableByName("player.lives", saveGame->files[slot].lives);
                        SetGlobalVariableByName("player.score", saveGame->files[slot].score);
                        SetGlobalVariableByName("player.scoreBonus", saveGame->files[slot].scoreBonus);
                        SetGlobalVariableByName("specialStage.emeralds", saveGame->files[slot].emeralds);
                        SetGlobalVariableByName("specialStage.listPos", saveGame->files[slot].specialStageID);
                        SetGlobalVariableByName("stage.player2Enabled", saveGame->files[slot].characterID == 3);
                        SetGlobalVariableByName("lampPostID", 0); // For S1
                        SetGlobalVariableByName("starPostID", 0); // For S2
                        SetGlobalVariableByName("options.vsMode", 0);

                        int nextStage = saveGame->files[slot].stageID;
                        if (nextStage >= 0x80) {
                            SetGlobalVariableByName("specialStage.nextZone", nextStage - 0x81);
                            InitStartingStage(STAGELIST_SPECIAL, saveGame->files[slot].specialStageID, saveGame->files[slot].characterID);
                        }
                        else if (nextStage >= 1) {
                            SetGlobalVariableByName("specialStage.nextZone", nextStage - 1);
                            InitStartingStage(STAGELIST_REGULAR, nextStage - 1, saveGame->files[slot].characterID);
                        }
                        else {
                            saveGame->files[slot].characterID    = 0;
                            saveGame->files[slot].lives          = 3;
                            saveGame->files[slot].score          = 0;
                            saveGame->files[slot].scoreBonus     = 50000;
                            saveGame->files[slot].stageID        = 0;
                            saveGame->files[slot].emeralds       = 0;
                            saveGame->files[slot].specialStageID = 0;
                            saveGame->files[slot].unused         = 0;

                            SetGlobalVariableByName("specialStage.nextZone", 0);
                            InitStartingStage(STAGELIST_REGULAR, 0, 0);
                        }
                    }
                    skipStartMenu = true;
                }
#endif
            }
        }
    }

#if !RETRO_USE_ORIGINAL_CODE
    gameType = GAME_SONIC2;
    if (strstr(gameWindowText, "Sonic 1")) {
        gameType = GAME_SONIC1;
    }
#endif

    InitNativeObjectSystem();

#if !RETRO_USE_ORIGINAL_CODE
    // Calculate Skip frame
    int lower        = getLowerRate(targetRefreshRate, refreshRate);
    renderFrameIndex = targetRefreshRate / lower;
    skipFrameIndex   = refreshRate / lower;

    ReadSaveRAMData();

    if (Engine.gameType == GAME_SONIC1) {
        AddAchievement("Ramp Ring Acrobatics",
                       "Without touching the ground,\rcollect all the rings in a\rtrapezoid formation in Green\rHill Zone Act 1");
        AddAchievement("Blast Processing", "Clear Green Hill Zone Act 1\rin under 30 seconds");
        AddAchievement("Secret of Marble Zone", "Travel though a secret\rroom in Marbale Zone Act 3");
        AddAchievement("Block Buster", "Break 16 blocks in a row\rwithout stopping");
        AddAchievement("Ring King", "Collect 200 Rings");
        AddAchievement("Secret of Labyrinth Zone", "Activate and ride the\rhidden platform in\rLabyrinth Zone Act 1");
        AddAchievement("Flawless Pursuit", "Clear the boss in Labyrinth\rZone without getting hurt");
        AddAchievement("Bombs Away", "Without touching the ground,\rcollect all the rings in a trapezoid formation in GHZ1");
        AddAchievement("Chaos Connoisseur", "Collect all the chaos\remeralds");
        AddAchievement("Hidden Transporter", "Without touching the ground,\rcollect all the rings in a trapezoid formation in GHZ1");
        AddAchievement("One For the Road", "As a parting gift, land a\rfinal hit on Dr. Eggman's\rescaping Egg Mobile");
        AddAchievement("Beat The Clock", "Clear the Time Attack\rmode in less than 45\rminutes");
    }
    else if (Engine.gameType == GAME_SONIC2) {
        AddAchievement("Quick Run", "Complete Emerald Hill\rZone Act 1 in under 35\rseconds");
        AddAchievement("100% Chemical Free", "Complete Chemical Plant\rwithout going underwater");
        AddAchievement("Early Bird Special", "Collect all the Chaos\rEmeralds before Chemical\rPlant");
        AddAchievement("Superstar", "Complete any Act as\rSuper Sonic");
        AddAchievement("Hit it Big", "Get a jackpot on the Casino Night slot machines");
        AddAchievement("Bop Non-stop", "Defeat any boss in 8\rconsecutive hits without\rtouching he ground");
        AddAchievement("Perfectionist", "Get a Perfect Bonus by\rcollecting every Ring in an\rAct");
        AddAchievement("A Secret Revealed", "Find and complete\rHidden Palace Zone");
        AddAchievement("Head 2 Head", "Win a 2P Versus race\ragainst a friend");
        AddAchievement("Metropolis Master", "Complete Any Metropolis\rZone Act without getting\rhurt");
        AddAchievement("Scrambled Egg", "Defeat Dr. Eggman's Boss\rAttack moed in under 7\rminutes");
        AddAchievement("Beat the Clock", "Complete the Time Attack\rmode in less than 45\rminutes");
    }

    if (skipStartMenu)
        Engine.gameMode = ENGINE_MAINGAME;
    else
        Engine.gameMode = ENGINE_WAIT;
#endif
}

void RetroEngine::Run()
{
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 frameStart = SDL_GetPerformanceCounter(), frameEnd = SDL_GetPerformanceCounter();
    Uint64 frameStartBlunt = SDL_GetTicks(), frameEndBlunt = SDL_GetTicks();
    float frameDelta      = 0.0f;
    float frameDeltaBlunt = 0.0f;
    Engine.deltaTime      = 0.0f;

#if RETRO_PLATFORM == RETRO_WIIU
    while (running && WiiU_ProcIsRunning()) {
#if defined(__WUT__) && !defined(RETRO_WIIU_CF_AROMA)
        // Handle ProcUI messages so the app can release foreground resources
        // (MEM1, foreground bucket, etc.) and respond to exit requests.
        int procStatus = ProcUIProcessMessages(FALSE);
        switch (procStatus) {
            case PROCUI_STATUS_RELEASE_FOREGROUND:
                // Allow the app to free foreground resources first.
                WiiU_OnReleaseForeground();
                ProcUIDrawDoneRelease();
                break;
            case PROCUI_STATUS_EXITING:
                // Requesting an exit: give app a chance to clean up and then
                // shut down ProcUI. Instead of forcing the main loop to stop
                // immediately, post an SDL_QUIT event so the normal SDL event
                // handling will observe it and allow the app to exit only
                // after the SDL_QUIT event is processed.
                WiiU_OnReleaseForeground();
                ProcUIShutdown();
                // devkitPro's ProcUI implementation can be buggy and apps
                // may not receive an SDL_QUIT event. Ensure SDL gets notified
                // so SDL-based event loops and cleanup paths run immediately.
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
                {
                    SDL_Event ev;
                    SDL_zero(ev);
                    ev.type = SDL_QUIT;
                    SDL_PushEvent(&ev);
                }
#endif
                break;
            case PROCUI_STATUS_IN_FOREGROUND:
                WiiU_OnAcquireForeground();
                break;
            default: break;
        }
#endif
#else
    while (running) {
#endif
#if !RETRO_USE_ORIGINAL_CODE
        frameStartBlunt = SDL_GetTicks();
        frameDeltaBlunt = frameStartBlunt - frameEndBlunt;
        ++frameDeltaBlunt;
        if (frameDeltaBlunt < 1000.0f / (float)refreshRate) {
            SDL_Delay(1000.0f / (float)refreshRate - frameDeltaBlunt);
            continue;
        }

        frameStart = SDL_GetPerformanceCounter();
        frameDelta = frameStart - frameEnd;
        if (frameDelta < frequency / (float)refreshRate) {
            continue;
        }
        frameEnd         = SDL_GetPerformanceCounter();
        frameEndBlunt    = SDL_GetTicks();
        Engine.deltaTime = 0.016666668;
#endif
        running = processEvents();

#if !RETRO_USE_ORIGINAL_CODE
        for (int s = 0; s < gameSpeed; ++s) {
            ProcessInput();
#endif

#if !RETRO_USE_ORIGINAL_CODE
            if (!masterPaused || frameStep) {
#endif
                ProcessNativeObjects();
#if !RETRO_USE_ORIGINAL_CODE
            }
#endif
        }

#if !RETRO_USE_ORIGINAL_CODE
        if (!masterPaused || frameStep) {
#endif
            FlipScreen();

#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_OPENGL && RETRO_USING_SDL2
            SDL_GL_SwapWindow(Engine.window);
#endif
            frameStep = false;
        }
#endif

        int hapticID = GetHapticEffectNum();
        if (hapticID >= 0) {
            // playHaptics(hapticID);
        }
        else if (hapticID == HAPTIC_STOP) {
            // stopHaptics();
        }
    }

    ReleaseAudioDevice();
    ReleaseRenderDevice();
#if !RETRO_USE_ORIGINAL_CODE
    ReleaseInputDevices();
#if RETRO_USE_NETWORKING
    disconnectNetwork();
#endif
    writeSettings();
#if RETRO_USE_MOD_LOADER
    saveMods();
#endif
#endif

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    SDL_Quit();
#endif
}

#if RETRO_USE_MOD_LOADER
const tinyxml2::XMLElement *firstXMLChildElement(tinyxml2::XMLDocument *doc, const tinyxml2::XMLElement *elementPtr, const char *name)
{
    if (doc) {
        if (!elementPtr)
            return doc->FirstChildElement(name);
        else
            return elementPtr->FirstChildElement(name);
    }
    return NULL;
}

const tinyxml2::XMLElement *nextXMLSiblingElement(tinyxml2::XMLDocument *doc, const tinyxml2::XMLElement *elementPtr, const char *name)
{
    if (doc) {
        if (!elementPtr)
            return doc->NextSiblingElement(name);
        else
            return elementPtr->NextSiblingElement(name);
    }
    return NULL;
}

const tinyxml2::XMLAttribute *findXMLAttribute(const tinyxml2::XMLElement *elementPtr, const char *name) { return elementPtr->FindAttribute(name); }
const char *getXMLAttributeName(const tinyxml2::XMLAttribute *attributePtr) { return attributePtr->Name(); }
int getXMLAttributeValueInt(const tinyxml2::XMLAttribute *attributePtr) { return attributePtr->IntValue(); }
bool getXMLAttributeValueBool(const tinyxml2::XMLAttribute *attributePtr) { return attributePtr->BoolValue(); }
const char *getXMLAttributeValueString(const tinyxml2::XMLAttribute *attributePtr) { return attributePtr->Value(); }

void RetroEngine::LoadXMLVariables()
{
    FileInfo info;
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement      = firstXMLChildElement(doc, nullptr, "game");
                const tinyxml2::XMLElement *variablesElement = firstXMLChildElement(doc, gameElement, "variables");
                if (variablesElement) {
                    const tinyxml2::XMLElement *varElement = firstXMLChildElement(doc, variablesElement, "variable");
                    if (varElement) {
                        do {
                            const tinyxml2::XMLAttribute *nameAttr = findXMLAttribute(varElement, "name");
                            const char *varName                    = "unknownVariable";
                            if (nameAttr)
                                varName = getXMLAttributeValueString(nameAttr);

                            const tinyxml2::XMLAttribute *valAttr = findXMLAttribute(varElement, "value");
                            int varValue                          = 0;
                            if (valAttr)
                                varValue = getXMLAttributeValueInt(valAttr);

                            StrCopy(globalVariableNames[globalVariablesCount], varName);
                            globalVariables[globalVariablesCount] = varValue;
                            globalVariablesCount++;

                        } while ((varElement = nextXMLSiblingElement(doc, varElement, "variable")));
                    }
                }
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
void RetroEngine::LoadXMLPalettes()
{
    FileInfo info;
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement    = firstXMLChildElement(doc, nullptr, "game");
                const tinyxml2::XMLElement *paletteElement = firstXMLChildElement(doc, gameElement, "palette");
                if (paletteElement) {
                    const tinyxml2::XMLElement *clrElement = firstXMLChildElement(doc, paletteElement, "color");
                    if (clrElement) {
                        do {
                            const tinyxml2::XMLAttribute *bankAttr = findXMLAttribute(clrElement, "bank");
                            int clrBank                            = 0;
                            if (bankAttr)
                                clrBank = getXMLAttributeValueInt(bankAttr);

                            const tinyxml2::XMLAttribute *indAttr = findXMLAttribute(clrElement, "index");
                            int clrInd                            = 0;
                            if (indAttr)
                                clrInd = getXMLAttributeValueInt(indAttr);

                            const tinyxml2::XMLAttribute *rAttr = findXMLAttribute(clrElement, "r");
                            int clrR                            = 0;
                            if (rAttr)
                                clrR = getXMLAttributeValueInt(rAttr);

                            const tinyxml2::XMLAttribute *gAttr = findXMLAttribute(clrElement, "g");
                            int clrG                            = 0;
                            if (gAttr)
                                clrG = getXMLAttributeValueInt(gAttr);

                            const tinyxml2::XMLAttribute *bAttr = findXMLAttribute(clrElement, "b");
                            int clrB                            = 0;
                            if (bAttr)
                                clrB = getXMLAttributeValueInt(bAttr);

                            SetPaletteEntry(clrBank, clrInd, clrR, clrG, clrB);

                        } while ((clrElement = nextXMLSiblingElement(doc, clrElement, "color")));
                    }
                }
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
void RetroEngine::LoadXMLObjects()
{
    FileInfo info;
    modObjCount = 0;

    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement    = firstXMLChildElement(doc, nullptr, "game");
                const tinyxml2::XMLElement *objectsElement = firstXMLChildElement(doc, gameElement, "objects");
                if (objectsElement) {
                    const tinyxml2::XMLElement *objElement = firstXMLChildElement(doc, objectsElement, "object");
                    if (objElement) {
                        do {
                            const tinyxml2::XMLAttribute *nameAttr = findXMLAttribute(objElement, "name");
                            const char *objName                    = "unknownObject";
                            if (nameAttr)
                                objName = getXMLAttributeValueString(nameAttr);

                            const tinyxml2::XMLAttribute *scrAttr = findXMLAttribute(objElement, "script");
                            const char *objScript                 = "unknownObject.txt";
                            if (scrAttr)
                                objScript = getXMLAttributeValueString(scrAttr);

                            byte flags = 0;

                            // forces the object to be loaded, this means the object doesn't have to be and *SHOULD NOT* be in the stage object list
                            // if it is, it'll cause issues!!!!
                            const tinyxml2::XMLAttribute *loadAttr = findXMLAttribute(objElement, "forceLoad");
                            int objForceLoad                       = false;
                            if (loadAttr)
                                objForceLoad = getXMLAttributeValueBool(loadAttr);

                            flags |= (objForceLoad & 1);

                            StrCopy(modTypeNames[modObjCount], objName);
                            StrCopy(modScriptPaths[modObjCount], objScript);
                            modScriptFlags[modObjCount] = flags;
                            modObjCount++;

                        } while ((objElement = nextXMLSiblingElement(doc, objElement, "object")));
                    }
                }
            }
            else {
                printLog("Failed to parse Game.xml File!");
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
void RetroEngine::LoadXMLSoundFX()
{
    FileInfo info;
    FileInfo infoStore;
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement   = firstXMLChildElement(doc, nullptr, "game");
                const tinyxml2::XMLElement *soundsElement = firstXMLChildElement(doc, gameElement, "sounds");
                if (soundsElement) {
                    const tinyxml2::XMLElement *sfxElement = firstXMLChildElement(doc, soundsElement, "soundfx");
                    if (sfxElement) {
                        do {
                            const tinyxml2::XMLAttribute *nameAttr = findXMLAttribute(sfxElement, "name");
                            const char *sfxName                    = "unknownSFX";
                            if (nameAttr)
                                sfxName = getXMLAttributeValueString(nameAttr);

                            const tinyxml2::XMLAttribute *valAttr = findXMLAttribute(sfxElement, "path");
                            const char *sfxPath                   = "unknownSFX.wav";
                            if (valAttr)
                                sfxPath = getXMLAttributeValueString(valAttr);

                            SetSfxName(sfxName, globalSFXCount);

                            GetFileInfo(&infoStore);
                            CloseFile();
                            LoadSfx((char *)sfxPath, globalSFXCount);
                            SetFileInfo(&infoStore);
                            globalSFXCount++;

                        } while ((sfxElement = nextXMLSiblingElement(doc, sfxElement, "soundfx")));
                    }
                }
            }
            else {
                printLog("Failed to parse Game.xml File!");
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
void RetroEngine::LoadXMLPlayers(TextMenu *menu)
{
    FileInfo info;

    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement    = firstXMLChildElement(doc, nullptr, "game");
                const tinyxml2::XMLElement *playersElement = firstXMLChildElement(doc, gameElement, "players");
                if (playersElement) {
                    const tinyxml2::XMLElement *plrElement = firstXMLChildElement(doc, playersElement, "player");
                    if (plrElement) {
                        do {
                            const tinyxml2::XMLAttribute *nameAttr = findXMLAttribute(plrElement, "name");
                            const char *plrName                    = "unknownPlayer";
                            if (nameAttr)
                                plrName = getXMLAttributeValueString(nameAttr);

                            if (menu)
                                AddTextMenuEntry(menu, plrName);
                            else
                                StrCopy(playerNames[playerCount++], plrName);

                        } while ((plrElement = nextXMLSiblingElement(doc, plrElement, "player")));
                    }
                }
            }
            else {
                printLog("Failed to parse Game.xml File!");
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
void RetroEngine::LoadXMLStages(TextMenu *menu, int listNo)
{
    FileInfo info;
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;

        SetActiveMod(m);
        if (LoadFile("Data/Game/Game.xml", &info)) {
            tinyxml2::XMLDocument *doc = new tinyxml2::XMLDocument;

            char *xmlData = new char[info.fileSize + 1];
            FileRead(xmlData, info.fileSize);
            xmlData[info.fileSize] = 0;

            bool success = doc->Parse(xmlData) == tinyxml2::XML_SUCCESS;

            if (success) {
                const tinyxml2::XMLElement *gameElement = firstXMLChildElement(doc, nullptr, "game");
                const char *elementNames[]              = { "presentationStages", "regularStages", "bonusStages", "specialStages" };

                for (int l = 0; l < STAGELIST_MAX; ++l) {
                    const tinyxml2::XMLElement *listElement = firstXMLChildElement(doc, gameElement, elementNames[l]);
                    if (listElement) {
                        const tinyxml2::XMLElement *stgElement = firstXMLChildElement(doc, listElement, "stage");
                        if (stgElement) {
                            do {
                                const tinyxml2::XMLAttribute *nameAttr = findXMLAttribute(stgElement, "name");
                                const char *stgName                    = "unknownStage";
                                if (nameAttr)
                                    stgName = getXMLAttributeValueString(nameAttr);

                                const tinyxml2::XMLAttribute *folderAttr = findXMLAttribute(stgElement, "folder");
                                const char *stgFolder                    = "unknownStageFolder";
                                if (nameAttr)
                                    stgFolder = getXMLAttributeValueString(folderAttr);

                                const tinyxml2::XMLAttribute *idAttr = findXMLAttribute(stgElement, "id");
                                const char *stgID                    = "unknownStageID";
                                if (idAttr)
                                    stgID = getXMLAttributeValueString(idAttr);

                                const tinyxml2::XMLAttribute *highlightAttr = findXMLAttribute(stgElement, "highlight");
                                bool stgHighlighted                         = false;
                                if (stgHighlighted)
                                    stgHighlighted = getXMLAttributeValueBool(highlightAttr);

                                if (menu) {
                                    if (listNo == 3 || listNo == 4) {
                                        if ((listNo == 4 && l == 2) || (listNo == 3 && l == 3)) {
                                            AddTextMenuEntry(menu, stgName);
                                            menu->entryHighlight[menu->rowCount - 1] = stgHighlighted;
                                        }
                                    }
                                    else if (listNo == l + 1) {
                                        AddTextMenuEntry(menu, stgName);
                                        menu->entryHighlight[menu->rowCount - 1] = stgHighlighted;
                                    }
                                }
                                else {
                                    StrCopy(stageList[l][stageListCount[l]].name, stgName);
                                    StrCopy(stageList[l][stageListCount[l]].folder, stgFolder);
                                    StrCopy(stageList[l][stageListCount[l]].id, stgID);
                                    stageList[l][stageListCount[l]].highlighted = stgHighlighted;

                                    stageListCount[l]++;
                                }

                            } while ((stgElement = nextXMLSiblingElement(doc, stgElement, "stage")));
                        }
                    }
                }
            }
            else {
                printLog("Failed to parse Game.xml File!");
            }

            delete[] xmlData;
            delete doc;

            CloseFile();
        }
    }
    SetActiveMod(-1);
}
#endif

bool RetroEngine::LoadGameConfig(const char *filePath)
{
    FileInfo info;
    byte fileBuffer  = 0;
    byte fileBuffer2 = 0;
    char strBuffer[0x40];
    StrCopy(gameWindowText, "Retro-Engine"); // this is the default window name

    globalVariablesCount = 0;
#if RETRO_USE_MOD_LOADER
    playerCount = 0;
#endif

    bool loaded = LoadFile(filePath, &info);
    if (loaded) {
        FileRead(&fileBuffer, 1);
        FileRead(gameWindowText, fileBuffer);
        gameWindowText[fileBuffer] = 0;

        FileRead(&fileBuffer, 1);
        FileRead(gameDescriptionText, fileBuffer);
        gameDescriptionText[fileBuffer] = 0;

        byte buf[3];
        for (int c = 0; c < 0x60; ++c) {
            FileRead(buf, 3);
            SetPaletteEntry(-1, c, buf[0], buf[1], buf[2]);
        }

        // Read Obect Names
        byte objectCount = 0;
        FileRead(&objectCount, 1);
        for (byte o = 0; o < objectCount; ++o) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        // Read Script Paths
        for (byte s = 0; s < objectCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        byte varCount = 0;
        FileRead(&varCount, 1);
        globalVariablesCount = varCount;
        for (int v = 0; v < varCount; ++v) {
            // Read Variable Name
            FileRead(&fileBuffer, 1);
            FileRead(&globalVariableNames[v], fileBuffer);
            globalVariableNames[v][fileBuffer] = 0;

            // Read Variable Value
            FileRead(&fileBuffer2, 1);
            globalVariables[v] = fileBuffer2 << 0;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 8;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 16;
            FileRead(&fileBuffer2, 1);
            globalVariables[v] += fileBuffer2 << 24;
        }

#if RETRO_USE_MOD_LOADER
        if (Engine.devMenu)
            SetGlobalVariableByName("options.devMenuFlag", true);
        else
            SetGlobalVariableByName("options.devMenuFlag", false);
#endif

        // Read SFX
        byte globalSFXCount = 0;
        FileRead(&globalSFXCount, 1);
        for (int s = 0; s < globalSFXCount; ++s) { // SFX Names
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }
        for (byte s = 0; s < globalSFXCount; ++s) { // SFX Paths
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;
        }

        // Read Player Names
        byte plrCount = 0;
        FileRead(&plrCount, 1);
        for (byte p = 0; p < plrCount; ++p) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
#if RETRO_USE_MOD_LOADER
            strBuffer[fileBuffer] = 0;
            StrCopy(playerNames[p], strBuffer);
#endif
        }

        for (byte c = 0; c < 4; ++c) {
            // Special Stages are stored as cat 2 in file, but cat 3 in game :(
            int cat = c;
            if (c == 2)
                cat = 3;
            else if (c == 3)
                cat = 2;
            stageListCount[cat] = 0;
            FileRead(&fileBuffer, 1);
            stageListCount[cat] = fileBuffer;
            for (byte s = 0; s < stageListCount[cat]; ++s) {

                // Read Stage Folder
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].folder, fileBuffer);
                stageList[cat][s].folder[fileBuffer] = 0;

                // Read Stage ID
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].id, fileBuffer);
                stageList[cat][s].id[fileBuffer] = 0;

                // Read Stage Name
                FileRead(&fileBuffer, 1);
                FileRead(&stageList[cat][s].name, fileBuffer);
                stageList[cat][s].name[fileBuffer] = 0;

                // Read Stage Mode
                FileRead(&fileBuffer, 1);
                stageList[cat][s].highlighted = fileBuffer;
            }
        }

        CloseFile();
#if RETRO_USE_MOD_LOADER
        LoadXMLVariables();
        LoadXMLPalettes();
        LoadXMLObjects();
        LoadXMLPlayers(NULL);
        LoadXMLStages(NULL, 0);
#endif
    }

    // These need to be set every time its reloaded
    nativeFunctionCount = 0;
    AddNativeFunction("SetAchievement", SetAchievement);
    AddNativeFunction("SetLeaderboard", SetLeaderboard);
#if RETRO_USE_HAPTICS
    AddNativeFunction("HapticEffect", HapticEffect);
#endif
    AddNativeFunction("Connect2PVS", Connect2PVS);
    AddNativeFunction("Disconnect2PVS", Disconnect2PVS);
    AddNativeFunction("SendEntity", SendEntity);
    AddNativeFunction("SendValue", SendValue);
    AddNativeFunction("ReceiveEntity", ReceiveEntity);
    AddNativeFunction("ReceiveValue", ReceiveValue);
    AddNativeFunction("TransmitGlobal", TransmitGlobal);
    AddNativeFunction("ShowPromoPopup", ShowPromoPopup);

#if RETRO_USE_NETWORKING
    AddNativeFunction("SetNetworkGameName", SetNetworkGameName);
#endif

#if RETRO_USE_MOD_LOADER
    AddNativeFunction("ExitGame", ExitGame);
    AddNativeFunction("FileExists", FileExists);
    AddNativeFunction("OpenModMenu", OpenModMenu); // Opens the dev menu-based mod menu incase you cant be bothered or smth
    AddNativeFunction("AddAchievement", AddGameAchievement);
    AddNativeFunction("SetAchievementDescription", SetAchievementDescription);
    AddNativeFunction("ClearAchievements", ClearAchievements);
    AddNativeFunction("GetAchievementCount", GetAchievementCount);
    AddNativeFunction("GetAchievement", GetAchievement);
    AddNativeFunction("GetAchievementName", GetAchievementName);
    AddNativeFunction("GetAchievementDescription", GetAchievementDescription);
    AddNativeFunction("SetScreenWidth", SetScreenWidth);
    AddNativeFunction("SetWindowScale", SetWindowScale);
    AddNativeFunction("SetWindowFullScreen", SetWindowFullScreen);
    AddNativeFunction("SetWindowBorderless", SetWindowBorderless);
    // AddNativeFunction("ApplyWindowChanges", ApplyWindowChanges); //todo: this prolly tbh
    AddNativeFunction("GetModCount", GetModCount);
    AddNativeFunction("GetModName", GetModName);
    AddNativeFunction("GetModDescription", GetModDescription);
    AddNativeFunction("GetModAuthor", GetModAuthor);
    AddNativeFunction("GetModVersion", GetModVersion);
    AddNativeFunction("GetModActive", GetModActive);
    AddNativeFunction("SetModActive", SetModActive);
    AddNativeFunction("RefreshEngine", RefreshEngine); // Reload engine after changing mod status
#endif

    return loaded;
}
