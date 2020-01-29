// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <glad/glad.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QOffscreenSurface>
#include <QOpenGLWindow>
#include <QPainter>
#include <QScreen>
#include <QStringList>
#include <QWindow>
#ifdef HAS_VULKAN
#include <QVulkanWindow>
#endif

#include <fmt/format.h>

#include "common/assert.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "core/core.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/frontend/scope_acquire_window_context.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"
#include "yuzu/bootmanager.h"
#include "yuzu/main.h"

EmuThread::EmuThread(GRenderWindow* render_window) : render_window(render_window) {}

EmuThread::~EmuThread() = default;

void EmuThread::run() {
    render_window->MakeCurrent();

    MicroProfileOnThreadCreate("EmuThread");

    emit LoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);

    Core::System::GetInstance().Renderer().Rasterizer().LoadDiskResources(
        stop_run, [this](VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total) {
            emit LoadProgress(stage, value, total);
        });

    emit LoadProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);

    if (Settings::values.use_asynchronous_gpu_emulation) {
        // Release OpenGL context for the GPU thread
        render_window->DoneCurrent();
    }

    // Holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            Core::System::ResultStatus result = Core::System::GetInstance().RunLoop();
            if (result != Core::System::ResultStatus::Success) {
                this->SetRunning(false);
                emit ErrorThrown(result, Core::System::GetInstance().GetStatusDetails());
            }

            was_active = running || exec_step;
            if (!was_active && !stop_run)
                emit DebugModeEntered();
        } else if (exec_step) {
            if (!was_active)
                emit DebugModeLeft();

            exec_step = false;
            Core::System::GetInstance().SingleStep();
            emit DebugModeEntered();
            yieldCurrentThread();

            was_active = false;
        } else {
            std::unique_lock lock{running_mutex};
            running_cv.wait(lock, [this] { return IsRunning() || exec_step || stop_run; });
        }
    }

    // Shutdown the core emulation
    Core::System::GetInstance().Shutdown();

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadExit();
#endif

    render_window->moveContext();
}

class GGLContext : public Core::Frontend::GraphicsContext {
public:
    explicit GGLContext(QOpenGLContext* shared_context) : shared_context{shared_context} {
        context.setFormat(shared_context->format());
        context.setShareContext(shared_context);
        context.create();
    }

    void MakeCurrent() override {
        context.makeCurrent(shared_context->surface());
    }

    void DoneCurrent() override {
        context.doneCurrent();
    }

    void SwapBuffers() override {}

private:
    QOpenGLContext* shared_context;
    QOpenGLContext context;
};

class GWidgetInternal : public QWindow {
public:
    GWidgetInternal(GRenderWindow* parent) : parent(parent) {}
    virtual ~GWidgetInternal() = default;

    void resizeEvent(QResizeEvent* ev) override {
        parent->OnClientAreaResized(ev->size().width(), ev->size().height());
        parent->OnFramebufferSizeChanged();
    }

    void keyPressEvent(QKeyEvent* event) override {
        InputCommon::GetKeyboard()->PressKey(event->key());
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        InputCommon::GetKeyboard()->ReleaseKey(event->key());
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->source() == Qt::MouseEventSynthesizedBySystem)
            return; // touch input is handled in TouchBeginEvent

        const auto pos{event->pos()};
        if (event->button() == Qt::LeftButton) {
            const auto [x, y] = parent->ScaleTouch(pos);
            parent->TouchPressed(x, y);
        } else if (event->button() == Qt::RightButton) {
            InputCommon::GetMotionEmu()->BeginTilt(pos.x(), pos.y());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->source() == Qt::MouseEventSynthesizedBySystem)
            return; // touch input is handled in TouchUpdateEvent

