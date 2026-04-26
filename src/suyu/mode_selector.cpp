// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QSettings>
#include <QVBoxLayout>

#include "suyu/mode_selector.h"

namespace {
constexpr auto kSettingsKey = "General/AppMode";
constexpr auto kRememberKey = "General/RememberMode";

// Use inline functions to avoid file-scope QString construction before QApplication
inline QString GamerDesc() {
    return QStringLiteral("Game library with controller support, Steam integration, and optimised "
                          "rendering. Best for playing games.");
}
inline QString ProgrammerDesc() {
    return QStringLiteral("Unity-like development environment with code editing, live reload, "
                          "compilation tools, and project management.");
}
inline QString HackerDesc() {
    return QStringLiteral("Advanced toolkit with memory viewer, MCP protocol tools, plugin system, "
                          "and low-level debugging utilities.");
}

QPixmap LoadBrandLogo(int target_width, int target_height) {
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(cwd).filePath(QStringLiteral("img/suyu_logo_variant_primary.png")),
        QDir(cwd).filePath(QStringLiteral("img/suyu_logo_variant_alt.png")),
        QDir(app_dir).filePath(QStringLiteral("branding/suyu_logo_variant_primary.png")),
        QDir(app_dir).filePath(QStringLiteral("branding/suyu_logo_variant_alt.png")),
        QStringLiteral(":/img/suyu_logo.svg"),
        QStringLiteral(":/img/suyu.svg"),
    };

    for (const QString& path : candidates) {
        if (!path.startsWith(QLatin1String(":/")) && !QFileInfo::exists(path)) {
            continue;
        }
        QPixmap px(path);
        if (!px.isNull()) {
            return px.scaled(target_width, target_height, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
        }
    }

    return {};
}
} // anonymous namespace

