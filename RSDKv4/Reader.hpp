#ifndef READER_H
#define READER_H

#define FileIO                                          FILE
static inline FileIO* fOpen(const char * pathname, const char * mode) {
  FileIO* f = fopen(pathname, mode);
  if (f) setvbuf(f, NULL, _IOFBF, 128*1024);
  return f;
}
#define fRead(buffer, elementSize, elementCount, file)  fread(buffer, elementSize, elementCount, file)
#define fSeek(file, offset, whence)                     fseek(file, offset, whence)
#define fTell(file)                                     ftell(file)
#define fClose(file)                                    fclose(file)
#define fWrite(buffer, elementSize, elementCount, file) fwrite(buffer, elementSize, elementCount, file)

#define RETRO_PACKFILE_COUNT (0x1000)
#define RETRO_PACK_COUNT     (0x4)

struct FileInfo {
    char fileName[0x100];
    int fileSize;
    int vfileSize;
    int readPos;
    int bufferPosition;
    int virtualFileOffset;
    byte eStringPosA;
    byte eStringPosB;
    byte eStringNo;
    byte eNybbleSwap;
    bool useEncryption;
    byte packID;
    byte encryptionStringA[0x10];
    byte encryptionStringB[0x10];
#if !RETRO_USE_ORIGINAL_CODE
    FileIO *cFileHandle;
    bool usingDataPack;
    void* modMemoryBuffer;
    bool bufferAllocated;
#endif
};

extern void* modFileMemoryBuffer;
extern bool allocatedFileMemoryBuffer;

struct RSDKFileInfo {
    byte hash[0x10];
    int offset;
    int filesize;
    bool encrypted;
    byte packID;
};

struct RSDKContainer {
    RSDKFileInfo files[RETRO_PACKFILE_COUNT];
    char packNames[RETRO_PACK_COUNT][0x400];
    int fileCount;
    int packCount;
};

extern RSDKContainer rsdkContainer;

extern char fileName[0x100];
extern byte fileBuffer[0x10000];
extern int fileSize;
extern int vFileSize;
extern int readPos;
extern int readSize;
extern int bufferPosition;
extern int virtualFileOffset;
extern bool useEncryption;
extern byte packID;
extern bool cFileUsingDataPack;
extern byte eStringPosA;
extern byte eStringPosB;
extern byte eStringNo;
extern byte eNybbleSwap;
extern byte encryptionStringA[0x10];
extern byte encryptionStringB[0x10];

extern FileIO *cFileHandle;
extern bool cFileHandleCanClose;

inline void CopyFilePath(char *dest, const char *src)
{
    strcpy(dest, src);
    for (int i = 0;; ++i) {
        if (i >= strlen(dest)) {
            break;
        }

        if (dest[i] == '/')
            dest[i] = '\\';
    }
}
bool CheckRSDKFile(const char *filePath);
extern byte* packMemoryBuffers[RETRO_PACK_COUNT];
void CloseRSDKContainers();

#if !RETRO_USE_ORIGINAL_CODE
int CheckFileInfo(const char *filepath);
#endif

bool LoadFile(const char *filePath, FileInfo *fileInfo);
inline bool CloseFile()
{
    int result = 0;
    if (cFileHandle && cFileHandleCanClose)
        result = fClose(cFileHandle);

#if !RETRO_USE_ORIGINAL_CODE
    if (allocatedFileMemoryBuffer && modFileMemoryBuffer) {
        free(modFileMemoryBuffer);
    }
    modFileMemoryBuffer = NULL;
    allocatedFileMemoryBuffer = false;
#endif

    cFileHandle = NULL;
    cFileHandleCanClose = false;
    cFileUsingDataPack = false;
    return result;
}

void GenerateELoadKeys(uint key1, uint key2);

void FileRead(void *dest, int size);
void FileSkip(int count);

inline size_t FillFileBuffer()
{
    int endPos = cFileUsingDataPack ? virtualFileOffset + vFileSize : fileSize;
    if (readPos >= endPos) {
        readSize       = 0;
        bufferPosition = 0;
        return 0;
    }

    int remaining = endPos - readPos;
    if (remaining > 0x10000)
        readSize = 0x10000;
    else
        readSize = remaining;

    size_t result = 0;
#if !RETRO_USE_ORIGINAL_CODE
    if (modFileMemoryBuffer) {
        memcpy(fileBuffer, (byte*)modFileMemoryBuffer + readPos, readSize);
        result = readSize;
    }
    else
#endif
    if (packID < RETRO_PACK_COUNT && packMemoryBuffers[packID]) {
        memcpy(fileBuffer, packMemoryBuffers[packID] + readPos, readSize);
        result = readSize;
    }
    else if (cFileHandle) {
        result = fRead(fileBuffer, 1u, readSize, cFileHandle);
    }
    readPos += readSize;
    bufferPosition = 0;
    return result;
}

void GetFileInfo(FileInfo *fileInfo);
void SetFileInfo(FileInfo *fileInfo);
size_t GetFilePosition();
void SetFilePosition(int newPos);
bool ReachedEndOfFile();

#endif // !READER_H