        const auto pos{event->pos()};
        const auto [x, y] = parent->ScaleTouch(pos);
        parent->TouchMoved(x, y);
        InputCommon::GetMotionEmu()->Tilt(pos.x(), pos.y());
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->source() == Qt::MouseEventSynthesizedBySystem)
            return; // touch input is handled in TouchEndEvent

        if (event->button() == Qt::LeftButton)
            parent->TouchReleased();
        else if (event->button() == Qt::RightButton)
            InputCommon::GetMotionEmu()->EndTilt();
    }

    void DisablePainting() {
        do_painting = false;
    }

    void EnablePainting() {
        do_painting = true;
    }

    std::pair<unsigned, unsigned> GetSize() const {
        return std::make_pair(width(), height());
    }

protected:
    bool IsPaintingEnabled() const {
        return do_painting;
    }

private:
    GRenderWindow* parent;
    bool do_painting = false;
};

// This class overrides paintEvent and resizeEvent to prevent the GUI thread from stealing GL
// context.
// The corresponding functionality is handled in EmuThread instead
class GGLWidgetInternal final : public GWidgetInternal, public QOpenGLWindow {
public:
    GGLWidgetInternal(GRenderWindow* parent, QOpenGLContext* shared_context)
        : GWidgetInternal(parent), QOpenGLWindow(shared_context) {}
    ~GGLWidgetInternal() override = default;

    void paintEvent(QPaintEvent* ev) override {
        if (IsPaintingEnabled()) {
            QPainter painter(this);
        }
    }
};

#ifdef HAS_VULKAN
class GVKWidgetInternal final : public GWidgetInternal {
public:
    GVKWidgetInternal(GRenderWindow* parent, QVulkanInstance* instance) : GWidgetInternal(parent) {
        setSurfaceType(QSurface::SurfaceType::VulkanSurface);
        setVulkanInstance(instance);
    }
    ~GVKWidgetInternal() override = default;
};
#endif

GRenderWindow::GRenderWindow(GMainWindow* parent, EmuThread* emu_thread)
    : QWidget(parent), emu_thread(emu_thread) {
    setWindowTitle(QStringLiteral("yuzu %1 | %2-%3")
                       .arg(QString::fromUtf8(Common::g_build_name),
                            QString::fromUtf8(Common::g_scm_branch),
                            QString::fromUtf8(Common::g_scm_desc)));
    setAttribute(Qt::WA_AcceptTouchEvents);

    InputCommon::Init();
    connect(this, &GRenderWindow::FirstFrameDisplayed, parent, &GMainWindow::OnLoadComplete);
}

GRenderWindow::~GRenderWindow() {
    InputCommon::Shutdown();

    // Avoid an unordered destruction that generates a segfault
    delete child;
}

void GRenderWindow::moveContext() {
    if (!context) {
        return;
    }
    DoneCurrent();

    // If the thread started running, move the GL Context to the new thread. Otherwise, move it
    // back.
    auto thread = (QThread::currentThread() == qApp->thread() && emu_thread != nullptr)
                      ? emu_thread
                      : qApp->thread();
    context->moveToThread(thread);
}

void GRenderWindow::SwapBuffers() {
    if (context) {
        context->swapBuffers(child);
    }
    if (!first_frame) {
        first_frame = true;
        emit FirstFrameDisplayed();
    }
}

void GRenderWindow::MakeCurrent() {
    if (context) {
        context->makeCurrent(child);
    }
}

void GRenderWindow::DoneCurrent() {
    if (context) {
        context->doneCurrent();
    }
}

void GRenderWindow::PollEvents() {}

bool GRenderWindow::IsShown() const {
    return !isMinimized();
}

void GRenderWindow::RetrieveVulkanHandlers(void* get_instance_proc_addr, void* instance,
                                           void* surface) const {
#ifdef HAS_VULKAN
    const auto instance_proc_addr = vk_instance->getInstanceProcAddr("vkGetInstanceProcAddr");
    const VkInstance instance_copy = vk_instance->vkInstance();
    const VkSurfaceKHR surface_copy = vk_instance->surfaceForWindow(child);

    std::memcpy(get_instance_proc_addr, &instance_proc_addr, sizeof(instance_proc_addr));
    std::memcpy(instance, &instance_copy, sizeof(instance_copy));
    std::memcpy(surface, &surface_copy, sizeof(surface_copy));
#else
    UNREACHABLE_MSG("Executing Vulkan code without compiling Vulkan");
#endif
}

