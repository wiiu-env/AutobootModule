#pragma once

#include <wut.h>
#include <nn/result.h>
#include <coreinit/filesystem.h>
#include <coreinit/memdefaultheap.h>

namespace nn::sl {
    typedef struct WUT_PACKED FileStreamInternal {
        WUT_UNKNOWN_BYTES(0x10);
    } FileStreamInternal;
    WUT_CHECK_SIZE(FileStreamInternal, 0x10);

    extern "C" nn::Result Initialize__Q3_2nn2sl10FileStreamFP8FSClientP10FSCmdBlockPCcT3(FileStreamInternal *, FSClient *, FSCmdBlock *, char const *, char const *);
    extern "C" FileStreamInternal *__ct__Q3_2nn2sl10FileStreamFv(FileStreamInternal *);
    extern "C" void __dt__Q3_2nn2sl10FileStreamFv(FileStreamInternal *);

    typedef struct WUT_PACKED LaunchInfo {
        uint64_t titleId;
        WUT_UNKNOWN_BYTES(0x810 - 0x08);
    } LaunchInfo;

    WUT_CHECK_OFFSET(LaunchInfo, 0x00, titleId);
    WUT_CHECK_SIZE(LaunchInfo, 0x810);

    void
    GetDefaultDatabasePath(char *, int size, uint32_t tid_hi, uint32_t tid_low)
    asm("GetDefaultDatabasePath__Q2_2nn2slFPcUiUL");

    nn::Result
    Initialize(MEMAllocFromDefaultHeapExFn, MEMFreeToDefaultHeapFn)
    asm("Initialize__Q2_2nn2slFPFUiT1_PvPFPv_v");

    nn::Result
    Finalize()
    asm("Finalize__Q2_2nn2slFv");

    typedef enum Region {
        REGION_JPN = 0,
        REGION_USA = 1,
        REGION_EUR = 2
    } Region;

    typedef struct WUT_PACKED LaunchInfoDatabaseInternal {
        WUT_UNKNOWN_BYTES(0x1C);
    } LaunchInfoDatabaseInternal;
    WUT_CHECK_SIZE(LaunchInfoDatabaseInternal, 0x1C);

    extern "C" LaunchInfoDatabaseInternal *__ct__Q3_2nn2sl18LaunchInfoDatabaseFv(LaunchInfoDatabaseInternal *);
    extern "C" nn::Result Load__Q3_2nn2sl18LaunchInfoDatabaseFRQ3_2nn2sl7IStreamQ3_2nn2sl6Region(LaunchInfoDatabaseInternal *, nn::sl::FileStreamInternal *, nn::sl::Region);
    extern "C" void Finalize__Q3_2nn2sl18LaunchInfoDatabaseFv(LaunchInfoDatabaseInternal *);
    extern "C" nn::Result GetLaunchInfoById__Q3_2nn2sl18LaunchInfoDatabaseCFPQ3_2nn2sl10LaunchInfoUL(LaunchInfoDatabaseInternal *, nn::sl::LaunchInfo *, uint64_t titleId);
}

