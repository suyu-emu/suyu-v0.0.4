// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suyu/hacker_environment.h"

#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <QFileDialog>

#include "common/fs/path_util.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <unistd.h>
#endif

HackerEnvironment::HackerEnvironment(QWidget* parent) : QWidget(parent) {
    SetupUI();
}

HackerEnvironment::~HackerEnvironment() {
    if (process_timer_) {
        process_timer_->stop();
    }
}

void HackerEnvironment::SetupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tab_widget_ = new QTabWidget(this);
    tab_widget_->setTabPosition(QTabWidget::South);
    tab_widget_->setDocumentMode(true);

    tab_widget_->addTab(CreateProcessMonitorTab(), QStringLiteral("Processes"));
    tab_widget_->addTab(CreateMemoryViewerTab(), QStringLiteral("Memory"));
    tab_widget_->addTab(CreateLogConsoleTab(), QStringLiteral("Log Console"));
    tab_widget_->addTab(CreateMcpToolsTab(), QStringLiteral("MCP Tools"));
    tab_widget_->addTab(CreateSystemInfoTab(), QStringLiteral("System Info"));
    tab_widget_->addTab(CreateRecompileTab(), QStringLiteral("Recompile"));

    layout->addWidget(tab_widget_);
    setLayout(layout);

    // Start auto-refresh for process table
    process_timer_ = new QTimer(this);
    process_timer_->setInterval(3000);
    connect(process_timer_, &QTimer::timeout, this, &HackerEnvironment::RefreshProcesses);
    process_timer_->start();

    // Initial population
    RefreshProcesses();
    RefreshSystemInfo();
}

// ---------------------------------------------------------------------------
// Process Monitor
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateProcessMonitorTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* toolbar = new QHBoxLayout();
    auto* lbl = new QLabel(QStringLiteral("Emulator Threads & Processes"));
    lbl->setStyleSheet(QStringLiteral("font-weight:bold; font-size:12px;"));
    toolbar->addWidget(lbl);
    toolbar->addStretch();

    auto* btn_refresh = new QPushButton(QStringLiteral("Refresh"));
    connect(btn_refresh, &QPushButton::clicked, this, &HackerEnvironment::RefreshProcesses);
    toolbar->addWidget(btn_refresh);
    layout->addLayout(toolbar);

    process_table_ = new QTableWidget(0, 5, widget);
    process_table_->setHorizontalHeaderLabels(
        {QStringLiteral("TID"), QStringLiteral("Name"), QStringLiteral("State"),
         QStringLiteral("CPU %"), QStringLiteral("Memory")});
    process_table_->horizontalHeader()->setStretchLastSection(true);
    process_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    process_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    process_table_->setAlternatingRowColors(true);
    process_table_->verticalHeader()->setVisible(false);
    layout->addWidget(process_table_);

    widget->setLayout(layout);
    return widget;
}

void HackerEnvironment::RefreshProcesses() {
    if (!process_table_) return;

    process_table_->setRowCount(0);

    // Show emulator-relevant threads from this process
#ifdef _WIN32
    DWORD pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te{};
    te.dwSize = sizeof(te);

    int row = 0;
    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                process_table_->insertRow(row);
                process_table_->setItem(
                    row, 0,
                    new QTableWidgetItem(QString::number(te.th32ThreadID)));
                process_table_->setItem(
                    row, 1, new QTableWidgetItem(QStringLiteral("Thread")));
                process_table_->setItem(
                    row, 2, new QTableWidgetItem(QStringLiteral("Running")));
                process_table_->setItem(
                    row, 3,
                    new QTableWidgetItem(
                        QString::number(te.tpBasePri)));
                process_table_->setItem(
                    row, 4, new QTableWidgetItem(QStringLiteral("-")));
                ++row;
            }
        } while (Thread32Next(snapshot, &te));
    }

    CloseHandle(snapshot);
