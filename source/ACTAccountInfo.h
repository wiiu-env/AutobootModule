#pragma once
#include <nn/act.h>
#include <string>
#include <stdint.h>

class AccountInfo {
public:
    AccountInfo() = default;

    nn::act::SlotNo slot{};
    std::string name;
    char accountId[nn::act::AccountIdSize];
    bool isNetworkAccount = false;
    uint8_t miiImageBuffer[65554];
    uint32_t miiImageSize = 0;
};