ModeSelector::ModeSelector(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("suyu | Setup Profile"));
    setMinimumSize(980, 640);
    setStyleSheet(QStringLiteral(
        "ModeSelector {"
        "  background-color: #07040d;"
        "  background-image: qradialgradient(cx:0.17, cy:0.14, radius:0.95, stop:0 #6800c3, stop:0.24 rgba(156, 77, 216, 0.25), stop:0.5 rgba(10, 4, 22, 0.96), stop:1 #05020a);"
        "}"
        "QFrame#ModeSelectorFrame {"
        "  background-color: rgba(18, 10, 28, 220);"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 32px;"
        "}"
        "QLabel#TitleLabel {"
        "  color: #ffffff;"
        "}"
        "QLabel#SubtitleLabel {"
        "  color: #b8b8c8;"
        "  font-size: 11pt;"
        "}"
        "QPushButton#ModeCard {"
        "  color: white;"
        "  background-color: rgba(255,255,255,0.08);"
        "  border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 32px;"
        "  min-width: 260px;"
        "  min-height: 340px;"
        "  padding: 26px;"
        "  text-align: left;"
        "}"
        "QPushButton#ModeCard:hover {"
        "  border-color: rgba(156, 77, 216, 0.75);"
        "  background-color: rgba(255,255,255,0.12);"
        "}"
        "QPushButton#ModeCard:pressed {"
        "  background-color: rgba(255,255,255,0.06);"
        "}"
        "QPushButton#FooterButton {"
        "  color: #dddddd;"
        "  background-color: rgba(255,255,255,0.08);"
        "  border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 22px;"
        "  min-width: 92px;"
        "  min-height: 92px;"
        "  padding: 10px;"
        "  font-size: 9pt;"
        "  text-align: center;"
        "}"
        "QPushButton#FooterButton:hover {"
        "  border-color: rgba(255,255,255,0.28);"
        "  background-color: rgba(255,255,255,0.14);"
        "}"
    ));

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(24, 24, 24, 24);
    main_layout->setSpacing(16);

    auto* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("ModeSelectorFrame"));
    auto* frame_layout = new QVBoxLayout(frame);
    frame_layout->setContentsMargins(36, 30, 36, 26);
    frame_layout->setSpacing(28);

    auto* brand_block = new QWidget(this);
    auto* brand_layout = new QVBoxLayout(brand_block);
    brand_layout->setContentsMargins(0, 0, 0, 0);
    brand_layout->setSpacing(8);

    auto* brand_logo = new QLabel(this);
    brand_logo->setAlignment(Qt::AlignCenter);
    const QPixmap logo_px = LoadBrandLogo(220, 96);
    if (!logo_px.isNull()) {
        brand_logo->setPixmap(logo_px);
    } else {
        brand_logo->setText(QStringLiteral("suyu"));
        brand_logo->setStyleSheet(QStringLiteral("font-size:42pt; font-weight:800; color:#ffffff;"));
    }
    brand_layout->addWidget(brand_logo);

    auto* title = new QLabel(
        QStringLiteral("<div style='text-align:center;'>"
                       "<span style='font-size:12pt; color:#dcdbde;'>Welcome to suyu</span><br>"
                       "<span style='font-size:18pt; font-weight:600; color:#ffffff;'>Choose Your Layout Profile</span>"
                       "</div>"),
        this);
    title->setObjectName(QStringLiteral("TitleLabel"));
    title->setTextFormat(Qt::RichText);
    title->setAlignment(Qt::AlignCenter);
    brand_layout->addWidget(title);

    frame_layout->addWidget(brand_block);

    const QString card_style = QStringLiteral(
        "QPushButton#ModeCard {"
        "  color: white;"
        "  background-color: rgba(255,255,255,0.08);"
        "  border: 1px solid rgba(255,255,255,0.14);"
        "  border-radius: 32px;"
        "  min-width: 260px;"
        "  min-height: 340px;"
        "  padding: 24px;"
        "  text-align: left;"
        "}"
        "QPushButton#ModeCard:hover {"
        "  border-color: rgba(156, 77, 216, 0.75);"
        "  background-color: rgba(255,255,255,0.12);"
        "}"
        "QPushButton#ModeCard:pressed {"
        "  background-color: rgba(255,255,255,0.06);"
        "}"
    );

    auto* btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(24);
    btn_layout->setAlignment(Qt::AlignHCenter);

    // Helper: QPushButton does not render rich text in its text property.
    // We place a QLabel child with Qt::RichText inside each button instead.
    auto setupCardLabel = [](QPushButton* btn, const QString& html) {
        auto* lay = new QVBoxLayout(btn);
        lay->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(html, btn);
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        lbl->setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0;"));
        lay->addWidget(lbl);
    };

    btn_gamer_ = new QPushButton(this);
    btn_gamer_->setObjectName(QStringLiteral("ModeCard"));
    btn_gamer_->setCursor(Qt::PointingHandCursor);
    btn_gamer_->setStyleSheet(card_style);
    setupCardLabel(btn_gamer_,
        QStringLiteral("<div style='text-align:left;'>"
                       "<span style='font-size:48pt;'>\xF0\x9F\x8E\xAE</span><br>"
                       "<span style='font-size:24pt; font-weight:700; color:white;'>Gamer</span><br>"
                       "<span style='color:#d4d4e6; font-size:11pt;'>Optimized for Gameplay</span>"
                       "</div>"));
    connect(btn_gamer_, &QPushButton::clicked, this, &ModeSelector::OnGamerClicked);
    btn_layout->addWidget(btn_gamer_);

    btn_programmer_ = new QPushButton(this);
    btn_programmer_->setObjectName(QStringLiteral("ModeCard"));
    btn_programmer_->setCursor(Qt::PointingHandCursor);
    btn_programmer_->setStyleSheet(card_style);
    setupCardLabel(btn_programmer_,
        QStringLiteral("<div style='text-align:left;'>"
                       "<span style='font-size:48pt;'>\xF0\x9F\x92\xBB</span><br>"
                       "<span style='font-size:24pt; font-weight:700; color:white;'>Programmer</span><br>"
                       "<span style='color:#d4d4e6; font-size:11pt;'>Developer tools &amp; debugging</span>"
                       "</div>"));
    connect(btn_programmer_, &QPushButton::clicked, this, &ModeSelector::OnProgrammerClicked);
    btn_layout->addWidget(btn_programmer_);

    btn_hacker_ = new QPushButton(this);
    btn_hacker_->setObjectName(QStringLiteral("ModeCard"));
    btn_hacker_->setCursor(Qt::PointingHandCursor);
    btn_hacker_->setStyleSheet(card_style);
    setupCardLabel(btn_hacker_,
        QStringLiteral("<div style='text-align:left;'>"
                       "<span style='font-size:48pt;'>\xF0\x9F\x94\xA7</span><br>"
                       "<span style='font-size:24pt; font-weight:700; color:white;'>Hacker</span><br>"
                       "<span style='color:#d4d4e6; font-size:11pt;'>Advanced configuration &amp; tweaks</span>"
                       "</div>"));
    connect(btn_hacker_, &QPushButton::clicked, this, &ModeSelector::OnHackerClicked);
    btn_layout->addWidget(btn_hacker_);

    frame_layout->addLayout(btn_layout);

    lbl_description_ = new QLabel(GamerDesc(), this);
    lbl_description_->setWordWrap(true);
    lbl_description_->setAlignment(Qt::AlignCenter);
    lbl_description_->setMinimumHeight(58);
    lbl_description_->setObjectName(QStringLiteral("SubtitleLabel"));
    lbl_description_->setStyleSheet(QStringLiteral("color: #c8c8d8; font-size: 12pt;"));
    frame_layout->addWidget(lbl_description_);

    auto* footer_layout = new QHBoxLayout();
    footer_layout->setSpacing(16);
    footer_layout->setAlignment(Qt::AlignHCenter);

    auto makeFooterButton = [this](const QString& icon, const QString& text) {
        auto* button = new QPushButton(this);
        button->setObjectName(QStringLiteral("FooterButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setFocusPolicy(Qt::NoFocus);
        auto* lay = new QVBoxLayout(button);
        lay->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(
            QStringLiteral("<div style='text-align:center;'>"
                           "<span style='font-size:22pt;'>%1</span><br>"
                           "<span style='color:#dddddd; font-size:9pt;'>%2</span>"
                           "</div>")
                .arg(icon, text),
            button);
        lbl->setTextFormat(Qt::RichText);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        lbl->setStyleSheet(QStringLiteral("background: transparent; border: none; padding: 0;"));
        lay->addWidget(lbl);
        return button;
    };

    footer_layout->addWidget(makeFooterButton(QStringLiteral("⚙"), QStringLiteral("Settings")));
    footer_layout->addWidget(makeFooterButton(QStringLiteral("❓"), QStringLiteral("Help")));
    footer_layout->addWidget(makeFooterButton(QStringLiteral("ℹ"), QStringLiteral("About suyu")));

    frame_layout->addLayout(footer_layout);

    main_layout->addWidget(frame);
    main_layout->addStretch();
    setLayout(main_layout);
}

ModeSelector::~ModeSelector() = default;

void ModeSelector::ApplySelection(AppMode mode) {
    selected_mode_ = mode;
    remember_choice_ = true;

    switch (mode) {
    case AppMode::Gamer:
        lbl_description_->setText(GamerDesc());
        break;
    case AppMode::Programmer:
        lbl_description_->setText(ProgrammerDesc());
        break;
    case AppMode::Hacker:
        lbl_description_->setText(HackerDesc());
        break;
    }

    if (remember_choice_) {
        SaveMode(mode);
    }
    accept();
}

void ModeSelector::OnGamerClicked() {
    ApplySelection(AppMode::Gamer);
}

void ModeSelector::OnProgrammerClicked() {
    ApplySelection(AppMode::Programmer);
}

void ModeSelector::OnHackerClicked() {
    ApplySelection(AppMode::Hacker);
}

AppMode ModeSelector::SelectedMode() const {
    return selected_mode_;
}

bool ModeSelector::RememberChoice() const {
    return remember_choice_;
}

AppMode ModeSelector::LoadSavedMode() {
    QSettings settings;
    const bool remember = settings.value(QLatin1String(kRememberKey), false).toBool();
    if (!remember) {
        return AppMode::Gamer;
    }
    const int val = settings.value(QLatin1String(kSettingsKey), 0).toInt();
    switch (val) {
    case 1:
        return AppMode::Programmer;
    case 2:
        return AppMode::Hacker;
    default:
        return AppMode::Gamer;
    }
}

void ModeSelector::SaveMode(AppMode mode) {
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKey), static_cast<int>(mode));
    settings.setValue(QLatin1String(kRememberKey), true);
}
