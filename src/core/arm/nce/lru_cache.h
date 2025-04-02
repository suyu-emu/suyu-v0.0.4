#pragma once

#include <list>
#include <unordered_map>

template<typename KeyType, typename ValueType>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity(capacity) {}

    ValueType* get(const KeyType& key) {
        auto it = cache_map.find(key);
        if (it == cache_map.end()) {
            return nullptr;
        }

        // Move the accessed item to the front of the list (most recently used)
        cache_list.splice(cache_list.begin(), cache_list, it->second.first);
        return &(it->second.second);
    }

    void put(const KeyType& key, const ValueType& value) {
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

    void clear() {
        cache_map.clear();
        cache_list.clear();
    }

    size_t size() const {
        return cache_map.size();
    }

private:
    size_t capacity;
    std::list<KeyType> cache_list;
    std::unordered_map<KeyType, std::pair<typename std::list<KeyType>::iterator, ValueType>> cache_map;
};