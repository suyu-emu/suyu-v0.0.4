// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 Torzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <QString>
#include <QObject>
#include "common/common_types.h"
#include "common/settings_enums.h"

namespace ConfigurationShared {
using TranslationMap = std::map<u32, std::pair<QString, QString>>;
using ComboboxTranslations = std::vector<std::pair<u32, QString>>;
using ComboboxTranslationMap = std::map<u32, ComboboxTranslations>;

std::unique_ptr<TranslationMap> InitializeTranslations(QObject *parent);

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QObject* parent);

static const std::map<Settings::AntiAliasing, QString> anti_aliasing_texts_map = {
    {Settings::AntiAliasing::None, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "None"))},
    {Settings::AntiAliasing::Fxaa, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "FXAA"))},
    {Settings::AntiAliasing::Smaa, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "SMAA"))},
};

static const std::map<Settings::ScalingFilter, QString> scaling_filter_texts_map = {
    {Settings::ScalingFilter::NearestNeighbor,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Nearest"))},
    {Settings::ScalingFilter::Bilinear,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Bilinear"))},
    {Settings::ScalingFilter::Bicubic, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Bicubic"))},
    {Settings::ScalingFilter::ZeroTangent, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Zero-Tangent"))},
    {Settings::ScalingFilter::BSpline, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "B-Spline"))},
    {Settings::ScalingFilter::Mitchell, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Mitchell"))},
     {Settings::ScalingFilter::Spline1,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Spline-1"))},
    {Settings::ScalingFilter::Gaussian,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Gaussian"))},
     {Settings::ScalingFilter::Lanczos,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Lanczos"))},
    {Settings::ScalingFilter::ScaleForce,
     QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "ScaleForce"))},
    {Settings::ScalingFilter::Fsr, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "FSR"))},
    {Settings::ScalingFilter::Area, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Area"))},
    {Settings::ScalingFilter::Mmpx, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "MMPX"))},
};

static const std::map<Settings::ConsoleMode, QString> use_docked_mode_texts_map = {
    {Settings::ConsoleMode::Docked, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Docked"))},
    {Settings::ConsoleMode::Handheld, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Handheld"))},
};

static const std::map<Settings::GpuAccuracy, QString> gpu_accuracy_texts_map = {
    {Settings::GpuAccuracy::Normal, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Normal"))},
    {Settings::GpuAccuracy::High, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "High"))},
    {Settings::GpuAccuracy::Extreme, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Extreme"))},
};

static const std::map<Settings::RendererBackend, QString> renderer_backend_texts_map = {
    {Settings::RendererBackend::Vulkan, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Vulkan"))},
    {Settings::RendererBackend::OpenGL, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "OpenGL"))},
    {Settings::RendererBackend::Null, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "Null"))},
};

static const std::map<Settings::ShaderBackend, QString> shader_backend_texts_map = {
    {Settings::ShaderBackend::Glsl, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "GLSL"))},
    {Settings::ShaderBackend::Glasm, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "GLASM"))},
    {Settings::ShaderBackend::SpirV, QStringLiteral(QT_TRANSLATE_NOOP("MainWindow", "SPIRV"))},
};

} // namespace ConfigurationShared
