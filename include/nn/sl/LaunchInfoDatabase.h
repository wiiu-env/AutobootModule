#pragma once

#include <wut.h>
#include <nn/result.h>
#include "common.h"
#include "FileStream.h"

namespace nn::sl {
    class LaunchInfoDatabase {
    public:
        LaunchInfoDatabase() {
            instance = __ct__Q3_2nn2sl18LaunchInfoDatabaseFv(nullptr);
        }

        ~LaunchInfoDatabase() {
            Finalize__Q3_2nn2sl18LaunchInfoDatabaseFv(instance);
        }

        // nn::sl::LaunchInfoDatabase::Load(nn::sl::IStream &, nn::sl::Region)
        nn::Result Load(nn::sl::FileStream *fileStream, nn::sl::Region region) {
            return Load__Q3_2nn2sl18LaunchInfoDatabaseFRQ3_2nn2sl7IStreamQ3_2nn2sl6Region(instance, fileStream->instance, region);
        }

        // nn::sl::LaunchInfoDatabase::GetLaunchInfoById(nn::sl::LaunchInfo *, unsigned long long) const
        nn::Result GetLaunchInfoById(nn::sl::LaunchInfo *launchInfo, uint64_t titleId) {
            return GetLaunchInfoById__Q3_2nn2sl18LaunchInfoDatabaseCFPQ3_2nn2sl10LaunchInfoUL(instance, launchInfo, titleId);
        }

    private:
        LaunchInfoDatabaseInternal *instance;
    };

}