// On Qt 5.0+, this correctly gets the size of the framebuffer (pixels).
//
// Older versions get the window size (density independent pixels),
// and hence, do not support DPI scaling ("retina" displays).
// The result will be a viewport that is smaller than the extent of the window.
void GRenderWindow::OnFramebufferSizeChanged() {
    // Screen changes potentially incur a change in screen DPI, hence we should update the
    // framebuffer size
    const qreal pixelRatio{GetWindowPixelRatio()};
    const auto size{child->GetSize()};
    UpdateCurrentFramebufferLayout(size.first * pixelRatio, size.second * pixelRatio);
}

void GRenderWindow::ForwardKeyPressEvent(QKeyEvent* event) {
    if (child) {
        child->keyPressEvent(event);
    }
}

void GRenderWindow::ForwardKeyReleaseEvent(QKeyEvent* event) {
    if (child) {
        child->keyReleaseEvent(event);
    }
}

void GRenderWindow::BackupGeometry() {
    geometry = QWidget::saveGeometry();
}

void GRenderWindow::RestoreGeometry() {
    // We don't want to back up the geometry here (obviously)
    QWidget::restoreGeometry(geometry);
}

void GRenderWindow::restoreGeometry(const QByteArray& geometry) {
    // Make sure users of this class don't need to deal with backing up the geometry themselves
    QWidget::restoreGeometry(geometry);
    BackupGeometry();
}

QByteArray GRenderWindow::saveGeometry() {
    // If we are a top-level widget, store the current geometry
    // otherwise, store the last backup
    if (parent() == nullptr) {
        return QWidget::saveGeometry();
    }

    return geometry;
}

qreal GRenderWindow::GetWindowPixelRatio() const {
    // windowHandle() might not be accessible until the window is displayed to screen.
    return windowHandle() ? windowHandle()->screen()->devicePixelRatio() : 1.0f;
}

std::pair<u32, u32> GRenderWindow::ScaleTouch(const QPointF pos) const {
    const qreal pixel_ratio{GetWindowPixelRatio()};
    return {static_cast<u32>(std::max(std::round(pos.x() * pixel_ratio), qreal{0.0})),
            static_cast<u32>(std::max(std::round(pos.y() * pixel_ratio), qreal{0.0}))};
}

void GRenderWindow::closeEvent(QCloseEvent* event) {
    emit Closed();
    QWidget::closeEvent(event);
}

void GRenderWindow::TouchBeginEvent(const QTouchEvent* event) {
    // TouchBegin always has exactly one touch point, so take the .first()
    const auto [x, y] = ScaleTouch(event->touchPoints().first().pos());
    this->TouchPressed(x, y);
}

void GRenderWindow::TouchUpdateEvent(const QTouchEvent* event) {
    QPointF pos;
    int active_points = 0;

    // average all active touch points
    for (const auto tp : event->touchPoints()) {
        if (tp.state() & (Qt::TouchPointPressed | Qt::TouchPointMoved | Qt::TouchPointStationary)) {
            active_points++;
            pos += tp.pos();
        }
    }

    pos /= active_points;

    const auto [x, y] = ScaleTouch(pos);
    this->TouchMoved(x, y);
}

void GRenderWindow::TouchEndEvent() {
    this->TouchReleased();
}

bool GRenderWindow::event(QEvent* event) {
    if (event->type() == QEvent::TouchBegin) {
        TouchBeginEvent(static_cast<QTouchEvent*>(event));
        return true;
    } else if (event->type() == QEvent::TouchUpdate) {
        TouchUpdateEvent(static_cast<QTouchEvent*>(event));
        return true;
    } else if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        TouchEndEvent();
        return true;
    }

    return QWidget::event(event);
}

void GRenderWindow::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    InputCommon::GetKeyboard()->ReleaseAllKeys();
}

