// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/ranges.h>

#include "common/common_types.h"
#include "shader_recompiler/exception.h"

namespace Shader::IR {

enum class Attribute : u64 {
#define SRIR_ATTRIBUTE_LIST \
    SRIR_ATTRIBUTE_ELEM(PrimitiveId, 24) \
    SRIR_ATTRIBUTE_ELEM(Layer, 25) \
    SRIR_ATTRIBUTE_ELEM(ViewportIndex, 26) \
    SRIR_ATTRIBUTE_ELEM(PointSize, 27) \
    SRIR_ATTRIBUTE_ELEM(PositionX, 28) \
    SRIR_ATTRIBUTE_ELEM(PositionY, 29) \
    SRIR_ATTRIBUTE_ELEM(PositionZ, 30) \
    SRIR_ATTRIBUTE_ELEM(PositionW, 31) \
    SRIR_ATTRIBUTE_ELEM(Generic0X, 32) \
    SRIR_ATTRIBUTE_ELEM(Generic0Y, 33) \
    SRIR_ATTRIBUTE_ELEM(Generic0Z, 34) \
    SRIR_ATTRIBUTE_ELEM(Generic0W, 35) \
    SRIR_ATTRIBUTE_ELEM(Generic1X, 36) \
    SRIR_ATTRIBUTE_ELEM(Generic1Y, 37) \
    SRIR_ATTRIBUTE_ELEM(Generic1Z, 38) \
    SRIR_ATTRIBUTE_ELEM(Generic1W, 39) \
    SRIR_ATTRIBUTE_ELEM(Generic2X, 40) \
    SRIR_ATTRIBUTE_ELEM(Generic2Y, 41) \
    SRIR_ATTRIBUTE_ELEM(Generic2Z, 42) \
    SRIR_ATTRIBUTE_ELEM(Generic2W, 43) \
    SRIR_ATTRIBUTE_ELEM(Generic3X, 44) \
    SRIR_ATTRIBUTE_ELEM(Generic3Y, 45) \
    SRIR_ATTRIBUTE_ELEM(Generic3Z, 46) \
    SRIR_ATTRIBUTE_ELEM(Generic3W, 47) \
    SRIR_ATTRIBUTE_ELEM(Generic4X, 48) \
    SRIR_ATTRIBUTE_ELEM(Generic4Y, 49) \
    SRIR_ATTRIBUTE_ELEM(Generic4Z, 50) \
    SRIR_ATTRIBUTE_ELEM(Generic4W, 51) \
    SRIR_ATTRIBUTE_ELEM(Generic5X, 52) \
    SRIR_ATTRIBUTE_ELEM(Generic5Y, 53) \
    SRIR_ATTRIBUTE_ELEM(Generic5Z, 54) \
    SRIR_ATTRIBUTE_ELEM(Generic5W, 55) \
    SRIR_ATTRIBUTE_ELEM(Generic6X, 56) \
    SRIR_ATTRIBUTE_ELEM(Generic6Y, 57) \
    SRIR_ATTRIBUTE_ELEM(Generic6Z, 58) \
    SRIR_ATTRIBUTE_ELEM(Generic6W, 59) \
    SRIR_ATTRIBUTE_ELEM(Generic7X, 60) \
    SRIR_ATTRIBUTE_ELEM(Generic7Y, 61) \
    SRIR_ATTRIBUTE_ELEM(Generic7Z, 62) \
    SRIR_ATTRIBUTE_ELEM(Generic7W, 63) \
    SRIR_ATTRIBUTE_ELEM(Generic8X, 64) \
    SRIR_ATTRIBUTE_ELEM(Generic8Y, 65) \
    SRIR_ATTRIBUTE_ELEM(Generic8Z, 66) \
    SRIR_ATTRIBUTE_ELEM(Generic8W, 67) \
    SRIR_ATTRIBUTE_ELEM(Generic9X, 68) \
    SRIR_ATTRIBUTE_ELEM(Generic9Y, 69) \
    SRIR_ATTRIBUTE_ELEM(Generic9Z, 70) \
    SRIR_ATTRIBUTE_ELEM(Generic9W, 71) \
    SRIR_ATTRIBUTE_ELEM(Generic10X, 72) \
    SRIR_ATTRIBUTE_ELEM(Generic10Y, 73) \
    SRIR_ATTRIBUTE_ELEM(Generic10Z, 74) \
    SRIR_ATTRIBUTE_ELEM(Generic10W, 75) \
    SRIR_ATTRIBUTE_ELEM(Generic11X, 76) \
    SRIR_ATTRIBUTE_ELEM(Generic11Y, 77) \
    SRIR_ATTRIBUTE_ELEM(Generic11Z, 78) \
    SRIR_ATTRIBUTE_ELEM(Generic11W, 79) \
    SRIR_ATTRIBUTE_ELEM(Generic12X, 80) \
    SRIR_ATTRIBUTE_ELEM(Generic12Y, 81) \
    SRIR_ATTRIBUTE_ELEM(Generic12Z, 82) \
    SRIR_ATTRIBUTE_ELEM(Generic12W, 83) \
    SRIR_ATTRIBUTE_ELEM(Generic13X, 84) \
    SRIR_ATTRIBUTE_ELEM(Generic13Y, 85) \
    SRIR_ATTRIBUTE_ELEM(Generic13Z, 86) \
    SRIR_ATTRIBUTE_ELEM(Generic13W, 87) \
    SRIR_ATTRIBUTE_ELEM(Generic14X, 88) \
    SRIR_ATTRIBUTE_ELEM(Generic14Y, 89) \
    SRIR_ATTRIBUTE_ELEM(Generic14Z, 90) \
    SRIR_ATTRIBUTE_ELEM(Generic14W, 91) \
    SRIR_ATTRIBUTE_ELEM(Generic15X, 92) \
    SRIR_ATTRIBUTE_ELEM(Generic15Y, 93) \
    SRIR_ATTRIBUTE_ELEM(Generic15Z, 94) \
    SRIR_ATTRIBUTE_ELEM(Generic15W, 95) \
    SRIR_ATTRIBUTE_ELEM(Generic16X, 96) \
    SRIR_ATTRIBUTE_ELEM(Generic16Y, 97) \
    SRIR_ATTRIBUTE_ELEM(Generic16Z, 98) \
    SRIR_ATTRIBUTE_ELEM(Generic16W, 99) \
    SRIR_ATTRIBUTE_ELEM(Generic17X, 100) \
    SRIR_ATTRIBUTE_ELEM(Generic17Y, 101) \
    SRIR_ATTRIBUTE_ELEM(Generic17Z, 102) \
    SRIR_ATTRIBUTE_ELEM(Generic17W, 103) \
    SRIR_ATTRIBUTE_ELEM(Generic18X, 104) \
    SRIR_ATTRIBUTE_ELEM(Generic18Y, 105) \
    SRIR_ATTRIBUTE_ELEM(Generic18Z, 106) \
    SRIR_ATTRIBUTE_ELEM(Generic18W, 107) \
    SRIR_ATTRIBUTE_ELEM(Generic19X, 108) \
    SRIR_ATTRIBUTE_ELEM(Generic19Y, 109) \
    SRIR_ATTRIBUTE_ELEM(Generic19Z, 110) \
    SRIR_ATTRIBUTE_ELEM(Generic19W, 111) \
    SRIR_ATTRIBUTE_ELEM(Generic20X, 112) \
    SRIR_ATTRIBUTE_ELEM(Generic20Y, 113) \
    SRIR_ATTRIBUTE_ELEM(Generic20Z, 114) \
    SRIR_ATTRIBUTE_ELEM(Generic20W, 115) \
    SRIR_ATTRIBUTE_ELEM(Generic21X, 116) \
    SRIR_ATTRIBUTE_ELEM(Generic21Y, 117) \
    SRIR_ATTRIBUTE_ELEM(Generic21Z, 118) \
    SRIR_ATTRIBUTE_ELEM(Generic21W, 119) \
    SRIR_ATTRIBUTE_ELEM(Generic22X, 120) \
    SRIR_ATTRIBUTE_ELEM(Generic22Y, 121) \
    SRIR_ATTRIBUTE_ELEM(Generic22Z, 122) \
    SRIR_ATTRIBUTE_ELEM(Generic22W, 123) \
    SRIR_ATTRIBUTE_ELEM(Generic23X, 124) \
    SRIR_ATTRIBUTE_ELEM(Generic23Y, 125) \
    SRIR_ATTRIBUTE_ELEM(Generic23Z, 126) \
    SRIR_ATTRIBUTE_ELEM(Generic23W, 127) \
    SRIR_ATTRIBUTE_ELEM(Generic24X, 128) \
    SRIR_ATTRIBUTE_ELEM(Generic24Y, 129) \
    SRIR_ATTRIBUTE_ELEM(Generic24Z, 130) \
    SRIR_ATTRIBUTE_ELEM(Generic24W, 131) \
    SRIR_ATTRIBUTE_ELEM(Generic25X, 132) \
    SRIR_ATTRIBUTE_ELEM(Generic25Y, 133) \
    SRIR_ATTRIBUTE_ELEM(Generic25Z, 134) \
    SRIR_ATTRIBUTE_ELEM(Generic25W, 135) \
    SRIR_ATTRIBUTE_ELEM(Generic26X, 136) \
    SRIR_ATTRIBUTE_ELEM(Generic26Y, 137) \
    SRIR_ATTRIBUTE_ELEM(Generic26Z, 138) \
    SRIR_ATTRIBUTE_ELEM(Generic26W, 139) \
    SRIR_ATTRIBUTE_ELEM(Generic27X, 140) \
    SRIR_ATTRIBUTE_ELEM(Generic27Y, 141) \
    SRIR_ATTRIBUTE_ELEM(Generic27Z, 142) \
    SRIR_ATTRIBUTE_ELEM(Generic27W, 143) \
    SRIR_ATTRIBUTE_ELEM(Generic28X, 144) \
    SRIR_ATTRIBUTE_ELEM(Generic28Y, 145) \
    SRIR_ATTRIBUTE_ELEM(Generic28Z, 146) \
    SRIR_ATTRIBUTE_ELEM(Generic28W, 147) \
    SRIR_ATTRIBUTE_ELEM(Generic29X, 148) \
    SRIR_ATTRIBUTE_ELEM(Generic29Y, 149) \
    SRIR_ATTRIBUTE_ELEM(Generic29Z, 150) \
    SRIR_ATTRIBUTE_ELEM(Generic29W, 151) \
    SRIR_ATTRIBUTE_ELEM(Generic30X, 152) \
    SRIR_ATTRIBUTE_ELEM(Generic30Y, 153) \
    SRIR_ATTRIBUTE_ELEM(Generic30Z, 154) \
    SRIR_ATTRIBUTE_ELEM(Generic30W, 155) \
    SRIR_ATTRIBUTE_ELEM(Generic31X, 156) \
    SRIR_ATTRIBUTE_ELEM(Generic31Y, 157) \
    SRIR_ATTRIBUTE_ELEM(Generic31Z, 158) \
    SRIR_ATTRIBUTE_ELEM(Generic31W, 159) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontDiffuseR, 160) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontDiffuseG, 161) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontDiffuseB, 162) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontDiffuseA, 163) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontSpecularR, 164) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontSpecularG, 165) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontSpecularB, 166) \
    SRIR_ATTRIBUTE_ELEM(ColorFrontSpecularA, 167) \
    SRIR_ATTRIBUTE_ELEM(ColorBackDiffuseR, 168) \
    SRIR_ATTRIBUTE_ELEM(ColorBackDiffuseG, 169) \
    SRIR_ATTRIBUTE_ELEM(ColorBackDiffuseB, 170) \
    SRIR_ATTRIBUTE_ELEM(ColorBackDiffuseA, 171) \
    SRIR_ATTRIBUTE_ELEM(ColorBackSpecularR, 172) \
    SRIR_ATTRIBUTE_ELEM(ColorBackSpecularG, 173) \
    SRIR_ATTRIBUTE_ELEM(ColorBackSpecularB, 174) \
    SRIR_ATTRIBUTE_ELEM(ColorBackSpecularA, 175) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance0, 176) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance1, 177) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance2, 178) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance3, 179) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance4, 180) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance5, 181) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance6, 182) \
    SRIR_ATTRIBUTE_ELEM(ClipDistance7, 183) \
    SRIR_ATTRIBUTE_ELEM(PointSpriteS, 184) \
    SRIR_ATTRIBUTE_ELEM(PointSpriteT, 185) \
    SRIR_ATTRIBUTE_ELEM(FogCoordinate, 186) \
    SRIR_ATTRIBUTE_ELEM(TessellationEvaluationPointU, 188) \
    SRIR_ATTRIBUTE_ELEM(TessellationEvaluationPointV, 189) \
    SRIR_ATTRIBUTE_ELEM(InstanceId, 190) \
    SRIR_ATTRIBUTE_ELEM(VertexId, 191) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture0S, 192) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture0T, 193) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture0R, 194) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture0Q, 195) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture1S, 196) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture1T, 197) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture1R, 198) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture1Q, 199) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture2S, 200) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture2T, 201) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture2R, 202) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture2Q, 203) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture3S, 204) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture3T, 205) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture3R, 206) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture3Q, 207) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture4S, 208) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture4T, 209) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture4R, 210) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture4Q, 211) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture5S, 212) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture5T, 213) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture5R, 214) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture5Q, 215) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture6S, 216) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture6T, 217) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture6R, 218) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture6Q, 219) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture7S, 220) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture7T, 221) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture7R, 222) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture7Q, 223) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture8S, 224) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture8T, 225) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture8R, 226) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture8Q, 227) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture9S, 228) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture9T, 229) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture9R, 230) \
    SRIR_ATTRIBUTE_ELEM(FixedFncTexture9Q, 231) \
    SRIR_ATTRIBUTE_ELEM(ViewportMask, 232) \
    SRIR_ATTRIBUTE_ELEM(FrontFace, 255) \
    /* Implementation attributes */ \
    SRIR_ATTRIBUTE_ELEM(BaseInstance, 256) \
    SRIR_ATTRIBUTE_ELEM(BaseVertex, 257) \
    SRIR_ATTRIBUTE_ELEM(DrawID, 258)