#else
    // On non-Windows, show basic info
    process_table_->insertRow(0);
    process_table_->setItem(0, 0, new QTableWidgetItem(QString::number(getpid())));
    process_table_->setItem(0, 1, new QTableWidgetItem(QStringLiteral("suyu")));
    process_table_->setItem(0, 2, new QTableWidgetItem(QStringLiteral("Running")));
    process_table_->setItem(0, 3, new QTableWidgetItem(QStringLiteral("-")));
    process_table_->setItem(0, 4, new QTableWidgetItem(QStringLiteral("-")));
#endif
}

// ---------------------------------------------------------------------------
// Memory Viewer
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateMemoryViewerTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(QStringLiteral("Region:")));

    region_combo_ = new QComboBox();
    region_combo_->addItems({QStringLiteral("Main"), QStringLiteral("Heap"),
                             QStringLiteral("Stack"), QStringLiteral("Code")});
    toolbar->addWidget(region_combo_);

    toolbar->addWidget(new QLabel(QStringLiteral("Address:")));
    address_input_ = new QLineEdit();
    address_input_->setPlaceholderText(QStringLiteral("0x00000000"));
    address_input_->setMaximumWidth(200);
    toolbar->addWidget(address_input_);

    auto* btn_go = new QPushButton(QStringLiteral("Go"));
    connect(btn_go, &QPushButton::clicked, this, &HackerEnvironment::RefreshMemory);
    toolbar->addWidget(btn_go);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    memory_table_ = new QTableWidget(16, 17, widget); // address + 16 bytes
    QStringList headers{QStringLiteral("Address")};
    for (int i = 0; i < 16; ++i)
        headers << QString::number(i, 16).toUpper().rightJustified(2, QLatin1Char('0'));
    memory_table_->setHorizontalHeaderLabels(headers);
    memory_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    memory_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    memory_table_->setFont(QFont(QStringLiteral("Courier New"), 9));
    memory_table_->verticalHeader()->setVisible(false);
    memory_table_->horizontalHeader()->setDefaultSectionSize(28);
    memory_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    // Fill with placeholder rows
    for (int r = 0; r < 16; ++r) {
        quint64 addr = static_cast<quint64>(r) * 16;
        memory_table_->setItem(
            r, 0,
            new QTableWidgetItem(
                QStringLiteral("0x%1").arg(addr, 8, 16, QLatin1Char('0'))));
        for (int c = 1; c <= 16; ++c) {
            memory_table_->setItem(r, c, new QTableWidgetItem(QStringLiteral("00")));
        }
    }

    layout->addWidget(memory_table_);
    widget->setLayout(layout);
    return widget;
}

void HackerEnvironment::RefreshMemory() {
    if (!memory_table_ || !address_input_) return;

    bool ok = false;
    quint64 base = address_input_->text().toULongLong(&ok, 16);
    if (!ok) base = 0;

    for (int r = 0; r < 16; ++r) {
        quint64 row_addr = base + static_cast<quint64>(r) * 16;
        memory_table_->item(r, 0)->setText(
            QStringLiteral("0x%1").arg(row_addr, 16, 16, QLatin1Char('0')));
        for (int c = 1; c <= 16; ++c) {
            memory_table_->item(r, c)->setText(QStringLiteral("??"));
        }
    }

    emit MemoryAddressSelected(base);
}

