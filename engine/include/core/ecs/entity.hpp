#pragma once

#include <cstdint>
#include <vector>
#include <iostream>
#include <functional>

struct Entity {
    uint32_t index{0};
    uint32_t generation{0};

    bool operator==(const Entity& other) const {
        return index == other.index && generation == other.generation;
    }

    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }
};

// std::hash specialization for Entity so it can be used in std::unordered_map / std::unordered_set
namespace std {
template <>
struct hash<Entity> {
    size_t operator()(const Entity& e) const noexcept {
        return std::hash<uint32_t>{}(e.index) ^ (std::hash<uint32_t>{}(e.generation) << 1);
    }
};
}

// Implements Display for Entity (std::cout << entity)
inline std::ostream& operator<<(std::ostream& os, const Entity& e) {
    return os << "Entity(idx: " << e.index << ", gen: " << e.generation << ")";
}

class EntityAllocator {
private:
    std::vector<uint32_t> generations;
    std::vector<uint32_t> free_list;

public:
    EntityAllocator() = default;

    Entity spawn() {
        if (!free_list.empty()) {
            uint32_t index = free_list.back();
            free_list.pop_back();
            return Entity{index, generations[index]};
        } else {
            uint32_t index = static_cast<uint32_t>(generations.size());
            generations.push_back(0);
            return Entity{index, 0};
        }
    }

    bool despawn(Entity entity) {
        if (!is_alive(entity)) {
            return false;
        }
        generations[entity.index] += 1;
        free_list.push_back(entity.index);
        return true;
    }

    bool is_alive(Entity entity) const {
        if (entity.index < generations.size()) {
            return generations[entity.index] == entity.generation;
        }
        return false;
    }
};