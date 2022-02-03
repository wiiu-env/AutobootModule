#include <codecvt>
#include <locale>
#include <malloc.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ACTAccountInfo.h"
#include "BootUtils.h"
#include "DrawUtils.h"
#include "MenuUtils.h"
#include "logger.h"

#include <coreinit/debug.h>
#include <coreinit/screen.h>
#include <gx2/state.h>
#include <nn/act.h>
#include <nn/cmpt/cmpt.h>
#include <padscore/kpad.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <vpad/input.h>

#include <iosuhax.h>

void handleAccountSelection();

void bootWiiUMenu() {
    nn::act::Initialize();
    nn::act::SlotNo slot = nn::act::GetSlotNo();
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
            accountInfo->slot = i;
            auto result = nn::act::GetMiiNameEx(reinterpret_cast<int16_t *>(nameOut), i);
            if (result.IsSuccess()) {
                std::u16string source;
                std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
                accountInfo->name = convert.to_bytes((char16_t *) nameOut);
            } else {
                accountInfo->name = "[UNKNOWN]";
            }
            accountInfo->isNetworkAccount = nn::act::IsNetworkAccountEx(i);
            if (accountInfo->isNetworkAccount) {
                nn::act::GetAccountIdEx(accountInfo->accountId, i);
            }

            uint32_t imageSize = 0;
            result = nn::act::GetMiiImageEx(&imageSize, accountInfo->miiImageBuffer, sizeof(accountInfo->miiImageBuffer), 0, i);
            if (result.IsSuccess()) {
                accountInfo->miiImageSize = imageSize;
            }
            accountInfoList.push_back(accountInfo);
        }

        if (accountInfoList.size() > 0) {
            auto slot = handleAccountSelectScreen(accountInfoList);

            DEBUG_FUNCTION_LINE("Load slot %d", slot);
            nn::act::LoadConsoleAccount(slot, 0, nullptr, false);
        }
    }
    nn::act::Finalize();
}

static void launchvWiiTitle(uint32_t titleId_low, uint32_t titleId_high) {
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

    if (titleId_low == 0 && titleId_high == 0) {
        CMPTLaunchMenu(dataBuffer, dataSize);
    } else {
        CMPTLaunchTitle(dataBuffer, dataSize, titleId_low, titleId_high);
    }

    free(dataBuffer);
}

void bootvWiiMenu() {
    launchvWiiTitle(0, 0);
}

void bootHomebrewChannel() {
    // fall back to booting the vWii system menu if anything fails
    uint64_t titleId = 0;

    if (IOSUHAX_Open(nullptr) >= 0) {
        int fsaFd = IOSUHAX_FSA_Open();
        if (fsaFd >= 0) {
            // mount the slccmpt
            if (IOSUHAX_FSA_Mount(fsaFd, "/dev/slccmpt01", "/vol/storage_slccmpt01", 2, nullptr, 0) >= 0) {
                fileStat_s stat;

                // test if the OHBC or HBC is installed
                if (IOSUHAX_FSA_GetStat(fsaFd, "/vol/storage_slccmpt01/title/00010001/4f484243/content/00000000.app", &stat) >= 0) {
                    titleId = 0x000100014F484243L; // 'OHBC'
                } else if (IOSUHAX_FSA_GetStat(fsaFd, "/vol/storage_slccmpt01/title/00010001/4c554c5a/content/00000000.app", &stat) >= 0) {
                    titleId = 0x000100014C554C5AL; // 'LULZ'
                } else {
                    DEBUG_FUNCTION_LINE("Cannot find HBC, booting vWii System Menu");
                }

                IOSUHAX_FSA_Unmount(fsaFd, "/vol/storage_slccmpt01", 2);
            } else {
                DEBUG_FUNCTION_LINE("Failed to mount SLCCMPT");
            }

            IOSUHAX_FSA_Close(fsaFd);
        } else {
            DEBUG_FUNCTION_LINE("Failed to open FSA");
        }

        IOSUHAX_Close();
    } else {
        DEBUG_FUNCTION_LINE("Failed to open IOSUHAX");
    }

    DEBUG_FUNCTION_LINE("Launching vWii title %016llx", titleId);
    launchvWiiTitle((uint32_t) (titleId >> 32), (uint32_t) (titleId & 0xffffffff));
}
