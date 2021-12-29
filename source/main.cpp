#include <malloc.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <coreinit/screen.h>
#include <coreinit/filesystem.h>
#include <coreinit/memdefaultheap.h>
#include <vpad/input.h>
#include <sysapp/launch.h>
#include <sysapp/title.h>
#include <padscore/kpad.h>
#include <nn/acp.h>
#include <nn/act.h>
#include <nn/cmpt.h>
#include <nn/ccr/sys_caffeine.h>
#include <nn/sl.h>
#include <nn/spm.h>

#include <whb/log_module.h>
#include <whb/log_udp.h>
#include <whb/log_cafe.h>

#include "DrawUtils.h"
#include "logger.h"

#define COLOR_WHITE      Color(0xffffffff)
#define COLOR_BLACK      Color(0, 0, 0, 255)
#define COLOR_BACKGROUND COLOR_BLACK
#define COLOR_TEXT       COLOR_WHITE
#define COLOR_TEXT2      Color(0xB3ffffff)
#define COLOR_AUTOBOOT   Color(0xaeea00ff)
#define COLOR_BORDER     Color(204, 204, 204, 255)
#define COLOR_BORDER_HIGHLIGHTED Color(0x3478e4ff)

enum {
    BOOT_OPTION_WII_U_MENU,
    BOOT_OPTION_HOMEBREW_LAUNCHER,
    BOOT_OPTION_VWII_SYSTEM_MENU,
    BOOT_OPTION_VWII_HOMEBREW_CHANNEL,
};

static const char *menu_options[] = {
        "Wii U Menu",
        "Homebrew Launcher",
        "vWii System Menu",
        "vWii Homebrew Channel",
};

static const char *autoboot_config_strings[] = {
        "wiiu_menu",
        "homebrew_launcher",
        "vwii_system_menu",
        "vwii_homebrew_channel",
};

std::string configPath;

