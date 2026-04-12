#include "RetroEngine.hpp"

#if RETRO_USE_MOD_LOADER
#include <string>
#include <map>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>

std::vector<ModInfo> modList;
int activeMod = -1;

char modsPath[0x100];

bool redirectSave = false;
char savePath[0x100];

char modTypeNames[OBJECT_COUNT][0x40];
char modScriptPaths[OBJECT_COUNT][0x40];
byte modScriptFlags[OBJECT_COUNT];
byte modObjCount = 0;

char playerNames[PLAYER_MAX][0x20];
byte playerCount = 0;

int OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
    return 1;
}

void scanModFolder(ModInfo *info)
{
    if (!info)
        return;



    char modDir[0x200];
    int mLen = StrLength(modsPath);
    if (mLen > 0) {
        if ((mLen >= 4 && StrComp(&modsPath[mLen - 4], "mods") == 0) ||
            (mLen >= 5 && StrComp(&modsPath[mLen - 5], "mods/") == 0)) {
            StrCopy(modDir, modsPath);
            if (modDir[StrLength(modDir) - 1] == '/') modDir[StrLength(modDir) - 1] = 0;
        }
        else {
            if (modsPath[mLen - 1] == '/')
                sprintf(modDir, "%smods", modsPath);
            else
                sprintf(modDir, "%s/mods", modsPath);
        }
    }
    else {
        sprintf(modDir, "mods");
    }
    StrAdd(modDir, "/");
    StrAdd(modDir, info->folder.c_str());

    info->fileMap.clear();
    for (auto const& [key, val] : info->memoryMap) {
        if (val.data) free(val.data);
    }
    info->memoryMap.clear();

    const char *subfolders[] = { "Data", "Scripts", "Bytecode", NULL };

    for (int s = 0; subfolders[s]; ++s) {
        char subfolderPath[0x300];
        sprintf(subfolderPath, "%s/%s", modDir, subfolders[s]);

        DIR *dir = opendir(subfolderPath);
        if (!dir) continue;

        struct Scanner {
            static void scan(const char *modDir, const char *searchPath, ModInfo *info) {
                DIR *d = opendir(searchPath);
                if (!d) return;

                struct dirent *ent;
                while ((ent = readdir(d)) != NULL) {
                    if (ent->d_name[0] == '.') continue;

                    char fullPath[0x400];
                    sprintf(fullPath, "%s/%s", searchPath, ent->d_name);

                    struct stat st;
                    if (stat(fullPath, &st) == 0) {
                        if (S_ISDIR(st.st_mode)) {
                            scan(modDir, fullPath, info);
                        }
                        else if (S_ISREG(st.st_mode)) {
                            char relPath[0x400];
                            int skip = StrLength(modDir) + 1;
                            StrCopy(relPath, &fullPath[skip]);

                            char pathLower[0x400];
                            memset(pathLower, 0, 0x400);
                            for (int c = 0; relPath[c]; ++c) {
                                pathLower[c] = tolower(relPath[c]);
                            }

                            info->fileMap.insert(std::pair<std::string, std::string>(pathLower, fullPath));
                        }
                    }
                }
                closedir(d);
            }
        };

        Scanner::scan(modDir, subfolderPath, info);
        closedir(dir);
    }
}

