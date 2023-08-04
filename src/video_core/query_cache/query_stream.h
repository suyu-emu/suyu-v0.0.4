// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <deque>
#include <optional>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/query_cache/bank_base.h"
#include "video_core/query_cache/query_base.h"

namespace VideoCommon {

class StreamerInterface {
public:
    StreamerInterface(size_t id_, u64 dependance_mask_ = 0) : id{id_}, dependance_mask{dependance_mask_} {}
    virtual ~StreamerInterface() = default;

    virtual QueryBase* GetQuery(size_t id) = 0;

    virtual void StartCounter() {
        /* Do Nothing */
    }

    virtual void PauseCounter() {
        /* Do Nothing */
    }

    virtual void ResetCounter() {
        /* Do Nothing */
    }

    virtual void CloseCounter() {
        /* Do Nothing */
    }

    virtual bool HasPendingSync() {
        return false;
    }

    virtual void PresyncWrites() {
        /* Do Nothing */
    }

    virtual void SyncWrites() {
        /* Do Nothing */
    }

    virtual size_t WriteCounter(VAddr address, bool has_timestamp, u32 value,
                                std::optional<u32> subreport = std::nullopt) = 0;

    virtual bool HasUnsyncedQueries() {
        return false;
    }

    virtual void PushUnsyncedQueries() {
        /* Do Nothing */
    }

    virtual void PopUnsyncedQueries() {
        /* Do Nothing */
    }

    virtual void Free(size_t query_id) = 0;

    size_t GetId() const {
        return id;
    }

    u64 GetDependenceMask() const {
        return dependance_mask;
    }

protected:
    const size_t id;
    const u64 dependance_mask;
};

template <typename QueryType>
class SimpleStreamer : public StreamerInterface {
public:
    SimpleStreamer(size_t id_, u64 dependance_mask_ = 0) : StreamerInterface{id_, dependance_mask_} {}
    virtual ~SimpleStreamer() = default;

protected:
    virtual QueryType* GetQuery(size_t query_id) override {
        if (query_id < slot_queries.size()) {
            return &slot_queries[query_id];
        }
        return nullptr;
    }

    virtual void Free(size_t query_id) override {
        std::scoped_lock lk(guard);
        ReleaseQuery(query_id);
    }

    template <typename... Args, typename = decltype(QueryType(std::declval<Args>()...))>
    size_t BuildQuery(Args&&... args) {
        std::scoped_lock lk(guard);
        if (!old_queries.empty()) {
            size_t new_id = old_queries.front();
            old_queries.pop_front();
            new (&slot_queries[new_id]) QueryType(std::forward<Args>(args)...);
            return new_id;
        }
        size_t new_id = slot_queries.size();
        slot_queries.emplace_back(std::forward<Args>(args)...);
        return new_id;
    }

    void ReleaseQuery(size_t query_id) {

        if (query_id < slot_queries.size()) {
            old_queries.push_back(query_id);
            return;
        }
        UNREACHABLE();
    }

    std::mutex guard;
    std::deque<QueryType> slot_queries;
    std::deque<size_t> old_queries;
};

} // namespace VideoCommon