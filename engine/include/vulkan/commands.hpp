#pragma once
#include "vulkan/vk_context.h"

namespace OLIA_ENGINE
{
    bool CreateCommandPoolAndBuffer(VKContext &ctx);
    void RecordCommandBuffer(VKContext &ctx);
    bool CreateSyncObjects(VKContext &ctx);
}