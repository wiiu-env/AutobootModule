#include "DrawUtils.h"
#include "QuickStartUtils.h"
#include "StorageUtils.h"
#include "logger.h"
#include <coreinit/debug.h>
#include <gx2/state.h>
#include <malloc.h>
#include <string>
#include <vpad/input.h>

#include "BootUtils.h"
#include "MenuUtils.h"

void clearScreen() {
    auto buffer = DrawUtils::InitOSScreen();
    if (!buffer) {
        OSFatal("Failed to alloc memory for screen");
    }
    DrawUtils::clear(COLOR_BACKGROUND);

    // Call GX2Init to shut down OSScreen
    GX2Init(nullptr);

    free(buffer);
}

int32_t main(int32_t argc, char **argv) {
    initLogging();
    DEBUG_FUNCTION_LINE("Hello from Autoboot Module");

    // Clear screen to avoid screen corruptions when loading the Wii U Menu
    clearScreen();

    initExternalStorage();

    if (getQuickBoot()) {
        deinitLogging();
        return 0;
    }

    std::string configPath     = "fs:/vol/external01/wiiu/autoboot.cfg";
    std::string drcSettingPath = "fs:/vol/external01/wiiu/drcenabled.cfg";

    if (argc >= 1) {
        configPath     = std::string(argv[0]) + "/autoboot.cfg";
        drcSettingPath = std::string(argv[0]) + "/drcenabled.cfg";
    }

    int32_t bootSelection = readAutobootOption(configPath);

    VPADStatus vpad{};
    VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

    if ((bootSelection == -1) || (vpad.hold & VPAD_BUTTON_PLUS)) {
        bootSelection = handleMenuScreen(configPath, drcSettingPath, bootSelection, readDrcEnabledOption(drcSettingPath));
    }

    if (bootSelection >= 0) {
        switch (bootSelection) {
            case BOOT_OPTION_WII_U_MENU:
                bootWiiUMenu();
                break;
            case BOOT_OPTION_HOMEBREW_LAUNCHER:
                bootHomebrewLauncher();
                break;
            case BOOT_OPTION_VWII_SYSTEM_MENU:
                bootvWiiMenu(readDrcEnabledOption(drcSettingPath));
                break;
            case BOOT_OPTION_VWII_HOMEBREW_CHANNEL:
                bootHomebrewChannel(readDrcEnabledOption(drcSettingPath));
                break;
            default:
                bootWiiUMenu();
                break;
        }
    } else {
        bootWiiUMenu();
    }

    deinitLogging();
    return 0;
}