bool loadMod(ModInfo *info, std::string modsDir, std::string folder, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = "";
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = "";
    info->active  = false;

    const std::string modDir = modsDir + "/" + folder;

    char iniPath[0x300];
    sprintf(iniPath, "%s/mod.ini", modDir.c_str());

    FileIO *f = fOpen(iniPath, "r");
    if (f) {
        fClose(f);
        IniParser modSettings(iniPath, false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        info->folder  = folder;

        char infoBuf[0x100];
        if (modSettings.GetString("", "Name", infoBuf)) info->name = infoBuf;
        if (modSettings.GetString("", "Description", infoBuf)) info->desc = infoBuf;
        if (modSettings.GetString("", "Author", infoBuf)) info->author = infoBuf;
        if (modSettings.GetString("", "Version", infoBuf)) info->version = infoBuf;

        info->active = active;

        scanModFolder(info);

        info->useScripts = false;
        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        if (info->useScripts && info->active)
            forceUseScripts = true;

        info->skipStartMenu = false;
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        if (info->skipStartMenu && info->active)
            skipStartMenu = true;

        info->disableFocusPause = false;
        modSettings.GetBool("", "DisableFocusPause", &info->disableFocusPause);
        if (info->disableFocusPause && info->active)
            disableFocusPause = true;

        info->redirectSave = false;
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave && info->active) {
            char path[0x100];
            sprintf(path, "mods/%s/", folder.c_str());
            info->savePath = path;
        }

        return true;
    }
    return false;
}

void initMods()
{
    printLog("initMods: modsPath='%s'", modsPath);
    for (auto& mod : modList) {
        for (auto const& [key, val] : mod.memoryMap) {
            if (val.data) free(val.data);
        }
    }
    modList.clear();
    forceUseScripts   = forceUseScripts_Config;
    disableFocusPause = disableFocusPause_Config;
    redirectSave      = false;
    sprintf(savePath, "");

    char modDir[0x200];
    int mLen = StrLength(modsPath);
    if (mLen > 0) {
        if ((mLen >= 4 && StrComp(&modsPath[mLen - 4], "mods") == 0) ||
            (mLen >= 5 && StrComp(&modsPath[mLen - 5], "mods/") == 0)) {
            StrCopy(modDir, modsPath);
            if (modDir[StrLength(modDir) - 1] == '/') modDir[StrLength(modDir) - 1] = 0;
        }
        else {
            if (modsPath[mLen - 1] == '/')
                sprintf(modDir, "%smods", modsPath);
            else
                sprintf(modDir, "%s/mods", modsPath);
        }
    }
    else {
        sprintf(modDir, "mods");
    }

    std::string mod_config;
#if RETRO_PLATFORM == RETRO_WIIU
    mod_config = std::string(gamePath) + "/modconfig.ini";
#else
    mod_config = std::string(modDir) + "/modconfig.ini";
#endif

    IniParser modConfig(mod_config.c_str(), false);
    bool hasConfig = (modConfig.items.size() > 0);

    DIR *dir = opendir(modDir);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;

            char fullPath[0x300];
            sprintf(fullPath, "%s/%s", modDir, ent->d_name);

            struct stat st;
            if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                bool active = false;
                if (hasConfig) {
                    modConfig.GetBool("mods", ent->d_name, &active);
                }
                else {
                    active = true;
                }

                ModInfo info;
                if (loadMod(&info, modDir, ent->d_name, active)) {
                    modList.push_back(info);
                }
            }
        }
        closedir(dir);
    }

    printLog("initMods: finished, discovered %d mods", (int)modList.size());

    forceUseScripts   = false;
    skipStartMenu     = skipStartMenu_Config;
    disableFocusPause = disableFocusPause_Config;
    forceUseScripts   = forceUseScripts_Config;
    sprintf(savePath, "");
    redirectSave = false;
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause = true;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
    }

    ReadSaveRAMData();
    ReadUserdata();
}

void saveMods()
{
    char modDir[0x200];
    int mLen = StrLength(modsPath);
    if (mLen > 0) {
        if ((mLen >= 4 && StrComp(&modsPath[mLen - 4], "mods") == 0) ||
            (mLen >= 5 && StrComp(&modsPath[mLen - 5], "mods/") == 0)) {
            StrCopy(modDir, modsPath);
            if (modDir[StrLength(modDir) - 1] == '/') modDir[StrLength(modDir) - 1] = 0;
        }
        else {
            if (modsPath[mLen - 1] == '/')
                sprintf(modDir, "%smods", modsPath);
            else
                sprintf(modDir, "%s/mods", modsPath);
        }
    }
    else {
        sprintf(modDir, "mods");
    }

    std::string mod_config;
#if RETRO_PLATFORM == RETRO_WIIU
    mod_config = std::string(gamePath) + "/modconfig.ini";
#else
    mod_config = std::string(modDir) + "/modconfig.ini";
#endif

    IniParser modConfig;
    for (int m = 0; m < modList.size(); ++m) {
        modConfig.SetBool("mods", modList[m].folder.c_str(), modList[m].active);
    }
    modConfig.Write(mod_config.c_str(), false);
}

