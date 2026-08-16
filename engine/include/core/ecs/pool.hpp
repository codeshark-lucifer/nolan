#pragma once

#include "./entity.hpp"
#include <vector>
#include <optional>
#include <cstddef>
#include <utility>

template <typename T>
class ComponentPool {
private:
    std::vector<Entity> dense_;
    std::vector<T> data_;
    std::vector<std::optional<size_t>> sparse_;

public:
    ComponentPool() = default;

    // Read-only accessors
    const std::vector<Entity>& dense() const { return dense_; }
    const std::vector<T>& data() const { return data_; }
    std::vector<T>& data() { return data_; }

    void insert(Entity entity, T component) {
        size_t idx = static_cast<size_t>(entity.index);
        if (idx >= sparse_.size()) {
            sparse_.resize(idx + 1, std::nullopt);
        }

        if (sparse_[idx].has_value()) {
            size_t dense_idx = sparse_[idx].value();
            data_[dense_idx] = std::move(component);
        } else {
            size_t dense_idx = data_.size();
            sparse_[idx] = dense_idx;
            dense_.push_back(entity);
            data_.push_back(std::move(component));
        }
    }

    std::optional<T> remove(Entity entity) {
        size_t idx = static_cast<size_t>(entity.index);
        if (idx >= sparse_.size() || !sparse_[idx].has_value()) {
            return std::nullopt;
        }

        size_t dense_idx = sparse_[idx].value();
        size_t last_dense_idx = data_.size() - 1;

        // Move component out for returning
        T removed_component = std::move(data_[dense_idx]);

        // Swap-remove with last element for O(1) removal
        if (dense_idx != last_dense_idx) {
            data_[dense_idx] = std::move(data_[last_dense_idx]);
            dense_[dense_idx] = dense_[last_dense_idx];

            // Update sparse index of swapped entity
            Entity swapped_entity = dense_[dense_idx];
            sparse_[swapped_entity.index] = dense_idx;
        }

        // Clear sparse entry for removed entity
        sparse_[idx] = std::nullopt;

        dense_.pop_back();
        data_.pop_back();

        return removed_component;
    }

    const T* get(Entity entity) const {
        size_t idx = static_cast<size_t>(entity.index);
        if (idx < sparse_.size() && sparse_[idx].has_value()) {
            return &data_[sparse_[idx].value()];
        }
        return nullptr;
    }

    T* get_mut(Entity entity) {
        size_t idx = static_cast<size_t>(entity.index);
        if (idx < sparse_.size() && sparse_[idx].has_value()) {
            return &data_[sparse_[idx].value()];
        }
        return nullptr;
    }

    // Iteration via callback helper (idiomatic in C++)
    template <typename Func>
    void for_each(Func&& func) const {
        for (size_t i = 0; i < dense_.size(); ++i) {
            func(dense_[i], data_[i]);
        }
    }

    template <typename Func>
    void for_each_mut(Func&& func) {
        for (size_t i = 0; i < dense_.size(); ++i) {
            func(dense_[i], data_[i]);
        }
    }

    size_t len() const { return data_.size(); }
    bool is_empty() const { return data_.empty(); }
};