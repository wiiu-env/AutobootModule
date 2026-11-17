#include "BootUtils.h"
#include "ACTAccountInfo.h"
#include "MenuUtils.h"
#include "logger.h"
#include <codecvt>
#include <coreinit/filesystem_fsa.h>
#include <cstdio>
#include <filesystem>
#include <locale>
#include <malloc.h>
#include <memory>
#include <mocha/mocha.h>
#include <nn/act.h>
#include <nn/cmpt/cmpt.h>
#include <padscore/kpad.h>
#include <sndcore2/core.h>
#include <string>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <vector>

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

static void launchvWiiTitle(uint64_t titleId) {
    // we need to init kpad for cmpt
    KPADInit();

    // Try to find a screen type that works
    CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_BOTH);
    if (CMPTCheckScreenState() < 0) {
        CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_DRC);
        if (CMPTCheckScreenState() < 0) {
            CMPTAcctSetScreenType(CMPT_SCREEN_TYPE_TV);
        }
    }

    uint32_t dataSize = 0;
    CMPTGetDataSize(&dataSize);

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

bool isVWiiTitle(uint64_t titleId) {
    return (titleId >> 48) == 0x0001;
}

bool isWiiUTitle(uint64_t titleId) {
    return (titleId >> 48) == 0x0005;
}

bool vWiiTitleExists(uint64_t titleId) {
    bool exists = false;

    FSAInit();
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            // mount the slccmpt
            if (FSAMount(client, "/dev/slccmpt01", "/vol/storage_abm_slccmpt01", FSA_MOUNT_FLAG_GLOBAL_MOUNT, nullptr, 0) >= 0) {
                // every installed title has a TMD; content file names depend on the title
                char path[0x80];
                snprintf(path, sizeof(path), "/vol/storage_abm_slccmpt01/title/%08x/%08x/content/title.tmd",
                         (uint32_t) (titleId >> 32), (uint32_t) titleId);
                FSStat stat;
                exists = FSAGetStat(client, path, &stat) >= 0;
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
    return exists;
}

bool titleExists(uint64_t titleId) {
    if (isVWiiTitle(titleId)) {
        return vWiiTitleExists(titleId);
    }
    if (isWiiUTitle(titleId)) {
        return SYSCheckTitleExists(titleId);
    }
    return false;
}

uint64_t getVWiiHBLTitleId() {
    // test if the OHBC or HBC is installed, fall back to booting the vWii system menu if anything fails
    if (vWiiTitleExists(0x000100014F484243L)) {
        return 0x000100014F484243L; // 'OHBC'
    }
    if (vWiiTitleExists(0x000100014C554C5AL)) {
        return 0x000100014C554C5AL; // 'LULZ'
    }
    DEBUG_FUNCTION_LINE("Cannot find HBC");
    return 0;
}

void bootHomebrewChannel() {
    uint64_t titleId = getVWiiHBLTitleId();
    DEBUG_FUNCTION_LINE("Launching vWii title %016llx", titleId);
    launchvWiiTitle(titleId);
}

void bootCustomTitle(uint64_t titleId) {
    if (isVWiiTitle(titleId)) {
        DEBUG_FUNCTION_LINE("Launching vWii title %016llx", titleId);
        launchvWiiTitle(titleId);
    } else {
        handleAccountSelection();
        DEBUG_FUNCTION_LINE("Launching Wii U title %016llx", titleId);
        _SYSLaunchTitleWithStdArgsInNoSplash(titleId, nullptr);
    }
}
