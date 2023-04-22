// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

#include "common/alignment.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "core/memory.h"

namespace VideoCommon {

constexpr u64 PAGES_PER_WORD = 64;
constexpr u64 BYTES_PER_PAGE = Core::Memory::YUZU_PAGESIZE;
constexpr u64 BYTES_PER_WORD = PAGES_PER_WORD * BYTES_PER_PAGE;

/// Vector tracking modified pages tightly packed with small vector optimization
template <size_t stack_words = 1>
union WordsArray {
    /// Returns the pointer to the words state
    [[nodiscard]] const u64* Pointer(bool is_short) const noexcept {
        return is_short ? stack.data() : heap;
    }

    /// Returns the pointer to the words state
    [[nodiscard]] u64* Pointer(bool is_short) noexcept {
        return is_short ? stack.data() : heap;
    }

    std::array<u64, stack_words> stack{}; ///< Small buffers storage
    u64* heap;                            ///< Not-small buffers pointer to the storage
};

template <size_t stack_words = 1>
struct Words {
    explicit Words() = default;
    explicit Words(u64 size_bytes_) : size_bytes{size_bytes_} {
        if (IsShort()) {
            cpu.stack.fill(~u64{0});
            gpu.stack.fill(0);
            cached_cpu.stack.fill(0);
            untracked.stack.fill(~u64{0});
        } else {
            const size_t num_words = NumWords();
            // Share allocation between CPU and GPU pages and set their default values
            u64* const alloc = new u64[num_words * 4];
            cpu.heap = alloc;
            gpu.heap = alloc + num_words;
            cached_cpu.heap = alloc + num_words * 2;
            untracked.heap = alloc + num_words * 3;
            std::fill_n(cpu.heap, num_words, ~u64{0});
            std::fill_n(gpu.heap, num_words, 0);
            std::fill_n(cached_cpu.heap, num_words, 0);
            std::fill_n(untracked.heap, num_words, ~u64{0});
        }
        // Clean up tailing bits
        const u64 last_word_size = size_bytes % BYTES_PER_WORD;
        const u64 last_local_page = Common::DivCeil(last_word_size, BYTES_PER_PAGE);
        const u64 shift = (PAGES_PER_WORD - last_local_page) % PAGES_PER_WORD;
        const u64 last_word = (~u64{0} << shift) >> shift;
        cpu.Pointer(IsShort())[NumWords() - 1] = last_word;
        untracked.Pointer(IsShort())[NumWords() - 1] = last_word;
    }

    ~Words() {
        Release();
    }

    Words& operator=(Words&& rhs) noexcept {
        Release();
        size_bytes = rhs.size_bytes;
        cpu = rhs.cpu;
        gpu = rhs.gpu;
        cached_cpu = rhs.cached_cpu;
        untracked = rhs.untracked;
        rhs.cpu.heap = nullptr;
        return *this;
    }

    Words(Words&& rhs) noexcept
        : size_bytes{rhs.size_bytes}, cpu{rhs.cpu}, gpu{rhs.gpu},
          cached_cpu{rhs.cached_cpu}, untracked{rhs.untracked} {
        rhs.cpu.heap = nullptr;
    }

    Words& operator=(const Words&) = delete;
    Words(const Words&) = delete;

    /// Returns true when the buffer fits in the small vector optimization
    [[nodiscard]] bool IsShort() const noexcept {
        return size_bytes <= stack_words * BYTES_PER_WORD;
    }

    /// Returns the number of words of the buffer
    [[nodiscard]] size_t NumWords() const noexcept {
        return Common::DivCeil(size_bytes, BYTES_PER_WORD);
    }

    /// Release buffer resources
    void Release() {
        if (!IsShort()) {
            // CPU written words is the base for the heap allocation
            delete[] cpu.heap;
        }
    }

