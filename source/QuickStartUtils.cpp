#include <malloc.h>

#include "BootUtils.h"
#include "MenuUtils.h"
#include "QuickStartUtils.h"
#include "logger.h"

#include <coreinit/exit.h>
#include <coreinit/foreground.h>
#include <coreinit/launch.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <nn/acp/title.h>
#include <nn/act/client_cpp.h>
#include <nn/ccr/sys_caffeine.h>
#include <nn/sl.h>
#include <nsysccr/cdc.h>
#include <optional>
#include <proc_ui/procui.h>
#include <rpxloader/rpxloader.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>

extern "C" void __fini_wut();
extern "C" void CCRSysCaffeineBootCheckAbort();

#define UPPER_TITLE_ID_HOMEBREW 0x0005000F
#define TITLE_ID_HOMEBREW_MASK  (((uint64_t) UPPER_TITLE_ID_HOMEBREW) << 32)

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

class FileStreamWrapper {
public:
    static std::unique_ptr<FileStreamWrapper> CreateFromPath(std::string_view path, std::string_view mode = "r") {
        return std::unique_ptr<FileStreamWrapper>(new FileStreamWrapper(path, mode));
    }

    ~FileStreamWrapper() {
        mFileStream.reset();
        FSDelClient(&mFsClient, FS_ERROR_FLAG_NONE);
    }

    nn::sl::details::IStreamBase &GetStream() {
        return *mFileStream;
    }

private:
    explicit FileStreamWrapper(std::string_view path, std::string_view mode) {
        FSAddClient(&mFsClient, FS_ERROR_FLAG_NONE);
        FSInitCmdBlock(&mCmdBlock);
        mFileStream = std::make_unique<nn::sl::FileStream>();
        mFileStream->Initialize(&mFsClient, &mCmdBlock, path.data(), mode.data());
    }

    std::unique_ptr<nn::sl::FileStream> mFileStream{};
    FSClient mFsClient{};
    FSCmdBlock mCmdBlock{};
};

class QuickStartAutoAbort {
public:
    QuickStartAutoAbort() {
        OSCreateAlarm(&mAlarm);
        OSSetPeriodicAlarm(&mDRCConnectedAlarm,
                           OSSecondsToTicks(10),
                           OSSecondsToTicks(1),
                           &AbortOnDRCDisconnect);
        OSSetAlarm(&mAlarm, OSSecondsToTicks(120), AbortQuickStartTitle);
        mDRCConnected = IsDRCConnected();
    }
    ~QuickStartAutoAbort() {
        OSCancelAlarm(&mDRCConnectedAlarm);
        OSCancelAlarm(&mAlarm);

        // Reconnect the DRC if it was connected at launch but then disconnected;
        if (mDRCConnected && !IsDRCConnected()) {
            DEBUG_FUNCTION_LINE("Wake up GamePad");
            CCRCDCWowlWakeDrcArg args = {.state = 1};
            CCRCDCWowlWakeDrc(&args);
        }
    }

    static bool IsDRCConnected() {
        CCRCDCDrcState state = {};
        CCRCDCSysGetDrcState(CCR_CDC_DESTINATION_DRC0, &state);
        return state.state != 0;
    }

    static void AbortQuickStartTitle(OSAlarm *alarm, OSContext *) {
        DEBUG_FUNCTION_LINE("Selecting a title takes too long, lets abort the quick start menu");
        CCRSysCaffeineBootCheckAbort();
    }

    static void AbortOnDRCDisconnect(OSAlarm *alarm, OSContext *) {
        if (!IsDRCConnected()) {
            // The gamepad does reconnect after selecting a title, make sure it's still disconnected after waiting 3 seconds
            OSSleepTicks(OSSecondsToTicks(3));
            if (!IsDRCConnected()) {
                DEBUG_FUNCTION_LINE("GamePad was disconnected, lets abort the quick start menu");
                CCRSysCaffeineBootCheckAbort();
            }
        }
    }

private:
    OSAlarm mDRCConnectedAlarm{};
    OSAlarm mAlarm{};
    bool mDRCConnected = false;
};

bool launchQuickStartTitle() {
    // Automatically abort quick start if selecting takes longer than 120 seconds or the DRC disconnects
    QuickStartAutoAbort quickStartAutoAbort;

    // Waits until the quick start menu has been closed.
    auto bootCheck = CCRSysCaffeineBootCheck();
    if (bootCheck == 0) {
        nn::sl::Initialize(MEMAllocFromDefaultHeapEx, MEMFreeToDefaultHeap);
        char path[0x80];
        nn::sl::GetDefaultDatabasePath(path, 0x80, 0x0005001010066000); // ECO process
        nn::sl::LaunchInfoDatabase launchInfoDatabase;
        nn::sl::LaunchInfo info;
        {
            // In theory the region doesn't even matter.
            // The region is to load a "system table" into the LaunchInfoDatabase which provides the LaunchInfos for
            // the Wii U Menu and System Settings. In the code below we check for all possible System Settings title id and
            // have a fallback to the Wii U Menu... This means we could get away a wrong region, but let's use the correct one
            // anyway
            const auto region = []() {
                if (SYSCheckTitleExists(0x0005001010047000L)) { // JPN System Settings
                    return nn::sl::REGION_JPN;
                } else if (SYSCheckTitleExists(0x0005001010047100L)) { // USA System Settings
                    return nn::sl::REGION_USA;
                } else if (SYSCheckTitleExists(0x0005001010047200L)) { // EUR System Settings
                    return nn::sl::REGION_EUR;
                }
                return nn::sl::REGION_EUR;
            }();
            auto fileStream = FileStreamWrapper::CreateFromPath(path);
            if (launchInfoDatabase.Load(fileStream->GetStream(), region).IsFailure()) {
                DEBUG_FUNCTION_LINE_ERR("Failed to load LaunchInfoDatabase");
                return false;
            }
        }

        CCRAppLaunchParam data; // load sys caffeine data
        // load app launch param
        CCRSysCaffeineGetAppLaunchParam(&data);

        if (data.launchInfoDatabaseEntryId == 1) { // This id is hardcoded into the nn_sl.rpl
            DEBUG_FUNCTION_LINE("Launch Quick Start Settings");
            SysAppSettingsArgs args{};
            args.jumpTo = SYS_SETTINGS_JUMP_TO_QUICK_START_SETTINGS; // quick start settings
            _SYSLaunchSettings(&args);
            return true;
        }

        loadConsoleAccount(data.uuid);

        auto result = launchInfoDatabase.GetLaunchInfoById(&info, data.launchInfoDatabaseEntryId);

        nn::sl::Finalize();

        if (!result.IsSuccess()) {
            DEBUG_FUNCTION_LINE("GetLaunchInfoById failed.");
            return false;
        }

        if ((info.titleId & TITLE_ID_HOMEBREW_MASK) == TITLE_ID_HOMEBREW_MASK) {
            std::string homebrewPath = info.parameter;
            DEBUG_FUNCTION_LINE("Trying to launch homebrew title: \"%s\"", homebrewPath.c_str());

            if (auto err = RPXLoader_LaunchHomebrew(homebrewPath.c_str()); err != RPX_LOADER_RESULT_SUCCESS) {
                DEBUG_FUNCTION_LINE_WARN("Failed to launch homebrew title: %s (%d)", RPXLoader_GetStatusStr(err), err);
                return false;
            }
            return true;
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
            DEBUG_FUNCTION_LINE("Launch System Settings");
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