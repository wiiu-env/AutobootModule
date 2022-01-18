#include <malloc.h>
#include <string>
#include <map>
#include <locale>
#include <vector>
#include <codecvt>
#include <memory>

#include "BootUtils.h"
#include "logger.h"
#include "DrawUtils.h"
#include "MenuUtils.h"
#include "ACTAccountInfo.h"

#include <coreinit/debug.h>
#include <coreinit/screen.h>
#include <nn/cmpt/cmpt.h>
#include <nn/act.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <padscore/kpad.h>
#include <vpad/input.h>
#include <gx2/state.h>


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
        auto slot = handleAccountSelectScreen(accountInfoList);

        DEBUG_FUNCTION_LINE("Load slot %d", slot);
        nn::act::LoadConsoleAccount(slot, 0, nullptr, false);
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
    launchvWiiTitle(0x00010001, 0x4f484243); // 'OHBC'
}
