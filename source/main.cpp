#include "BootUtils.h"
#include "DrawUtils.h"
#include "MenuUtils.h"
#include "QuickStartUtils.h"
#include "StorageUtils.h"
#include "logger.h"
#include <coreinit/debug.h>
#include <coreinit/filesystem_fsa.h>
#include <gx2/state.h>
#include <malloc.h>
#include <mocha/mocha.h>
#include <sndcore2/core.h>
#include <string>
#include <sys/stat.h>
#include <vpad/input.h>

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
    AXInit();

    // Clear screen to avoid screen corruptions when loading the Wii U Menu
    clearScreen();

    initExternalStorage();

    if (getQuickBoot()) {
        AXQuit();
        deinitLogging();
        return 0;
    }

    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        OSFatal("AutobootModule: Mocha_InitLibrary failed");
    }

    VPADStatus vpad{};
    // Buffer vpad read.
    VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

    FSAInit();
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            // test if the update folder exists
            FSADirectoryHandle dirHandle;
            if (FSAOpenDir(client, "/vol/storage_mlc01/sys/update", &dirHandle) >= 0) {
                FSACloseDir(client, dirHandle);
                handleUpdateWarningScreen();
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to unlock FSA Client");
        }

        FSADelClient(client);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to create FSA Client");
    }

    bool showvHBL          = getVWiiHBLTitleId() != 0;
    bool showNintendont    = getVWiiNintendontTitleId() != 0;
    bool showHBL           = false;
    std::string configPath = "fs:/vol/external01/wiiu/autoboot.cfg";
    if (argc >= 1) {
        configPath = std::string(argv[0]) + "/autoboot.cfg";

        auto hblInstallerPath = std::string(argv[0]) + "/modules/setup/50_hbl_installer.rpx";
        struct stat st {};
        if (stat(hblInstallerPath.c_str(), &st) >= 0) {
            showHBL = true;
        }
    }

    int32_t bootSelection = readAutobootOption(configPath);

    std::map<uint32_t, std::string> menu;
    menu[BOOT_OPTION_WII_U_MENU] = "Wii U Menu";
    if (showHBL) {
        menu[BOOT_OPTION_HOMEBREW_LAUNCHER] = "Homebrew Launcher";
    }
    menu[BOOT_OPTION_VWII_SYSTEM_MENU] = "vWii System Menu";
    if (showvHBL) {
        menu[BOOT_OPTION_VWII_HOMEBREW_CHANNEL] = "vWii Homebrew Channel";
    }
    if (showNintendont) {
        menu[BOOT_OPTION_VWII_NINTENDONT] = "vWii Nintendont";
    }

    if ((bootSelection == -1) ||
        (bootSelection == BOOT_OPTION_HOMEBREW_LAUNCHER && !showHBL) ||
        (bootSelection == BOOT_OPTION_VWII_HOMEBREW_CHANNEL && !showvHBL) ||
        (bootSelection == BOOT_OPTION_VWII_NINTENDONT && !showNintendont) ||
        (vpad.hold & VPAD_BUTTON_PLUS)) {
        bootSelection = handleMenuScreen(configPath, bootSelection, menu);
    }

    if (bootSelection >= 0) {
        switch (bootSelection) {
            case BOOT_OPTION_WII_U_MENU:
                bootWiiUMenu();
                break;
            case BOOT_OPTION_HOMEBREW_LAUNCHER:
                if (!showHBL) {
                    bootWiiUMenu();
                    break;
                }
                bootHomebrewLauncher();
                break;
            case BOOT_OPTION_VWII_SYSTEM_MENU:
                bootvWiiMenu();
                break;
            case BOOT_OPTION_VWII_HOMEBREW_CHANNEL:
                if (!showvHBL) {
                    bootvWiiMenu();
                    break;
                }
                bootHomebrewChannel();
                break;
            case BOOT_OPTION_VWII_NINTENDONT:
                if (!showNintendont) {
                    bootvWiiMenu();
                    break;
                }
                bootNintendont();
                break;
            default:
                bootWiiUMenu();
                break;
        }
    } else {
        bootWiiUMenu();
    }

    AXQuit();
    Mocha_DeInitLibrary();
    deinitLogging();

    return 0;
}
