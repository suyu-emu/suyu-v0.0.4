#pragma once

#include <biscuit/assembler.hpp>
#include <cstdint>

namespace biscuit {

template <typename T>
inline Assembler MakeAssembler32(T& buffer) {
    return Assembler{reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer), ArchFeature::RV32};
}

template <typename T>
inline Assembler MakeAssembler64(T& buffer) {
    return Assembler{reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer), ArchFeature::RV64};
}

template <typename T>
inline Assembler MakeAssembler128(T& buffer) {
    return Assembler{reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer), ArchFeature::RV128};
}

} // namespace biscuit