    u64 size_bytes = 0;
    WordsArray<stack_words> cpu;
    WordsArray<stack_words> gpu;
    WordsArray<stack_words> cached_cpu;
    WordsArray<stack_words> untracked;
};

enum class Type {
    CPU,
    GPU,
    CachedCPU,
    Untracked,
};

template <class RasterizerInterface, size_t stack_words = 1>
class WordManager {
public:
    explicit WordManager(VAddr cpu_addr_, RasterizerInterface& rasterizer_, u64 size_bytes)
        : cpu_addr{cpu_addr_}, rasterizer{&rasterizer_}, words{size_bytes} {}

    explicit WordManager() = default;

    void SetCpuAddress(VAddr new_cpu_addr) {
        cpu_addr = new_cpu_addr;
    }

    VAddr GetCpuAddr() const {
        return cpu_addr;
    }

    /**
     * Change the state of a range of pages
     *
     * @param dirty_addr    Base address to mark or unmark as modified
     * @param size          Size in bytes to mark or unmark as modified
     */
    template <Type type, bool enable>
    void ChangeRegionState(u64 dirty_addr, s64 size) noexcept(type == Type::GPU) {
        const s64 difference = dirty_addr - cpu_addr;
        const u64 offset = std::max<s64>(difference, 0);
        size += std::min<s64>(difference, 0);
        if (offset >= SizeBytes() || size < 0) {
            return;
        }
        u64* const untracked_words = Array<Type::Untracked>();
        u64* const state_words = Array<type>();
        const u64 offset_end = std::min(offset + size, SizeBytes());
        const u64 begin_page_index = offset / BYTES_PER_PAGE;
        const u64 begin_word_index = begin_page_index / PAGES_PER_WORD;
        const u64 end_page_index = Common::DivCeil(offset_end, BYTES_PER_PAGE);
        const u64 end_word_index = Common::DivCeil(end_page_index, PAGES_PER_WORD);
        u64 page_index = begin_page_index % PAGES_PER_WORD;
        u64 word_index = begin_word_index;
        while (word_index < end_word_index) {
            const u64 next_word_first_page = (word_index + 1) * PAGES_PER_WORD;
            const u64 left_offset =
                std::min(next_word_first_page - end_page_index, PAGES_PER_WORD) % PAGES_PER_WORD;
            const u64 right_offset = page_index;
            u64 bits = ~u64{0};
            bits = (bits >> right_offset) << right_offset;
            bits = (bits << left_offset) >> left_offset;
            if constexpr (type == Type::CPU || type == Type::CachedCPU) {
                NotifyRasterizer<!enable>(word_index, untracked_words[word_index], bits);
            }
            if constexpr (enable) {
                state_words[word_index] |= bits;
                if constexpr (type == Type::CPU || type == Type::CachedCPU) {
                    untracked_words[word_index] |= bits;
                }
            } else {
                state_words[word_index] &= ~bits;
                if constexpr (type == Type::CPU || type == Type::CachedCPU) {
                    untracked_words[word_index] &= ~bits;
                }
            }
            page_index = 0;
            ++word_index;
        }
    }

