#include "BootUtils.h"
#include "DrawUtils.h"
#include "InputUtils.h"
#include "MenuUtils.h"
#include "QuickStartUtils.h"
#include "StorageUtils.h"
#include "logger.h"
#include <coreinit/debug.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/thread.h>
#include <coreinit/title.h>
#include <mocha/mocha.h>
#include <rpxloader/rpxloader.h>
#include <sndcore2/core.h>
#include <string>
#include <sys/stat.h>
#include <sysapp/launch.h>
#include <vpad/input.h>

bool gUpdatesBlocked = false;

int32_t main(int32_t argc, char **argv) {
    initLogging();
    DEBUG_FUNCTION_LINE("Hello from Autoboot Module");

    InputUtils::Init();

    initExternalStorage();

    // Use librpxloader.
    RPXLoaderStatus error3;
    if ((error3 = RPXLoader_InitLibrary()) != RPX_LOADER_RESULT_SUCCESS) {
        DEBUG_FUNCTION_LINE_ERR("AutobootModule: Failed to init RPXLoader. This can be ignored when not running Aroma. Error %s [%d]", RPXLoader_GetStatusStr(error3), error3);
    }

    // If we are in System Transfer context we need to restart the app to actually
    if (OSGetTitleID() == 0x0005001010062000L || OSGetTitleID() == 0x0005001010062100L || OSGetTitleID() == 0x0005001010062200L) {
        _SYSLaunchTitleWithStdArgsInNoSplash(OSGetTitleID(), nullptr);
        deinitLogging();
        return 0;
    }

    if (launchQuickStartTitle()) {
        deinitLogging();
        return 0;
    }

    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        OSFatal("AutobootModule: Mocha_InitLibrary failed");
    }

    InputUtils::InputData buttons = InputUtils::getControllerInput();

    bool hadMenu = false;

    FSAInit();
    auto client = FSAAddClient(nullptr);
    if (client > 0) {
        if (Mocha_UnlockFSClientEx(client) == MOCHA_RESULT_SUCCESS) {
            // test if the update folder exists
            FSADirectoryHandle dirHandle{};
            if (FSAOpenDir(client, "/vol/storage_mlc01/sys/update", &dirHandle) >= 0) {
                FSACloseDir(client, dirHandle);
                gUpdatesBlocked = false;
                if (!AXIsInit()) {
                    AXInit();
                }
                handleUpdateWarningScreen();
                hadMenu = true;
            } else {
                FSAStat st{};
                if (FSAGetStat(client, "/vol/storage_mlc01/sys/update", &st) != FS_ERROR_OK) {
                    DEBUG_FUNCTION_LINE_INFO("Created \"/vol/storage_mlc01/sys/update\" as file");
                    FSAFileHandle fd;
                    FSAOpenFileEx(client, "/vol/storage_mlc01/sys/update", "w", static_cast<FSMode>(0x666), FS_OPEN_FLAG_NONE, 0, &fd);
                }
                gUpdatesBlocked = true;
            }
        } else {
            DEBUG_FUNCTION_LINE_ERR("Failed to unlock FSA Client");
        }

        FSADelClient(client);
    } else {
        DEBUG_FUNCTION_LINE_ERR("Failed to create FSA Client");
    }

    bool showvHBL          = getVWiiHBLTitleId() != 0;
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

    if ((bootSelection == -1) ||
        (bootSelection == BOOT_OPTION_HOMEBREW_LAUNCHER && !showHBL) ||
        (bootSelection == BOOT_OPTION_VWII_HOMEBREW_CHANNEL && !showvHBL) ||
        (buttons.hold & VPAD_BUTTON_PLUS)) {
        if (!AXIsInit()) {
            AXInit();
        }
        bootSelection = handleMenuScreen(configPath, bootSelection, menu);
        hadMenu       = true;
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
            default:
                bootWiiUMenu();
                break;
        }
    } else {
        bootWiiUMenu();
    }

    InputUtils::DeInit();
    Mocha_DeInitLibrary();
    deinitLogging();
    if (AXIsInit()) {
        AXQuit();
    }

    if (!hadMenu) {
        DrawUtils::ClearSavedFrameBuffers();
    }

    return 0;
}
