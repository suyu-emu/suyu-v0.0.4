// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#pragma once

#include <array>
#include <bit>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/kernel/memory/memory_types.h"

namespace Kernel::Memory {

class PageHeap final : NonCopyable {
public:
    static constexpr s32 GetAlignedBlockIndex(std::size_t num_pages, std::size_t align_pages) {
        const auto target_pages{std::max(num_pages, align_pages)};
        for (std::size_t i = 0; i < NumMemoryBlockPageShifts; i++) {
            if (target_pages <=
                (static_cast<std::size_t>(1) << MemoryBlockPageShifts[i]) / PageSize) {
                return static_cast<s32>(i);
            }
        }
        return -1;
    }

    static constexpr s32 GetBlockIndex(std::size_t num_pages) {
        for (s32 i{static_cast<s32>(NumMemoryBlockPageShifts) - 1}; i >= 0; i--) {
            if (num_pages >= (static_cast<std::size_t>(1) << MemoryBlockPageShifts[i]) / PageSize) {
                return i;
            }
        }
        return -1;
    }

    static constexpr std::size_t GetBlockSize(std::size_t index) {
        return static_cast<std::size_t>(1) << MemoryBlockPageShifts[index];
    }

    static constexpr std::size_t GetBlockNumPages(std::size_t index) {
        return GetBlockSize(index) / PageSize;
    }

private:
    static constexpr std::size_t NumMemoryBlockPageShifts{7};
    static constexpr std::array<std::size_t, NumMemoryBlockPageShifts> MemoryBlockPageShifts{
        0xC, 0x10, 0x15, 0x16, 0x19, 0x1D, 0x1E,
    };

    class Block final : NonCopyable {
    private:
        class Bitmap final : NonCopyable {
        public:
            static constexpr std::size_t MaxDepth{4};

        private:
            std::array<u64*, MaxDepth> bit_storages{};
            std::size_t num_bits{};
            std::size_t used_depths{};

        public:
            constexpr Bitmap() = default;

            constexpr std::size_t GetNumBits() const {
                return num_bits;
            }
            constexpr s32 GetHighestDepthIndex() const {
                return static_cast<s32>(used_depths) - 1;
            }

            constexpr u64* Initialize(u64* storage, std::size_t size) {
                //* Initially, everything is un-set
                num_bits = 0;

                // Calculate the needed bitmap depth
                used_depths = static_cast<std::size_t>(GetRequiredDepth(size));
                ASSERT(used_depths <= MaxDepth);

                // Set the bitmap pointers
                for (s32 depth{GetHighestDepthIndex()}; depth >= 0; depth--) {
                    bit_storages[depth] = storage;
                    size = Common::AlignUp(size, 64) / 64;
                    storage += size;
                }

                return storage;
            }

            s64 FindFreeBlock() const {
                uintptr_t offset{};
                s32 depth{};

                do {
                    const u64 v{bit_storages[depth][offset]};
                    if (v == 0) {
                        // Non-zero depth indicates that a previous level had a free block
                        ASSERT(depth == 0);
                        return -1;
                    }
                    offset = offset * 64 + static_cast<u32>(std::countr_zero(v));
                    ++depth;
                } while (depth < static_cast<s32>(used_depths));

                return static_cast<s64>(offset);
            }

            constexpr void SetBit(std::size_t offset) {
                SetBit(GetHighestDepthIndex(), offset);
                num_bits++;
            }

            constexpr void ClearBit(std::size_t offset) {
                ClearBit(GetHighestDepthIndex(), offset);
                num_bits--;
            }

            constexpr bool ClearRange(std::size_t offset, std::size_t count) {
                const s32 depth{GetHighestDepthIndex()};
                const auto bit_ind{offset / 64};
                u64* bits{bit_storages[depth]};
                if (count < 64) {
                    const auto shift{offset % 64};
                    ASSERT(shift + count <= 64);
                    // Check that all the bits are set
                    const u64 mask{((1ULL << count) - 1) << shift};
                    u64 v{bits[bit_ind]};
                    if ((v & mask) != mask) {
                        return false;
                    }

                    // Clear the bits
                    v &= ~mask;
                    bits[bit_ind] = v;
                    if (v == 0) {
                        ClearBit(depth - 1, bit_ind);
                    }
                } else {
                    ASSERT(offset % 64 == 0);
                    ASSERT(count % 64 == 0);
                    // Check that all the bits are set
                    std::size_t remaining{count};
                    std::size_t i = 0;
                    do {
                        if (bits[bit_ind + i++] != ~u64(0)) {
                            return false;
                        }
                        remaining -= 64;
                    } while (remaining > 0);

                    // Clear the bits
                    remaining = count;
                    i = 0;
                    do {
                        bits[bit_ind + i] = 0;
                        ClearBit(depth - 1, bit_ind + i);
                        i++;
                        remaining -= 64;
                    } while (remaining > 0);
                }

                num_bits -= count;
                return true;
            }

        private:
            constexpr void SetBit(s32 depth, std::size_t offset) {
                while (depth >= 0) {
                    const auto ind{offset / 64};
                    const auto which{offset % 64};
                    const u64 mask{1ULL << which};

                    u64* bit{std::addressof(bit_storages[depth][ind])};
                    const u64 v{*bit};
                    ASSERT((v & mask) == 0);
                    *bit = v | mask;
                    if (v) {
                        break;
                    }
                    offset = ind;
                    depth--;
                }
            }

