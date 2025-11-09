// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FRONTEND_H
#define FRONTEND_H

#include <QGuiApplication>
#include "qt_common/qt_common.h"

#ifdef YUZU_QT_WIDGETS
#include <QFileDialog>
#include <QWidget>
#include <QMessageBox>
#endif

/**
 * manages common functionality e.g. message boxes and such for Qt/QML
 */
namespace QtCommon::Frontend {

Q_NAMESPACE

#ifdef YUZU_QT_WIDGETS
using Options = QFileDialog::Options;
using Option = QFileDialog::Option;

using StandardButton = QMessageBox::StandardButton;
using StandardButtons = QMessageBox::StandardButtons;

using Icon = QMessageBox::Icon;
#else
enum Option {
    ShowDirsOnly = 0x00000001,
    DontResolveSymlinks = 0x00000002,
    DontConfirmOverwrite = 0x00000004,
    DontUseNativeDialog = 0x00000008,
    ReadOnly = 0x00000010,
    HideNameFilterDetails = 0x00000020,
    DontUseCustomDirectoryIcons = 0x00000040
};
Q_ENUM_NS(Option)
Q_DECLARE_FLAGS(Options, Option)
Q_FLAG_NS(Options)

enum StandardButton {
    // keep this in sync with QDialogButtonBox::StandardButton and QPlatformDialogHelper::StandardButton
    NoButton           = 0x00000000,
    Ok                 = 0x00000400,
    Save               = 0x00000800,
    SaveAll            = 0x00001000,
    Open               = 0x00002000,
    Yes                = 0x00004000,
    YesToAll           = 0x00008000,
    No                 = 0x00010000,
    NoToAll            = 0x00020000,
    Abort              = 0x00040000,
    Retry              = 0x00080000,
    Ignore             = 0x00100000,
    Close              = 0x00200000,
    Cancel             = 0x00400000,
    Discard            = 0x00800000,
    Help               = 0x01000000,
    Apply              = 0x02000000,
    Reset              = 0x04000000,
    RestoreDefaults    = 0x08000000,

    FirstButton        = Ok,                // internal
    LastButton         = RestoreDefaults,   // internal

    YesAll             = YesToAll,          // obsolete
    NoAll              = NoToAll,           // obsolete

    Default            = 0x00000100,        // obsolete
    Escape             = 0x00000200,        // obsolete
    FlagMask           = 0x00000300,        // obsolete
    ButtonMask         = ~FlagMask          // obsolete
};
Q_ENUM_NS(StandardButton)

#if QT_VERSION < QT_VERSION_CHECK(7, 0, 0)
typedef StandardButton Button;
#endif
Q_DECLARE_FLAGS(StandardButtons, StandardButton)
Q_FLAG_NS(StandardButtons)

enum Icon {
    // keep this in sync with QMessageDialogOptions::StandardIcon
    NoIcon = 0,
    Information = 1,
    Warning = 2,
    Critical = 3,
    Question = 4
};
Q_ENUM_NS(Icon)

#endif

// TODO(crueter) widgets-less impl, choices et al.
StandardButton ShowMessage(Icon icon,
                           const QString &title,
                           const QString &text,
                           StandardButtons buttons = StandardButton::NoButton,
                           QObject *parent = nullptr);

#define UTIL_OVERRIDES(level) \
    inline StandardButton level(QObject *parent, \
                                             const QString &title, \
                                             const QString &text, \
                                             StandardButtons buttons = StandardButton::Ok) \
    { \
        return ShowMessage(Icon::level, title, text, buttons, parent); \
    } \
    inline StandardButton level(const QString title, \
                                             const QString &text, \
                                             StandardButtons buttons \
                                             = StandardButton::Ok) \
    { \
        return ShowMessage(Icon::level, title, text, buttons, rootObject); \
    }

UTIL_OVERRIDES(Information)
UTIL_OVERRIDES(Warning)
UTIL_OVERRIDES(Critical)
UTIL_OVERRIDES(Question)

const QString GetOpenFileName(const QString &title,
                              const QString &dir,
                              const QString &filter,
                              QString *selectedFilter = nullptr,
                              Options options = Options());

const QString GetSaveFileName(const QString &title,
                              const QString &dir,
                              const QString &filter,
                              QString *selectedFilter = nullptr,
                              Options options = Options());

const QString GetExistingDirectory(const QString &caption = QString(),
                                    const QString &dir = QString(),
                                    Options options = Option::ShowDirsOnly);

} // namespace QtCommon::Frontend
#endif // FRONTEND_H
