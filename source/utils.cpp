#include "logger.h"
#include <coreinit/mcp.h>

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