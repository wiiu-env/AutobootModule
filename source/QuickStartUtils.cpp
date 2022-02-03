#include <malloc.h>

#include "BootUtils.h"
#include "QuickStartUtils.h"
#include "logger.h"

#include <coreinit/exit.h>
#include <coreinit/foreground.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <nn/acp/title.h>
#include <nn/act/client_cpp.h>
#include <nn/ccr/sys_caffeine.h>
#include <nn/sl.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>

extern "C" void __fini_wut();

static void StartAppletAndExit() {
    DEBUG_FUNCTION_LINE("Wait for applet");
    ProcUIInit(OSSavesDone_ReadyToRelease);

    bool doProcUi = true;
    bool launchWiiUMenuOnNextForeground = false;
    while (true) {
        switch (ProcUIProcessMessages(true)) {
            case PROCUI_STATUS_EXITING: {
                doProcUi = false;
                break;
            }
            case PROCUI_STATUS_RELEASE_FOREGROUND: {
                ProcUIDrawDoneRelease();
                launchWiiUMenuOnNextForeground = true;
                break;
            }
            case PROCUI_STATUS_IN_FOREGROUND: {
                if (launchWiiUMenuOnNextForeground) {
                    bootWiiUMenu();
                    launchWiiUMenuOnNextForeground = false;
                }
                break;
            }
            case PROCUI_STATUS_IN_BACKGROUND: {
                break;
            }
            default:
                break;
        }
        OSSleepTicks(OSMillisecondsToTicks(1));
        if (!doProcUi) {
            break;
        }
    }
    ProcUIShutdown();

    DEBUG_FUNCTION_LINE("Exit to Wii U Menu");

    deinitLogging();
    __fini_wut();
    _Exit(0);
}

extern "C" int32_t SYSSwitchToBrowser(void *);
extern "C" int32_t SYSSwitchToEShop(void *);
extern "C" int32_t _SYSSwitchTo(uint32_t pfid);

void loadConsoleAccount(const char *data_uuid) {
    nn::act::Initialize();
    for (int32_t i = 0; i < 13; i++) {
        char uuid[16];
        auto result = nn::act::GetUuidEx(uuid, i);
        if (result.IsSuccess()) {
            if (memcmp(uuid, data_uuid, 8) == 0) {
                DEBUG_FUNCTION_LINE("Load Console account %d", i);
                nn::act::LoadConsoleAccount(i, 0, nullptr, false);
                break;
            }
        }
    }
    nn::act::Finalize();
}

bool getQuickBoot() {
    auto bootCheck = CCRSysCaffeineBootCheck();
    if (bootCheck == 0) {
        nn::sl::Initialize(MEMAllocFromDefaultHeapEx, MEMFreeToDefaultHeap);
        char path[0x80];
        nn::sl::GetDefaultDatabasePath(path, 0x80, 0x0005001010066000); // ECO process
        FSCmdBlock cmdBlock;
        FSInitCmdBlock(&cmdBlock);

        auto fileStream = new nn::sl::FileStream;
        auto *fsClient = (FSClient *) memalign(0x40, sizeof(FSClient));
        memset(fsClient, 0, sizeof(*fsClient));
        FSAddClient(fsClient, FS_ERROR_FLAG_NONE);

        fileStream->Initialize(fsClient, &cmdBlock, path, "r");

        auto database = new nn::sl::LaunchInfoDatabase;
        database->Load(fileStream, nn::sl::REGION_EUR);

        CCRAppLaunchParam data; // load sys caffeine data
        // load app launch param
        CCRSysCaffeineGetAppLaunchParam(&data);

        // get launch info for id
        nn::sl::LaunchInfo info;
        auto result = database->GetLaunchInfoById(&info, data.titleId);

        delete database;
        delete fileStream;

        FSDelClient(fsClient, FS_ERROR_FLAG_NONE);

        nn::sl::Finalize();

        if (!result.IsSuccess()) {
            DEBUG_FUNCTION_LINE("GetLaunchInfoById failed.");
            return false;
        }

        DEBUG_FUNCTION_LINE("Trying to autoboot for titleId %016llX", info.titleId);

        if (info.titleId == 0x0005001010040000L ||
            info.titleId == 0x0005001010040100L ||
            info.titleId == 0x0005001010040200L) {
            DEBUG_FUNCTION_LINE("Skip quick starting into the Wii U Menu");
            return false;
        }

        if (info.titleId == 0x000500301001220AL ||
            info.titleId == 0x000500301001210AL ||
            info.titleId == 0x000500301001200AL) {
            DEBUG_FUNCTION_LINE("Launching the browser");
            loadConsoleAccount(data.uuid);
            SYSSwitchToBrowser(nullptr);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001400AL ||
            info.titleId == 0x000500301001410AL ||
            info.titleId == 0x000500301001420AL) {
            DEBUG_FUNCTION_LINE("Launching the Eshop");
            loadConsoleAccount(data.uuid);
            SYSSwitchToEShop(nullptr);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001800AL ||
            info.titleId == 0x000500301001810AL ||
            info.titleId == 0x000500301001820AL) {
            DEBUG_FUNCTION_LINE("Launching the Download Management");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(12);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001600AL ||
            info.titleId == 0x000500301001610AL ||
            info.titleId == 0x000500301001620AL) {
            DEBUG_FUNCTION_LINE("Launching Miiverse");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(9);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001500AL ||
            info.titleId == 0x000500301001510AL ||
            info.titleId == 0x000500301001520AL) {
            DEBUG_FUNCTION_LINE("Launching Friendlist");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(11);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001300AL ||
            info.titleId == 0x000500301001310AL ||
            info.titleId == 0x000500301001320AL) {
            DEBUG_FUNCTION_LINE("Launching TVii");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(3);

            StartAppletAndExit();

            return true;
        }

        if (info.titleId == 0x0005001010004000L) { // OSv0
            DEBUG_FUNCTION_LINE("Launching vWii System Menu");
            bootvWiiMenu();

            return true;
        }

        if (!SYSCheckTitleExists(info.titleId)) {
            DEBUG_FUNCTION_LINE("Title %016llX doesn't exist", info.titleId);
            return false;
        }

        MCPTitleListType titleInfo;
        int32_t handle = MCP_Open();
        auto err = MCP_GetTitleInfo(handle, info.titleId, &titleInfo);
        MCP_Close(handle);
        if (err == 0) {
            loadConsoleAccount(data.uuid);
            ACPAssignTitlePatch(&titleInfo);
            _SYSLaunchTitleWithStdArgsInNoSplash(info.titleId, nullptr);
            return true;
        }

        return false;
    } else {
        DEBUG_FUNCTION_LINE("No quick start");
    }
    return false;
}