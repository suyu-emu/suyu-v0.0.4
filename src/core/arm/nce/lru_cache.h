#pragma once

#include <list>
#include <unordered_map>
#include <optional>
#include "common/logging/log.h"

template<typename KeyType, typename ValueType>
class LRUCache {
private:
    bool enabled = true;
    size_t capacity;
    std::list<KeyType> cache_list;
    std::unordered_map<KeyType, std::pair<typename std::list<KeyType>::iterator, ValueType>> cache_map;

public:
    explicit LRUCache(size_t capacity, bool enabled = true) : enabled(enabled), capacity(capacity) {
        cache_map.reserve(capacity);
        LOG_WARNING(Core, "LRU Cache initialized with state: {}", enabled ? "enabled" : "disabled");
    }

    // Returns pointer to value if found, nullptr otherwise
    ValueType* get(const KeyType& key) {
        if (!enabled) return nullptr;

        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return nullptr;
        }

        // Move the accessed item to the front of the list (most recently used)
        cache_list.splice(cache_list.begin(), cache_list, it->second.first);
        return &(it->second.second);
    }

    // Returns pointer to value if found (without promoting it), nullptr otherwise
    ValueType* peek(const KeyType& key) const {
        if (!enabled) return nullptr;

        auto it = cache_map.find(key);
        return it != cache_map.end() ? &(it->second.second) : nullptr;
    }

    // Inserts or updates a key-value pair
    void put(const KeyType& key, const ValueType& value) {
        if (!enabled) return;

        auto it = cache_map.find(key);

        if (it != cache_map.end()) {
            // Key exists, update value and move to front
            it->second.second = value;
            cache_list.splice(cache_list.begin(), cache_list, it->second.first);
            return;
        }

        // Remove the least recently used item if cache is full
        if (cache_map.size() >= capacity) {
            auto last = cache_list.back();
            cache_map.erase(last);
            cache_list.pop_back();
        }

        // Insert new item at the front
        cache_list.push_front(key);
        cache_map[key] = {cache_list.begin(), value};
    }

    // Enable or disable the LRU cache
    void setEnabled(bool state) {
        enabled = state;
        LOG_WARNING(Core, "LRU Cache state changed to: {}", state ? "enabled" : "disabled");
        if (!enabled) {
            clear();
        }
    }

    // Check if the cache is enabled
    bool isEnabled() const {
        return enabled;
    }

    // Attempts to get value, returns std::nullopt if not found
    std::optional<ValueType> try_get(const KeyType& key) {
        auto* val = get(key);
        return val ? std::optional<ValueType>(*val) : std::nullopt;
    }

    // Checks if key exists in cache
    bool contains(const KeyType& key) const {
        if (!enabled) return false;
        return cache_map.find(key) != cache_map.end();
    }

    // Removes a key from the cache if it exists
    bool erase(const KeyType& key) {
        if (!enabled) return false;

        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return false;
        }
        cache_list.erase(it->second.first);
        cache_map.erase(it);
        return true;
    }

    // Removes all elements from the cache
    void clear() {
        cache_map.clear();
        cache_list.clear();
    }

    // Returns current number of elements in cache
    size_t size() const {
        return enabled ? cache_map.size() : 0;
    }

    // Returns maximum capacity of cache
    size_t get_capacity() const {
        return capacity;
    }

    // Resizes the cache, evicting LRU items if new capacity is smaller
    void resize(size_t new_capacity) {
        if (!enabled) return;

        capacity = new_capacity;
        while (cache_map.size() > capacity) {
            auto last = cache_list.back();
            cache_map.erase(last);
            cache_list.pop_back();
        }
        cache_map.reserve(capacity);
    }
};