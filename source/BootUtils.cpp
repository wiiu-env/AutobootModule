#include "BootUtils.h"
#include "ACTAccountInfo.h"
#include "MenuUtils.h"
#include "logger.h"
#include <codecvt>
#include <coreinit/dynload.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <filesystem>
#include <locale>
#include <malloc.h>
#include <memory>
#include <mocha/mocha.h>
#include <nn/act.h>
#include <nn/cmpt/cmpt.h>
#include <nsysccr/cdc.h>
#include <padscore/kpad.h>
#include <sndcore2/core.h>
#include <string>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <vector>
#include <vpad/input.h>

void handleAccountSelection();

void bootWiiUMenu() {
    nn::act::Initialize();
    nn::act::SlotNo slot        = nn::act::GetSlotNo();
    nn::act::SlotNo defaultSlot = nn::act::GetDefaultAccount();
    nn::act::Finalize();

    if (defaultSlot) { //normal menu boot
        SYSLaunchMenu();
    } else { //show mii select
        _SYSLaunchMenuWithCheckingAccount(slot);
    }
}

void bootHomebrewLauncher() {
    handleAccountSelection();

    uint64_t titleId = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_MII_MAKER);
    _SYSLaunchTitleWithStdArgsInNoSplash(titleId, nullptr);
}

void handleAccountSelection() {
    nn::act::Initialize();
    nn::act::SlotNo defaultSlot = nn::act::GetDefaultAccount();

    if (!defaultSlot) { // No default account is set.
        std::vector<std::shared_ptr<AccountInfo>> accountInfoList;
        for (int32_t i = 0; i < 13; i++) {
            if (!nn::act::IsSlotOccupied(i)) {
                continue;
            }
            char16_t nameOut[nn::act::MiiNameSize];
            std::shared_ptr<AccountInfo> accountInfo = std::make_shared<AccountInfo>();
            accountInfo->slot                        = i;
            auto result                              = nn::act::GetMiiNameEx(reinterpret_cast<int16_t *>(nameOut), i);
            if (result.IsSuccess()) {
                accountInfo->name = std::filesystem::path(nameOut).string();
            } else {
                accountInfo->name = "[UNKNOWN]";
            }
            accountInfo->isNetworkAccount = nn::act::IsNetworkAccountEx(i);
            if (accountInfo->isNetworkAccount) {
                nn::act::GetAccountIdEx(accountInfo->accountId, i);
            }

            uint32_t imageSize = 0;
            result             = nn::act::GetMiiImageEx(&imageSize, accountInfo->miiImageBuffer, sizeof(accountInfo->miiImageBuffer), 0, i);
            if (result.IsSuccess()) {
                accountInfo->miiImageSize = imageSize;
            }
            accountInfoList.push_back(accountInfo);
        }

        if (accountInfoList.size() > 0) {
            if (!AXIsInit()) {
                AXInit();
            }
            auto slot = handleAccountSelectScreen(accountInfoList);

            DEBUG_FUNCTION_LINE("Load slot %d", slot);
            nn::act::LoadConsoleAccount(slot, 0, nullptr, false);
        }
    }
    nn::act::Finalize();
}

// Exported by nn_cmpt.rpl but not declared in wut's cmpt.h; the Wii U Menu
// calls this before launching vWii titles to signal GamePad support.
extern "C" int32_t CMPTAcctSetDrcCtrlEnabled(int32_t enabled);

// With no GamePad connected, nn_cmpt refuses vWii launches (-512): its switch of the
// TV to Wii mode (AVMSetDCWiiCompat) also reprograms the GamePad's display pipe
// from avm.rpl's GamePad settings, and with no GamePad those say "off" (what
// GX2SetDRCBuffer with GX2_DRC_RENDER_MODE_DISABLE stores), which dc.rpl rejects
// (DCGetBufferRequirement = -2). Give the GamePad pipe a normal 854x480 mode
// first, with the call GX2SetDRCBuffer makes for GX2_DRC_RENDER_MODE_SINGLE and
// double buffering: AVMSetDRCBufferAttr(bufferingMode, mode, pitch).
static void setDrcModeForWiiMode() {
    OSDynLoad_Module avm   = nullptr;
    void *setDrcBufferAttr = nullptr;
    if (OSDynLoad_Acquire("avm.rpl", &avm) != OS_DYNLOAD_OK ||
        OSDynLoad_FindExport(avm, OS_DYNLOAD_EXPORT_FUNC, "AVMSetDRCBufferAttr", &setDrcBufferAttr) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("AVMSetDRCBufferAttr not found");
        return;
    }
    [[maybe_unused]] int32_t result = ((int32_t(*)(int32_t, int32_t, int32_t)) setDrcBufferAttr)(2, 1, 896);
    DEBUG_FUNCTION_LINE("AVMSetDRCBufferAttr(2, 1, 896) = %d", result);
}

