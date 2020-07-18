// This file is under the public domain.

#pragma once

#include <cstddef>
#include <type_traits>

namespace Common {

template <typename T>
constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    auto mod{static_cast<T>(value % size)};
    value -= mod;
    return static_cast<T>(mod == T{0} ? value : value + size);
}

template <typename T>
constexpr T AlignDown(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return static_cast<T>(value - value % size);
}

template <typename T>
constexpr T AlignBits(T value, std::size_t align) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return static_cast<T>((value + ((1ULL << align) - 1)) >> align << align);
}

template <typename T>
constexpr bool Is4KBAligned(T value) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return (value & 0xFFF) == 0;
}

template <typename T>
constexpr bool IsWordAligned(T value) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return (value & 0b11) == 0;
}

template <typename T>
constexpr bool IsAligned(T value, std::size_t alignment) {
    using U = typename std::make_unsigned<T>::type;
    const U mask = static_cast<U>(alignment - 1);
    return (value & mask) == 0;
}

template <typename T, std::size_t Align = 16>
class AlignmentAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    constexpr AlignmentAllocator() noexcept = default;

    template <typename T2>
    constexpr AlignmentAllocator(const AlignmentAllocator<T2, Align>&) noexcept {}

    T* allocate(size_type n) {
        return static_cast<T*>(::operator new (n * sizeof(T), std::align_val_t{Align}));
    }

    void deallocate(T* p, size_type n) {
        ::operator delete (p, n * sizeof(T), std::align_val_t{Align});
    }

    template <typename T2>
    struct rebind {
        using other = AlignmentAllocator<T2, Align>;
    };
};

} // namespace Common