#define SRIR_ATTRIBUTE_ELEM(n, v) n = v,
    SRIR_ATTRIBUTE_LIST
#undef SRIR_ATTRIBUTE_ELEM
};

constexpr size_t NUM_GENERICS = 32;
constexpr size_t NUM_FIXEDFNCTEXTURE = 10;

[[nodiscard]] inline bool IsGeneric(Attribute attribute) noexcept {
    return attribute >= Attribute::Generic0X && attribute <= Attribute::Generic31X;
}

[[nodiscard]] inline u32 GenericAttributeIndex(Attribute attribute) {
    if (!IsGeneric(attribute))
        throw InvalidArgument("Attribute is not generic {}", attribute);
    return (u32(attribute) - u32(Attribute::Generic0X)) / 4u;
}

[[nodiscard]] inline u32 GenericAttributeElement(Attribute attribute) {
    if (!IsGeneric(attribute))
        throw InvalidArgument("Attribute is not generic {}", attribute);
    return u32(attribute) % 4;
}

[[nodiscard]] inline std::string NameOf(Attribute attribute) {
    switch (attribute) {
#define SRIR_ATTRIBUTE_ELEM(n, v) case Attribute::n: return #n;
    SRIR_ATTRIBUTE_LIST
#undef SRIR_ATTRIBUTE_ELEM
    default:
        return fmt::format("<reserved attribute {}>", int(attribute));
    }
}

[[nodiscard]] constexpr IR::Attribute operator+(IR::Attribute attribute, size_t value) noexcept {
    return IR::Attribute(size_t(attribute) + value);
}

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::Attribute> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::Attribute& attribute, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", Shader::IR::NameOf(attribute));
    }
};