void RefreshEngine()
{
    Engine.LoadGameConfig("Data/Game/GameConfig.bin");
#if RETRO_USING_SDL2
    if (Engine.window) {
        char gameTitle[0x40];
        sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile ? "" : " (Using Data Folder)");
        SDL_SetWindowTitle(Engine.window, gameTitle);
    }
#endif
    ClearMeshData();
    ClearTextures(true);

    nativeEntityCountBackup = 0;
    memset(backupEntityList, 0, sizeof(backupEntityList));
    memset(objectEntityBackup, 0, sizeof(objectEntityBackup));

    nativeEntityCountBackupS = 0;
    memset(backupEntityListS, 0, sizeof(backupEntityListS));
    memset(objectEntityBackupS, 0, sizeof(objectEntityBackupS));

    for (int i = 0; i < FONTLIST_COUNT; ++i) {
        fontList[i].count = 2;
    }

    ReleaseStageSfx();
    ReleaseGlobalSfx();
    LoadGlobalSfx();
    InitLocalizedStrings();

    for (nativeEntityPos = 0; nativeEntityPos < nativeEntityCount; ++nativeEntityPos) {
        NativeEntity *entity = &objectEntityBank[activeEntityList[nativeEntityPos]];
        entity->createPtr(entity);
    }

    Engine.gameType = GAME_SONIC2;
    if (strstr(Engine.gameWindowText, "Sonic 1")) {
        Engine.gameType = GAME_SONIC1;
    }

    forceUseScripts   = false;
    skipStartMenu     = skipStartMenu_Config;
    disableFocusPause = disableFocusPause_Config;
    forceUseScripts   = forceUseScripts_Config;
    sprintf(savePath, "");
    redirectSave = false;
    for (int m = 0; m < modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause = true;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
    }

    saveMods();
    ReadSaveRAMData();
    ReadUserdata();
}

void GetModCount() { scriptEng.checkResult = (int)modList.size(); }
void GetModName(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size()) return;
    TextMenu *menu = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].name.c_str());
}
void GetModDescription(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size()) return;
    TextMenu *menu = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].desc.c_str());
}
void GetModAuthor(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size()) return;
    TextMenu *menu = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].author.c_str());
}
void GetModVersion(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size()) return;
    TextMenu *menu = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].version.c_str());
}
void GetModActive(uint *id, int *unused)
{
    scriptEng.checkResult = false;
    if (*id >= modList.size()) return;
    scriptEng.checkResult = modList[*id].active;
}
void SetModActive(uint *id, int *active)
{
    if (*id >= modList.size()) return;
    modList[*id].active = *active;
}

int GetSceneID(byte listID, const char *sceneName)
{
    if (listID >= 3) return -1;
    char scnName[0x40];
    int scnPos = 0, pos = 0;
    while (sceneName[scnPos]) {
        if (sceneName[scnPos] != ' ') scnName[pos++] = sceneName[scnPos];
        ++scnPos;
    }
    scnName[pos] = 0;
    for (int s = 0; s < stageListCount[listID]; ++s) {
        char nameBuffer[0x40];
        scnPos = 0, pos = 0;
        while (stageList[listID][s].name[scnPos]) {
            if (stageList[listID][s].name[scnPos] != ' ') nameBuffer[pos++] = stageList[listID][s].name[scnPos];
            ++scnPos;
        }
        nameBuffer[pos] = 0;
        if (StrComp(scnName, nameBuffer)) return s;
    }
    return -1;
}
#endif
