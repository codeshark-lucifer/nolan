#pragma once
#include "vulkan/vk_context.h"

namespace OLIA_ENGINE
{
    void CreateOffscreenResources(VKContext &ctx);
    void DestroyOffscreenResources(VKContext &ctx);

    bool CreateOffscreenRenderPass(VKContext &ctx);
    bool CreateOffscreenFramebuffer(VKContext &ctx);

    bool CreatePostProcessPipeline(VKContext &ctx);
    void CreatePostProcessDescriptors(VKContext &ctx);
    void DestroyPostProcessPipeline(VKContext &ctx);
}