#pragma once

#include <wut.h>
#include <nn/result.h>
#include <coreinit/filesystem.h>
#include "common.h"

namespace nn::sl {
    class FileStream {
    public:
        FileStream() {
            instance = __ct__Q3_2nn2sl10FileStreamFv(nullptr);
        }

        ~FileStream() {
            if (instance != nullptr) {
                __dt__Q3_2nn2sl10FileStreamFv(instance);
            }
        }

        nn::Result Initialize(FSClient *client, FSCmdBlock *cmdBlock, char const *path, char const *mode) {
            return Initialize__Q3_2nn2sl10FileStreamFP8FSClientP10FSCmdBlockPCcT3(this->instance, client, cmdBlock, path, mode);
        }

        FileStreamInternal *instance = nullptr;
    };

}