// ---------------------------------------------------------------------------
// Log Console
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateLogConsoleTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    log_output_ = new QTextEdit();
    log_output_->setReadOnly(true);
    log_output_->setFont(QFont(QStringLiteral("Courier New"), 9));
    log_output_->setStyleSheet(
        QStringLiteral("background-color:#1e1e1e; color:#dcdcdc;"));
    layout->addWidget(log_output_);

    auto* input_layout = new QHBoxLayout();
    command_input_ = new QLineEdit();
    command_input_->setPlaceholderText(QStringLiteral("Enter command..."));
    connect(command_input_, &QLineEdit::returnPressed, this, [this]() {
        const QString cmd = command_input_->text().trimmed();
        if (cmd.isEmpty()) return;

        AppendLog(QStringLiteral("> %1").arg(cmd));

        if (cmd == QStringLiteral("help")) {
            AppendLog(QStringLiteral("Available commands: help, clear, info, refresh, mcp-status"));
        } else if (cmd == QStringLiteral("clear")) {
            log_output_->clear();
        } else if (cmd == QStringLiteral("info")) {
            AppendLog(QStringLiteral("suyu Hacker Console v1.0"));
            AppendLog(QStringLiteral("Type 'help' for available commands."));
        } else if (cmd == QStringLiteral("refresh")) {
            Refresh();
            AppendLog(QStringLiteral("Refreshed all panels."));
        } else if (cmd == QStringLiteral("mcp-status")) {
            AppendLog(QStringLiteral("MCP Server status: check Hacker mode toolbar."));
        } else {
            AppendLog(QStringLiteral("Unknown command: %1").arg(cmd));
        }

        command_input_->clear();
    });
    input_layout->addWidget(command_input_);

    auto* btn_clear = new QPushButton(QStringLiteral("Clear"));
    connect(btn_clear, &QPushButton::clicked, log_output_, &QTextEdit::clear);
    input_layout->addWidget(btn_clear);
    layout->addLayout(input_layout);

    widget->setLayout(layout);
    return widget;
}

void HackerEnvironment::AppendLog(const QString& message) {
    if (!log_output_) return;

    const QString timestamp =
        QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    log_output_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

// ---------------------------------------------------------------------------
// MCP Tools
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateMcpToolsTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* splitter = new QSplitter(Qt::Horizontal, widget);

    // Left: tool tree
    tool_tree_ = new QTreeWidget();
    tool_tree_->setHeaderLabels({QStringLiteral("Tool"), QStringLiteral("Description")});
    tool_tree_->setColumnWidth(0, 180);

    // Built-in MCP tools
    const struct {
        const char* name;
        const char* desc;
    } builtin_tools[] = {
        {"get_emulator_state", "Current emulation status"},
        {"get_rom_info", "Info about loaded ROM"},
        {"list_save_states", "Available save states"},
        {"list_installed_titles", "Installed game titles"},
        {"get_system_info", "Host system information"},
        {"list_game_directories", "Configured game dirs"},
        {"get_keys_status", "Encryption key status"},
        {"get_log_tail", "Recent log entries"},
    };

    auto* root = new QTreeWidgetItem(tool_tree_, {QStringLiteral("Built-in Tools")});
    root->setExpanded(true);
    for (const auto& t : builtin_tools) {
        new QTreeWidgetItem(root, {QString::fromLatin1(t.name), QString::fromLatin1(t.desc)});
    }

    splitter->addWidget(tool_tree_);

    // Right: output + invoke
    auto* right_widget = new QWidget();
    auto* right_layout = new QVBoxLayout(right_widget);
    right_layout->setContentsMargins(0, 0, 0, 0);

    tool_output_ = new QTextEdit();
    tool_output_->setReadOnly(true);
    tool_output_->setFont(QFont(QStringLiteral("Courier New"), 9));
    tool_output_->setStyleSheet(
        QStringLiteral("background-color:#1e1e1e; color:#dcdcdc;"));
    right_layout->addWidget(tool_output_);

    auto* invoke_layout = new QHBoxLayout();
    tool_args_input_ = new QLineEdit();
    tool_args_input_->setPlaceholderText(QStringLiteral("Tool arguments (JSON)..."));
    invoke_layout->addWidget(tool_args_input_);

    auto* btn_invoke = new QPushButton(QStringLiteral("Invoke"));
    connect(btn_invoke, &QPushButton::clicked, this, [this]() {
        auto* item = tool_tree_->currentItem();
        if (!item || !item->parent()) {
            tool_output_->append(QStringLiteral("Select a tool first."));
            return;
        }
        const QString tool_name = item->text(0);
        const QString args = tool_args_input_->text();
        tool_output_->append(
            QStringLiteral("Invoking %1(%2)...").arg(tool_name, args));
        emit ToolInvoked(tool_name, args);
    });
    invoke_layout->addWidget(btn_invoke);
    right_layout->addLayout(invoke_layout);

    right_widget->setLayout(right_layout);
    splitter->addWidget(right_widget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter);
    widget->setLayout(layout);
    return widget;
}