void GRenderWindow::OnClientAreaResized(u32 width, u32 height) {
    NotifyClientAreaSizeChanged(std::make_pair(width, height));
}

std::unique_ptr<Core::Frontend::GraphicsContext> GRenderWindow::CreateSharedContext() const {
    return std::make_unique<GGLContext>(context.get());
}

bool GRenderWindow::InitRenderTarget() {
    shared_context.reset();
    context.reset();
    if (child) {
        delete child;
    }
    if (container) {
        delete container;
    }
    if (layout()) {
        delete layout();
    }

    first_frame = false;

    switch (Settings::values.renderer_backend) {
    case Settings::RendererBackend::OpenGL:
        if (!InitializeOpenGL()) {
            return false;
        }
        break;
    case Settings::RendererBackend::Vulkan:
        if (!InitializeVulkan()) {
            return false;
        }
        break;
    }

    container = QWidget::createWindowContainer(child, this);
    QBoxLayout* layout = new QHBoxLayout(this);

    layout->addWidget(container);
    layout->setMargin(0);
    setLayout(layout);

    // Reset minimum required size to avoid resizing issues on the main window after restarting.
    setMinimumSize(1, 1);

    // Show causes the window to actually be created and the gl context as well, but we don't want
    // the widget to be shown yet, so immediately hide it.
    show();
    hide();

    resize(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);
    child->resize(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);
    container->resize(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);

    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);

    OnFramebufferSizeChanged();
    NotifyClientAreaSizeChanged(child->GetSize());

    BackupGeometry();

    if (Settings::values.renderer_backend == Settings::RendererBackend::OpenGL) {
        if (!LoadOpenGL()) {
            return false;
        }
    }

    return true;
}

void GRenderWindow::CaptureScreenshot(u32 res_scale, const QString& screenshot_path) {
    auto& renderer = Core::System::GetInstance().Renderer();

    if (res_scale == 0) {
        res_scale = VideoCore::GetResolutionScaleFactor(renderer);
    }

    const Layout::FramebufferLayout layout{Layout::FrameLayoutFromResolutionScale(res_scale)};
    screenshot_image = QImage(QSize(layout.width, layout.height), QImage::Format_RGB32);
    renderer.RequestScreenshot(
        screenshot_image.bits(),
        [=] {
            const std::string std_screenshot_path = screenshot_path.toStdString();
            if (screenshot_image.mirrored(false, true).save(screenshot_path)) {
                LOG_INFO(Frontend, "Screenshot saved to \"{}\"", std_screenshot_path);
            } else {
                LOG_ERROR(Frontend, "Failed to save screenshot to \"{}\"", std_screenshot_path);
            }
        },
        layout);
}

void GRenderWindow::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    setMinimumSize(minimal_size.first, minimal_size.second);
}

bool GRenderWindow::InitializeOpenGL() {
    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground,
    // WA_DontShowOnScreen, WA_DeleteOnClose
    QSurfaceFormat fmt;
    fmt.setVersion(4, 3);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setOption(QSurfaceFormat::FormatOption::DeprecatedFunctions);
    // TODO: expose a setting for buffer value (ie default/single/double/triple)
    fmt.setSwapBehavior(QSurfaceFormat::DefaultSwapBehavior);
    shared_context = std::make_unique<QOpenGLContext>();
    shared_context->setFormat(fmt);
    shared_context->create();
    context = std::make_unique<QOpenGLContext>();
    context->setShareContext(shared_context.get());
    context->setFormat(fmt);
    context->create();
    fmt.setSwapInterval(false);

    child = new GGLWidgetInternal(this, shared_context.get());
    return true;
}