    /**
     * Loop over each page in the given range, turn off those bits and notify the rasterizer if
     * needed. Call the given function on each turned off range.
     *
     * @param query_cpu_range Base CPU address to loop over
     * @param size            Size in bytes of the CPU range to loop over
     * @param func            Function to call for each turned off region
     */
    template <Type type, typename Func>
    void ForEachModifiedRange(VAddr query_cpu_range, s64 size, bool clear, Func&& func) {
        static_assert(type != Type::Untracked);

        const s64 difference = query_cpu_range - cpu_addr;
        const u64 query_begin = std::max<s64>(difference, 0);
        size += std::min<s64>(difference, 0);
        if (query_begin >= SizeBytes() || size < 0) {
            return;
        }
        [[maybe_unused]] u64* const untracked_words = Array<Type::Untracked>();
        [[maybe_unused]] u64* const cpu_words = Array<Type::CPU>();
        u64* const state_words = Array<type>();
        const u64 query_end = query_begin + std::min(static_cast<u64>(size), SizeBytes());
        u64* const words_begin = state_words + query_begin / BYTES_PER_WORD;
        u64* const words_end = state_words + Common::DivCeil(query_end, BYTES_PER_WORD);
        u64 first_page = (query_begin / BYTES_PER_PAGE) % PAGES_PER_WORD;

        const auto modified = [](u64 word) { return word != 0; };
        const auto first_modified_word = std::find_if(words_begin, words_end, modified);
        if (first_modified_word == words_end) {
            // Exit early when the buffer is not modified
            return;
        }
        if (first_modified_word != words_begin) {
            first_page = 0;
        }
        std::reverse_iterator<u64*> first_word_reverse(first_modified_word);
        std::reverse_iterator<u64*> last_word_iterator(words_end);
        auto last_word_result = std::find_if(last_word_iterator, first_word_reverse, modified);
        u64* const last_modified_word = &(*last_word_result) + 1;

        const u64 word_index_begin = std::distance(state_words, first_modified_word);
        const u64 word_index_end = std::distance(state_words, last_modified_word);
        const unsigned local_page_begin = std::countr_zero(*first_modified_word);
        const unsigned local_page_end =
            static_cast<unsigned>(PAGES_PER_WORD) - std::countl_zero(last_modified_word[-1]);
        const u64 word_page_begin = word_index_begin * PAGES_PER_WORD;
        const u64 word_page_end = (word_index_end - 1) * PAGES_PER_WORD;
        const u64 query_page_begin = query_begin / BYTES_PER_PAGE;
        const u64 query_page_end = Common::DivCeil(query_end, BYTES_PER_PAGE);
        const u64 page_index_begin = std::max(word_page_begin + local_page_begin, query_page_begin);
        const u64 page_index_end = std::min(word_page_end + local_page_end, query_page_end);
        const u64 first_word_page_begin = page_index_begin % PAGES_PER_WORD;
        const u64 last_word_page_end = (page_index_end - 1) % PAGES_PER_WORD + 1;

        u64 page_begin = std::max(first_word_page_begin, first_page);
        u64 current_base = 0;
        u64 current_size = 0;
        bool on_going = false;
        for (u64 word_index = word_index_begin; word_index < word_index_end; ++word_index) {
            const bool is_last_word = word_index + 1 == word_index_end;
            const u64 page_end = is_last_word ? last_word_page_end : PAGES_PER_WORD;
            const u64 right_offset = page_begin;
            const u64 left_offset = PAGES_PER_WORD - page_end;
            u64 bits = ~u64{0};
            bits = (bits >> right_offset) << right_offset;
            bits = (bits << left_offset) >> left_offset;

            const u64 current_word = state_words[word_index] & bits;
            if (clear) {
                state_words[word_index] &= ~bits;
            }

            if constexpr (type == Type::CachedCPU) {
                NotifyRasterizer<false>(word_index, untracked_words[word_index], current_word);
                untracked_words[word_index] |= current_word;
                cpu_words[word_index] |= current_word;
            }

            if constexpr (type == Type::CPU) {
                const u64 current_bits = untracked_words[word_index] & bits;
                untracked_words[word_index] &= ~bits;
                NotifyRasterizer<true>(word_index, current_bits, ~u64{0});
            }
            const u64 word = current_word & ~(type == Type::GPU ? untracked_words[word_index] : 0);
            u64 page = page_begin;
            page_begin = 0;

            while (page < page_end) {
                const int empty_bits = std::countr_zero(word >> page);
                if (on_going && empty_bits != 0) {
                    InvokeModifiedRange(func, current_size, current_base);
                    current_size = 0;
                    on_going = false;
                }
                if (empty_bits == PAGES_PER_WORD) {
                    break;
                }
                page += empty_bits;

                const int continuous_bits = std::countr_one(word >> page);
                if (!on_going && continuous_bits != 0) {
                    current_base = word_index * PAGES_PER_WORD + page;
                    on_going = true;
                }
                current_size += continuous_bits;
                page += continuous_bits;
            }
        }
        if (on_going && current_size > 0) {
            InvokeModifiedRange(func, current_size, current_base);
        }
    }

