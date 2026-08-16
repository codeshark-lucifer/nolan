#pragma once

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <mutex>
#include <cstring>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace OLIA_ENGINE
{

    class Allocator
    {
    public:
        Allocator() noexcept
        {
            m_callbacks.pUserData = this;

            m_callbacks.pfnAllocation =
                &Allocator::allocate;

            m_callbacks.pfnReallocation =
                &Allocator::reallocate;

            m_callbacks.pfnFree =
                &Allocator::free;

            m_callbacks.pfnInternalAllocation =
                &Allocator::internalAllocate;

            m_callbacks.pfnInternalFree =
                &Allocator::internalFree;
        }

        ~Allocator() = default;

        Allocator(const Allocator &) = delete;
        Allocator &operator=(const Allocator &) = delete;

        const VkAllocationCallbacks *callbacks() const noexcept
        {
            return &m_callbacks;
        }

    private:
        static void *VKAPI_CALL allocate(
            void *pUserData,
            size_t size,
            size_t alignment,
            VkSystemAllocationScope allocationScope)
        {
            auto *allocator =
                static_cast<Allocator *>(pUserData);

            void *ptr = nullptr;

#if defined(_WIN32)
            size_t align = alignment < sizeof(void *) ? sizeof(void *) : alignment;
            ptr = _aligned_malloc(
                size,
                align);

#else
            size_t align = alignment < sizeof(void *) ? sizeof(void *) : alignment;
            if (posix_memalign(
                    &ptr,
                    align,
                    size) != 0)
            {
                ptr = nullptr;
            }

#endif

            if (ptr != nullptr)
            {
                std::lock_guard<std::mutex> lock(
                    allocator->m_mutex);

                allocator->m_allocations[ptr] =
                    size;
            }

            return ptr;
        }

        static void *VKAPI_CALL reallocate(
            void *pUserData,
            void *pOriginal,
            size_t size,
            size_t alignment,
            VkSystemAllocationScope allocationScope)
        {
            auto *allocator =
                static_cast<Allocator *>(pUserData);

            if (size == 0)
            {
                free(
                    allocator,
                    pOriginal);

                return nullptr;
            }

            void *newPtr = nullptr;

#if defined(_WIN32)
            size_t align = alignment < sizeof(void *) ? sizeof(void *) : alignment;
            newPtr = _aligned_malloc(
                size,
                align);

#else
            size_t align = alignment < sizeof(void *) ? sizeof(void *) : alignment;
            if (posix_memalign(
                    &newPtr,
                    align,
                    size) != 0)
            {
                newPtr = nullptr;
            }

#endif

            if (newPtr == nullptr)
            {
                return nullptr;
            }

            size_t oldSize = 0;

            if (pOriginal != nullptr)
            {
                std::lock_guard<std::mutex> lock(
                    allocator->m_mutex);

                auto it =
                    allocator->m_allocations.find(
                        pOriginal);

                if (it != allocator->m_allocations.end())
                {
                    oldSize = it->second;
                }
            }

            if (pOriginal != nullptr && oldSize > 0)
            {
                const size_t copySize =
                    oldSize < size
                        ? oldSize
                        : size;

                std::memcpy(
                    newPtr,
                    pOriginal,
                    copySize);
            }

            if (pOriginal != nullptr)
            {
                std::lock_guard<std::mutex> lock(
                    allocator->m_mutex);

                allocator->m_allocations.erase(
                    pOriginal);
            }

            {
                std::lock_guard<std::mutex> lock(
                    allocator->m_mutex);

                allocator->m_allocations[newPtr] =
                    size;
            }

#if defined(_WIN32)
            if (pOriginal != nullptr)
                _aligned_free(pOriginal);
#else
            if (pOriginal != nullptr)
                std::free(pOriginal);
#endif

            return newPtr;
        }

        static void VKAPI_CALL free(
            void *pUserData,
            void *pMemory)
        {
            auto *allocator =
                static_cast<Allocator *>(pUserData);

            if (pMemory == nullptr)
                return;

            {
                std::lock_guard<std::mutex> lock(
                    allocator->m_mutex);

                allocator->m_allocations.erase(
                    pMemory);
            }

#if defined(_WIN32)
            _aligned_free(pMemory);
#else
            std::free(pMemory);
#endif
        }

        static void VKAPI_CALL internalAllocate(
            void *pUserData,
            size_t size,
            VkInternalAllocationType allocationType,
            VkSystemAllocationScope allocationScope)
        {
            (void)pUserData;
        }

        static void VKAPI_CALL internalFree(
            void *pUserData,
            size_t size,
            VkInternalAllocationType allocationType,
            VkSystemAllocationScope allocationScope)
        {
            (void)pUserData;
        }

    private:
        VkAllocationCallbacks m_callbacks{};

        std::unordered_map<void *, size_t>
            m_allocations;

        std::mutex m_mutex;
    };

} // namespace OLIA_ENGINE