// Whether a GamePad is connected: it answers a ping over its radio link. Not VPADRead, which
// a plugin can make succeed with no GamePad there (the DRC Read Result Spoofer does).
// Asked for up to half a second, as a GamePad switched on with the console may still be
// connecting when autoboot gets here.
static bool isGamePadConnected() {
    for (int i = 0; i < 10; i++) {
        if (CCRCDCDevicePing(CCR_CDC_DESTINATION_DRC0) == 0) {
            return true;
        }
        OSSleepTicks(OSMillisecondsToTicks(50));
    }
    return false;
}

static void launchvWiiTitle(uint64_t titleId) {
    // we need to init kpad for cmpt
    KPADInit();

    // With a GamePad connected, launch as before: the vWii can show on both screens. Without
    // one, the launch hangs on a black screen or is refused, see
    // https://github.com/wiiu-env/AutobootModule/issues/67, so launch for the TV alone.
    const bool gamePadConnected = isGamePadConnected();
    if (gamePadConnected) {
        // Try to find a screen type that works
        CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_BOTH);
        if (CMPTCheckScreenState() < 0) {
            CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_DRC);
            if (CMPTCheckScreenState() < 0) {
                CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_TV);
            }
        }
    } else {
        DEBUG_FUNCTION_LINE("No GamePad connected: launching the vWii for the TV only");
        // GamePad control off and the TV as the only screen
        CMPTAcctSetDrcCtrlEnabled(0);
        CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_TV);

        // On autoboot this code runs much earlier than a manual selection from
        // the boot menu, and launching before the compat subsystem is ready
        // results in a black screen. Wait for it, but launch anyway after 5s.
        for (int i = 0; i < 50 && CMPTCheckScreenState() < 0; i++) {
            OSSleepTicks(OSMillisecondsToTicks(100));
        }
    }

    uint32_t dataSize = 0;
    CMPTGetDataSize(&dataSize);

    if (!gamePadConnected) {
        setDrcModeForWiiMode();
    }

    void *dataBuffer = memalign(0x40, dataSize);

    if (titleId == 0) {
        CMPTLaunchMenu(dataBuffer, dataSize);
    } else {
        CMPTLaunchTitle(dataBuffer, dataSize, titleId);
    }

    free(dataBuffer);
}

void bootvWiiMenu() {
    launchvWiiTitle(0);
}

uint64_t getVWiiHBLTitleId() {
    // fall back to booting the vWii system menu if anything fails
    uint64_t titleId = 0;

    FSAInit();
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            // mount the slccmpt
            if (FSAMount(client, "/dev/slccmpt01", "/vol/storage_abm_slccmpt01", FSA_MOUNT_FLAG_GLOBAL_MOUNT, nullptr, 0) >= 0) {
                FSStat stat;

                // test if the OHBC or HBC is installed
                if (FSAGetStat(client, "/vol/storage_abm_slccmpt01/title/00010001/4f484243/content/00000000.app", &stat) >= 0) {
                    titleId = 0x000100014F484243L; // 'OHBC'
                } else if (FSAGetStat(client, "/vol/storage_abm_slccmpt01/title/00010001/4c554c5a/content/00000000.app", &stat) >= 0) {
                    titleId = 0x000100014C554C5AL; // 'LULZ'
                } else {
                    DEBUG_FUNCTION_LINE("Cannot find HBC");
                }
                FSAUnmount(client, "/vol/storage_abm_slccmpt01", FSA_UNMOUNT_FLAG_FORCE);
            } else {
                DEBUG_FUNCTION_LINE_ERR("Failed to mount slccmpt01");
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to unlock FSClient");
        }
        FSADelClient(client);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to add FSAClient");
    }
    return titleId;
}

void bootHomebrewChannel() {
    uint64_t titleId = getVWiiHBLTitleId();
    DEBUG_FUNCTION_LINE("Launching vWii title %016llx", titleId);
    launchvWiiTitle(titleId);
}
