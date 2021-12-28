#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ScreenType {
    SCREEN_TYPE_TV = 1,
    SCREEN_TYPE_DRC,
    SCREEN_TYPE_BOTH,
} ScreenType;

int32_t
CMPTGetDataSize(uint32_t* outSize);

int32_t
CMPTLaunchTitle(void* dataBuffer, uint32_t bufferSize, uint32_t titleId_low, uint32_t titleId_high);

int32_t
CMPTAcctSetScreenType(ScreenType type);

int32_t
CMPTCheckScreenState(void);

#ifdef __cplusplus
}
#endif
