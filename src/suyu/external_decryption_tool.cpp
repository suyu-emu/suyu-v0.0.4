// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "common/logging/log.h"
#include "suyu/external_decryption_tool.h"

// ============================================================================
// ExternalDecryptionTool — logic
// ============================================================================

ExternalDecryptionTool::ExternalDecryptionTool(QObject* parent) : QObject(parent) {
    QSettings settings;
    tool_id_ = settings.value(QStringLiteral("ExternalDecryption/ToolId")).toString();
    tool_path_ = settings.value(QStringLiteral("ExternalDecryption/ToolPath")).toString();
}

ExternalDecryptionTool::~ExternalDecryptionTool() = default;

std::vector<ExternalToolInfo> ExternalDecryptionTool::KnownTools() {
    return {
        {QStringLiteral("hactool"),
         QStringLiteral("hactool (SciresM)"),
         QStringLiteral("Command-line tool to view, decrypt, and extract Nintendo Switch "
                        "file formats (NCA, NSP, XCI). Requires prod.keys."),
         {QStringLiteral("hactool.exe"), QStringLiteral("hactool")}},
        {QStringLiteral("ryujinx"),
         QStringLiteral("Ryujinx / Ryubing"),
         QStringLiteral("Nintendo Switch emulator whose key derivation can be used for "
                        "decryption. Point to the Ryujinx executable."),
         {QStringLiteral("Ryujinx.exe"), QStringLiteral("Ryujinx"),
          QStringLiteral("Ryujinx.Headless.SDL2.exe"),
          QStringLiteral("Ryujinx.Headless.SDL2")}},
        {QStringLiteral("suyu"),
         QStringLiteral("suyu"),
         QStringLiteral("suyu emulator. Point to the eden executable; "
                        "suyu will invoke its CLI for decryption."),
         {QStringLiteral("suyu.exe"), QStringLiteral("suyu")}},
        {QStringLiteral("yuzu"),
         QStringLiteral("yuzu (legacy)"),
         QStringLiteral("Previous yuzu builds. Point to yuzu.exe or yuzu-cmd.exe. "
                        "Keys must be installed in yuzu's key directory."),
         {QStringLiteral("yuzu.exe"), QStringLiteral("yuzu-cmd.exe"),
          QStringLiteral("yuzu"), QStringLiteral("yuzu-cmd")}},
        {QStringLiteral("suyu_legacy"),
         QStringLiteral("suyu (previous build)"),
         QStringLiteral("An older suyu build. Point to suyu.exe or suyu-cmd.exe. "
                        "suyu will invoke it for decryption support."),
         {QStringLiteral("suyu.exe"), QStringLiteral("suyu-cmd.exe"),
          QStringLiteral("suyu"), QStringLiteral("suyu-cmd")}},
        {QStringLiteral("custom"),
         QStringLiteral("Custom tool"),
         QStringLiteral("Any hactool-compatible CLI tool. Must accept the same argument "
                        "format as hactool (--keyset, --exefsdir, --plaintext, etc.)."),
         {}},
    };
}

QString ExternalDecryptionTool::ConfiguredToolId() const {
    return tool_id_;
}

QString ExternalDecryptionTool::ConfiguredToolPath() const {
    return tool_path_;
}

