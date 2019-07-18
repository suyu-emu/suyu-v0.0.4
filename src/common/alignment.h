// This file is under the public domain.

#pragma once

#include <cstddef>
#include <cstdlib>
#include <type_traits>
#include <malloc.h>
#include <stdlib.h>

namespace Common {

template <typename T>
constexpr T AlignUp(T value, std::size_t size) {
    static_assert(std::is_unsigned_v<T>, "T must be an unsigned value.");
    return static_cast<T>(value + (size - value % size) % size);
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

template <typename T, std::size_t Align = 16>
class AlignmentAllocator {
public:
    typedef T value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    typedef T* pointer;
    typedef const T* const_pointer;

    typedef T& reference;
    typedef const T& const_reference;

public:
    inline AlignmentAllocator() throw() {}

    template <typename T2>
    inline AlignmentAllocator(const AlignmentAllocator<T2, Align>&) throw() {}

    inline ~AlignmentAllocator() throw() {}

    inline pointer adress(reference r) {
        return &r;
    }

    inline const_pointer adress(const_reference r) const {
        return &r;
    }

#if (defined _MSC_VER)
    inline pointer allocate(size_type n) {
        return (pointer)_aligned_malloc(n * sizeof(value_type), Align);
    }

    inline void deallocate(pointer p, size_type) {
        _aligned_free(p);
    }
#else
    inline pointer allocate(size_type n) {
        return (pointer)std::aligned_alloc(Align, n * sizeof(value_type));
    }

    inline void deallocate(pointer p, size_type) {
        std::free(p);
    }
#endif

    inline void construct(pointer p, const value_type& wert) {
        new (p) value_type(wert);
    }

    inline void destroy(pointer p) {
        p->~value_type();
    }

    inline size_type max_size() const throw() {
        return size_type(-1) / sizeof(value_type);
    }

    template <typename T2>
    struct rebind {
        typedef AlignmentAllocator<T2, Align> other;
    };

    bool operator!=(const AlignmentAllocator<T, Align>& other) const {
        return !(*this == other);
    }

    // Returns true if and only if storage allocated from *this
    // can be deallocated from other, and vice versa.
    // Always returns true for stateless allocators.
    bool operator==(const AlignmentAllocator<T, Align>& other) const {
        return true;
    }
};

} // namespace Common