            constexpr void ClearBit(s32 depth, std::size_t offset) {
                while (depth >= 0) {
                    const auto ind{offset / 64};
                    const auto which{offset % 64};
                    const u64 mask{1ULL << which};

                    u64* bit{std::addressof(bit_storages[depth][ind])};
                    u64 v{*bit};
                    ASSERT((v & mask) != 0);
                    v &= ~mask;
                    *bit = v;
                    if (v) {
                        break;
                    }
                    offset = ind;
                    depth--;
                }
            }

        private:
            static constexpr s32 GetRequiredDepth(std::size_t region_size) {
                s32 depth = 0;
                while (true) {
                    region_size /= 64;
                    depth++;
                    if (region_size == 0) {
                        return depth;
                    }
                }
            }

        public:
            static constexpr std::size_t CalculateMetadataOverheadSize(std::size_t region_size) {
                std::size_t overhead_bits = 0;
                for (s32 depth{GetRequiredDepth(region_size) - 1}; depth >= 0; depth--) {
                    region_size = Common::AlignUp(region_size, 64) / 64;
                    overhead_bits += region_size;
                }
                return overhead_bits * sizeof(u64);
            }
        };

    private:
        Bitmap bitmap;
        VAddr heap_address{};
        uintptr_t end_offset{};
        std::size_t block_shift{};
        std::size_t next_block_shift{};

    public:
        constexpr Block() = default;

        constexpr std::size_t GetShift() const {
            return block_shift;
        }
        constexpr std::size_t GetNextShift() const {
            return next_block_shift;
        }
        constexpr std::size_t GetSize() const {
            return static_cast<std::size_t>(1) << GetShift();
        }
        constexpr std::size_t GetNumPages() const {
            return GetSize() / PageSize;
        }
        constexpr std::size_t GetNumFreeBlocks() const {
            return bitmap.GetNumBits();
        }
        constexpr std::size_t GetNumFreePages() const {
            return GetNumFreeBlocks() * GetNumPages();
        }

        constexpr u64* Initialize(VAddr addr, std::size_t size, std::size_t bs, std::size_t nbs,
                                  u64* bit_storage) {
            // Set shifts
            block_shift = bs;
            next_block_shift = nbs;

            // Align up the address
            VAddr end{addr + size};
            const auto align{(next_block_shift != 0) ? (1ULL << next_block_shift)
                                                     : (1ULL << block_shift)};
            addr = Common::AlignDown((addr), align);
            end = Common::AlignUp((end), align);

            heap_address = addr;
            end_offset = (end - addr) / (1ULL << block_shift);
            return bitmap.Initialize(bit_storage, end_offset);
        }

        constexpr VAddr PushBlock(VAddr address) {
            // Set the bit for the free block
            std::size_t offset{(address - heap_address) >> GetShift()};
            bitmap.SetBit(offset);

            // If we have a next shift, try to clear the blocks below and return the address
            if (GetNextShift()) {
                const auto diff{1ULL << (GetNextShift() - GetShift())};
                offset = Common::AlignDown(offset, diff);
                if (bitmap.ClearRange(offset, diff)) {
                    return heap_address + (offset << GetShift());
                }
            }

            // We couldn't coalesce, or we're already as big as possible
            return 0;
        }

        VAddr PopBlock() {
            // Find a free block
            const s64 soffset{bitmap.FindFreeBlock()};
            if (soffset < 0) {
                return 0;
            }
            const auto offset{static_cast<std::size_t>(soffset)};

            // Update our tracking and return it
            bitmap.ClearBit(offset);
            return heap_address + (offset << GetShift());
        }

    public:
        static constexpr std::size_t CalculateMetadataOverheadSize(std::size_t region_size,
                                                                   std::size_t cur_block_shift,
                                                                   std::size_t next_block_shift) {
            const auto cur_block_size{(1ULL << cur_block_shift)};
            const auto next_block_size{(1ULL << next_block_shift)};
            const auto align{(next_block_shift != 0) ? next_block_size : cur_block_size};
            return Bitmap::CalculateMetadataOverheadSize(
                (align * 2 + Common::AlignUp(region_size, align)) / cur_block_size);
        }
    };

public:
    PageHeap() = default;

    constexpr VAddr GetAddress() const {
        return heap_address;
    }
    constexpr std::size_t GetSize() const {
        return heap_size;
    }
    constexpr VAddr GetEndAddress() const {
        return GetAddress() + GetSize();
    }
    constexpr std::size_t GetPageOffset(VAddr block) const {
        return (block - GetAddress()) / PageSize;
    }

    void Initialize(VAddr heap_address, std::size_t heap_size, std::size_t metadata_size);
    VAddr AllocateBlock(s32 index);
    void Free(VAddr addr, std::size_t num_pages);

    void UpdateUsedSize() {
        used_size = heap_size - (GetNumFreePages() * PageSize);
    }

    static std::size_t CalculateMetadataOverheadSize(std::size_t region_size);

private:
    constexpr std::size_t GetNumFreePages() const {
        std::size_t num_free{};

        for (const auto& block : blocks) {
            num_free += block.GetNumFreePages();
        }

        return num_free;
    }

    void FreeBlock(VAddr block, s32 index);

    VAddr heap_address{};
    std::size_t heap_size{};
    std::size_t used_size{};
    std::array<Block, NumMemoryBlockPageShifts> blocks{};
    std::vector<u64> metadata;
};

} // namespace Kernel::Memory
