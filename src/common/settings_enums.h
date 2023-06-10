// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <type_traits>
#include <typeindex>
#include "common/common_types.h"

namespace Settings {

enum class AudioEngine : u32 {
    Auto,
    Cubeb,
    Sdl2,
    Null,
};

enum class AudioMode : u32 {
    Mono,
    Stereo,
    Surround,
};

enum class Language : u32 {
    Japanese,
    EnglishAmerican,
    French,
    German,
    Italian,
    Spanish,
    Chinese,
    Korean,
    Dutch,
    Portuguese,
    Russian,
    Taiwanese,
    EnglishBritish,
    FrenchCanadian,
    SpanishLatin,
    ChineseSimplified,
    ChineseTraditional,
    PortugueseBrazilian,
};

enum class Region : u32 {
    Japan,
    Usa,
    Europe,
    Australia,
    China,
    Korea,
    Taiwan,
};

enum class TimeZone : u32 {
    Auto,
    Default,
    CET,
    CST6CDT,
    Cuba,
    EET,
    Egypt,
    Eire,
    EST,
    EST5EDT,
    GB,
    GBEire,
    GMT,
    GMTPlusZero,
    GMTMinusZero,
    GMTZero,
    Greenwich,
    Hongkong,
    HST,
    Iceland,
    Iran,
    Israel,
    Jamaica,
    Japan,
    Kwajalein,
    Libya,
    MET,
    MST,
    MST7MDT,
    Navajo,
    NZ,
    NZCHAT,
    Poland,
    Portugal,
    PRC,
    PST8PDT,
    ROC,
    ROK,
    Singapore,
    Turkey,
    UCT,
    Universal,
    UTC,
    W_SU,
    WET,
    Zulu,
};

enum class AnisotropyMode : u32 {
    Automatic = 0,
    Default = 1,
    X2 = 2,
    X4 = 3,
    X8 = 4,
    X16 = 5,
};

enum class AstcDecodeMode : u32 {
    Cpu = 0,
    Gpu = 1,
    CpuAsynchronous = 2,
};

enum class AstcRecompression : u32 {
    Uncompressed = 0,
    Bc1 = 1,
    Bc3 = 2,
};

enum class VSyncMode : u32 {
    Immediate = 0,
    Mailbox = 1,
    Fifo = 2,
    FifoRelaxed = 3,
};

enum class RendererBackend : u32 {
    OpenGL = 0,
    Vulkan = 1,
    Null = 2,
};

enum class ShaderBackend : u32 {
    Glsl = 0,
    Glasm = 1,
    SpirV = 2,
};

enum class GpuAccuracy : u32 {
    Normal = 0,
    High = 1,
    Extreme = 2,
};

enum class CpuAccuracy : u32 {
    Auto = 0,
    Accurate = 1,
    Unsafe = 2,
    Paranoid = 3,
};

enum class FullscreenMode : u32 {
    Borderless = 0,
    Exclusive = 1,
};

enum class NvdecEmulation : u32 {
    Off = 0,
    Cpu = 1,
    Gpu = 2,
};

enum class ResolutionSetup : u32 {
    Res1_2X = 0,
    Res3_4X = 1,
    Res1X = 2,
    Res3_2X = 3,
    Res2X = 4,
    Res3X = 5,
    Res4X = 6,
    Res5X = 7,
    Res6X = 8,
    Res7X = 9,
    Res8X = 10,
};

enum class ScalingFilter : u32 {
    NearestNeighbor = 0,
    Bilinear = 1,
    Bicubic = 2,
    Gaussian = 3,
    ScaleForce = 4,
    Fsr = 5,
    LastFilter = Fsr,
};

enum class AntiAliasing : u32 {
    None = 0,
    Fxaa = 1,
    Smaa = 2,
    LastAA = Smaa,
};

enum class AspectRatio : u32 {
    R16_9,
    R4_3,
    R21_9,
    R16_10,
    Stretch,
};

#define X(ENUM, NAME)                                                                              \
    { (#NAME), static_cast<u32>(ENUM::NAME) }

static const std::map<std::type_index, std::map<std::string, u32>> canonicalizations = {
    {typeid(AudioEngine),
     {
         {"auto", static_cast<u32>(AudioEngine::Auto)},
         {"cubeb", static_cast<u32>(AudioEngine::Cubeb)},
         {"sdl2", static_cast<u32>(AudioEngine::Sdl2)},
         {"null", static_cast<u32>(AudioEngine::Null)},
     }},
    {typeid(AudioMode),
     {
         X(AudioMode, Mono),
         X(AudioMode, Stereo),
         X(AudioMode, Surround),
     }},
    {typeid(Language),
     {
         X(Language, Japanese),
         X(Language, EnglishAmerican),
         X(Language, French),
         X(Language, German),
         X(Language, Italian),
         X(Language, Spanish),
         X(Language, Chinese),
         X(Language, Korean),
         X(Language, Dutch),
         X(Language, Portuguese),
         X(Language, Russian),
         X(Language, Taiwanese),
         X(Language, EnglishBritish),
         X(Language, FrenchCanadian),
         X(Language, SpanishLatin),
         X(Language, ChineseSimplified),
         X(Language, ChineseTraditional),
         X(Language, PortugueseBrazilian),
     }},
    {typeid(Region),
     {
         X(Region, Japan),
         X(Region, Usa),
         X(Region, Europe),
         X(Region, Australia),
         X(Region, China),
         X(Region, Korea),
         X(Region, Taiwan),
     }},
    {typeid(TimeZone),
     {
         X(TimeZone, Auto),        X(TimeZone, Default),      X(TimeZone, CET),
         X(TimeZone, CST6CDT),     X(TimeZone, Cuba),         X(TimeZone, EET),
         X(TimeZone, Egypt),       X(TimeZone, Eire),         X(TimeZone, EST5EDT),
         X(TimeZone, GB),          X(TimeZone, GBEire),       X(TimeZone, GMT),
         X(TimeZone, GMTPlusZero), X(TimeZone, GMTMinusZero), X(TimeZone, GMTZero),
         X(TimeZone, Greenwich),   X(TimeZone, Hongkong),     X(TimeZone, HST),
         X(TimeZone, Iceland),     X(TimeZone, Iran),         X(TimeZone, Israel),
         X(TimeZone, Jamaica),     X(TimeZone, Japan),        X(TimeZone, Kwajalein),
         X(TimeZone, Libya),       X(TimeZone, MET),          X(TimeZone, MST),
         X(TimeZone, MST7MDT),     X(TimeZone, Navajo),       X(TimeZone, NZ),
         X(TimeZone, NZCHAT),      X(TimeZone, Poland),       X(TimeZone, Portugal),
         X(TimeZone, PRC),         X(TimeZone, ROC),          X(TimeZone, ROK),
         X(TimeZone, Singapore),   X(TimeZone, Turkey),       X(TimeZone, UCT),
         X(TimeZone, Universal),   X(TimeZone, UTC),          X(TimeZone, W_SU),
         X(TimeZone, WET),         X(TimeZone, Zulu),
     }},
    {typeid(AnisotropyMode),
     {
         X(AnisotropyMode, Automatic),
         X(AnisotropyMode, Default),
         X(AnisotropyMode, X2),
         X(AnisotropyMode, X4),
         X(AnisotropyMode, X8),
         X(AnisotropyMode, X16),
     }},
    {typeid(AstcDecodeMode),
     {
         X(AstcDecodeMode, Cpu),
         X(AstcDecodeMode, Gpu),
         X(AstcDecodeMode, CpuAsynchronous),
     }},
    {typeid(AstcRecompression),
     {
         X(AstcRecompression, Uncompressed),
         X(AstcRecompression, Bc1),
         X(AstcRecompression, Bc3),
     }},
    {typeid(VSyncMode),
     {
         X(VSyncMode, Immediate),
         X(VSyncMode, Mailbox),
         X(VSyncMode, Fifo),
         X(VSyncMode, FifoRelaxed),
     }},
    {typeid(RendererBackend),
     {
         X(RendererBackend, OpenGL),
         X(RendererBackend, Vulkan),
         X(RendererBackend, Null),
     }},
    {typeid(ShaderBackend),
     {
         X(ShaderBackend, Glsl),
         X(ShaderBackend, Glasm),
         X(ShaderBackend, SpirV),
     }},
    {typeid(GpuAccuracy),
     {
         X(GpuAccuracy, Normal),
         X(GpuAccuracy, High),
         X(GpuAccuracy, Extreme),
     }},
    {typeid(CpuAccuracy),
     {
         X(CpuAccuracy, Auto),
         X(CpuAccuracy, Accurate),
         X(CpuAccuracy, Unsafe),
         X(CpuAccuracy, Paranoid),
     }},
    {typeid(FullscreenMode),
     {
         X(FullscreenMode, Borderless),
         X(FullscreenMode, Exclusive),
     }},
    {typeid(NvdecEmulation),
     {
         X(NvdecEmulation, Off),
         X(NvdecEmulation, Cpu),
         X(NvdecEmulation, Gpu),
     }},
    {typeid(ResolutionSetup),
     {
         X(ResolutionSetup, Res1_2X),
         X(ResolutionSetup, Res3_4X),
         X(ResolutionSetup, Res1X),
         X(ResolutionSetup, Res3_2X),
         X(ResolutionSetup, Res2X),
         X(ResolutionSetup, Res3X),
         X(ResolutionSetup, Res4X),
         X(ResolutionSetup, Res5X),
         X(ResolutionSetup, Res6X),
         X(ResolutionSetup, Res7X),
         X(ResolutionSetup, Res8X),
     }},
    {typeid(ScalingFilter),
     {
         X(ScalingFilter, NearestNeighbor),
         X(ScalingFilter, Bilinear),
         X(ScalingFilter, Bicubic),
         X(ScalingFilter, Gaussian),
         X(ScalingFilter, ScaleForce),
         X(ScalingFilter, Fsr),
     }},
    {typeid(AntiAliasing),
     {
         X(AntiAliasing, None),
         X(AntiAliasing, Fxaa),
         X(AntiAliasing, Smaa),
     }},
    {typeid(AspectRatio),
     {
         X(AspectRatio, R16_9),
         X(AspectRatio, R4_3),
         X(AspectRatio, R21_9),
         X(AspectRatio, R16_10),
         X(AspectRatio, Stretch),
     }}};

#undef X

static const std::string invalid_string{"(invalid setting)"};

template <typename Type>
const std::string& CanonicalizeEnum(Type id) {
    auto& group = canonicalizations.at(typeid(Type));
    for (auto& [name, value] : group) {
        if (static_cast<Type>(value) == id) {
            return name;
        }
    }
    return invalid_string;
}

template <typename Type>
static Type ToEnum(const std::string& canonicalization) {
    return static_cast<Type>(canonicalizations.at(typeid(Type)).at(canonicalization));
}
} // namespace Settings
