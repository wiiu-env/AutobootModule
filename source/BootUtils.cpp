#include <malloc.h>

#include "BootUtils.h"

#include <nn/cmpt/cmpt.h>
#include <nn/act.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <padscore/kpad.h>

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
    uint64_t titleId = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_MII_MAKER);
    _SYSLaunchTitleWithStdArgsInNoSplash(titleId, nullptr);
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
