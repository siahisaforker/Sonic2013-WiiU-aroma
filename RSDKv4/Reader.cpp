#include "RetroEngine.hpp"
#include <map>
#include <string>

RSDKContainer rsdkContainer;

void* modFileMemoryBuffer = NULL;
bool allocatedFileMemoryBuffer = false;

char fileName[0x100];
byte fileBuffer[0x10000];
int fileSize          = 0;
int vFileSize         = 0;
int readPos           = 0;
int readSize          = 0;
int bufferPosition    = 0;
int virtualFileOffset = 0;
bool useEncryption    = false;
byte packID           = 0;
bool cFileUsingDataPack = false;
byte eStringPosA;
byte eStringPosB;
byte eStringNo;
byte eNybbleSwap;
byte encryptionStringA[0x10];
byte encryptionStringB[0x10];

byte* packMemoryBuffers[RETRO_PACK_COUNT] = { NULL };

typedef struct {
  FileIO* fileHandle;
  int fileSize;
} CacheFileHandle;
static CacheFileHandle cHandles[255] = { 0 };

typedef struct {
    byte *data;
    int size;
} CachedLooseFile;

static std::map<std::string, CachedLooseFile> looseFileCache;

FileIO *cFileHandle = nullptr;
bool cFileHandleCanClose = false;

static void CloseCurrentFileHandle()
{
    if (cFileHandle && cFileHandleCanClose)
        fClose(cFileHandle);
    cFileHandle = NULL;
    cFileHandleCanClose = false;
}

static bool CacheDataPack(byte id)
{
    if (id >= RETRO_PACK_COUNT)
        return false;

    if (packMemoryBuffers[id])
        return cHandles[id].fileSize > 0;

    FileIO *packFile = fOpen(rsdkContainer.packNames[id], "rb");
    if (!packFile) {
        printLog("Failed to open DataPack %d for RAM cache", id);
        return false;
    }

    fSeek(packFile, 0, SEEK_END);
    int packSize = (int)fTell(packFile);
    fSeek(packFile, 0, SEEK_SET);
    if (packSize <= 0) {
        fClose(packFile);
        printLog("DataPack %d has invalid size %d", id, packSize);
        return false;
    }

    byte *packBuffer = (byte *)malloc(packSize);
    if (!packBuffer) {
        fClose(packFile);
        printLog("Failed to allocate DataPack %d RAM cache (%d bytes)", id, packSize);
        return false;
    }

    size_t bytesRead = fRead(packBuffer, 1, packSize, packFile);
    fClose(packFile);

    if (bytesRead != (size_t)packSize) {
        free(packBuffer);
        printLog("Failed to read DataPack %d into RAM (%d/%d bytes)", id, (int)bytesRead, packSize);
        return false;
    }

    packMemoryBuffers[id] = packBuffer;
    cHandles[id].fileHandle = NULL;
    cHandles[id].fileSize = packSize;
    printLog("Cached DataPack %d into RAM (%d bytes)", id, packSize);
    return true;
}

static CachedLooseFile *FindCachedLooseFile(const char *path)
{
    std::map<std::string, CachedLooseFile>::iterator it = looseFileCache.find(path);
    if (it == looseFileCache.end())
        return NULL;
    return &it->second;
}

static CachedLooseFile *CacheLooseFile(const char *path, FileIO *fileHandle, int looseFileSize)
{
    CachedLooseFile cachedFile = { NULL, looseFileSize };

    if (looseFileSize > 0) {
        cachedFile.data = (byte *)malloc(looseFileSize);
        if (!cachedFile.data) {
            printLog("Failed to allocate loose file RAM cache '%s' (%d bytes)", path, looseFileSize);
            return NULL;
        }

        size_t bytesRead = fRead(cachedFile.data, 1, looseFileSize, fileHandle);
        if (bytesRead != (size_t)looseFileSize) {
            free(cachedFile.data);
            printLog("Failed to read loose file into RAM '%s' (%d/%d bytes)", path, (int)bytesRead, looseFileSize);
            return NULL;
        }
    }

    std::map<std::string, CachedLooseFile>::iterator inserted = looseFileCache.insert(std::make_pair(std::string(path), cachedFile)).first;
    printLog("Cached loose file into RAM '%s' (%d bytes)", path, looseFileSize);
    return &inserted->second;
}

