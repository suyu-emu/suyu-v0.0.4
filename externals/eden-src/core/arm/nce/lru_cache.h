// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <list>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include "common/logging/log.h"

template <typename KeyType, typename ValueType>
class LRUCache {
public:
    using key_type   = KeyType;
    using value_type = ValueType;
    using size_type  = std::size_t;

    struct Statistics {
        size_type hits   = 0;
        size_type misses = 0;
        void reset() noexcept { hits = misses = 0; }
    };

    explicit LRUCache(size_type capacity, bool enabled = true)
        : enabled_{enabled}, capacity_{capacity} {
        cache_map_.reserve(capacity_);
        LOG_WARNING(Core, "LRU Cache initialised (state: {} | capacity: {})", enabled_ ? "enabled" : "disabled", capacity_);
    }

    // Non-movable copy semantics
    LRUCache(const LRUCache&)            = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    LRUCache(LRUCache&& other) noexcept { *this = std::move(other); }
    LRUCache& operator=(LRUCache&& other) noexcept {
        if (this == &other) return *this;
        std::unique_lock this_lock(mutex_, std::defer_lock);
        std::unique_lock other_lock(other.mutex_, std::defer_lock);
        std::lock(this_lock, other_lock);
        enabled_ = other.enabled_;
        capacity_ = other.capacity_;
        cache_list_ = std::move(other.cache_list_);
        cache_map_ = std::move(other.cache_map_);
        stats_ = other.stats_;
        return *this;
    }
    ~LRUCache() = default;

    [[nodiscard]] value_type* get(const key_type& key) {
        if (!enabled_) [[unlikely]] return nullptr;
        std::unique_lock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            ++stats_.misses;
            return nullptr;
        }
        move_to_front(it);
        ++stats_.hits;
        return &it->second.second;
    }

    [[nodiscard]] value_type* peek(const key_type& key) const {
        if (!enabled_) [[unlikely]] return nullptr;
        std::shared_lock lock(mutex_);
        auto it = cache_map_.find(key);
        return it == cache_map_.end() ? nullptr : &it->second.second;
    }

    template <typename V>
    void put(const key_type& key, V&& value) {
        if (!enabled_) [[unlikely]] return;
        std::unique_lock lock(mutex_);
        insert_or_update(key, std::forward<V>(value));
    }

    template <typename ValueFactory>
    value_type& get_or_emplace(const key_type& key, ValueFactory&& factory) {
        std::unique_lock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            move_to_front(it);
            return it->second.second;
        }
        value_type new_value = factory();
        insert_or_update(key, std::move(new_value));
        return cache_map_.find(key)->second.second;
    }

    [[nodiscard]] bool contains(const key_type& key) const {
        if (!enabled_) return false;
        std::shared_lock lock(mutex_);
        return cache_map_.find(key) != cache_map_.end();
    }

    bool erase(const key_type& key) {
        if (!enabled_) return false;
        std::unique_lock lock(mutex_);
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) return false;
        cache_list_.erase(it->second.first);
        cache_map_.erase(it);
        return true;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_list_.clear();
        cache_map_.clear();
        stats_.reset();
    }

    [[nodiscard]] size_type size() const {
        if (!enabled_) return 0;
        std::shared_lock lock(mutex_);
        return cache_map_.size();
    }

    [[nodiscard]] size_type get_capacity() const { return capacity_; }

    void resize(size_type new_capacity) {
        if (!enabled_) return;
        std::unique_lock lock(mutex_);
        capacity_ = new_capacity;
        shrink_if_needed();
        cache_map_.reserve(capacity_);
    }

    void setEnabled(bool state) {
        std::unique_lock lock(mutex_);
        enabled_ = state;
        LOG_WARNING(Core, "LRU Cache state changed to: {}", state ? "enabled" : "disabled");
        if (!enabled_) clear();
    }

    [[nodiscard]] bool isEnabled() const { return enabled_; }

    [[nodiscard]] Statistics stats() const {
        std::shared_lock lock(mutex_);
        return stats_;
    }

private:
    using list_type      = std::list<key_type>;
    using list_iterator  = typename list_type::iterator;
    using map_value_type = std::pair<list_iterator, value_type>;
    using map_type       = std::unordered_map<key_type, map_value_type>;

    template <typename V>
    void insert_or_update(const key_type& key, V&& value) {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            it->second.second = std::forward<V>(value);
            move_to_front(it);
            return;
        }
        // evict LRU if full
        if (cache_map_.size() >= capacity_) {
            const auto& lru_key = cache_list_.back();
            cache_map_.erase(lru_key);
            cache_list_.pop_back();
        }
        cache_list_.push_front(key);
        cache_map_[key] = {cache_list_.begin(), std::forward<V>(value)};
    }

    void move_to_front(typename map_type::iterator it) {
        cache_list_.splice(cache_list_.begin(), cache_list_, it->second.first);
        it->second.first = cache_list_.begin();
    }

    void shrink_if_needed() {
        while (cache_map_.size() > capacity_) {
            const auto& lru_key = cache_list_.back();
            cache_map_.erase(lru_key);
            cache_list_.pop_back();
        }
    }

private:
    mutable std::shared_mutex mutex_;
    bool enabled_{true};
    size_type capacity_;
    list_type cache_list_;
    map_type cache_map_;
    mutable Statistics stats_;
};