bool ExternalDecryptionTool::IsToolConfigured() const {
    if (tool_id_.isEmpty() || tool_path_.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(tool_path_);
}

void ExternalDecryptionTool::SetTool(const QString& tool_id, const QString& exe_path) {
    tool_id_ = tool_id;
    tool_path_ = exe_path;

    QSettings settings;
    settings.setValue(QStringLiteral("ExternalDecryption/ToolId"), tool_id);
    settings.setValue(QStringLiteral("ExternalDecryption/ToolPath"), exe_path);

    emit ToolChanged(tool_id);
}

void ExternalDecryptionTool::ClearTool() {
    SetTool({}, {});
}

// ---------------------------------------------------------------------------
// Argument builders
// ---------------------------------------------------------------------------

QStringList ExternalDecryptionTool::BuildHactoolArgs(const QString& input,
                                                      const QString& output_dir,
                                                      const QString& keys_path,
                                                      const QString& operation) {
    // All supported tools use hactool-compatible CLI flags, or we adapt here.
    QStringList args;

    if (!keys_path.isEmpty()) {
        args << QStringLiteral("--keyset") << keys_path;
    }

    if (operation == QStringLiteral("decrypt_nca")) {
        args << QStringLiteral("--plaintext")
             << QDir(output_dir).filePath(QStringLiteral("decrypted.nca"));
        args << input;
    } else if (operation == QStringLiteral("extract_exefs")) {
        args << QStringLiteral("--exefsdir") << output_dir;
        args << input;
    } else if (operation == QStringLiteral("extract_romfs")) {
        args << QStringLiteral("--romfsdir") << output_dir;
        args << input;
    } else if (operation == QStringLiteral("decrypt_nsp")) {
        // For NSP: hactool treats it as PFS0
        args << QStringLiteral("-t") << QStringLiteral("pfs0");
        args << QStringLiteral("--outdir") << output_dir;
        args << input;
    } else if (operation == QStringLiteral("decrypt_xci")) {
        args << QStringLiteral("-t") << QStringLiteral("xci");
        args << QStringLiteral("--securedir") << output_dir;
        args << input;
    } else {
        // Default: just pass input and output dir
        args << QStringLiteral("--outdir") << output_dir;
        args << input;
    }

    return args;
}

// ---------------------------------------------------------------------------
// Tool invocation
// ---------------------------------------------------------------------------

bool ExternalDecryptionTool::RunTool(const QStringList& args, int timeout_ms) {
    if (!IsToolConfigured()) {
        last_error_ = QStringLiteral("No external decryption tool is configured.");
        return false;
    }

    LOG_INFO(Frontend, "Running external decryption tool: {} {}", tool_path_.toStdString(),
             args.join(QStringLiteral(" ")).toStdString());

    emit DecryptionProgress(QStringLiteral("Starting %1...").arg(tool_id_));

    QProcess process;
    process.setProgram(tool_path_);
    process.setArguments(args);
    process.start();

    if (!process.waitForStarted(10000)) {
        last_error_ = QStringLiteral("Failed to start tool: %1").arg(process.errorString());
        LOG_ERROR(Frontend, "{}", last_error_.toStdString());
        return false;
    }

    if (!process.waitForFinished(timeout_ms)) {
        last_error_ = QStringLiteral("Tool timed out after %1 ms").arg(timeout_ms);
        LOG_ERROR(Frontend, "{}", last_error_.toStdString());
        process.kill();
        return false;
    }

    if (process.exitCode() != 0) {
        const QString stderr_output = QString::fromUtf8(process.readAllStandardError());
        const QString stdout_output = QString::fromUtf8(process.readAllStandardOutput());
        last_error_ = QStringLiteral("Tool exited with code %1.\nstderr: %2\nstdout: %3")
                           .arg(process.exitCode())
                           .arg(stderr_output.left(2000))
                           .arg(stdout_output.left(2000));
        LOG_ERROR(Frontend, "{}", last_error_.toStdString());
        return false;
    }

    LOG_INFO(Frontend, "External decryption tool finished successfully.");
    return true;
}

bool ExternalDecryptionTool::DecryptNca(const QString& nca_path, const QString& output_dir,
                                         const QString& keys_path) {
    emit DecryptionStarted(nca_path);
    const auto args = BuildHactoolArgs(nca_path, output_dir, keys_path,
                                        QStringLiteral("decrypt_nca"));
    const bool ok = RunTool(args);
    emit DecryptionFinished(nca_path, ok);
    return ok;
}

bool ExternalDecryptionTool::ExtractExeFs(const QString& nca_path, const QString& output_dir,
                                           const QString& keys_path) {
    emit DecryptionStarted(nca_path);
    const auto args = BuildHactoolArgs(nca_path, output_dir, keys_path,
                                        QStringLiteral("extract_exefs"));
    const bool ok = RunTool(args);
    emit DecryptionFinished(nca_path, ok);
    return ok;
}

bool ExternalDecryptionTool::DecryptRom(const QString& rom_path, const QString& output_dir,
                                         const QString& keys_path) {
    emit DecryptionStarted(rom_path);

    // Determine operation based on file extension
    const QString ext = QFileInfo(rom_path).suffix().toLower();
    QString operation;
    if (ext == QStringLiteral("nsp")) {
        operation = QStringLiteral("decrypt_nsp");
    } else if (ext == QStringLiteral("xci")) {
        operation = QStringLiteral("decrypt_xci");
    } else if (ext == QStringLiteral("nca")) {
        operation = QStringLiteral("decrypt_nca");
    } else {
        // Try as generic
        operation = QStringLiteral("decrypt_nca");
    }

    const auto args = BuildHactoolArgs(rom_path, output_dir, keys_path, operation);
    const bool ok = RunTool(args);
    emit DecryptionFinished(rom_path, ok);
    return ok;
}

QString ExternalDecryptionTool::LastError() const {
    return last_error_;
}

// ============================================================================
// ExternalDecryptionToolDialog — UI
// ============================================================================

ExternalDecryptionToolDialog::ExternalDecryptionToolDialog(ExternalDecryptionTool* tool,
                                                            QWidget* parent)
    : QDialog(parent), tool_(tool) {
    setWindowTitle(QStringLiteral("Configure External Decryption Tool"));
    setMinimumWidth(520);
    SetupUi();
    RefreshStatus();
}

ExternalDecryptionToolDialog::~ExternalDecryptionToolDialog() = default;

void ExternalDecryptionToolDialog::SetupUi() {
    auto* main_layout = new QVBoxLayout(this);

    // Explanation
    auto* lbl_intro = new QLabel(this);
    lbl_intro->setWordWrap(true);
    lbl_intro->setText(QStringLiteral(
        "<p><b>suyu does not perform built-in decryption.</b></p>"
        "<p>If your games require decryption, you must configure an external tool. "
        "suyu will invoke the tool you select to handle decryption on your behalf.</p>"
        "<p>Supported tools: <b>hactool</b>, <b>Ryujinx</b>, <b>Eden</b>, "
        "<b>yuzu</b> (legacy), or a previous <b>suyu</b> build.</p>"));
    main_layout->addWidget(lbl_intro);

    // Tool selection group
    auto* group = new QGroupBox(QStringLiteral("External Tool"), this);
    auto* grid = new QGridLayout(group);

    grid->addWidget(new QLabel(QStringLiteral("Tool:"), group), 0, 0);
    combo_tool_ = new QComboBox(group);
    const auto tools = ExternalDecryptionTool::KnownTools();
    for (const auto& t : tools) {
        combo_tool_->addItem(t.display_name, t.id);
    }
    grid->addWidget(combo_tool_, 0, 1, 1, 2);

    lbl_description_ = new QLabel(group);
    lbl_description_->setWordWrap(true);
    lbl_description_->setStyleSheet(QStringLiteral("color: gray;"));
    grid->addWidget(lbl_description_, 1, 0, 1, 3);

    grid->addWidget(new QLabel(QStringLiteral("Executable:"), group), 2, 0);
    edit_path_ = new QLineEdit(group);
    edit_path_->setPlaceholderText(QStringLiteral("Path to the tool executable..."));
    grid->addWidget(edit_path_, 2, 1);

    btn_browse_ = new QPushButton(QStringLiteral("Browse..."), group);
    grid->addWidget(btn_browse_, 2, 2);

    btn_auto_detect_ = new QPushButton(QStringLiteral("Auto-Detect"), group);
    grid->addWidget(btn_auto_detect_, 3, 1);

    group->setLayout(grid);
    main_layout->addWidget(group);

    // Status
    lbl_status_ = new QLabel(this);
    lbl_status_->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(lbl_status_);

    // Buttons
    auto* btn_layout = new QHBoxLayout();
    btn_ok_ = new QPushButton(QStringLiteral("Save"), this);
    btn_cancel_ = new QPushButton(QStringLiteral("Cancel"), this);
    btn_layout->addStretch();
    btn_layout->addWidget(btn_ok_);
    btn_layout->addWidget(btn_cancel_);
    main_layout->addLayout(btn_layout);

    // Pre-fill current config
    const QString current_id = tool_->ConfiguredToolId();
    for (int i = 0; i < combo_tool_->count(); ++i) {
        if (combo_tool_->itemData(i).toString() == current_id) {
            combo_tool_->setCurrentIndex(i);
            break;
        }
    }
    edit_path_->setText(tool_->ConfiguredToolPath());

    // Connections
    connect(combo_tool_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExternalDecryptionToolDialog::OnToolSelected);
    connect(btn_browse_, &QPushButton::clicked,
            this, &ExternalDecryptionToolDialog::OnBrowse);
    connect(btn_auto_detect_, &QPushButton::clicked,
            this, &ExternalDecryptionToolDialog::OnAutoDetect);
    connect(btn_ok_, &QPushButton::clicked,
            this, &ExternalDecryptionToolDialog::OnAccept);
    connect(btn_cancel_, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(edit_path_, &QLineEdit::textChanged,
            this, [this](const QString&) { RefreshStatus(); });

    OnToolSelected(combo_tool_->currentIndex());
}

void ExternalDecryptionToolDialog::OnToolSelected(int index) {
    const auto tools = ExternalDecryptionTool::KnownTools();
    if (index >= 0 && index < static_cast<int>(tools.size())) {
        lbl_description_->setText(tools[index].description);
    }
    RefreshStatus();
}

void ExternalDecryptionToolDialog::OnBrowse() {
#ifdef _WIN32
    const QString filter = QStringLiteral("Executables (*.exe);;All files (*)");
#else
    const QString filter = QStringLiteral("All files (*)");
#endif
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Decryption Tool Executable"), {}, filter);
    if (!file.isEmpty()) {
        edit_path_->setText(file);
        RefreshStatus();
    }
}

void ExternalDecryptionToolDialog::OnAutoDetect() {
    const auto tools = ExternalDecryptionTool::KnownTools();
    const int idx = combo_tool_->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(tools.size())) {
        return;
    }

    const auto& info = tools[idx];

    // Search PATH and common install locations
    for (const auto& exe_name : info.exe_names) {
        const QString found = QStandardPaths::findExecutable(exe_name);
        if (!found.isEmpty()) {
            edit_path_->setText(found);
            RefreshStatus();
            QMessageBox::information(
                this, QStringLiteral("Tool Found"),
                QStringLiteral("Found %1 at:\n%2").arg(info.display_name, found));
            return;
        }
    }

    QMessageBox::warning(
        this, QStringLiteral("Not Found"),
        QStringLiteral("Could not find %1 in PATH.\nPlease browse manually.")
            .arg(info.display_name));
}

void ExternalDecryptionToolDialog::OnAccept() {
    const QString tool_id = combo_tool_->currentData().toString();
    const QString path = edit_path_->text().trimmed();

    if (path.isEmpty()) {
        // User wants to clear the tool
        tool_->ClearTool();
        accept();
        return;
    }

    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Invalid Path"),
                             QStringLiteral("The specified file does not exist."));
        return;
    }

    tool_->SetTool(tool_id, path);
    accept();
}

void ExternalDecryptionToolDialog::RefreshStatus() {
    const QString path = edit_path_->text().trimmed();
    if (path.isEmpty()) {
        lbl_status_->setText(QStringLiteral(
            "<span style='color:orange;'>No tool configured — decryption disabled</span>"));
    } else if (QFileInfo::exists(path)) {
        lbl_status_->setText(QStringLiteral(
            "<span style='color:green;font-size:12pt;'>✓ Tool found</span>"));
    } else {
        lbl_status_->setText(QStringLiteral(
            "<span style='color:red;'>✗ File not found</span>"));
    }
}