int32_t readAutobootOption() {
    FILE *f = fopen(configPath.c_str(), "r");
    if (f) {
        char buf[128]{};
        fgets(buf, sizeof(buf), f);
        fclose(f);

        for (uint32_t i = 0; i < sizeof(autoboot_config_strings) / sizeof(char *); i++) {
            if (strncmp(autoboot_config_strings[i], buf, strlen(autoboot_config_strings[i])) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void writeAutobootOption(int32_t autobootOption) {
    FILE *f = fopen(configPath.c_str(), "w");
    if (f) {
        if (autobootOption >= 0) {
            fputs(autoboot_config_strings[autobootOption], f);
        } else {
            fputs("none", f);
        }

        fclose(f);
    }
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
        auto *fsClient = (FSClient *) memalign(0x40, sizeof(FSClient));
        memset(fsClient, 0, sizeof(*fsClient));
        FSAddClient(fsClient, FS_ERROR_FLAG_NONE);

        fileStream->Initialize(fsClient, &cmdBlock, path, "r");

        auto database = new nn::sl::LaunchInfoDatabase;
        database->Load(fileStream, nn::sl::REGION_EUR);

        CCRAppLaunchParam data;    // load sys caffeine data
        // load app launch param
        CCRSysCaffeineGetAppLaunchParam(&data);

        // get launch info for id
        nn::sl::LaunchInfo info;
        auto result = database->GetLaunchInfoById(&info, data.titleId);

        delete database;
        delete fileStream;

        FSDelClient(fsClient, FS_ERROR_FLAG_NONE);

        nn::sl::Finalize();

        if (!result.IsSuccess()) {
            DEBUG_FUNCTION_LINE("GetLaunchInfoById failed.");
            return false;
        }

        if (info.titleId == 0x0005001010040000L ||
            info.titleId == 0x0005001010040100L ||
            info.titleId == 0x0005001010040200L) {
            DEBUG_FUNCTION_LINE("Skip quick booting into the Wii U Menu");
            return false;
        }
        if (!SYSCheckTitleExists(info.titleId)) {
            DEBUG_FUNCTION_LINE("Title %016llX doesn't exist", info.titleId);
            return false;
        }

        MCPTitleListType titleInfo;
        int32_t handle = MCP_Open();
        auto err = MCP_GetTitleInfo(handle, info.titleId, &titleInfo);
        MCP_Close(handle);
        if (err == 0) {
            nn::act::Initialize();
            for (int32_t i = 0; i < 13; i++) {
                char uuid[16];
                result = nn::act::GetUuidEx(uuid, i);
                if (result.IsSuccess()) {
                    if (memcmp(uuid, data.uuid, 8) == 0) {
                        DEBUG_FUNCTION_LINE("Load Console account %d", i);
                        nn::act::LoadConsoleAccount(i, 0, 0, 0);
                        break;
                    }
                }
            }
            nn::act::Finalize();

            ACPAssignTitlePatch(&titleInfo);
            _SYSLaunchTitleWithStdArgsInNoSplash(info.titleId, nullptr);
            return true;
        }

        return false;
    } else {
        DEBUG_FUNCTION_LINE("No quick boot");
    }
    return false;
}

static void initExternalStorage() {
    nn::spm::Initialize();

    nn::spm::StorageListItem items[0x20];
    int32_t numItems = nn::spm::GetStorageList(items, 0x20);

    bool found = false;
    for (int32_t i = 0; i < numItems; i++) {
        if (items[i].type == nn::spm::STORAGE_TYPE_WFS) {
            nn::spm::StorageInfo info{};
            if (nn::spm::GetStorageInfo(&info, &items[i].index) == 0) {
                DEBUG_FUNCTION_LINE("Using %s for extended storage", info.path);

                nn::spm::SetExtendedStorage(&items[i].index);
                ACPMountExternalStorage();

                nn::spm::SetDefaultExtendedStorageVolumeId(info.volumeId);

                found = true;
                break;
            }
        }
    }
    if (!found) {
        DEBUG_FUNCTION_LINE("Fallback to empty ExtendedStorage");
        nn::spm::VolumeId empty{};
        nn::spm::SetDefaultExtendedStorageVolumeId(empty);

        nn::spm::StorageIndex storageIndex = 0;
        nn::spm::SetExtendedStorage(&storageIndex);
    }

    nn::spm::Finalize();
}

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

int32_t handleMenuScreen(int32_t autobootOptionInput) {
    OSScreenInit();

    uint32_t tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    uint32_t drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    auto *screenBuffer = (uint8_t *) memalign(0x100, tvBufferSize + drcBufferSize);

    OSScreenSetBufferEx(SCREEN_TV, screenBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, screenBuffer + tvBufferSize);

    OSScreenEnableEx(SCREEN_TV, TRUE);
    OSScreenEnableEx(SCREEN_DRC, TRUE);

    DrawUtils::initBuffers(screenBuffer, tvBufferSize, screenBuffer + tvBufferSize, drcBufferSize);
    DrawUtils::initFont();

    uint32_t selected = 0;
    int autoboot = autobootOptionInput;
    bool redraw = true;
    while (true) {
        VPADStatus vpad{};
        VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

        if (vpad.trigger & VPAD_BUTTON_UP) {
            if (selected > 0) {
                selected--;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
            if (selected < sizeof(menu_options) / sizeof(char *) - 1) {
                selected++;
                redraw = true;
            }
        } else if (vpad.trigger & VPAD_BUTTON_A) {
            break;
        } else if (vpad.trigger & VPAD_BUTTON_X) {
            autoboot = -1;
            redraw = true;
        } else if (vpad.trigger & VPAD_BUTTON_Y) {
            autoboot = selected;
            redraw = true;
        }

        if (redraw) {
            DrawUtils::beginDraw();
            DrawUtils::clear(COLOR_BACKGROUND);

            // draw buttons
            uint32_t index = 8 + 24 + 8 + 4;
            for (uint32_t i = 0; i < sizeof(menu_options) / sizeof(char *); i++) {
                if (i == selected) {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 4, COLOR_BORDER_HIGHLIGHTED);
                } else {
                    DrawUtils::drawRect(16, index, SCREEN_WIDTH - 16 * 2, 44, 2, (i == (uint32_t) autoboot) ? COLOR_AUTOBOOT : COLOR_BORDER);
                }

                DrawUtils::setFontSize(24);
                DrawUtils::setFontColor((i == (uint32_t) autoboot) ? COLOR_AUTOBOOT : COLOR_TEXT);
                DrawUtils::print(16 * 2, index + 8 + 24, menu_options[i]);
                index += 42 + 8;
            }

            DrawUtils::setFontColor(COLOR_TEXT);

            // draw top bar
            DrawUtils::setFontSize(24);
            DrawUtils::print(16, 6 + 24, "Tiramisu Boot Selector");
            DrawUtils::drawRectFilled(8, 8 + 24 + 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);

            // draw bottom bar
            DrawUtils::drawRectFilled(8, SCREEN_HEIGHT - 24 - 8 - 4, SCREEN_WIDTH - 8 * 2, 3, COLOR_WHITE);
            DrawUtils::setFontSize(18);
            DrawUtils::print(16, SCREEN_HEIGHT - 8, "\ue07d Navigate ");
            DrawUtils::print(SCREEN_WIDTH - 16, SCREEN_HEIGHT - 8, "\ue000 Choose", true);
            const char *autobootHints = "\ue002 Clear Autoboot / \ue003 Select Autboot";
            DrawUtils::print(SCREEN_WIDTH / 2 + DrawUtils::getTextWidth(autobootHints) / 2, SCREEN_HEIGHT - 8, autobootHints, true);

            DrawUtils::endDraw();

            redraw = false;
        }
    }

    DrawUtils::clear(COLOR_BLACK);
    DrawUtils::endDraw();

    if (autoboot != autobootOptionInput) {
        writeAutobootOption(autoboot);
    }

    DrawUtils::deinitFont();

    free(screenBuffer);

    return selected;
}


int32_t main(int32_t argc, char **argv) {
    if (!WHBLogModuleInit()) {
        WHBLogCafeInit();
        WHBLogUdpInit();
    }
    DEBUG_FUNCTION_LINE("Hello from Autoboot");

    initExternalStorage();

    if (getQuickBoot()) {
        return 0;
    }

    configPath = "fs:/vol/exernal01/wiiu/autoboot.cfg";
    if (argc >= 1) {
        configPath = std::string(argv[0]) + "/autoboot.cfg";
    }

    int32_t bootSelection = readAutobootOption();

    VPADStatus vpad{};
    VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr);

    if ((bootSelection == -1) || (vpad.hold & VPAD_BUTTON_PLUS)) {
        bootSelection = handleMenuScreen(bootSelection);
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
                bootvWiiMenu();
                break;
            case BOOT_OPTION_VWII_HOMEBREW_CHANNEL:
                bootHomebrewChannel();
                break;
            default:
                bootWiiUMenu();
                break;
        }
    } else {
        bootWiiUMenu();
    }

    return 0;
}