void CloseRSDKContainers()
{
    CloseCurrentFileHandle();

#if !RETRO_USE_ORIGINAL_CODE
    modFileMemoryBuffer = NULL;
    allocatedFileMemoryBuffer = false;
#endif

    for (int i = 0; i < RETRO_PACK_COUNT; ++i) {
        strcpy(rsdkContainer.packNames[i], "");
        if (packMemoryBuffers[i]) {
            free(packMemoryBuffers[i]);
            packMemoryBuffers[i] = NULL;
        }
    }

    for (std::map<std::string, CachedLooseFile>::iterator it = looseFileCache.begin(); it != looseFileCache.end(); ++it) {
        free(it->second.data);
    }
    looseFileCache.clear();

#if RETRO_USE_MOD_LOADER
    for (int m = 0; m < (int)modList.size(); ++m) {
        for (std::map<std::string, ModMemoryBuffer>::iterator it = modList[m].memoryMap.begin(); it != modList[m].memoryMap.end(); ++it) {
            free(it->second.data);
        }
        modList[m].memoryMap.clear();
    }
#endif

    memset(cHandles, 0, sizeof(cHandles));
    rsdkContainer.packCount = 0;
    rsdkContainer.fileCount = 0;
    cFileUsingDataPack = false;
}

bool CheckRSDKFile(const char *filePath)
{
    FileInfo info;

    char filePathBuffer[0x100];
    sprintf(filePathBuffer, "%s", filePath);
#if RETRO_PLATFORM == RETRO_OSX
    char pathBuf[0x100];
    sprintf(pathBuf, "%s/%s", gamePath, filePathBuffer);
    sprintf(filePathBuffer, "%s", pathBuf);
#endif

    cFileHandle = fOpen(filePathBuffer, "rb");
    cFileHandleCanClose = true;
    if (cFileHandle) {
        byte signature[6] = { 'R', 'S', 'D', 'K', 'v', 'B' };
        byte buf          = 0;
        for (int i = 0; i < 6; ++i) {
            fRead(&buf, 1, 1, cFileHandle);
            if (buf != signature[i]) {
                CloseCurrentFileHandle();
                return false;
            }
        }

        Engine.usingDataFile = true;

        StrCopy(rsdkContainer.packNames[rsdkContainer.packCount], filePathBuffer);

        ushort fileCount = 0;
        fRead(&fileCount, 2, 1, cFileHandle);
        fileCount = RETRO_LE16(fileCount);
        for (int f = 0; f < fileCount; ++f) {
            for (int y = 0; y < 16; y += 4) {
                fRead(&rsdkContainer.files[f].hash[y + 3], 1, 1, cFileHandle);
                fRead(&rsdkContainer.files[f].hash[y + 2], 1, 1, cFileHandle);
                fRead(&rsdkContainer.files[f].hash[y + 1], 1, 1, cFileHandle);
                fRead(&rsdkContainer.files[f].hash[y + 0], 1, 1, cFileHandle);
            }

            uint32_t offset, filesize;
            fRead(&offset, 4, 1, cFileHandle);
            fRead(&filesize, 4, 1, cFileHandle);
            rsdkContainer.files[f].offset = RETRO_LE32(offset);
            rsdkContainer.files[f].filesize = RETRO_LE32(filesize);

            rsdkContainer.files[f].encrypted = (rsdkContainer.files[f].filesize & 0x80000000);
            rsdkContainer.files[f].filesize &= 0x7FFFFFFF;

            rsdkContainer.files[f].packID = rsdkContainer.packCount;

            rsdkContainer.fileCount++;
        }

        CloseCurrentFileHandle();
        if (LoadFile("Bytecode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        printLog("loaded datapack '%s'", filePathBuffer);

        rsdkContainer.packCount++;
        return true;
    }
    else {
        Engine.usingDataFile = false;
        cFileHandle          = NULL;

        if (LoadFile("Bytecode/GlobalCode.bin", &info)) {
            Engine.usingBytecode = true;
            CloseFile();
        }
        printLog("Couldn't load datapack '%s'", filePathBuffer);
        return false;
    }
}

#if !RETRO_USE_ORIGINAL_CODE
int CheckFileInfo(const char *filepath)
{
    char candidateBuf[6][0x100];
    int candCount = 0;

    char pathBuf[0x100];
    StringLowerCase(pathBuf, filepath);

    
    StrCopy(candidateBuf[candCount++], pathBuf);

    if (strncmp(pathBuf, "data/", 5) == 0) {
        StrCopy(candidateBuf[candCount++], pathBuf + 5);
    }
    else {
        char tmp[0x100];
        sprintf(tmp, "data/%s", pathBuf);
        StrCopy(candidateBuf[candCount++], tmp);
    }

    if (pathBuf[0] == '/' || pathBuf[0] == '\\') {
        StrCopy(candidateBuf[candCount++], pathBuf + 1);
    }

    if (candCount < 6) {
        const char *p = strchr(pathBuf, '/');
        if (p && *(p + 1)) {
            StrCopy(candidateBuf[candCount++], p + 1);
        }
    }

    byte buffer[0x10];

    for (int ci = 0; ci < candCount; ++ci) {
        int len = StrLength(candidateBuf[ci]);
        GenerateMD5FromString(candidateBuf[ci], len, buffer);

        for (int f = 0; f < rsdkContainer.fileCount; ++f) {
            RSDKFileInfo *file = &rsdkContainer.files[f];

            bool match = true;
            for (int h = 0; h < 0x10; ++h) {
                if (buffer[h] != file->hash[h]) {
                    match = false;
                    break;
                }
            }
            if (!match)
                continue;

            return f;
        }
    }

    return -1;
}
#endif

bool LoadFile(const char *filePath, FileInfo *fileInfo)
{
    MEM_ZEROP(fileInfo);

    CloseCurrentFileHandle();
    cFileUsingDataPack = false;
#if !RETRO_USE_ORIGINAL_CODE
    modFileMemoryBuffer = NULL;
    allocatedFileMemoryBuffer = false;
#endif

    char filePathBuf[0x100];
    StrCopy(filePathBuf, filePath);
    bool forceFolder = false;
#if RETRO_USE_MOD_LOADER
    // Fixes ".ani" ".Ani" bug and any other case differences
    char pathLower[0x100];
    memset(pathLower, 0, sizeof(char) * 0x100);
    for (int c = 0; c < strlen(filePathBuf); ++c) {
        pathLower[c] = tolower(filePathBuf[c]);
    }

    bool addPath = true;
    int foundModIndex = -1;
    if (activeMod != -1) {
        char buf[0x100];
        sprintf(buf, "%s", filePathBuf);
        int mLen = StrLength(modsPath);
        if (mLen >= 5 && StrComp(&modsPath[mLen - 5], "mods/") == 0)
            sprintf(filePathBuf, "%s%s/%s", modsPath, modList[activeMod].folder.c_str(), buf);
        else
            sprintf(filePathBuf, "%smods/%s/%s", modsPath, modList[activeMod].folder.c_str(), buf);
        forceFolder = true;
        addPath     = false;
        foundModIndex = activeMod;
    }
    else {
        for (int m = 0; m < modList.size(); ++m) {
            if (modList[m].active) {
                std::map<std::string, std::string>::const_iterator iter = modList[m].fileMap.find(pathLower);
                if (iter != modList[m].fileMap.cend()) {
                    StrCopy(filePathBuf, iter->second.c_str());
                    forceFolder = true;
                    addPath     = false;
                    foundModIndex = m;
                    break;
                }
            }
        }
    }
#endif

#if RETRO_PLATFORM == RETRO_OSX || RETRO_PLATFORM == RETRO_ANDROID || RETRO_PLATFORM == RETRO_WIIU
    if (addPath) {
        char pathBuf[0x100];
        sprintf(pathBuf, "%s%s", gamePath, filePathBuf);
        sprintf(filePathBuf, "%s", pathBuf);
    }
#endif

    cFileHandle = NULL;
#if !RETRO_USE_ORIGINAL_CODE
    if (CheckFileInfo(filePath) != -1 && !forceFolder) {
#else
    if (Engine.usingDataFile) {
#endif
        StringLowerCase(fileInfo->fileName, filePath);
        StrCopy(fileName, fileInfo->fileName);
        byte buffer[0x10];
        int len = StrLength(fileInfo->fileName);
        GenerateMD5FromString(fileInfo->fileName, len, buffer);
        
#if !RETRO_USE_ORIGINAL_CODE
        modFileMemoryBuffer = NULL;
        fileInfo->modMemoryBuffer = NULL;
        allocatedFileMemoryBuffer = false;
        fileInfo->bufferAllocated = false;
#endif

        for (int f = 0; f < rsdkContainer.fileCount; ++f) {
            RSDKFileInfo *file = &rsdkContainer.files[f];

            bool match = true;
            for (int h = 0; h < 0x10; ++h) {
                if (buffer[h] != file->hash[h]) {
                    match = false;
                    break;
                }
            }
            if (!match)
                continue;

            packID      = file->packID;
            if (!CacheDataPack(packID)) {
                printLog("Couldn't cache datapack for '%s'", filePath);
                return false;
            }
            cFileHandle = NULL;
            cFileHandleCanClose = false;
            cFileUsingDataPack = true;
            fileSize = cHandles[packID].fileSize;

            vFileSize         = file->filesize;
            virtualFileOffset = file->offset;
            readPos           = file->offset;
            readSize          = 0;
            bufferPosition    = 0;

            useEncryption = file->encrypted;
            memset(fileInfo->encryptionStringA, 0, 0x10 * sizeof(byte));
            memset(fileInfo->encryptionStringB, 0, 0x10 * sizeof(byte));
            if (useEncryption) {
                GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
                eStringNo   = (vFileSize & 0x1FC) >> 2;
                eStringPosA = 0;
                eStringPosB = 8;
                eNybbleSwap = 0;
                memcpy(fileInfo->encryptionStringA, encryptionStringA, 0x10 * sizeof(byte));
                memcpy(fileInfo->encryptionStringB, encryptionStringB, 0x10 * sizeof(byte));
            }

            fileInfo->readPos           = readPos;
            fileInfo->fileSize          = fileSize;
            fileInfo->vfileSize         = vFileSize;
            fileInfo->virtualFileOffset = virtualFileOffset;
            fileInfo->eStringNo         = eStringNo;
            fileInfo->eStringPosB       = eStringPosB;
            fileInfo->eStringPosA       = eStringPosA;
            fileInfo->eNybbleSwap       = eNybbleSwap;
            fileInfo->bufferPosition    = bufferPosition;
            fileInfo->useEncryption     = useEncryption;
            fileInfo->packID            = packID;
            fileInfo->usingDataPack     = true;
            printLog("Loaded Data File '%s'", filePath);

#if !RETRO_USE_ORIGINAL_CODE
            Engine.usingDataFile = true;
#endif

            return true;
        }
        printLog("Couldn't load file '%s'", filePath);
        return false;
    }
    else {
        StrCopy(fileInfo->fileName, filePathBuf);
        StrCopy(fileName, fileInfo->fileName);

        int knownFileSize = -1;
        void* knownFileBuffer = NULL;

#if RETRO_USE_MOD_LOADER
        if (foundModIndex != -1) {
            auto memIter = modList[foundModIndex].memoryMap.find(pathLower);
            if (memIter != modList[foundModIndex].memoryMap.end()) {
                knownFileBuffer = memIter->second.data;
                knownFileSize = memIter->second.size;
                cFileHandle = NULL;
            } else {
                FILE* disk_f = fopen(fileInfo->fileName, "rb");
                if (disk_f) {
                    fseek(disk_f, 0, SEEK_END);
                    long sz = ftell(disk_f);
                    fseek(disk_f, 0, SEEK_SET);
                    if (sz > 0) {
                        void* mem = malloc(sz);
                        if (mem) {
                            size_t bytesRead = fread(mem, 1, sz, disk_f);
                            fclose(disk_f);
                            if (bytesRead == (size_t)sz) {
                                ModMemoryBuffer mBuf;
                                mBuf.data = mem;
                                mBuf.size = sz;
                                modList[foundModIndex].memoryMap[pathLower] = mBuf;
                                knownFileBuffer = mem;
                                knownFileSize = sz;
                                cFileHandle = NULL;
                                printLog("Cached mod file into RAM '%s' (%ld bytes)", fileInfo->fileName, sz);
                            }
                            else {
                                free(mem);
                                printLog("Couldn't fully cache mod file '%s'", fileInfo->fileName);
                                cFileHandle = NULL;
                            }
                        } else {
                            fclose(disk_f);
                            cFileHandle = NULL;
                            printLog("Couldn't allocate mod file RAM cache '%s' (%ld bytes)", fileInfo->fileName, sz);
                        }
                    } else {
                        fclose(disk_f);
                        knownFileBuffer = NULL;
                        knownFileSize = 0;
                        cFileHandle = NULL;
                    }
                } else {
                    cFileHandle = NULL;
                }
            }
        } else {
#endif
            CachedLooseFile *cachedLooseFile = FindCachedLooseFile(fileInfo->fileName);
            if (cachedLooseFile) {
                knownFileBuffer = cachedLooseFile->data;
                knownFileSize = cachedLooseFile->size;
                cFileHandle = NULL;
            }
            else {
                cFileHandle = fOpen(fileInfo->fileName, "rb");
            }
#if RETRO_USE_MOD_LOADER
        }
#endif
        cFileHandleCanClose = true;
        if (!cFileHandle && knownFileSize < 0) {
            printLog("Couldn't load file '%s'", filePath);
            return false;
        }
        virtualFileOffset = 0;
        
        modFileMemoryBuffer = knownFileBuffer;
        fileInfo->modMemoryBuffer = knownFileBuffer;

        if (knownFileSize != -1) {
            fileInfo->fileSize = knownFileSize;
#if !RETRO_USE_ORIGINAL_CODE
            allocatedFileMemoryBuffer = false;
            fileInfo->bufferAllocated = false;
#endif
        } else if (cFileHandle) {
            fSeek(cFileHandle, 0, SEEK_END);
            fileInfo->fileSize = (int)fTell(cFileHandle);
            fSeek(cFileHandle, 0, SEEK_SET);
#if !RETRO_USE_ORIGINAL_CODE
            CachedLooseFile *cachedLooseFile = CacheLooseFile(fileInfo->fileName, cFileHandle, fileInfo->fileSize);
            fClose(cFileHandle);
            cFileHandle = NULL;
            cFileHandleCanClose = false;
            if (!cachedLooseFile) {
                printLog("Couldn't cache loose file '%s'", filePath);
                return false;
            }
            modFileMemoryBuffer = cachedLooseFile->data;
            fileInfo->modMemoryBuffer = cachedLooseFile->data;
            allocatedFileMemoryBuffer = false;
            fileInfo->bufferAllocated = false;
#endif
        }
        
        fileSize = fileInfo->vfileSize = fileInfo->fileSize;
        readPos           = 0;
        fileInfo->readPos = readPos;
        packID = fileInfo->packID = -1;
        cFileUsingDataPack = false;
        fileInfo->usingDataPack   = false;
        bufferPosition            = 0;
        readSize                  = 0;
        useEncryption             = false;

#if !RETRO_USE_ORIGINAL_CODE
        Engine.usingDataFile = false;
#endif

        printLog("Loaded File '%s'", filePath);
        return true;
    }
}

void GenerateELoadKeys(uint key1, uint key2)
{
    char buffer[0x20];
    byte hash[0x10];

    // StringA
    ConvertIntegerToString(buffer, key1);
    int len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, hash);

    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringA[y + 3] = hash[y + 0];
        encryptionStringA[y + 2] = hash[y + 1];
        encryptionStringA[y + 1] = hash[y + 2];
        encryptionStringA[y + 0] = hash[y + 3];
    }

    // StringB
    ConvertIntegerToString(buffer, key2);
    len = StrLength(buffer);
    GenerateMD5FromString(buffer, len, hash);

    for (int y = 0; y < 0x10; y += 4) {
        encryptionStringB[y + 3] = hash[y + 0];
        encryptionStringB[y + 2] = hash[y + 1];
        encryptionStringB[y + 1] = hash[y + 2];
        encryptionStringB[y + 0] = hash[y + 3];
    }
}

const uint ENC_KEY_2 = 0x24924925;
const uint ENC_KEY_1 = 0xAAAAAAAB;
int mulUnsignedHigh(uint arg1, int arg2) { return (int)(((unsigned long long)arg1 * (unsigned long long)arg2) >> 32); }

void FileRead(void *dest, int size)
{
    byte *data = (byte *)dest;
    memset(data, 0, size);

    if (readPos <= fileSize) {
        if (useEncryption) {
            while (size > 0) {
                if (bufferPosition == readSize && !FillFileBuffer())
                    break;

                *data = encryptionStringB[eStringPosB] ^ eStringNo ^ fileBuffer[bufferPosition++];
                if (eNybbleSwap)
                    *data = ((*data << 4) + (*data >> 4)) & 0xFF;
                *data ^= encryptionStringA[eStringPosA];

                ++eStringPosA;
                ++eStringPosB;
                if (eStringPosA <= 0x0F) {
                    if (eStringPosB > 0x0C) {
                        eStringPosB = 0;
                        eNybbleSwap ^= 0x01;
                    }
                }
                else if (eStringPosB <= 0x08) {
                    eStringPosA = 0;
                    eNybbleSwap ^= 0x01;
                }
                else {
                    eStringNo += 2;
                    eStringNo &= 0x7F;

                    if (eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 0;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosA = eStringNo - temp1 / 4 * 7;
                        eStringPosB = eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 1;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosB = eStringNo - temp1 / 4 * 7;
                        eStringPosA = eStringNo - temp2 * 4 + 3;
                    }
                }

                ++data;
                --size;
            }
        }
        else {
            while (size > 0) {
                if (bufferPosition == readSize && !FillFileBuffer())
                    break;

                *data++ = fileBuffer[bufferPosition++];
                size--;
            }
        }
    }
}

void FileSkip(int count)
{
    if (readPos <= fileSize) {
        if (useEncryption) {
            while (count > 0) {
                if (bufferPosition == readSize && !FillFileBuffer())
                    break;
                bufferPosition++;

                ++eStringPosA;
                ++eStringPosB;
                if (eStringPosA <= 0x0F) {
                    if (eStringPosB > 0x0C) {
                        eStringPosB = 0;
                        eNybbleSwap ^= 0x01;
                    }
                }
                else if (eStringPosB <= 0x08) {
                    eStringPosA = 0;
                    eNybbleSwap ^= 0x01;
                }
                else {
                    eStringNo += 2;
                    eStringNo &= 0x7F;

                    if (eNybbleSwap != 0) {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 0;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosA = eStringNo - temp1 / 4 * 7;
                        eStringPosB = eStringNo - temp2 * 4 + 2;
                    }
                    else {
                        int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                        int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                        eNybbleSwap = 1;

                        int temp1 = key2 + (eStringNo - key2) / 2;
                        int temp2 = key1 / 8 * 3;

                        eStringPosB = eStringNo - temp1 / 4 * 7;
                        eStringPosA = eStringNo - temp2 * 4 + 3;
                    }
                }

                --count;
            }
        }
        else {
            while (count > 0) {
                if (bufferPosition == readSize && !FillFileBuffer())
                    break;
                bufferPosition++;
                count--;
            }
        }
    }
}

void GetFileInfo(FileInfo *fileInfo)
{
    StrCopy(fileInfo->fileName, fileName);
    fileInfo->bufferPosition    = bufferPosition;
    fileInfo->readPos           = readPos - readSize;
    fileInfo->fileSize          = fileSize;
    fileInfo->vfileSize         = vFileSize;
    fileInfo->virtualFileOffset = virtualFileOffset;
    fileInfo->eStringPosA       = eStringPosA;
    fileInfo->eStringPosB       = eStringPosB;
    fileInfo->eStringNo         = eStringNo;
    fileInfo->eNybbleSwap       = eNybbleSwap;
    fileInfo->useEncryption     = useEncryption;
    fileInfo->packID            = packID;
    fileInfo->usingDataPack     = cFileUsingDataPack;
#if !RETRO_USE_ORIGINAL_CODE
    fileInfo->modMemoryBuffer   = modFileMemoryBuffer;
    fileInfo->cFileHandle       = cFileHandle;
    fileInfo->bufferAllocated   = allocatedFileMemoryBuffer;
#endif
    memcpy(fileInfo->encryptionStringA, encryptionStringA, 0x10 * sizeof(byte));
    memcpy(fileInfo->encryptionStringB, encryptionStringB, 0x10 * sizeof(byte));
}

void SetFileInfo(FileInfo *fileInfo)
{
#if !RETRO_USE_ORIGINAL_CODE
    if (fileInfo->usingDataPack) {
#else
    if (Engine.usingDataFile) {
#endif
        packID               = fileInfo->packID;
        if (!CacheDataPack(packID)) {
            printLog("Couldn't restore datapack cache for '%s'", fileInfo->fileName);
            return;
        }
        cFileHandle = NULL;
        cFileHandleCanClose = false;
        cFileUsingDataPack = true;
        fileSize = cHandles[packID].fileSize;

        virtualFileOffset = fileInfo->virtualFileOffset;
        vFileSize         = fileInfo->vfileSize;
        readPos  = fileInfo->readPos;
        FillFileBuffer();
        bufferPosition       = fileInfo->bufferPosition;
        eStringPosA          = fileInfo->eStringPosA;
        eStringPosB          = fileInfo->eStringPosB;
        eStringNo            = fileInfo->eStringNo;
        eNybbleSwap          = fileInfo->eNybbleSwap;
        useEncryption        = fileInfo->useEncryption;

        Engine.usingDataFile = fileInfo->usingDataPack;
        if (useEncryption) {
            GenerateELoadKeys(vFileSize, (vFileSize >> 1) + 1);
        }
#if !RETRO_USE_ORIGINAL_CODE
        modFileMemoryBuffer = NULL;
        allocatedFileMemoryBuffer = false;
#endif
    }
    else {
        CloseCurrentFileHandle();
        cFileUsingDataPack = false;
        StrCopy(fileName, fileInfo->fileName);
#if !RETRO_USE_ORIGINAL_CODE
        modFileMemoryBuffer = fileInfo->modMemoryBuffer;
        cFileHandle       = fileInfo->cFileHandle;
        allocatedFileMemoryBuffer = fileInfo->bufferAllocated;
        if (!cFileHandle && !modFileMemoryBuffer) {
            CachedLooseFile *cachedLooseFile = FindCachedLooseFile(fileInfo->fileName);
            if (cachedLooseFile)
                modFileMemoryBuffer = cachedLooseFile->data;
        }
#else
        cFileHandle       = fOpen(fileInfo->fileName, "rb");
#endif
        cFileHandleCanClose = cFileHandle != NULL;
        virtualFileOffset = 0;
        fileSize          = fileInfo->fileSize;
        readPos           = fileInfo->readPos;
        if (cFileHandle) fSeek(cFileHandle, readPos, SEEK_SET);
        FillFileBuffer();
        bufferPosition       = fileInfo->bufferPosition;
        eStringPosA          = 0;
        eStringPosB          = 0;
        eStringNo            = 0;
        eNybbleSwap          = 0;
        useEncryption        = fileInfo->useEncryption;
        packID               = fileInfo->packID;
        Engine.usingDataFile = fileInfo->usingDataPack;
    }
}

size_t GetFilePosition()
{
    if (cFileUsingDataPack)
        return bufferPosition + readPos - readSize - virtualFileOffset;
    else
        return bufferPosition + readPos - readSize;
}

void SetFilePosition(int newPos)
{
    if (useEncryption) {
        readPos     = virtualFileOffset + newPos;
        eStringNo   = (vFileSize & 0x1FC) >> 2;
        eStringPosA = 0;
        eStringPosB = 8;
        eNybbleSwap = false;
        while (newPos) {
            ++eStringPosA;
            ++eStringPosB;
            if (eStringPosA <= 0x0F) {
                if (eStringPosB > 0x0C) {
                    eStringPosB = 0;
                    eNybbleSwap ^= 0x01;
                }
            }
            else if (eStringPosB <= 0x08) {
                eStringPosA = 0;
                eNybbleSwap ^= 0x01;
            }
            else {
                eStringNo += 2;
                eStringNo &= 0x7F;

                if (eNybbleSwap != 0) {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 0;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosA = eStringNo - temp1 / 4 * 7;
                    eStringPosB = eStringNo - temp2 * 4 + 2;
                }
                else {
                    int key1    = mulUnsignedHigh(ENC_KEY_1, eStringNo);
                    int key2    = mulUnsignedHigh(ENC_KEY_2, eStringNo);
                    eNybbleSwap = 1;

                    int temp1 = key2 + (eStringNo - key2) / 2;
                    int temp2 = key1 / 8 * 3;

                    eStringPosB = eStringNo - temp1 / 4 * 7;
                    eStringPosA = eStringNo - temp2 * 4 + 3;
                }
            }
            --newPos;
        }
    }
    else {
        if (cFileUsingDataPack)
            readPos = virtualFileOffset + newPos;
        else
            readPos = newPos;
    }
    if (cFileHandle) fSeek(cFileHandle, readPos, SEEK_SET);
    FillFileBuffer();
}

bool ReachedEndOfFile()
{
    if (cFileUsingDataPack)
        return bufferPosition + readPos - readSize - virtualFileOffset >= vFileSize;
    else
        return bufferPosition + readPos - readSize >= fileSize;
}
