// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <utility>

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <qglobal.h>
#include <qnamespace.h>
#include <qobjectdefs.h>

#include "common/common_types.h"
#include "core/frontend/emu_window.h"

class MainWindow;
class QCamera;
class QImageCapture;
class QMediaCaptureSession;
class QCloseEvent;
class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QObject;
class QResizeEvent;
class QShowEvent;
class QTouchEvent;
class QWheelEvent;

class EmuThread;
namespace Core {
class System;
} // namespace Core

namespace InputCommon {
class InputSubsystem;
enum class MouseButton;
} // namespace InputCommon

namespace InputCommon::TasInput {
enum class TasState;
} // namespace InputCommon::TasInput

namespace VideoCore {
enum class LoadCallbackStage;
} // namespace VideoCore

class GRenderWindow : public QWidget, public Core::Frontend::EmuWindow {
    Q_OBJECT

public:
    explicit GRenderWindow(MainWindow* parent,
                           std::shared_ptr<InputCommon::InputSubsystem> input_subsystem_);
    ~GRenderWindow() override;

    // EmuWindow implementation.
    void OnFrameDisplayed() override;
    bool IsShown() const override;
    std::unique_ptr<Core::Frontend::GraphicsContext> CreateSharedContext() const override;

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry_); // overridden
    QByteArray saveGeometry();                         // overridden

    qreal windowPixelRatio() const;

    std::pair<u32, u32> ScaleTouch(const QPointF& pos) const;

    void closeEvent(QCloseEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

    /// Converts a Qt keyboard key into NativeKeyboard key
    static int QtKeyToSwitchKey(Qt::Key qt_keys);

    /// Converts a Qt modifier keys into NativeKeyboard modifier keys
    static int QtModifierToSwitchModifier(Qt::KeyboardModifiers qt_modifiers);

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    /// Converts a Qt mouse button into MouseInput mouse button
    static InputCommon::MouseButton QtButtonToMouseButton(Qt::MouseButton button);

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void InitializeCamera();
    void FinalizeCamera();

    bool event(QEvent* event) override;

    void focusOutEvent(QFocusEvent* event) override;

    bool InitRenderTarget();

    /// Destroy the previous run's child_widget which should also destroy the child_window
    void ReleaseRenderTarget();

    bool IsLoadingComplete() const;

    void CaptureScreenshot(const QString& screenshot_path);

    /**
     * Instructs the window to re-launch the application using the specified program_index.
     * @param program_index Specifies the index within the application of the program to launch.
     */
    void ExecuteProgram(std::size_t program_index);

    /// Instructs the window to exit the application.
    void Exit();

public slots:
    void OnFramebufferSizeChanged();

signals:
    /// Emitted when the window is closed
    void Closed();
    void FirstFrameDisplayed();
    void ExecuteProgramSignal(std::size_t program_index);
    void ExitSignal();
    void MouseActivity();
    void TasPlaybackStateChanged();

private:
    void TouchBeginEvent(const QTouchEvent* event);
    void TouchUpdateEvent(const QTouchEvent* event);
    void TouchEndEvent();
    void ConstrainMouse();

    void RequestCameraCapture();
    void OnCameraCapture(int requestId, const QImage& img);

    void OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) override;

    bool InitializeOpenGL();
    bool InitializeVulkan();
    void InitializeNull();
    bool LoadOpenGL();
    QStringList GetUnsupportedGLExtensions() const;

    std::shared_ptr<InputCommon::InputSubsystem> input_subsystem;

    // Main context that will be shared with all other contexts that are requested.
    // If this is used in a shared context setting, then this should not be used directly, but
    // should instead be shared from
    std::shared_ptr<Core::Frontend::GraphicsContext> main_context;

    /// Temporary storage of the screenshot taken
    QImage screenshot_image;

    QByteArray geometry;

    QWidget* child_widget = nullptr;

    bool first_frame = false;
    InputCommon::TasInput::TasState last_tas_state;

#if YUZU_USE_QT_MULTIMEDIA
    bool is_virtual_camera;
    int pending_camera_snapshots;
    std::vector<u32> camera_data;
    std::unique_ptr<QCamera> camera;
    std::unique_ptr<QImageCapture> camera_capture;
    std::unique_ptr<QMediaCaptureSession> capture_session;
    std::unique_ptr<QTimer> camera_timer;
#endif

    QTimer mouse_constrain_timer;

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event) override;
};
