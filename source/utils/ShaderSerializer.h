#pragma once

#include <gx2/shaders.h>
#include <memory>
#include <span>
#include <vector>

class GX2PixelShaderWrapper {
public:
    [[nodiscard]] GX2PixelShader *getPixelShader() {
        return &pixelShader;
    }

    ~GX2PixelShaderWrapper() {
        if (pixelShader.program) {
            free(pixelShader.program);
        }

        if (pixelShader.uniformBlocks) {
            for (uint32_t i = 0; i < pixelShader.uniformBlockCount; i++) {
                free((void *) pixelShader.uniformBlocks[i].name);
            }

            free(pixelShader.uniformBlocks);
        }

        if (pixelShader.uniformVars) {
            for (uint32_t i = 0; i < pixelShader.uniformVarCount; i++) {
                free((void *) pixelShader.uniformVars[i].name);
            }
            free(pixelShader.uniformVars);
        }

        if (pixelShader.initialValues) {
            free(pixelShader.initialValues);
        }

        if (pixelShader.samplerVars) {
            for (uint32_t i = 0; i < pixelShader.samplerVarCount; i++) {
                free((void *) pixelShader.samplerVars[i].name);
            }
            free(pixelShader.samplerVars);
        }

        if (pixelShader.loopVars) {
            free(pixelShader.loopVars);
        }

        if (pixelShader.gx2rBuffer.buffer) {
            free(pixelShader.gx2rBuffer.buffer);
        }
    }

private:
    alignas(0x40) GX2PixelShader pixelShader;
};

class GX2VertexShaderWrapper {
public:
    [[nodiscard]] GX2VertexShader *getVertexShader() {
        return &vertexShader;
    }

    ~GX2VertexShaderWrapper() {
        if (vertexShader.program) {
            free(vertexShader.program);
        }

        if (vertexShader.uniformBlocks) {
            for (uint32_t i = 0; i < vertexShader.uniformBlockCount; i++) {
                free((void *) vertexShader.uniformBlocks[i].name);
            }

            free(vertexShader.uniformBlocks);
        }

        if (vertexShader.uniformVars) {
            for (uint32_t i = 0; i < vertexShader.uniformVarCount; i++) {
                free((void *) vertexShader.uniformVars[i].name);
            }

            free(vertexShader.uniformVars);
        }

        if (vertexShader.initialValues) {
            free(vertexShader.initialValues);
        }

        if (vertexShader.loopVars) {
            free(vertexShader.loopVars);
        }

        if (vertexShader.samplerVars) {
            for (uint32_t i = 0; i < vertexShader.samplerVarCount; i++) {
                free((void *) vertexShader.samplerVars[i].name);
            }

            free(vertexShader.samplerVars);
        }

        if (vertexShader.attribVars) {
            for (uint32_t i = 0; i < vertexShader.attribVarCount; i++) {
                free((void *) vertexShader.attribVars[i].name);
            }

            free(vertexShader.attribVars);
        }
    }

private:
    alignas(0x40) GX2VertexShader vertexShader;
};

std::vector<uint8_t> SerializeVertexShader(GX2VertexShader *vertexShader);

std::vector<uint8_t> SerializePixelShader(GX2PixelShader *pixelShader);

std::unique_ptr<GX2VertexShaderWrapper> DeserializeVertexShader(const std::span<const uint8_t> &data);

std::unique_ptr<GX2PixelShaderWrapper> DeserializePixelShader(const std::span<const uint8_t> &data);
