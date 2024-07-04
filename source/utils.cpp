#include "logger.h"
#include <coreinit/filesystem_fsa.h>
#include <coreinit/mcp.h>
#include <cstdint>
#include <cstdio>
#include <mocha/mocha.h>
#include <string_view>
#include <sys/stat.h>
#include <vector>
#include <whb/log.h>

bool GetTitleIdOfDisc(uint64_t *titleId, bool *discPresent) {
    if (discPresent) {
        *discPresent = false;
    }
    alignas(0x40) MCPTitleListType titles[4];
    uint32_t count = 0;

    int handle = MCP_Open();
    if (handle < 0) {
        DEBUG_FUNCTION_LINE_ERR("MCP_Open failed");
        return false;
    }
    auto res = MCP_TitleListByDeviceType(handle, MCP_DEVICE_TYPE_ODD, &count, titles, sizeof(titles));
    MCP_Close(handle);

    if (res >= 0 && count > 0) {
        if (discPresent) {
            *discPresent = true;
        }
        for (uint32_t i = 0; i < count; i++) {
            if ((titles[i].titleId & 0xFFFFFFFF00000000L) == (0x0005000000000000)) {
                if (titleId) {
                    *titleId = titles[i].titleId;
                }
                return true;
            }
        }
    }
    return false;
}

bool DeleteMLCUpdateDirectory() {
    bool result = false;
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            if (FSARemove(client, "/vol/storage_mlc01/sys/update") != FS_ERROR_OK) {
                DEBUG_FUNCTION_LINE_ERR("Failed to remove update directory");
            } else {
                FSAFileHandle fd;
                if (FSAOpenFileEx(client, "/vol/storage_mlc01/sys/update", "w", static_cast<FSMode>(0x666), FS_OPEN_FLAG_NONE, 0, &fd) != FS_ERROR_OK) {
                    DEBUG_FUNCTION_LINE_WARN("Failed to create update file");
                }
                result = true;
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to unlock FSA Client");
        }
        FSADelClient(client);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to create FSA Client");
    }
    return result;
}

bool RestoreMLCUpdateDirectory() {
    bool result = false;
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            FSARemove(client, "/vol/storage_mlc01/sys/update"); // Remove any existing files
            if (FSAMakeDir(client, "/vol/storage_mlc01/sys/update", static_cast<FSMode>(0x666)) != FS_ERROR_OK) {
                DEBUG_FUNCTION_LINE_WARN("Failed to restore update directory");
            }
            result = true;
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to unlock FSA Client");
        }
        FSADelClient(client);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to create FSA Client");
    }
    return result;
}


bool LoadFileIntoBuffer(std::string_view path, std::vector<uint8_t> &buffer) {
    struct stat st {};
    if (stat(path.data(), &st) < 0 || !S_ISREG(st.st_mode)) {
        DEBUG_FUNCTION_LINE_INFO("\"%s\" doesn't exists", path.data());
        return false;
    }

    FILE *f = fopen(path.data(), "rb");
    if (!f) {
        return false;
    }
    buffer.resize(st.st_size);

    if (fread(buffer.data(), 1, st.st_size, f) != st.st_size) {
        DEBUG_FUNCTION_LINE_WARN("Failed load %s", path.data());
        return false;
    }

    fclose(f);
    return true;
}