    template <typename Func>
    void InvokeModifiedRange(Func&& func, u64 current_size, u64 current_base) {
        const u64 current_size_bytes = current_size * BYTES_PER_PAGE;
        const u64 offset_begin = current_base * BYTES_PER_PAGE;
        const u64 offset_end = std::min(offset_begin + current_size_bytes, SizeBytes());
        func(cpu_addr + offset_begin, offset_end - offset_begin);
    }

    /**
     * Returns true when a region has been modified
     *
     * @param offset Offset in bytes from the start of the buffer
     * @param size   Size in bytes of the region to query for modifications
     */
    template <Type type>
    [[nodiscard]] bool IsRegionModified(u64 offset, u64 size) const noexcept {
        static_assert(type != Type::Untracked);

        const u64* const untracked_words = Array<Type::Untracked>();
        const u64* const state_words = Array<type>();
        const u64 num_query_words = size / BYTES_PER_WORD + 1;
        const u64 word_begin = offset / BYTES_PER_WORD;
        const u64 word_end = std::min(word_begin + num_query_words, NumWords());
        const u64 page_limit = Common::DivCeil(offset + size, BYTES_PER_PAGE);
        u64 page_index = (offset / BYTES_PER_PAGE) % PAGES_PER_WORD;
        for (u64 word_index = word_begin; word_index < word_end; ++word_index, page_index = 0) {
            const u64 off_word = type == Type::GPU ? untracked_words[word_index] : 0;
            const u64 word = state_words[word_index] & ~off_word;
            if (word == 0) {
                continue;
            }
            const u64 page_end = std::min((word_index + 1) * PAGES_PER_WORD, page_limit);
            const u64 local_page_end = page_end % PAGES_PER_WORD;
            const u64 page_end_shift = (PAGES_PER_WORD - local_page_end) % PAGES_PER_WORD;
            if (((word >> page_index) << page_index) << page_end_shift != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * Returns a begin end pair with the inclusive modified region
     *
     * @param offset Offset in bytes from the start of the buffer
     * @param size   Size in bytes of the region to query for modifications
     */
    template <Type type>
    [[nodiscard]] std::pair<u64, u64> ModifiedRegion(u64 offset, u64 size) const noexcept {
        static_assert(type != Type::Untracked);
        const u64* const state_words = Array<type>();
        const u64 num_query_words = size / BYTES_PER_WORD + 1;
        const u64 word_begin = offset / BYTES_PER_WORD;
        const u64 word_end = std::min(word_begin + num_query_words, NumWords());
        const u64 page_base = offset / BYTES_PER_PAGE;
        u64 page_begin = page_base & (PAGES_PER_WORD - 1);
        u64 page_end =
            Common::DivCeil(offset + size, BYTES_PER_PAGE) - (page_base & ~(PAGES_PER_WORD - 1));
        u64 begin = std::numeric_limits<u64>::max();
        u64 end = 0;
        for (u64 word_index = word_begin; word_index < word_end; ++word_index) {
            const u64 base_mask = (1ULL << page_begin) - 1ULL;
            const u64 end_mask = page_end >= PAGES_PER_WORD ? 0ULL : ~((1ULL << page_end) - 1ULL);
            const u64 off_word = end_mask | base_mask;
            const u64 word = state_words[word_index] & ~off_word;
            if (word == 0) {
                page_begin = 0;
                page_end -= PAGES_PER_WORD;
                continue;
            }
            const u64 local_page_begin = std::countr_zero(word);
            const u64 local_page_end = PAGES_PER_WORD - std::countl_zero(word);
            const u64 page_index = word_index * PAGES_PER_WORD;
            begin = std::min(begin, page_index + local_page_begin);
            end = page_index + local_page_end;
            page_begin = 0;
            page_end -= PAGES_PER_WORD;
        }
        static constexpr std::pair<u64, u64> EMPTY{0, 0};
        return begin < end ? std::make_pair(begin * BYTES_PER_PAGE, end * BYTES_PER_PAGE) : EMPTY;
    }

    /// Returns the number of words of the manager
    [[nodiscard]] size_t NumWords() const noexcept {
        return words.NumWords();
    }

    /// Returns the size in bytes of the manager
    [[nodiscard]] u64 SizeBytes() const noexcept {
        return words.size_bytes;
    }

    /// Returns true when the buffer fits in the small vector optimization
    [[nodiscard]] bool IsShort() const noexcept {
        return words.IsShort();
    }

    void FlushCachedWrites() noexcept {
        const u64 num_words = NumWords();
        u64* const cached_words = Array<Type::CachedCPU>();
        u64* const untracked_words = Array<Type::Untracked>();
        u64* const cpu_words = Array<Type::CPU>();
        for (u64 word_index = 0; word_index < num_words; ++word_index) {
            const u64 cached_bits = cached_words[word_index];
            NotifyRasterizer<false>(word_index, untracked_words[word_index], cached_bits);
            untracked_words[word_index] |= cached_bits;
            cpu_words[word_index] |= cached_bits;
            cached_words[word_index] = 0;
        }
    }

private:
    template <Type type>
    u64* Array() noexcept {
        if constexpr (type == Type::CPU) {
            return words.cpu.Pointer(IsShort());
        } else if constexpr (type == Type::GPU) {
            return words.gpu.Pointer(IsShort());
        } else if constexpr (type == Type::CachedCPU) {
            return words.cached_cpu.Pointer(IsShort());
        } else if constexpr (type == Type::Untracked) {
            return words.untracked.Pointer(IsShort());
        }
    }

    template <Type type>
    const u64* Array() const noexcept {
        if constexpr (type == Type::CPU) {
            return words.cpu.Pointer(IsShort());
        } else if constexpr (type == Type::GPU) {
            return words.gpu.Pointer(IsShort());
        } else if constexpr (type == Type::CachedCPU) {
            return words.cached_cpu.Pointer(IsShort());
        } else if constexpr (type == Type::Untracked) {
            return words.untracked.Pointer(IsShort());
        }
    }

    /**
     * Notify rasterizer about changes in the CPU tracking state of a word in the buffer
     *
     * @param word_index   Index to the word to notify to the rasterizer
     * @param current_bits Current state of the word
     * @param new_bits     New state of the word
     *
     * @tparam add_to_rasterizer True when the rasterizer should start tracking the new pages
     */
    template <bool add_to_rasterizer>
    void NotifyRasterizer(u64 word_index, u64 current_bits, u64 new_bits) const {
        u64 changed_bits = (add_to_rasterizer ? current_bits : ~current_bits) & new_bits;
        VAddr addr = cpu_addr + word_index * BYTES_PER_WORD;
        while (changed_bits != 0) {
            const int empty_bits = std::countr_zero(changed_bits);
            addr += empty_bits * BYTES_PER_PAGE;
            changed_bits >>= empty_bits;

            const u32 continuous_bits = std::countr_one(changed_bits);
            const u64 size = continuous_bits * BYTES_PER_PAGE;
            const VAddr begin_addr = addr;
            addr += size;
            changed_bits = continuous_bits < PAGES_PER_WORD ? (changed_bits >> continuous_bits) : 0;
            rasterizer->UpdatePagesCachedCount(begin_addr, size, add_to_rasterizer ? 1 : -1);
        }
    }

    VAddr cpu_addr = 0;
    RasterizerInterface* rasterizer = nullptr;
    Words<stack_words> words;
};

} // namespace VideoCommon
