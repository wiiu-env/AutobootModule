#include <malloc.h>

#include "BootUtils.h"
#include "MenuUtils.h"
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

    bool doProcUi                       = true;
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
        auto *fsClient  = (FSClient *) memalign(0x40, sizeof(FSClient));
        if (!fsClient) {
            DEBUG_FUNCTION_LINE("Couldn't alloc memory for fsClient.");
            return false;
        }
        memset(fsClient, 0, sizeof(*fsClient));
        FSAddClient(fsClient, FS_ERROR_FLAG_NONE);

        fileStream->Initialize(fsClient, &cmdBlock, path, "r");

        auto database = new nn::sl::LaunchInfoDatabase;
        database->Load(fileStream, nn::sl::REGION_EUR);

        CCRAppLaunchParam data; // load sys caffeine data
        // load app launch param
        CCRSysCaffeineGetAppLaunchParam(&data);

        loadConsoleAccount(data.uuid);

        // get launch info for id
        nn::sl::LaunchInfo info;
        auto result = database->GetLaunchInfoById(&info, data.titleId);

        delete database;
        delete fileStream;

        FSDelClient(fsClient, FS_ERROR_FLAG_NONE);
        free(fsClient);

        nn::sl::Finalize();

        if (!result.IsSuccess()) {
            DEBUG_FUNCTION_LINE("GetLaunchInfoById failed.");
            return false;
        }

        if (info.titleId == 0x0005001010040000L ||
            info.titleId == 0x0005001010040100L ||
            info.titleId == 0x0005001010040200L) {
            DEBUG_FUNCTION_LINE("Skip quick starting into the Wii U Menu");
            return false;
        }

        if (info.titleId == 0x0005001010047000L ||
            info.titleId == 0x0005001010047100L ||
            info.titleId == 0x0005001010047200L) {
            DEBUG_FUNCTION_LINE("Launch Settings");
            _SYSLaunchSettings(nullptr);
            return true;
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
            _SYSSwitchTo(SYSAPP_PFID_DOWNLOAD_MANAGEMENT);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001600AL ||
            info.titleId == 0x000500301001610AL ||
            info.titleId == 0x000500301001620AL) {
            DEBUG_FUNCTION_LINE("Launching Miiverse");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(SYSAPP_PFID_MIIVERSE);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001500AL ||
            info.titleId == 0x000500301001510AL ||
            info.titleId == 0x000500301001520AL) {
            DEBUG_FUNCTION_LINE("Launching Friendlist");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(SYSAPP_PFID_FRIENDLIST);

            StartAppletAndExit();

            return true;
        }
        if (info.titleId == 0x000500301001300AL ||
            info.titleId == 0x000500301001310AL ||
            info.titleId == 0x000500301001320AL) {
            DEBUG_FUNCTION_LINE("Launching TVii");
            loadConsoleAccount(data.uuid);
            _SYSSwitchTo(SYSAPP_PFID_TVII);

            StartAppletAndExit();

            return true;
        }

        if (info.titleId == 0x0005001010004000L) { // OSv0
            DEBUG_FUNCTION_LINE("Launching vWii System Menu");
            bootvWiiMenu();

            return true;
        }

        uint64_t titleIdToLaunch = info.titleId;

        switch (info.mediaType) {
            case nn::sl::NN_SL_MEDIA_TYPE_ODD: {
                if (!handleDiscInsertScreen(titleIdToLaunch, &titleIdToLaunch)) {
                    DEBUG_FUNCTION_LINE("Launch Wii U Menu!");
                    return false;
                }
                break;
            }
            default: {
                if (!SYSCheckTitleExists(titleIdToLaunch)) {
                    DEBUG_FUNCTION_LINE("Title %016llX doesn't exist", titleIdToLaunch);
                    return false;
                }
            }
        }

        MCPTitleListType titleInfo;
        int32_t handle = MCP_Open();
        auto err       = MCP_GetTitleInfo(handle, titleIdToLaunch, &titleInfo);
        MCP_Close(handle);
        if (err == 0) {
            DEBUG_FUNCTION_LINE("Launch %016llX", titleIdToLaunch);
            ACPAssignTitlePatch(&titleInfo);
            _SYSLaunchTitleWithStdArgsInNoSplash(titleIdToLaunch, nullptr);
            return true;
        }

        DEBUG_FUNCTION_LINE("Launch Wii U Menu!");
        return false;
    } else {
        DEBUG_FUNCTION_LINE("No quick start");
    }
    return false;
}