bool GRenderWindow::InitializeVulkan() {
#ifdef HAS_VULKAN
    vk_instance = std::make_unique<QVulkanInstance>();
    vk_instance->setApiVersion(QVersionNumber(1, 1, 0));
    vk_instance->setFlags(QVulkanInstance::Flag::NoDebugOutputRedirect);
    if (Settings::values.renderer_debug) {
        const auto supported_layers{vk_instance->supportedLayers()};
        const bool found =
            std::find_if(supported_layers.begin(), supported_layers.end(), [](const auto& layer) {
                constexpr const char searched_layer[] = "VK_LAYER_LUNARG_standard_validation";
                return layer.name == searched_layer;
            });
        if (found) {
            vk_instance->setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
            vk_instance->setExtensions(QByteArrayList() << VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    }
    if (!vk_instance->create()) {
        QMessageBox::critical(
            this, tr("Error while initializing Vulkan 1.1!"),
            tr("Your OS doesn't seem to support Vulkan 1.1 instances, or you do not have the "
               "latest graphics drivers."));
        return false;
    }

    child = new GVKWidgetInternal(this, vk_instance.get());
    return true;
#else
    QMessageBox::critical(this, tr("Vulkan not available!"),
                          tr("yuzu has not been compiled with Vulkan support."));
    return false;
#endif
}

bool GRenderWindow::LoadOpenGL() {
    Core::Frontend::ScopeAcquireWindowContext acquire_context{*this};
    if (!gladLoadGL()) {
        QMessageBox::critical(this, tr("Error while initializing OpenGL 4.3!"),
                              tr("Your GPU may not support OpenGL 4.3, or you do not have the "
                                 "latest graphics driver."));
        return false;
    }

    QStringList unsupported_gl_extensions = GetUnsupportedGLExtensions();
    if (!unsupported_gl_extensions.empty()) {
        QMessageBox::critical(
            this, tr("Error while initializing OpenGL!"),
            tr("Your GPU may not support one or more required OpenGL extensions. Please ensure you "
               "have the latest graphics driver.<br><br>Unsupported extensions:<br>") +
                unsupported_gl_extensions.join(QStringLiteral("<br>")));
        return false;
    }
    return true;
}

QStringList GRenderWindow::GetUnsupportedGLExtensions() const {
    QStringList unsupported_ext;

    if (!GLAD_GL_ARB_buffer_storage)
        unsupported_ext.append(QStringLiteral("ARB_buffer_storage"));
    if (!GLAD_GL_ARB_direct_state_access)
        unsupported_ext.append(QStringLiteral("ARB_direct_state_access"));
    if (!GLAD_GL_ARB_vertex_type_10f_11f_11f_rev)
        unsupported_ext.append(QStringLiteral("ARB_vertex_type_10f_11f_11f_rev"));
    if (!GLAD_GL_ARB_texture_mirror_clamp_to_edge)
        unsupported_ext.append(QStringLiteral("ARB_texture_mirror_clamp_to_edge"));
    if (!GLAD_GL_ARB_multi_bind)
        unsupported_ext.append(QStringLiteral("ARB_multi_bind"));
    if (!GLAD_GL_ARB_clip_control)
        unsupported_ext.append(QStringLiteral("ARB_clip_control"));

    // Extensions required to support some texture formats.
    if (!GLAD_GL_EXT_texture_compression_s3tc)
        unsupported_ext.append(QStringLiteral("EXT_texture_compression_s3tc"));
    if (!GLAD_GL_ARB_texture_compression_rgtc)
        unsupported_ext.append(QStringLiteral("ARB_texture_compression_rgtc"));
    if (!GLAD_GL_ARB_depth_buffer_float)
        unsupported_ext.append(QStringLiteral("ARB_depth_buffer_float"));

    for (const QString& ext : unsupported_ext)
        LOG_CRITICAL(Frontend, "Unsupported GL extension: {}", ext.toStdString());

    return unsupported_ext;
}

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;
    child->DisablePainting();
}

void GRenderWindow::OnEmulationStopping() {
    emu_thread = nullptr;
    child->EnablePainting();
}

void GRenderWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // windowHandle() is not initialized until the Window is shown, so we connect it here.
    connect(windowHandle(), &QWindow::screenChanged, this, &GRenderWindow::OnFramebufferSizeChanged,
            Qt::UniqueConnection);
}
