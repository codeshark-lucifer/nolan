#pragma once

#include "./entity.hpp"
#include "./pool.hpp"
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <optional>
#include "engine/export.h"

namespace OLIA_ENGINE
{
    class World
    {
    public:
        EntityAllocator entities;

    private:
        // Type-erased base class for component pools
        struct IPool
        {
            virtual ~IPool() = default;
        };

        template <typename T>
        struct PoolWrapper : public IPool
        {
            ComponentPool<T> pool;
        };

        // Equivalent to Rust's UnsafeCell HashMap lookup
        mutable std::unordered_map<std::type_index, std::unique_ptr<IPool>> pools_;

    public:
        World() = default;

        Entity spawn()
        {
            Entity ent = entities.spawn();
            return ent;
        }

        template <typename T>
        T *get_mut(Entity entity)
        {
            return pool_mut<T>().get_mut(entity);
        }

        template <typename T>
        const T *get(Entity entity) const
        {
            const auto *pool = this->pool<T>();

            if (!pool)
                return nullptr;

            return pool->get(entity);
        }

        template <typename T>
        ComponentPool<T> &pool_mut() const
        {
            std::type_index type_id = typeid(T);
            auto it = pools_.find(type_id);
            if (it == pools_.end())
            {
                auto wrapper = std::make_unique<PoolWrapper<T>>();
                ComponentPool<T> &pool_ref = wrapper->pool;
                pools_[type_id] = std::move(wrapper);
                return pool_ref;
            }
            return static_cast<PoolWrapper<T> *>(it->second.get())->pool;
        }

        template <typename T>
        const ComponentPool<T> *pool() const
        {
            std::type_index type_id = typeid(T);
            auto it = pools_.find(type_id);
            if (it == pools_.end())
            {
                return nullptr;
            }
            return &static_cast<const PoolWrapper<T> *>(it->second.get())->pool;
        }

        template <typename T>
        void add_component(Entity entity, T component) const
        {
            pool_mut<T>().insert(entity, std::move(component));
        }

        template <typename T>
        std::optional<T> remove_component(Entity entity) const
        {
            return pool_mut<T>().remove(entity);
        }

        bool despawn(Entity entity)
        {
            return entities.despawn(entity);
        }
    };
}

extern "C" OLIA_API OLIA_ENGINE::World *world;