// ---------------------------------------------------------------------------
// System Info
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateSystemInfoTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* toolbar = new QHBoxLayout();
    auto* lbl = new QLabel(QStringLiteral("Host & Emulated System Information"));
    lbl->setStyleSheet(QStringLiteral("font-weight:bold; font-size:12px;"));
    toolbar->addWidget(lbl);
    toolbar->addStretch();
    auto* btn_refresh = new QPushButton(QStringLiteral("Refresh"));
    connect(btn_refresh, &QPushButton::clicked, this, &HackerEnvironment::RefreshSystemInfo);
    toolbar->addWidget(btn_refresh);
    layout->addLayout(toolbar);

    system_tree_ = new QTreeWidget();
    system_tree_->setHeaderLabels({QStringLiteral("Property"), QStringLiteral("Value")});
    system_tree_->setColumnWidth(0, 250);
    system_tree_->setAlternatingRowColors(true);
    layout->addWidget(system_tree_);

    widget->setLayout(layout);
    return widget;
}

void HackerEnvironment::RefreshSystemInfo() {
    if (!system_tree_) return;
    system_tree_->clear();

    // Host section
    auto* host = new QTreeWidgetItem(system_tree_, {QStringLiteral("Host")});
    host->setExpanded(true);

#ifdef _WIN32
    new QTreeWidgetItem(host, {QStringLiteral("OS"), QStringLiteral("Windows")});
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    new QTreeWidgetItem(host,
                        {QStringLiteral("Processors"),
                         QString::number(si.dwNumberOfProcessors)});
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    new QTreeWidgetItem(
        host, {QStringLiteral("Total RAM"),
               QStringLiteral("%1 MB").arg(ms.ullTotalPhys / (1024 * 1024))});
    new QTreeWidgetItem(
        host, {QStringLiteral("Available RAM"),
               QStringLiteral("%1 MB").arg(ms.ullAvailPhys / (1024 * 1024))});
#else
    new QTreeWidgetItem(host, {QStringLiteral("OS"), QStringLiteral("POSIX")});
    new QTreeWidgetItem(
        host, {QStringLiteral("Page Size"), QString::number(sysconf(_SC_PAGESIZE))});
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    new QTreeWidgetItem(
        host, {QStringLiteral("Total RAM"),
               QStringLiteral("%1 MB").arg((pages * page_size) / (1024 * 1024))});
#endif

    // Emulated section
    auto* emu = new QTreeWidgetItem(system_tree_, {QStringLiteral("Emulated System")});
    emu->setExpanded(true);
    new QTreeWidgetItem(emu, {QStringLiteral("Console"), QStringLiteral("Nintendo Switch")});
    new QTreeWidgetItem(emu, {QStringLiteral("CPU"), QStringLiteral("ARM Cortex-A57 (4 cores)")});
    new QTreeWidgetItem(emu, {QStringLiteral("GPU"), QStringLiteral("NVIDIA Tegra X1 Maxwell")});
    new QTreeWidgetItem(emu, {QStringLiteral("RAM"), QStringLiteral("4 GB LPDDR4")});

    // Paths section
    auto* paths = new QTreeWidgetItem(system_tree_, {QStringLiteral("Paths")});
    paths->setExpanded(true);
    new QTreeWidgetItem(paths,
                        {QStringLiteral("User Dir"),
                         QString::fromStdString(
                             Common::FS::GetSuyuPath(Common::FS::SuyuPath::SuyuDir).string())});
    new QTreeWidgetItem(paths,
                        {QStringLiteral("Keys Dir"),
                         QString::fromStdString(
                             Common::FS::GetSuyuPath(Common::FS::SuyuPath::KeysDir).string())});
    new QTreeWidgetItem(paths,
                        {QStringLiteral("NAND Dir"),
                         QString::fromStdString(
                             Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir).string())});
}

