// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include <QObject>

#ifdef YUZU_USE_QT_WEB_ENGINE
#include <QWebEngineView>
#endif

#include "core/frontend/applets/web_browser.h"

enum class HIDButton : u8;

class InputInterpreter;
class GMainWindow;
class UrlRequestInterceptor;

namespace Core {
class System;
}

#ifdef YUZU_USE_QT_WEB_ENGINE

enum class UserAgent {
    WebApplet,
    ShopN,
    LoginApplet,
    ShareApplet,
    LobbyApplet,
    WifiWebAuthApplet,
};

class QtNXWebEngineView : public QWebEngineView {
    Q_OBJECT

public:
    explicit QtNXWebEngineView(QWidget* parent, Core::System& system);
    ~QtNXWebEngineView() override;

    /**
     * Loads a HTML document that exists locally. Cannot be used to load external websites.
     *
     * @param main_url The url to the file.
     * @param additional_args Additional arguments appended to the main url.
     */
    void LoadLocalWebPage(std::string_view main_url, std::string_view additional_args);

    /**
     * Sets the background color of the web page.
     *
     * @param color The color to set.
     */
    void SetBackgroundColor(QColor color);

    /**
     * Sets the user agent of the web browser.
     *
     * @param user_agent The user agent enum.
     */
    void SetUserAgent(UserAgent user_agent);

    [[nodiscard]] bool IsFinished() const;
    void SetFinished(bool finished_);

    [[nodiscard]] WebExitReason GetExitReason() const;
    void SetExitReason(WebExitReason exit_reason_);

    [[nodiscard]] const std::string& GetLastURL() const;
    void SetLastURL(std::string last_url_);

    /**
     * This gets the current URL that has been requested by the webpage.
     * This only applies to the main frame. Sub frames and other resources are ignored.
     *
     * @return Currently requested URL
     */
    [[nodiscard]] QString GetCurrentURL() const;

public slots:
    void hide();

private:
    /**
     * Handles button presses to execute functions assigned in yuzu_key_callbacks.
     * yuzu_key_callbacks contains specialized functions for the buttons in the window footer
     * that can be overriden by games to achieve desired functionality.
     *
     * @tparam HIDButton The list of buttons contained in yuzu_key_callbacks
     */
    template <HIDButton... T>
    void HandleWindowFooterButtonPressedOnce();

    /**
     * Handles button presses and converts them into keyboard input.
     * This should only be used to convert D-Pad or Analog Stick input into arrow keys.
     *
     * @tparam HIDButton The list of buttons that can be converted into keyboard input.
     */
    template <HIDButton... T>
    void HandleWindowKeyButtonPressedOnce();

    /**
     * Handles button holds and converts them into keyboard input.
     * This should only be used to convert D-Pad or Analog Stick input into arrow keys.
     *
     * @tparam HIDButton The list of buttons that can be converted into keyboard input.
     */
    template <HIDButton... T>
    void HandleWindowKeyButtonHold();

    /**
     * Sends a key press event to QWebEngineView.
     *
     * @param key Qt key code.
     */
    void SendKeyPressEvent(int key);

    /**
     * Sends multiple key press events to QWebEngineView.
     *
     * @tparam int Qt key code.
     */
    template <int... T>
    void SendMultipleKeyPressEvents() {
        (SendKeyPressEvent(T), ...);
    }

    void StartInputThread();
    void StopInputThread();

    /// The thread where input is being polled and processed.
    void InputThread();

    std::unique_ptr<UrlRequestInterceptor> url_interceptor;

    std::unique_ptr<InputInterpreter> input_interpreter;

    std::thread input_thread;

    std::atomic<bool> input_thread_running{};

    std::atomic<bool> finished{};

    WebExitReason exit_reason{WebExitReason::EndButtonPressed};

    std::string last_url{"http://localhost/"};
};

#endif

class QtWebBrowser final : public QObject, public Core::Frontend::WebBrowserApplet {
    Q_OBJECT

public:
    explicit QtWebBrowser(GMainWindow& parent);
    ~QtWebBrowser() override;

    void OpenLocalWebPage(std::string_view local_url,
                          std::function<void(WebExitReason, std::string)> callback) const override;

signals:
    void MainWindowOpenLocalWebPage(std::string_view main_url,
                                    std::string_view additional_args) const;

private:
    void MainWindowWebBrowserClosed(WebExitReason exit_reason, std::string last_url);

    mutable std::function<void(WebExitReason, std::string)> callback;
};