// ---------------------------------------------------------------------------
// Recompile Export
// ---------------------------------------------------------------------------

QWidget* HackerEnvironment::CreateRecompileTab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* title = new QLabel(QStringLiteral("Static Recompiler — AArch64 to Native"));
    title->setStyleSheet(QStringLiteral("font-weight:bold; font-size:12px;"));
    layout->addWidget(title);

    auto* desc = new QLabel(QStringLiteral(
        "Export the loaded game's code as portable C source that compiles to a native "
        "executable (Windows .exe, Linux ELF, macOS Mach-O) or as raw source for inspection."));
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral("color: #aaa; font-size:11px; margin-bottom: 6px;"));
    layout->addWidget(desc);

    auto* form = new QHBoxLayout();

    form->addWidget(new QLabel(QStringLiteral("Output:")));
    recomp_path_input_ = new QLineEdit();
    recomp_path_input_->setPlaceholderText(QStringLiteral("C:/output/recompiled"));
    recomp_path_input_->setText(
        QString::fromStdString(
            Common::FS::GetSuyuPath(Common::FS::SuyuPath::SuyuDir).string()) +
        QStringLiteral("/recompiled"));
    form->addWidget(recomp_path_input_, 1);

    auto* btn_browse = new QPushButton(QStringLiteral("Browse..."));
    connect(btn_browse, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Recompile Output Directory"),
            recomp_path_input_->text());
        if (!dir.isEmpty()) {
            recomp_path_input_->setText(dir);
        }
    });
    form->addWidget(btn_browse);

    form->addWidget(new QLabel(QStringLiteral("Platform:")));
    recomp_platform_ = new QComboBox();
    recomp_platform_->addItems({QStringLiteral("Windows (.exe)"),
                                 QStringLiteral("Linux/BSD (ELF)"),
                                 QStringLiteral("macOS (Mach-O)"),
                                 QStringLiteral("All Platforms")});
    form->addWidget(recomp_platform_);

    form->addWidget(new QLabel(QStringLiteral("Mode:")));
    recomp_mode_ = new QComboBox();
    recomp_mode_->addItems({QStringLiteral("Source + Build Scripts"),
                             QStringLiteral("Source Only (no build)")});
    form->addWidget(recomp_mode_);

    layout->addLayout(form);

    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* btn_export = new QPushButton(QStringLiteral("Export Recompiled Source"));
    btn_export->setStyleSheet(QStringLiteral(
        "QPushButton { background: #2a6; color: white; padding: 6px 16px; "
        "border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #3b7; }"));
    connect(btn_export, &QPushButton::clicked, this, [this]() {
        const QString output_dir = recomp_path_input_->text().trimmed();
        if (output_dir.isEmpty()) {
            if (recomp_output_) {
                recomp_output_->append(QStringLiteral("[ERROR] No output directory specified."));
            }
            return;
        }
        const bool source_only = recomp_mode_ && recomp_mode_->currentIndex() == 1;
        if (recomp_output_) {
            recomp_output_->append(
                QStringLiteral("[INFO] Starting recompile export to: %1 (source_only=%2)")
                    .arg(output_dir)
                    .arg(source_only ? QStringLiteral("true") : QStringLiteral("false")));
        }
        emit ExportRecompiledSource(output_dir);
    });
    btn_layout->addWidget(btn_export);
    layout->addLayout(btn_layout);

    recomp_output_ = new QTextEdit();
    recomp_output_->setReadOnly(true);
    recomp_output_->setFont(QFont(QStringLiteral("Courier New"), 9));
    recomp_output_->setStyleSheet(
        QStringLiteral("background-color:#1e1e1e; color:#dcdcdc;"));
    recomp_output_->setPlaceholderText(
        QStringLiteral("Recompile output will appear here..."));
    layout->addWidget(recomp_output_, 1);

    widget->setLayout(layout);
    return widget;
}

void HackerEnvironment::Refresh() {
    RefreshProcesses();
    RefreshMemory();
    RefreshSystemInfo();
}
