// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "suyu/game_export.h"

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QCoreApplication>
#include <QPixmap>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QImage>
#include <QListWidget>
#include <QMessageBox>
#include <QCloseEvent>
#include <QEventLoop>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "common/assert.h"
#include "common/logging/log.h"
#include "dynarmic/common/fp/fpcr.h"
#include "dynarmic/frontend/A64/a64_location_descriptor.h"
#include "dynarmic/frontend/A64/translate/a64_translate.h"
#include "dynarmic/ir/basic_block.h"

#include "common/common_types.h"
#include "common/fs/path_util.h"
#include "common/hex_util.h"
#include "common/lz4_compression.h"
#include "common/swap.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/loader/loader.h"
#include "core/loader/nso.h"
#include "core/recompiler/arm64_to_c.h"

// ---------------------------------------------------------------------------
// Filesystem helpers
// ---------------------------------------------------------------------------

static bool CopyDirectoryRecursive(const QString& src, const QString& dst);

#ifdef _WIN32
// RT_ICON resources contain a DIB, not a PNG file. Build a 32-bit,
// bottom-up DIB with an opaque alpha mask so Explorer and the shell can read
// the icon after it has been attached to the copied launcher executable.
static QByteArray MakeIconDib(const QPixmap& pixmap) {
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);
    if (image.isNull() || image.width() != image.height()) {
        return {};
    }

    const int width = image.width();
    const int height = image.height();
    const qsizetype xor_size = static_cast<qsizetype>(width) * height * 4;
    const qsizetype and_stride = ((width + 31) / 32) * 4;
    const qsizetype and_size = and_stride * height;
    QByteArray dib(static_cast<qsizetype>(sizeof(BITMAPINFOHEADER)) + xor_size + and_size,
                   Qt::Uninitialized);

    BITMAPINFOHEADER header{};
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = width;
    header.biHeight = height * 2;
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(xor_size + and_size);
    std::memcpy(dib.data(), &header, sizeof(header));

    auto* pixels = reinterpret_cast<BYTE*>(dib.data() + sizeof(header));
    for (int y = 0; y < height; ++y) {
        const auto* source = image.constScanLine(height - 1 - y);
        auto* destination = pixels + static_cast<qsizetype>(y) * width * 4;
        for (int x = 0; x < width; ++x) {
            const auto* rgba = source + x * 4;
            destination[x * 4 + 0] = rgba[2];
            destination[x * 4 + 1] = rgba[1];
            destination[x * 4 + 2] = rgba[0];
            destination[x * 4 + 3] = rgba[3];
        }
    }
    // A zeroed AND mask means every pixel is taken from the 32-bit XOR image.
    return dib;
}
#endif

static bool CopyFileReplacingExisting(const QString& src, const QString& dst) {
    const QFileInfo src_info(src);
    if (!src_info.exists() || !src_info.isFile()) {
        return false;
    }

    const QFileInfo dst_info(dst);
    if (!QDir().mkpath(dst_info.absolutePath())) {
        return false;
    }

    // An unchanged copy is left alone. The bundled payload is the ROM itself -
    // 18.5 GB for Smash Ultimate - and re-copying it on every export dominates
    // the run for no gain when the same file is already sitting there.
    if (dst_info.exists() && dst_info.isFile() && dst_info.size() == src_info.size() &&
        dst_info.lastModified() >= src_info.lastModified()) {
        return true;
    }

    if (QFile::exists(dst) && !QFile::remove(dst)) {
        return false;
    }

    return QFile::copy(src, dst);
}

// True when both paths name the same location on disk, so a "copy" between
// them would be a no-op at best.
static bool IsSamePath(const QString& a, const QString& b) {
    return QDir::cleanPath(QFileInfo(a).absoluteFilePath()).compare(
               QDir::cleanPath(QFileInfo(b).absoluteFilePath()), Qt::CaseInsensitive) == 0;
}

// Copy a directory unless it is already in place.
//
// The AOT cache is now generated straight into the package, so the packaging
// step finds source and destination identical. Copying it anyway meant
// duplicating gigabytes of generated C - Smash Ultimate's cache is about 6 GB -
// onto itself, which is where exports were dying.
static bool CopyDirectoryUnlessInPlace(const QString& src, const QString& dst) {
    if (IsSamePath(src, dst)) {
        return QDir(src).exists();
    }
    return CopyDirectoryRecursive(src, dst);
}

static bool CopyDirectoryRecursive(const QString& src, const QString& dst) {
    QDir src_dir(src);
    if (!src_dir.exists()) {
        return false;
    }
    if (!QDir().mkpath(dst)) {
        return false;
    }

    for (const QFileInfo& entry : src_dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        const QString dest_file = dst + QDir::separator() + entry.fileName();
        if (!CopyFileReplacingExisting(entry.absoluteFilePath(), dest_file)) {
            return false;
        }
    }

    for (const QFileInfo& entry : src_dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!CopyDirectoryRecursive(entry.absoluteFilePath(),
                                    dst + QDir::separator() + entry.fileName())) {
            return false;
        }
    }

    return true;
}

static bool CopyTitleSaveData(const QString& save_root, const QString& dst_root, quint64 title_id) {
    QDir root_dir(save_root);
    if (!root_dir.exists()) {
        return true;
    }

    const QString title_id_hex = QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char('0')).toUpper();

    QDirIterator it(save_root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileName().compare(title_id_hex, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QString relative = QDir(save_root).relativeFilePath(it.filePath());
        const QString target_dir = dst_root + QDir::separator() + relative;
        if (!CopyDirectoryRecursive(it.filePath(), target_dir)) {
            return false;
        }
    }

    return true;
}

static bool CopyPortableSupportData(quint64 program_id, const QString& package_root,
                                      bool include_save, bool include_shader,
                                      bool include_config) {
    const QString title_id_hex = QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
    const QString output_user_root = package_root + QDir::separator() + QStringLiteral("user");

    if (!QDir().mkpath(output_user_root)) {
        return false;
    }

    if (include_save) {
        const QString nand_save_root = QString::fromStdString(
            Common::FS::PathToUTF8String(Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir))) +
            QDir::separator() + QStringLiteral("user") + QDir::separator() + QStringLiteral("save");
        const QString output_nand_root = output_user_root + QDir::separator() + QStringLiteral("nand") +
                                         QDir::separator() + QStringLiteral("user") +
                                         QDir::separator() + QStringLiteral("save");
        if (!CopyTitleSaveData(nand_save_root, output_nand_root, program_id)) {
            return false;
        }
    }

    if (include_config) {
        const QString config_file_name = QStringLiteral("%1.ini").arg(program_id, 16, 16, QLatin1Char('0')).toUpper();
        const QString config_src = QString::fromStdString(
            Common::FS::PathToUTF8String(Common::FS::GetSuyuPath(Common::FS::SuyuPath::ConfigDir))) +
            QDir::separator() + QStringLiteral("custom") + QDir::separator() + config_file_name;
        const QString config_dst_dir = output_user_root + QDir::separator() + QStringLiteral("config") +
                                       QDir::separator() + QStringLiteral("custom");
        if (QFile::exists(config_src)) {
            if (!QDir().mkpath(config_dst_dir)) {
                return false;
            }
            const QString config_dst = config_dst_dir + QDir::separator() + config_file_name;
            if (!CopyFileReplacingExisting(config_src, config_dst)) {
                return false;
            }
        }
    }

    if (include_shader) {
        const QString shader_src = QString::fromStdString(
            Common::FS::PathToUTF8String(Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir))) +
            QDir::separator() + title_id_hex;
        const QString shader_dst = output_user_root + QDir::separator() + QStringLiteral("shader") +
                                   QDir::separator() + title_id_hex;
        if (QDir(shader_src).exists()) {
            if (!CopyDirectoryRecursive(shader_src, shader_dst)) {
                return false;
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Dialog setup
// ---------------------------------------------------------------------------

GameExportDialog::GameExportDialog(Core::System& system, QWidget* parent)
    : QDialog(parent), system_(system) {
    setWindowTitle(tr("Export Game — AOT Static Recompilation"));
    setMinimumSize(540, 420);
    SetupUi();
}

void GameExportDialog::SetupUi() {
    auto* layout = new QVBoxLayout(this);

    // ROM path row
    auto* rom_row = new QHBoxLayout();
    rom_row->addWidget(new QLabel(tr("ROM:"), this));
    rom_path_edit = new QLineEdit(this);
    rom_path_edit->setReadOnly(false);
    rom_path_edit->setPlaceholderText(tr("Select a ROM file (.nsp, .xci, .nca, .nro) ..."));
    rom_row->addWidget(rom_path_edit);
    auto* rom_library_btn = new QPushButton(tr("From Library..."), this);
    rom_row->addWidget(rom_library_btn);
    auto* rom_browse_btn = new QPushButton(tr("Browse..."), this);
    rom_row->addWidget(rom_browse_btn);
    layout->addLayout(rom_row);

    // Output path row
    auto* out_row = new QHBoxLayout();
    out_row->addWidget(new QLabel(tr("Output:"), this));
    output_path_edit = new QLineEdit(this);
    output_path_edit->setPlaceholderText(tr("Select output directory..."));
    out_row->addWidget(output_path_edit);
    auto* browse_btn = new QPushButton(tr("Browse..."), this);
    out_row->addWidget(browse_btn);
    layout->addLayout(out_row);

    const QString default_output = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!default_output.isEmpty()) {
        output_path_edit->setText(default_output);
    }

    // Target platform
    auto* plat_row = new QHBoxLayout();
    plat_row->addWidget(new QLabel(tr("Target:"), this));
    platform_combo = new QComboBox(this);
    platform_combo->addItem(tr("Windows artifact bundle"), static_cast<int>(TargetPlatform::Windows));
    platform_combo->addItem(tr("Linux artifact bundle"), static_cast<int>(TargetPlatform::Linux));
    platform_combo->addItem(tr("macOS artifact bundle"), static_cast<int>(TargetPlatform::MacOS));
#ifdef _WIN32
    platform_combo->setCurrentIndex(0);
#elif defined(__APPLE__)
    platform_combo->setCurrentIndex(2);
#else
    platform_combo->setCurrentIndex(1);
#endif
    plat_row->addWidget(platform_combo);
    layout->addLayout(plat_row);

    // Recompiler backend
    auto* backend_row = new QHBoxLayout();
    backend_row->addWidget(new QLabel(tr("AOT Backend:"), this));
    backend_combo = new QComboBox(this);
    backend_combo->addItem(tr("Dynarmic (stable)"),
                           static_cast<int>(RecompileBackend::Dynarmic));
    backend_combo->addItem(tr("Ballistic (WIP)"),
                           static_cast<int>(RecompileBackend::Ballistic));
    backend_combo->setCurrentIndex(0);
    backend_row->addWidget(backend_combo);
    layout->addLayout(backend_row);

    // AOT options
    aot_full_scan_checkbox = new QCheckBox(
        tr("Full code scan (slower — pre-compiles more blocks for better cold-start)"), this);
    aot_full_scan_checkbox->setChecked(false);
    layout->addWidget(aot_full_scan_checkbox);

    fallback_to_interpreter_checkbox = new QCheckBox(
        tr("Fall back to interpreter if a module fails to recompile"), this);
    fallback_to_interpreter_checkbox->setChecked(true);
    fallback_to_interpreter_checkbox->setToolTip(
        tr("When checked: if a module cannot be recompiled (e.g. too complex, "
           "unsupported instructions), the export continues and that module will "
           "use the dynarmic JIT at runtime instead of the static recompiled code. "
           "When unchecked: any recompile failure aborts the entire export."));
    layout->addWidget(fallback_to_interpreter_checkbox);

    // Source vs Build is a real, explicit choice rather than an easily-missed
    // checkbox, because the two produce completely different deliverables and
    // "I picked build and got a folder of C" was the reported complaint. Source
    // stays the default: a large title lifts to gigabytes of C - Smash
    // Ultimate's main module alone is ~3 GB across 139 translation units - and
    // compiling that is hours of C-compiler work, so it must be asked for, not
    // stumbled into. When Build IS chosen the export compiles all the way to a
    // binary and fails loudly if it cannot, instead of silently degrading.
    auto* format_row = new QHBoxLayout();
    format_row->addWidget(new QLabel(tr("Export Format:"), this));
    output_format_combo = new QComboBox(this);
    output_format_combo->addItem(
        tr("Source — generate C project only (fast, compile it yourself)"));
    output_format_combo->addItem(
        tr("Build — compile to a standalone executable (slow, needs CMake + a C compiler)"));
    output_format_combo->setToolTip(
        tr("Source writes the recompiled C plus a CMakeLists.txt and a build script, and stops "
           "there. Build additionally runs CMake to completion, producing the standalone "
           "'recompiled' executable and the shared library suyu loads to run the game on its own "
           "recompiler. Build can take hours on large titles; the window stays responsive while "
           "it works."));
    output_format_combo->setCurrentIndex(0);
    format_row->addWidget(output_format_combo, 1);
    layout->addLayout(format_row);

    include_save_data_checkbox = new QCheckBox(tr("Include save data for this game"), this);
    include_save_data_checkbox->setChecked(true);
    layout->addWidget(include_save_data_checkbox);

    include_shader_cache_checkbox = new QCheckBox(tr("Include transferable shader cache"), this);
    include_shader_cache_checkbox->setChecked(true);
    layout->addWidget(include_shader_cache_checkbox);

    include_custom_config_checkbox = new QCheckBox(tr("Include custom game configuration"), this);
    include_custom_config_checkbox->setChecked(true);
    layout->addWidget(include_custom_config_checkbox);

    auto* note_label = new QLabel(
          tr("Translates the game's ARM64 code into C. Output mirrors the ROM structure: "
             "exefs/ holds one C project per module (main, rtld, sdk, ...). "
             "Build has suyu compile it for you (slow on large titles). "
             "Source gives you the C + CMakeLists.txt to compile yourself. "
             "With fallback enabled, modules that fail recompile use the dynarmic JIT at runtime."),
        this);
    note_label->setWordWrap(true);
    note_label->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(note_label);

    layout->addStretch();

    // Progress
    progress_bar = new QProgressBar(this);
    progress_bar->setRange(0, 100);
    progress_bar->setValue(0);
    progress_bar->setVisible(false);
    layout->addWidget(progress_bar);

    status_label = new QLabel(this);
    status_label->setStyleSheet(QStringLiteral("color: #888;"));
    layout->addWidget(status_label);

    // Export button
    export_button = new QPushButton(tr("Export Game"), this);
    layout->addWidget(export_button);

    connect(rom_library_btn, &QPushButton::clicked, this, &GameExportDialog::OnSelectFromLibrary);
    connect(rom_browse_btn, &QPushButton::clicked, this, &GameExportDialog::OnBrowseRom);
    connect(browse_btn, &QPushButton::clicked, this, &GameExportDialog::OnBrowseOutput);
    connect(export_button, &QPushButton::clicked, this, &GameExportDialog::OnExport);
}

void GameExportDialog::SetLibraryEntries(QVector<LibraryEntry> entries) {
    library_entries_ = std::move(entries);
}

void GameExportDialog::SetGameIcon(const QPixmap& icon) {
    game_icon_ = icon;
}

void GameExportDialog::SetRomPath(const QString& path, quint64 program_id) {
    rom_path_edit->setText(path);
    rom_program_id = program_id;

    const bool allow_portable_data = program_id != 0;
    include_save_data_checkbox->setEnabled(allow_portable_data);
    include_shader_cache_checkbox->setEnabled(allow_portable_data);
    include_custom_config_checkbox->setEnabled(allow_portable_data);

    if (!allow_portable_data) {
        include_save_data_checkbox->setChecked(false);
        include_shader_cache_checkbox->setChecked(false);
        include_custom_config_checkbox->setChecked(false);
    }
}

void GameExportDialog::TriggerExportForTesting(const QString& rom_path, const QString& output_dir,
                                               int format_index) {
    if (format_index >= 0 && output_format_combo &&
        format_index < output_format_combo->count()) {
        output_format_combo->setCurrentIndex(format_index);
    }
    SetRomPath(rom_path);
    output_path_edit->setText(output_dir);
    OnExport();
}

void GameExportDialog::OnBrowseRom() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select ROM"), QString(),
        tr("Switch game files (*.nsp *.xci *.nca *.nro);;All files (*.*)"), nullptr,
        QFileDialog::ReadOnly | QFileDialog::DontUseNativeDialog);
    if (!file.isEmpty()) {
        rom_path_edit->setText(file);
        rom_program_id = 0;
        include_save_data_checkbox->setChecked(false);
        include_shader_cache_checkbox->setChecked(false);
        include_custom_config_checkbox->setChecked(false);
        include_save_data_checkbox->setEnabled(false);
        include_shader_cache_checkbox->setEnabled(false);
        include_custom_config_checkbox->setEnabled(false);
    }
}

void GameExportDialog::OnSelectFromLibrary() {
    if (library_entries_.isEmpty()) {
        QMessageBox::information(this, tr("Library"),
                                 tr("No launchable local games were found in your library."));
        return;
    }

    QDialog chooser(this);
    chooser.setWindowTitle(tr("Select Game from Library"));
    chooser.resize(680, 420);

    auto* layout = new QVBoxLayout(&chooser);
    auto* list = new QListWidget(&chooser);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setAlternatingRowColors(true);

    for (const auto& entry : library_entries_) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(entry.title, entry.path), list);
        item->setData(Qt::UserRole, entry.path);
        item->setData(Qt::UserRole + 1, QVariant::fromValue<qulonglong>(entry.program_id));
        item->setToolTip(entry.path);
    }

    if (list->count() > 0) {
        list->setCurrentRow(0);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         Qt::Horizontal, &chooser);
    connect(buttons, &QDialogButtonBox::accepted, &chooser, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &chooser, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &chooser, [&chooser](QListWidgetItem*) {
        chooser.accept();
    });

    layout->addWidget(list);
    layout->addWidget(buttons);

    if (chooser.exec() != QDialog::Accepted || !list->currentItem()) {
        return;
    }

    const QString selected_path = list->currentItem()->data(Qt::UserRole).toString();
    const quint64 selected_program_id =
        static_cast<quint64>(list->currentItem()->data(Qt::UserRole + 1).toULongLong());
    SetRomPath(selected_path, selected_program_id);

    for (const auto& entry : library_entries_) {
        if (entry.path == selected_path && !entry.icon.isNull()) {
            SetGameIcon(entry.icon);
            break;
        }
    }
}

void GameExportDialog::OnBrowseOutput() {
    const QString start_dir = output_path_edit->text().isEmpty() ? QString() : output_path_edit->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), start_dir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
    if (!dir.isEmpty()) {
        output_path_edit->setText(dir);
    }
}

// ---------------------------------------------------------------------------
// ARM64 basic block analysis
// ---------------------------------------------------------------------------

namespace {

/// Represents a discovered ARM64 basic block in the NSO .text segment.
struct Arm64BasicBlock {
    u32 vaddr;       ///< Virtual address offset within the segment
    u32 size;        ///< Block size in bytes
    u32 instruction_count;
    bool is_entry;   ///< Whether this is the segment entry point
};

/// Classify ARM64 instructions to detect basic block boundaries.
/// Returns true if the instruction is a block-terminating branch/system call.
static bool IsBlockTerminator(u32 insn) {
    // B  (unconditional branch immediate)
    if ((insn & 0xFC000000) == 0x14000000) return true;
    // BL (branch with link — call, but still ends the block)
    if ((insn & 0xFC000000) == 0x94000000) return true;
    // BR (branch register — indirect jump)
    if ((insn & 0xFFFFFC1F) == 0xD61F0000) return true;
    // BLR (branch with link register)
    if ((insn & 0xFFFFFC1F) == 0xD63F0000) return true;
    // RET
    if ((insn & 0xFFFFFC1F) == 0xD65F0000) return true;
    // CBZ
    if ((insn & 0x7F000000) == 0x34000000) return true;
    // CBNZ
    if ((insn & 0x7F000000) == 0x35000000) return true;
    // TBZ
    if ((insn & 0x7F000000) == 0x36000000) return true;
    // TBNZ
    if ((insn & 0x7F000000) == 0x37000000) return true;
    // B.cond (conditional branch)
    if ((insn & 0xFF000010) == 0x54000000) return true;
    // SVC (supervisor call — system call boundary)
    if ((insn & 0xFFE0001F) == 0xD4000001) return true;
    return false;
}

/// Returns true if the instruction is a direct branch (B or BL) and extracts the target offset.
static bool GetDirectBranchTarget(u32 insn, u32 pc, u32& target_out) {
    // B: imm26 is a signed offset in instructions
    if ((insn & 0xFC000000) == 0x14000000) {
        s32 imm26 = static_cast<s32>(insn << 6) >> 6; // sign-extend 26 bits
        target_out = pc + static_cast<u32>(imm26 * 4);
        return true;
    }
    // BL: same encoding
    if ((insn & 0xFC000000) == 0x94000000) {
        s32 imm26 = static_cast<s32>(insn << 6) >> 6;
        target_out = pc + static_cast<u32>(imm26 * 4);
        return true;
    }
    return false;
}

/// Perform a linear sweep over ARM64 .text to identify basic blocks.
/// This discovers block boundaries by looking for branch instructions and branch targets.
static std::vector<Arm64BasicBlock> AnalyzeArm64BasicBlocks(std::span<const u8> text_data,
                                                             u32 base_vaddr, bool full_scan) {
    if (text_data.size() < 4) {
        return {};
    }

    const u32 num_instructions = static_cast<u32>(text_data.size() / 4);
    const u32* insn_ptr = reinterpret_cast<const u32*>(text_data.data());

    // First pass: identify all branch targets so we know where blocks start
    std::vector<bool> is_block_start(num_instructions, false);
    is_block_start[0] = true; // Entry point of the segment

    for (u32 i = 0; i < num_instructions; ++i) {
        u32 insn = insn_ptr[i];
        u32 pc = base_vaddr + i * 4;

        if (IsBlockTerminator(insn)) {
            // The instruction after a terminator starts a new block
            if (i + 1 < num_instructions) {
                is_block_start[i + 1] = true;
            }

            // If it's a direct branch, the target also starts a block
            u32 target = 0;
            if (GetDirectBranchTarget(insn, pc, target)) {
                u32 target_index = (target - base_vaddr) / 4;
                if (target_index < num_instructions) {
                    is_block_start[target_index] = true;
                }
            }
        }
    }

    // Second pass: build block list from boundaries
    std::vector<Arm64BasicBlock> blocks;
    blocks.reserve(num_instructions / 8); // Heuristic: average 8 instructions per block

    u32 block_start_idx = 0;
    for (u32 i = 1; i <= num_instructions; ++i) {
        if (i == num_instructions || is_block_start[i]) {
            Arm64BasicBlock block{};
            block.vaddr = base_vaddr + block_start_idx * 4;
            block.size = (i - block_start_idx) * 4;
            block.instruction_count = i - block_start_idx;
            block.is_entry = (block_start_idx == 0);
            blocks.push_back(block);
            block_start_idx = i;
        }
    }

    return blocks;
}

/// Compute a hex string from a build ID array.
static QString BuildIdToHex(const std::array<u8, 0x20>& build_id) {
    QString hex;
    hex.reserve(0x40);
    for (u8 byte : build_id) {
        hex.append(QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')));
    }
    return hex;
}

/// Information extracted from a single NSO module.
struct NsoAnalysisResult {
    QString name;
    QString build_id_hex;
    u32 text_vaddr{};
    u32 text_size{};
    u32 rodata_vaddr{};
    u32 rodata_size{};
    u32 data_vaddr{};
    u32 data_size{};
    u32 total_blocks{};
    u32 total_instructions{};
    /// Guest address of the first real instruction. An NSO's .text does not
    /// start with code, so this is not simply text_vaddr.
    u64 entry_vaddr{};
    std::vector<u8> text_bytes;
    std::vector<u8> rodata_bytes;
    std::vector<u8> data_bytes;
    std::vector<Arm64BasicBlock> blocks;
};

/// Offset of the first real instruction within a decompressed .text.
///
/// An NSO's .text opens with a branch word, then the MOD0 header the offset at
/// +4 points at, then zero padding - none of which is code. Starting a
/// recompiled image at .text+0 therefore begins mid-header, and because
/// nothing branches to the true entry it never becomes a block start either.
static u32 FindNsoEntryOffset(std::span<const u8> text) {
    if (text.size() < 0x10) {
        return 0;
    }
    u32 mod0_off = 0;
    std::memcpy(&mod0_off, text.data() + 4, sizeof(mod0_off));

    // MOD0 itself is 0x1C bytes; walk past it and then over the padding to the
    // first non-zero word.
    size_t off = (static_cast<size_t>(mod0_off) + 0x1C + 3) & ~size_t{3};
    if (off >= text.size()) {
        return 0;
    }
    while (off + 4 <= text.size()) {
        u32 word = 0;
        std::memcpy(&word, text.data() + off, sizeof(word));
        if (word != 0) {
            return static_cast<u32>(off);
        }
        off += 4;
    }
    return 0;
}

/// Reads bytes at a module-relative virtual address out of whichever of the
/// three decompressed segments actually contains it. .dynamic/.dynsym/.dynstr
/// can live in text or rodata depending on toolchain, so callers walking them
/// need one accessor spanning all three rather than assuming a segment.
static bool ReadModuleBytes(const NsoAnalysisResult& mod, u64 vaddr, u8* out, size_t len) {
    auto try_seg = [&](u64 seg_vaddr, const std::vector<u8>& bytes) {
        if (vaddr < seg_vaddr) return false;
        const u64 off = vaddr - seg_vaddr;
        if (off + len > bytes.size()) return false;
        std::memcpy(out, bytes.data() + off, len);
        return true;
    };
    return try_seg(mod.text_vaddr, mod.text_bytes) ||
           try_seg(mod.rodata_vaddr, mod.rodata_bytes) ||
           try_seg(mod.data_vaddr, mod.data_bytes);
}

static u32 ReadModuleU32(const NsoAnalysisResult& mod, u64 vaddr) {
    u32 v = 0;
    ReadModuleBytes(mod, vaddr, reinterpret_cast<u8*>(&v), sizeof(v));
    return v;
}

static u64 ReadModuleU64(const NsoAnalysisResult& mod, u64 vaddr) {
    u64 v = 0;
    ReadModuleBytes(mod, vaddr, reinterpret_cast<u8*>(&v), sizeof(v));
    return v;
}

/// Collects every defined dynsym symbol's address from a module's .dynamic
/// section, mirroring ArmRecomp::Impl::ParseDynamic/IndexExports at runtime
/// (src/core/arm/recomp/arm_recomp.cpp) but reading from the decompressed
/// export-time buffers rather than live guest memory. These become extra
/// block-discovery roots: a function reached only via another module's
/// resolved GOT/PLT entry (e.g. nn::init::Start) never gets a direct branch
/// inside its own module, so without this its real entry address can fall
/// mid-block - behind alignment padding after the previous function's return
/// - and the runtime dispatcher can never resolve a call landing exactly on it.
/// Conservatively scans .rodata and .data for 8-byte-aligned values that look
/// like a pointer into this module's own .text - vtables, HIPC/service
/// dispatch tables, and other function-pointer tables the SDK builds at
/// static-init time, all of which reach their target only through an
/// indirect call (BLR) with no direct branch anywhere in .text pointing at
/// it. Exported dynsym symbols (CollectExportedSymbolAddresses) don't cover
/// these - a table entry like this is never exported, it's purely an
/// implementation detail the module reads out of its own data section - so
/// without also seeding roots from here, any function reached exclusively
/// through such a table falls mid-block (typically right behind whichever
/// neighboring function's RET happened to end the previous block) and the
/// runtime dispatcher can never resolve a BLR landing exactly on its real
/// entry address ("No recompiled block at PC").
///
/// This is deliberately over-inclusive: ordinary data that happens to look
/// like a text address becomes a spurious extra block boundary, which only
/// costs a slightly smaller block - never a wrong one - so there's no need to
/// separate real function-pointer tables from incidental matches.
static std::vector<u64> ScanDataForCodePointers(const NsoAnalysisResult& mod) {
    std::vector<u64> out;
    const u64 text_lo = mod.text_vaddr;
    const u64 text_hi = mod.text_vaddr + mod.text_bytes.size();
    const auto scan = [&](const std::vector<u8>& bytes) {
        if (bytes.size() < 8) return;
        for (size_t off = 0; off + 8 <= bytes.size(); off += 8) {
            u64 v = 0;
            std::memcpy(&v, bytes.data() + off, sizeof(v));
            if (v >= text_lo && v < text_hi && (v & 3) == 0) {
                out.push_back(v);
            }
        }
    };
    scan(mod.rodata_bytes);
    scan(mod.data_bytes);
    return out;
}

/// Function-pointer roots recovered from the module's relocation tables.
///
/// ScanDataForCodePointers reads .rodata/.data as they sit in the file, which
/// is *before* relocation: a vtable slot or static function-pointer table entry
/// is written by the linker as a relocation, and the slot itself usually holds
/// nothing (or a bare addend). The real target only exists in the RELA entry -
/// R_AARCH64_RELATIVE carries it in the addend, ABS64 in symbol value + addend.
/// So the addresses reached exclusively through those tables - virtual methods,
/// service dispatch entries, and every function handed to the OS as a callback,
/// thread entry points above all - are invisible to a raw data scan.
///
/// Missing them is what leaves a brand-new guest thread starting at an address
/// no block covers, so every thread the game spawns drops straight to the
/// interpreter. Seeding them here keeps that work in statically recompiled
/// code. Over-inclusive on purpose: a value that merely looks like a text
/// address only ever costs one extra block boundary, never a wrong block.
static std::vector<u64> ScanRelocationsForCodePointers(const NsoAnalysisResult& mod) {
    std::vector<u64> out;
    if (mod.text_bytes.size() < 8) return out;

    u32 mod0_off = 0;
    std::memcpy(&mod0_off, mod.text_bytes.data() + 4, sizeof(mod0_off));
    const u64 mod0_va = mod.text_vaddr + mod0_off;
    if (ReadModuleU32(mod, mod0_va) != 0x30444F4Du) return out;

    const u32 dyn_rel_off = ReadModuleU32(mod, mod0_va + 4);
    const u64 dyn_va = mod0_va + dyn_rel_off;

    constexpr u32 DT_NULL = 0, DT_PLTRELSZ = 2, DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8,
                   DT_JMPREL = 23;
    u64 rela_va = 0, rela_sz = 0, jmprel_va = 0, jmprel_sz = 0, symtab_va = 0;
    for (u64 p = dyn_va, guard = 0; guard < 0x1000; p += 16, guard += 16) {
        const u64 tag = ReadModuleU64(mod, p);
        const u64 val = ReadModuleU64(mod, p + 8);
        if (tag == DT_NULL) break;
        if (tag == DT_RELA) rela_va = val;
        if (tag == DT_RELASZ) rela_sz = val;
        if (tag == DT_JMPREL) jmprel_va = val;
        if (tag == DT_PLTRELSZ) jmprel_sz = val;
        if (tag == DT_SYMTAB) symtab_va = val;
    }

    const u64 text_lo = mod.text_vaddr;
    const u64 text_hi = mod.text_vaddr + mod.text_bytes.size();
    constexpr u32 R_AARCH64_ABS64 = 0x101, R_AARCH64_GLOB_DAT = 0x401,
                   R_AARCH64_JUMP_SLOT = 0x402, R_AARCH64_RELATIVE = 0x403;
    const auto scan_table = [&](u64 table_va, u64 table_sz) {
        for (u64 p = table_va; p + 24 <= table_va + table_sz; p += 24) {
            const u64 r_info = ReadModuleU64(mod, p + 8);
            const u64 r_addend = ReadModuleU64(mod, p + 16);
            const u32 r_type = static_cast<u32>(r_info & 0xFFFFFFFF);
            const u32 r_sym = static_cast<u32>(r_info >> 32);
            u64 target = 0;
            if (r_type == R_AARCH64_RELATIVE) {
                target = r_addend;
            } else if ((r_type == R_AARCH64_ABS64 || r_type == R_AARCH64_GLOB_DAT ||
                        r_type == R_AARCH64_JUMP_SLOT) &&
                       symtab_va != 0 && r_sym != 0) {
                target = ReadModuleU64(mod, symtab_va + static_cast<u64>(r_sym) * 24 + 8) +
                         (r_type == R_AARCH64_ABS64 ? r_addend : 0);
            } else {
                continue;
            }
            if (target >= text_lo && target < text_hi && (target & 3) == 0) {
                out.push_back(target);
            }
        }
    };
    if (rela_va && rela_sz) scan_table(rela_va, rela_sz);
    if (jmprel_va && jmprel_sz) scan_table(jmprel_va, jmprel_sz);
    return out;
}

static std::vector<u64> CollectExportedSymbolAddresses(const NsoAnalysisResult& mod) {
    std::vector<u64> out;
    if (mod.text_bytes.size() < 8) return out;

    // MOD0 offset is stored at text+4, relative to the start of .text (same
    // field FindNsoEntryOffset walks past to find the real entry point).
    u32 mod0_off = 0;
    std::memcpy(&mod0_off, mod.text_bytes.data() + 4, sizeof(mod0_off));
    const u64 mod0_va = mod.text_vaddr + mod0_off;
    if (ReadModuleU32(mod, mod0_va) != 0x30444F4Du) return out; // "MOD0"

    const u32 dyn_rel_off = ReadModuleU32(mod, mod0_va + 4);
    const u64 dyn_va = mod0_va + dyn_rel_off;

    constexpr u32 DT_NULL = 0, DT_STRTAB = 5, DT_SYMTAB = 6;
    u64 symtab_va = 0, strtab_va = 0;
    for (u64 p = dyn_va, guard = 0; guard < 0x1000; p += 16, guard += 16) {
        const u64 tag = ReadModuleU64(mod, p);
        const u64 val = ReadModuleU64(mod, p + 8);
        if (tag == DT_NULL) break;
        if (tag == DT_SYMTAB) symtab_va = val;
        if (tag == DT_STRTAB) strtab_va = val;
    }
    if (!symtab_va || !strtab_va || strtab_va <= symtab_va) return out;

    // .dynsym/.dynstr are laid out back to back, so the gap between them
    // bounds the entry count (no explicit count exists for a plain DT_SYMTAB).
    const u64 span = strtab_va - symtab_va;
    const u32 max_index = static_cast<u32>(std::min<u64>(span / 24, 65536));
    for (u32 i = 1; i < max_index; ++i) { // index 0 is always the null symbol
        const u64 sym_va = symtab_va + static_cast<u64>(i) * 24;
        const u16 shndx = static_cast<u16>(ReadModuleU32(mod, sym_va + 6) & 0xFFFF);
        if (shndx == 0) continue; // SHN_UNDEF - an import, not an export
        const u64 value = ReadModuleU64(mod, sym_va + 8);
        if (value) out.push_back(value);
    }
    return out;
}

/// Parse and analyze a single NSO file using the VFS.
static std::optional<NsoAnalysisResult> AnalyzeNsoFile(const FileSys::VirtualFile& nso_file,
                                                        bool full_scan) {
    if (!nso_file || nso_file->GetSize() < sizeof(Loader::NSOHeader)) {
        return std::nullopt;
    }

    Loader::NSOHeader header{};
    if (nso_file->ReadObject(&header) != sizeof(Loader::NSOHeader)) {
        return std::nullopt;
    }

    if (header.magic != Common::MakeMagic('N', 'S', 'O', '0')) {
        return std::nullopt;
    }

    NsoAnalysisResult result{};
    result.name = QString::fromStdString(nso_file->GetName());
    result.build_id_hex = BuildIdToHex(header.build_id);

    // Extract segment metadata
    result.text_vaddr = header.segments[0].location;
    result.text_size = header.segments[0].size;
    result.rodata_vaddr = header.segments[1].location;
    result.rodata_size = header.segments[1].size;
    result.data_vaddr = header.segments[2].location;
    result.data_size = header.segments[2].size;

    // Read and decompress .text segment (segment 0)
    std::vector<u8> text_data = nso_file->ReadBytes(
        header.segments_compressed_size[0], header.segments[0].offset);

    if (text_data.empty()) {
        return std::nullopt;
    }

    if (header.IsSegmentCompressed(0)) {
        text_data = Common::Compression::DecompressDataLZ4(text_data, header.segments[0].size);
        if (text_data.empty()) {
            return std::nullopt;
        }
    }

    result.text_bytes = text_data;
    result.entry_vaddr = static_cast<u64>(result.text_vaddr) + FindNsoEntryOffset(result.text_bytes);

    // Read and decompress .rodata segment (segment 1)
    {
        std::vector<u8> seg = nso_file->ReadBytes(
            header.segments_compressed_size[1], header.segments[1].offset);
        if (!seg.empty() && header.IsSegmentCompressed(1)) {
            seg = Common::Compression::DecompressDataLZ4(seg, header.segments[1].size);
        }
        result.rodata_bytes = std::move(seg);
    }

    // Read and decompress .data segment (segment 2)
    {
        std::vector<u8> seg = nso_file->ReadBytes(
            header.segments_compressed_size[2], header.segments[2].offset);
        if (!seg.empty() && header.IsSegmentCompressed(2)) {
            seg = Common::Compression::DecompressDataLZ4(seg, header.segments[2].size);
        }
        result.data_bytes = std::move(seg);
    }

    // Analyze ARM64 basic blocks in the .text segment
    result.blocks = AnalyzeArm64BasicBlocks(
        std::span<const u8>{result.text_bytes.data(), result.text_bytes.size()},
        header.segments[0].location, full_scan);

    result.total_blocks = static_cast<u32>(result.blocks.size());
    result.total_instructions = 0;
    for (const auto& block : result.blocks) {
        result.total_instructions += block.instruction_count;
    }

    return result;
}

/// Attempt to get the ExeFS VirtualDir from a ROM file using the VFS infrastructure.
static FileSys::VirtualDir ExtractExeFsFromRom(const std::string& rom_path) {
    // RealVfsFile holds a raw RealVfsFilesystem& (not a shared_ptr), so a
    // locally-scoped vfs would dangle once files it opened outlive this
    // function - keep one filesystem instance alive for the process.
    static const auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    auto file = vfs->OpenFile(rom_path, FileSys::OpenMode::Read);
    if (!file) {
        return nullptr;
    }

    const std::string name = file->GetName();
    const std::string ext = [&name]() {
        auto pos = name.rfind('.');
        if (pos == std::string::npos) return std::string{};
        std::string e = name.substr(pos);
        std::transform(e.begin(), e.end(), e.begin(), ::tolower);
        return e;
    }();

    // Extract the Program NCA's ExeFS from an NSP. NSP::GetExeFS() only
    // returns a populated result for pre-extracted directory-style NSPs;
    // for a real packed/encrypted NSP (the normal case) its exefs/romfs
    // members are never set by the constructor, so it always returns null
    // there regardless of whether the NSP parsed successfully. The actual
    // content lives on the Program-type NCA, keyed by the NSP's own program
    // title ID.
    const auto exefs_from_nsp = [](const std::shared_ptr<FileSys::NSP>& nsp) -> FileSys::VirtualDir {
        if (nsp->GetStatus() != Loader::ResultStatus::Success) {
            return nullptr;
        }
        if (auto exefs = nsp->GetExeFS()) {
            return exefs; // Pre-extracted NSP - already populated.
        }
        const auto t_nca_start = std::chrono::steady_clock::now();
        const auto program_nca =
            nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Program);
        const auto t_nca_got = std::chrono::steady_clock::now();
        const auto exefs = program_nca ? program_nca->GetExeFS() : nullptr;
        const auto t_exefs_got = std::chrono::steady_clock::now();
        LOG_INFO(Frontend, "AOT diag: GetNCA took {} ms, NCA::GetExeFS took {} ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(t_nca_got - t_nca_start).count(),
                 std::chrono::duration_cast<std::chrono::milliseconds>(t_exefs_got - t_nca_got).count());
        return exefs;
    };

    // Try NSP
    if (ext == ".nsp") {
        const auto t_ctor_start = std::chrono::steady_clock::now();
        auto nsp = std::make_shared<FileSys::NSP>(file);
        const auto t_ctor_end = std::chrono::steady_clock::now();
        LOG_INFO(Frontend, "AOT diag: NSP ctor took {} ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(t_ctor_end - t_ctor_start).count());
        if (auto exefs = exefs_from_nsp(nsp)) {
            return exefs;
        }
    }

    // Try XCI
    if (ext == ".xci") {
        auto xci = std::make_shared<FileSys::XCI>(file);
        if (xci->GetStatus() == Loader::ResultStatus::Success) {
            auto secure_nsp = xci->GetSecurePartitionNSP();
            if (secure_nsp) {
                if (auto exefs = exefs_from_nsp(secure_nsp)) {
                    return exefs;
                }
            }
        }
    }

    // Try NCA
    if (ext == ".nca") {
        auto nca = std::make_shared<FileSys::NCA>(file);
        if (nca->GetStatus() == Loader::ResultStatus::Success) {
            auto exefs = nca->GetExeFS();
            if (exefs) return exefs;
        }
    }

    // Standalone NSO — wrap in a synthetic directory
    if (ext == ".nso" || ext == "") {
        // Check if it's an NSO by magic
        u32 magic = 0;
        if (file->ReadObject(&magic) == sizeof(magic) &&
            magic == Common::MakeMagic('N', 'S', 'O', '0')) {
            // Return nullptr — caller will handle single NSO files
        }
    }

    return nullptr;
}

// Save every file in a VirtualDir to disk recursively.
// Returns number of bytes written, -1 on failure.
static qint64 DumpVirtualDir(const FileSys::VirtualDir& vdir, const QString& dest_dir) {
    if (!vdir) return 0;
    QDir().mkpath(dest_dir);
    qint64 total = 0;
    for (const auto& f : vdir->GetFiles()) {
        const auto data = f->ReadAllBytes();
        QFile out(dest_dir + QLatin1Char('/') + QString::fromStdString(f->GetName()));
        if (!out.open(QIODevice::WriteOnly)) return -1;
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<qint64>(data.size()));
        total += static_cast<qint64>(data.size());
    }
    for (const auto& sub : vdir->GetSubdirectories()) {
        const qint64 r = DumpVirtualDir(
            sub, dest_dir + QLatin1Char('/') + QString::fromStdString(sub->GetName()));
        if (r < 0) return -1;
        total += r;
    }
    return total;
}

// Extract the romfs VirtualFile from a ROM (NSP/XCI/NCA). Returns nullptr if not available.
static FileSys::VirtualFile ExtractRomFsFromRom(const std::string& rom_path) {
    // RealVfsFile holds a raw RealVfsFilesystem& (not a shared_ptr), so a
    // locally-scoped vfs would dangle once files it opened outlive this
    // function - keep one filesystem instance alive for the process.
    static const auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    auto file = vfs->OpenFile(rom_path, FileSys::OpenMode::Read);
    if (!file) return nullptr;
    const std::string name = file->GetName();
    auto pos = name.rfind('.'); std::string ext;
    if (pos != std::string::npos) { ext = name.substr(pos); std::transform(ext.begin(),ext.end(),ext.begin(),::tolower); }

    const auto romfs_from_nsp = [](const std::shared_ptr<FileSys::NSP>& nsp) -> FileSys::VirtualFile {
        if (nsp->GetStatus() != Loader::ResultStatus::Success) return nullptr;
        const auto nca = nsp->GetNCA(nsp->GetProgramTitleID(), FileSys::ContentRecordType::Program);
        return nca ? nca->GetRomFS() : nullptr;
    };

    if (ext == ".nsp") {
        auto nsp = std::make_shared<FileSys::NSP>(file);
        if (auto r = romfs_from_nsp(nsp)) return r;
    }
    if (ext == ".xci") {
        auto xci = std::make_shared<FileSys::XCI>(file);
        if (xci->GetStatus() == Loader::ResultStatus::Success) {
            auto sec = xci->GetSecurePartitionNSP();
            if (sec) if (auto r = romfs_from_nsp(sec)) return r;
        }
    }
    if (ext == ".nca") {
        auto nca = std::make_shared<FileSys::NCA>(file);
        if (nca->GetStatus() == Loader::ResultStatus::Success)
            return nca->GetRomFS();
    }
    return nullptr;
}

static std::optional<u32> ReadArm64InstructionAt(std::span<const u8> text, u32 text_vaddr,
                                                 u64 vaddr) {
    if (vaddr < text_vaddr) {
        return std::nullopt;
    }

    const size_t offset = static_cast<size_t>(vaddr - text_vaddr);
    if (offset + sizeof(u32) > text.size()) {
        return std::nullopt;
    }

    u32 instruction = 0;
    std::memcpy(&instruction, text.data() + offset, sizeof(instruction));
    return instruction;
}

static bool SerializeTranslatedBlocks(const NsoAnalysisResult& mod, const QString& ir_root,
                                      const QString& code_root, u32* serialized_blocks,
                                      u32* failed_blocks) {
    const QString module_ir_dir = ir_root + QDir::separator() + mod.name;
    const QString module_code_dir = code_root + QDir::separator() + mod.name;
    QDir().mkpath(module_ir_dir);
    QDir().mkpath(module_code_dir);

    const std::span<const u8> text_span{mod.text_bytes.data(), mod.text_bytes.size()};

    // This phase dumps two files per basic block - the raw bytes and a
    // Dynarmic IR listing - purely as debugging material. It is off by default
    // because the counts involved make it unusable otherwise: Smash Ultimate
    // has about 2.68 million blocks, so it wants roughly 5.4 million files, and
    // the filesystem becomes the entire cost of an export that otherwise takes
    // seconds. Measured before this was gated: 187,000 files written in a few
    // minutes with no end in sight, which is what the long-standing report of
    // the exporter "hanging past 15%" actually was. Nothing in the recompiler
    // path reads these - EmitProject works from mod.text_bytes directly.
    //
    // An earlier comment here put the block count at "90,000+" and explained
    // the symptom as the window merely failing to repaint. Both were wrong.
    const bool dump_blocks =
        !qEnvironmentVariableIsEmpty("SUYU_AOT_DUMP_BLOCKS");
    if (!dump_blocks) {
        // Leave the count at zero rather than reporting the block total: the
        // manifest publishes this as "ir_blocks_serialized", and nothing was
        // serialized. The number that matters for the recompiler is
        // recompiled_c_blocks, which is counted separately.
        return true;
    }

    size_t blocks_processed = 0;
    for (const auto& block : mod.blocks) {
        if (++blocks_processed % 250 == 0) {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
        const QString stem = QStringLiteral("%1_%2")
                                 .arg(mod.name)
                                 .arg(block.vaddr, 8, 16, QLatin1Char('0'));

        const size_t offset = static_cast<size_t>(block.vaddr - mod.text_vaddr);
        if (offset + block.size > mod.text_bytes.size()) {
            ++(*failed_blocks);
            continue;
        }

        QFile guest_code_file(module_code_dir + QDir::separator() + stem + QStringLiteral(".guest.bin"));
        if (guest_code_file.open(QIODevice::WriteOnly)) {
            guest_code_file.write(reinterpret_cast<const char*>(mod.text_bytes.data() + offset),
                                  static_cast<qint64>(block.size));
            guest_code_file.close();
        }

        const u64 block_begin = block.vaddr;
        const u64 block_end = block.vaddr + block.size;
        auto read_code = [&](u64 vaddr) -> std::optional<u32> {
            if (vaddr < block_begin || vaddr + sizeof(u32) > block_end) {
                return std::nullopt;
            }
            return ReadArm64InstructionAt(text_span, mod.text_vaddr, vaddr);
        };

        Dynarmic::A64::TranslationOptions options{};
        options.hook_hint_instructions = false;
        const Dynarmic::A64::LocationDescriptor descriptor{static_cast<u64>(block.vaddr),
                                                           Dynarmic::FP::FPCR{}};
        Dynarmic::IR::Block ir_block{descriptor};
        Dynarmic::A64::Translate(ir_block, descriptor, read_code, options);

        QFile ir_file(module_ir_dir + QDir::separator() + stem + QStringLiteral(".ir.txt"));
        if (!ir_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            ++(*failed_blocks);
            continue;
        }

        QTextStream ir_out(&ir_file);
        ir_out << "module=" << mod.name << "\n";
        ir_out << "block_vaddr=0x" << QStringLiteral("%1").arg(block.vaddr, 8, 16, QLatin1Char('0')) << "\n";
        ir_out << "block_size=" << block.size << "\n";
        ir_out << "instruction_count=" << block.instruction_count << "\n\n";
        ir_out << QString::fromStdString(Dynarmic::IR::DumpBlock(ir_block));
        ir_file.close();
        ++(*serialized_blocks);
    }

    return true;
}

// suyu's own build (and the module/launcher builds this dialog spawns) needs a
// newer CMake than most systems have first on PATH. Prefer the VS-bundled one.
static QString FindBestCmakeExecutable() {
    QStringList candidates;
    const QString path_cmake = QStandardPaths::findExecutable(QStringLiteral("cmake"));
    for (const auto& vs : QDir(QStringLiteral("C:/Program Files/Microsoft Visual Studio"))
                             .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        for (const auto& ed :
             QDir(QStringLiteral("C:/Program Files/Microsoft Visual Studio/") + vs)
                 .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            candidates.append(
                QStringLiteral("C:/Program Files/Microsoft Visual Studio/%1/%2/Common7/IDE/"
                               "CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")
                    .arg(vs, ed));
        }
    }
    for (const auto& c : candidates) {
        if (QFile::exists(c)) {
            return c;
        }
    }
    return path_cmake;
}

// Run a child process to completion while keeping the GUI responsive.
//
// The output MUST be drained on every poll iteration. A child that writes more
// than the OS pipe buffer (~64 KiB on Windows) blocks forever in its own
// write() if nobody reads, and QProcess only buffers what it has actually
// read - QApplication::processEvents() alone does not pump a QProcess that the
// caller is simultaneously blocking on inside waitForFinished(). A cmake build
// of a recompiled module emits far more than 64 KiB (100+ translation units,
// each with MSVC C4127/C4723 warnings), so an undrained loop deadlocks: the
// parent hangs in waitForFinished, the child hangs in write, and no compiler
// ever gets spawned for the remaining files. Everything read is accumulated
// into `captured` so callers still get the full log for diagnostics.
static int RunProcessDrained(QProcess& proc, const QString& program, const QStringList& args,
                             QString* captured = nullptr) {
    QString sink;
    QString& out = captured ? *captured : sink;
    out.clear();

    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForStarted(30000)) {
        out += QStringLiteral("<process failed to start: %1>").arg(program);
        return -1;
    }

    const auto drain = [&] {
        const QByteArray chunk = proc.readAllStandardOutput();
        if (!chunk.isEmpty()) {
            out += QString::fromLocal8Bit(chunk);
            // Keep the retained log bounded; only the tail is ever reported.
            if (out.size() > 1 << 20) {
                out = out.right(1 << 19);
            }
        }
    };

    while (proc.state() != QProcess::NotRunning) {
        // waitForReadyRead returns immediately once the child has closed its
        // stdout, so it cannot be the only thing throttling this loop - fall
        // back to waiting on the process itself when no data arrived, or the
        // loop spins a core for the rest of the build.
        if (!proc.waitForReadyRead(100)) {
            if (proc.waitForFinished(50)) {
                break;
            }
        }
        drain();
        // ExcludeUserInputEvents is load-bearing, not tidiness. This pump runs
        // for the whole of a multi-minute child build with the caller's state
        // on the stack; delivering user input here lets a stray click or an
        // Escape keypress close the dialog (destroying the object this code is
        // running inside) or press a button that re-enters the export. Timers,
        // socket notifiers and repaints still run, so the window stays alive
        // and responsive to the OS.
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    // The child may have exited with data still sitting in the pipe.
    proc.waitForFinished(5000);
    drain();
    return proc.exitCode();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// AOT Pre-compilation — Real Implementation
// ---------------------------------------------------------------------------

bool GameExportDialog::WantsCompiledOutput() const {
    return output_format_combo && output_format_combo->currentIndex() == 1;
}

QString GameExportDialog::RunAotPrecompile(const QString& exefs_dir,
                                           const QString& cache_dir,
                                           RecompileBackend backend,
                                           const QString& game_name) {
    QDir().mkpath(cache_dir);

    const QString manifest_path = cache_dir + QDir::separator() + QStringLiteral("aot_manifest.json");

    // blockmaps/, ir/ and code/ are debugging material for a codegen stage that
    // no longer exists: nothing in suyu or in the generated project reads any of
    // them, and EmitProject works from the module's text bytes directly. They
    // used to be created (and blockmaps written) on every export, which put
    // three dead directories at the top of every package. They are now produced
    // only when explicitly asked for, by the same switch that gates the
    // per-block dumps.
    const bool dump_debug_artifacts = !qEnvironmentVariableIsEmpty("SUYU_AOT_DUMP_BLOCKS");
    const QString debug_root = cache_dir + QDir::separator() + QStringLiteral("debug");
    const QString blockmap_dir = debug_root + QDir::separator() + QStringLiteral("blockmaps");
    const QString code_dir = debug_root + QDir::separator() + QStringLiteral("code");
    const QString ir_dir = debug_root + QDir::separator() + QStringLiteral("ir");
    if (dump_debug_artifacts) {
        QDir().mkpath(blockmap_dir);
        QDir().mkpath(code_dir);
        QDir().mkpath(ir_dir);
    }

    const bool full_scan = aot_full_scan_checkbox->isChecked();
    const bool ballistic_requested = backend == RecompileBackend::Ballistic;
    const QString requested_backend_name =
        ballistic_requested ? QStringLiteral("ballistic") : QStringLiteral("dynarmic");
    const QString effective_backend_name = QStringLiteral("dynarmic");

    // A completed export is immutable for a given game/output directory and
    // scan mode. Reusing it makes re-opening the export dialog or packaging
    // the same title again effectively instant instead of decompressing every
    // NSO and regenerating gigabytes of C.
    if (QFile::exists(manifest_path)) {
        QFile manifest(manifest_path);
        if (manifest.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString contents = QString::fromUtf8(manifest.readAll());
            const bool same_scan = contents.contains(
                QStringLiteral("\"full_scan\": ") + (full_scan ? QStringLiteral("true")
                                                                  : QStringLiteral("false")));
            const bool same_backend = contents.contains(
                QStringLiteral("\"effective_backend\": \"") + effective_backend_name +
                QStringLiteral("\""));
            const bool has_recompiled_project =
                QDir(cache_dir + QDir::separator() + QStringLiteral("exefs")).exists();
            const bool has_required_launcher =
                !WantsCompiledOutput() ||
                QFile::exists(cache_dir + QDir::separator() + QStringLiteral("launcher") +
                              QDir::separator() + QStringLiteral("static_launcher.exe"));
            if (same_scan && same_backend && has_recompiled_project && has_required_launcher) {
                LOG_INFO(Frontend, "Reusing completed AOT cache at {}", cache_dir.toStdString());
                return cache_dir;
            }
        }
    }

    // Collect NSO files to analyze — either from VFS (ROM containers) or from extracted ExeFS
    std::vector<NsoAnalysisResult> module_results;
    bool used_vfs = false;

    // First, try to open the ROM via VFS to extract ExeFS directly
    const QString rom_path = rom_path_edit->text();
    if (!rom_path.isEmpty() && QFile::exists(rom_path) && QFileInfo(rom_path).isFile()) {
        const auto t_extract_start = std::chrono::steady_clock::now();
        auto exefs_vdir = ExtractExeFsFromRom(rom_path.toStdString());
        const auto t_extract_end = std::chrono::steady_clock::now();
        LOG_INFO(Frontend, "AOT diag: ExtractExeFsFromRom took {} ms",
                 std::chrono::duration_cast<std::chrono::milliseconds>(t_extract_end - t_extract_start).count());
        if (exefs_vdir) {
            used_vfs = true;
            const auto t_getfiles_start = std::chrono::steady_clock::now();
            const auto nso_files = exefs_vdir->GetFiles();
            const auto t_getfiles_end = std::chrono::steady_clock::now();
            LOG_INFO(Frontend, "AOT diag: GetFiles() took {} ms, {} entries",
                     std::chrono::duration_cast<std::chrono::milliseconds>(t_getfiles_end - t_getfiles_start).count(),
                     nso_files.size());

            // Standard NSO module names in load order
            static const std::vector<std::string> module_names = {
                "rtld", "main", "subsdk0", "subsdk1", "subsdk2", "subsdk3",
                "subsdk4", "subsdk5", "subsdk6", "subsdk7", "subsdk8", "subsdk9", "sdk"
            };

            for (const auto& nso_file : nso_files) {
                const auto t_file_start = std::chrono::steady_clock::now();
                auto result = AnalyzeNsoFile(nso_file, full_scan);
                const auto t_file_end = std::chrono::steady_clock::now();
                LOG_INFO(Frontend, "AOT diag: AnalyzeNsoFile({}) took {} ms, size={}",
                         nso_file->GetName(),
                         std::chrono::duration_cast<std::chrono::milliseconds>(t_file_end - t_file_start).count(),
                         nso_file->GetSize());
                if (result.has_value()) {
                    module_results.push_back(std::move(*result));
                }
            }

            // Also save extracted ExeFS content to cache
            const QString exefs_cache = cache_dir + QDir::separator() + QStringLiteral("exefs");
            QDir().mkpath(exefs_cache);
            for (const auto& f : nso_files) {
                const auto t_cache_start = std::chrono::steady_clock::now();
                const QString out_path = exefs_cache + QDir::separator() +
                                         QString::fromStdString(f->GetName());
                QFile out_file(out_path);
                if (out_file.open(QIODevice::WriteOnly)) {
                    std::vector<u8> nso_bytes = f->ReadAllBytes();
                    out_file.write(reinterpret_cast<const char*>(nso_bytes.data()),
                                static_cast<qint64>(nso_bytes.size()));
                    out_file.close();
                }
                const auto t_cache_end = std::chrono::steady_clock::now();
                LOG_INFO(Frontend, "AOT diag: cache copy of {} took {} ms", f->GetName(),
                         std::chrono::duration_cast<std::chrono::milliseconds>(t_cache_end - t_cache_start).count());
            }
        }
    }

    // Fallback: read NSO files from the extracted exefs directory on disk
    if (!used_vfs && QDir(exefs_dir).exists()) {
        CopyDirectoryRecursive(exefs_dir, cache_dir + QDir::separator() + QStringLiteral("exefs"));

        // RealVfsFile holds a raw RealVfsFilesystem& (not a shared_ptr), so a
    // locally-scoped vfs would dangle once files it opened outlive this
    // function - keep one filesystem instance alive for the process.
    static const auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
        QDirIterator it(exefs_dir, QDir::Files | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            auto nso_file = vfs->OpenFile(it.filePath().toStdString(), FileSys::OpenMode::Read);
            if (nso_file) {
                auto result = AnalyzeNsoFile(nso_file, full_scan);
                if (result.has_value()) {
                    module_results.push_back(std::move(*result));
                }
            }
        }
    }

    if (module_results.empty()) {
        return QString();
    }

    // Compute totals
    u32 total_blocks = 0;
    u32 total_instructions = 0;
    u32 total_text_bytes = 0;
    u32 total_ir_blocks = 0;
    u32 total_ir_failures = 0;
    for (const auto& mod : module_results) {
        total_blocks += mod.total_blocks;
        total_instructions += mod.total_instructions;
        total_text_bytes += mod.text_size;
        if (dump_debug_artifacts) {
            SerializeTranslatedBlocks(mod, ir_dir, code_dir, &total_ir_blocks, &total_ir_failures);
        }
    }

    // Static recompilation: lift each module's AArch64 .text into a buildable, cross-platform C
    // project. Output mirrors the ROM's exefs structure (hactool/NxFileViewer convention):
    //   exefs/
    //     nso/       <- raw NSO binaries (already extracted above)
    //     main/      <- C project for the main module
    //     rtld/      <- C project for rtld
    //     sdk/       <- C project for sdk
    //     ...
    //     CMakeLists.txt  <- top-level: builds all modules
    const QString recomp_root = cache_dir + QDir::separator() + QStringLiteral("exefs");
    QDir().mkpath(recomp_root);

    // Move raw NSOs into exefs/nso/ so the layout stays clean
    const QString nso_raw_dir = recomp_root + QDir::separator() + QStringLiteral("nso");
    QDir().mkpath(nso_raw_dir);
    {
        const QString old_exefs = cache_dir + QDir::separator() + QStringLiteral("exefs");
        // Snapshot the listing before renaming anything: renaming an entry out
        // of the same directory a QDirIterator is actively walking invalidates
        // its cursor, so only the first file (alphabetically "main") ever got
        // moved and the rest - rtld/sdk/subsdk0 - were left behind as raw
        // blobs sitting at exefs/<name>, colliding with the mkpath() below
        // that needs that same path to be a directory for the module's C
        // project ("is a file, not a directory" cmake configure failure).
        QStringList to_move;
        QDirIterator nso_it(old_exefs, QDir::Files | QDir::NoDotAndDotDot);
        while (nso_it.hasNext()) {
            to_move << nso_it.next();
        }
        for (const QString& src : to_move) {
            const QString dst = nso_raw_dir + QDir::separator() + QFileInfo(src).fileName();
            if (src != dst) {
                QFile::rename(src, dst);
            }
        }
    }

    const bool fallback_enabled = fallback_to_interpreter_checkbox &&
                                  fallback_to_interpreter_checkbox->isChecked();

    u64 recomp_total_blocks = 0;
    QStringList recomp_module_dirs;
    QStringList fallback_modules;

    for (const auto& mod : module_results) {
        if (mod.text_bytes.empty()) {
            continue;
        }
        const QString mod_dir = recomp_root + QDir::separator() + mod.name;
        QDir().mkpath(mod_dir);

        std::vector<u64> exported_roots = CollectExportedSymbolAddresses(mod);
        {
            std::vector<u64> data_ptr_roots = ScanDataForCodePointers(mod);
            exported_roots.insert(exported_roots.end(), data_ptr_roots.begin(), data_ptr_roots.end());
            std::vector<u64> reloc_roots = ScanRelocationsForCodePointers(mod);
            exported_roots.insert(exported_roots.end(), reloc_roots.begin(), reloc_roots.end());
            std::sort(exported_roots.begin(), exported_roots.end());
            exported_roots.erase(std::unique(exported_roots.begin(), exported_roots.end()),
                                 exported_roots.end());
        }

        suyu::recomp::RecompileStats stats{};
        bool emit_ok = false;
        try {
            stats = suyu::recomp::EmitProject(
                mod.name.toStdString(), mod.text_bytes.data(), mod.text_bytes.size(),
                mod.text_vaddr, mod_dir.toStdString(), /*source_only=*/false,
                mod.rodata_bytes.empty() ? nullptr : mod.rodata_bytes.data(),
                mod.rodata_bytes.size(),
                mod.data_bytes.empty() ? nullptr : mod.data_bytes.data(), mod.data_bytes.size(),
                mod.entry_vaddr, game_name.toStdString(), &exported_roots);
            emit_ok = true;
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "EmitProject failed for module {}: {}", mod.name.toStdString(),
                      e.what());
        } catch (...) {
            LOG_ERROR(Frontend, "EmitProject failed for module {} (unknown error)",
                      mod.name.toStdString());
        }

        if (!emit_ok) {
            if (!fallback_enabled) {
                QMessageBox::critical(
                    this, tr("Export Failed"),
                    tr("Module '%1' could not be recompiled.\n\nEnable 'Fall back to interpreter' "
                       "to skip failed modules and use the dynarmic JIT for them at runtime.")
                        .arg(mod.name));
                return {};
            }
            // Emit a stub CMakeLists so the top-level build doesn't break on this dir
            {
                QFile stub(mod_dir + QDir::separator() + QStringLiteral("CMakeLists.txt"));
                if (stub.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream o(&stub);
                    o << "# Module " << mod.name
                      << " fell back to dynarmic JIT — no static recompilation available.\n"
                         "# At runtime suyu will use the interpreter for this module.\n"
                         "message(STATUS \"[fallback] " << mod.name << " uses dynarmic\")\n";
                }
            }
            fallback_modules.append(mod.name);
            LOG_WARNING(Frontend, "Module {} fell back to dynarmic interpreter", mod.name.toStdString());
            continue;
        }

        recomp_total_blocks += stats.blocks;
        recomp_module_dirs.append(mod.name);

        // Actually build it (only in Build mode).
        const QString cmake = WantsCompiledOutput() ? FindBestCmakeExecutable() : QString();
        if (WantsCompiledOutput() && cmake.isEmpty()) {
            LOG_ERROR(Frontend, "Build export requested but cmake was not found");
            QMessageBox::critical(
                this, tr("Export Failed"),
                tr("Export Format is set to Build, but CMake could not be found, so "
                   "no executable can be produced.\n\nInstall CMake (and a C compiler) and try "
                   "again, or choose the Source export format if you only want the generated C."));
            return {};
        }
        if (!cmake.isEmpty()) {
            status_label->setText(tr("Compiling %1 (this takes a while)...").arg(mod.name));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            const QString build_dir = mod_dir + QDir::separator() + QStringLiteral("build");
            QString configure_log;
            QString build_log;

            QProcess configure;
            const int configure_rc = RunProcessDrained(
                configure, cmake,
                {QStringLiteral("-S"), mod_dir, QStringLiteral("-B"), build_dir,
                 // The package ships one self-contained exe, so only the static
                 // library is ever consumed. Without this the generated project
                 // also builds a standalone exe and a loadable DLL from the same
                 // sources - three full compiles of a translation unit that can
                 // take 40 minutes each on a large title.
                 QStringLiteral("-DRECOMP_STATIC_ONLY=ON")},
                &configure_log);
            if (configure_rc == 0) {
                QProcess build;
                const int build_rc =
                    RunProcessDrained(build, cmake,
                                      {QStringLiteral("--build"), build_dir,
                                       QStringLiteral("--config"), QStringLiteral("Release"),
                                       QStringLiteral("--parallel")},
                                      &build_log);
                if (build_rc != 0) {
                    LOG_ERROR(Frontend, "Recompiled module {} failed to compile:\n{}",
                              mod.name.toStdString(), build_log.right(4000).toStdString());
                    if (fallback_enabled) {
                        LOG_WARNING(Frontend, "Module {} compile failed, falling back to dynarmic",
                                    mod.name.toStdString());
                        fallback_modules.append(mod.name);
                        continue;
                    }
                    QMessageBox::critical(
                        this, tr("Build Failed"),
                        tr("Compiling module '%1' failed.\n\nThe generated sources are still in:\n"
                           "%2\n\nEnable 'Fall back to interpreter' to continue despite build "
                           "failures. See the suyu log for compiler output.")
                            .arg(mod.name, mod_dir));
                    return {};
                }
            } else {
                LOG_ERROR(Frontend, "cmake could not configure recompiled module {}:\n{}",
                          mod.name.toStdString(), configure_log.right(4000).toStdString());
                if (fallback_enabled) {
                    LOG_WARNING(Frontend, "Module {} cmake configure failed, falling back to dynarmic",
                                mod.name.toStdString());
                    fallback_modules.append(mod.name);
                    continue;
                }
                QMessageBox::critical(
                    this, tr("Build Failed"),
                    tr("CMake could not configure module '%1'.\n\nThe generated sources are in:\n"
                       "%2\n\nEnable 'Fall back to interpreter' to continue despite failures.")
                        .arg(mod.name, mod_dir));
                return {};
            }

            const QStringList produced =
                // With RECOMP_STATIC_ONLY the only artifact is the static library,
                // so accept that as proof the module compiled.
                QDir(build_dir).entryList({QStringLiteral("*.exe"), QStringLiteral("*.dll"),
                                           QStringLiteral("*.so"), QStringLiteral("*.dylib"),
                                           QStringLiteral("*.lib"), QStringLiteral("*.a"),
                                           QStringLiteral("recompiled")},
                                          QDir::Files, QDir::Name) +
                QDir(build_dir + QDir::separator() + QStringLiteral("Release"))
                    .entryList({QStringLiteral("*.exe"), QStringLiteral("*.dll"),
                                QStringLiteral("*.lib"), QStringLiteral("*.a")},
                               QDir::Files, QDir::Name);
            if (produced.isEmpty()) {
                LOG_ERROR(Frontend, "Build of module {} reported success but produced no binary",
                          mod.name.toStdString());
                if (fallback_enabled) {
                    fallback_modules.append(mod.name);
                    continue;
                }
                QMessageBox::critical(
                    this, tr("Build Failed"),
                    tr("CMake reported success for module '%1' but no binary was found in:\n%2")
                        .arg(mod.name, build_dir));
                return {};
            }
            LOG_INFO(Frontend, "Built recompiled module {}: {}", mod.name.toStdString(),
                     produced.join(QStringLiteral(", ")).toStdString());
        }
    }

    // Top-level CMakeLists.txt: includes all module subdirs so `cmake -S exefs -B build`
    // builds everything in one shot.
    {
        QFile top_cmake(recomp_root + QDir::separator() + QStringLiteral("CMakeLists.txt"));
        if (top_cmake.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&top_cmake);
            o << "cmake_minimum_required(VERSION 3.13)\n"
                 "project(" << game_name << "_recompiled C)\n\n"
                 "# Add each recompiled module as a subdirectory.\n"
                 "# Each module builds its own 'recompiled' exe and 'recompiled_image' shared lib.\n";
            for (const auto& m : recomp_module_dirs) {
                o << "if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/" << m << "/CMakeLists.txt\")\n"
                  << "  add_subdirectory(" << m << ")\n"
                  << "endif()\n";
            }
            if (!fallback_modules.isEmpty()) {
                o << "\n# Modules using dynarmic JIT fallback (not recompiled):\n";
                for (const auto& m : fallback_modules) {
                    o << "# " << m << "\n";
                }
            }
        }
    }

    // ── Single self-contained executable ────────────────────────────────────
    // Every module also builds as a static library (see arm64_to_c.h's
    // EmitProject). Linking those into a private copy of suyu-cmd produces one
    // exe that carries the recompiled CPU code inside it, so the package has no
    // recompiled_*.dll siblings at all. The registration file below is what
    // tells that build which modules exist and in what order they load.
    if (WantsCompiledOutput() && !recomp_module_dirs.isEmpty()) {
        // NSO load order, which is also the index order Core's base setter uses.
        QStringList ordered;
        const auto take = [&](const QString& name) {
            if (recomp_module_dirs.contains(name)) {
                ordered.append(name);
            }
        };
        take(QStringLiteral("rtld"));
        take(QStringLiteral("main"));
        for (int i = 0; i < 10; ++i) {
            take(QStringLiteral("subsdk%1").arg(i));
        }
        take(QStringLiteral("sdk"));
        for (const auto& m : recomp_module_dirs) {
            if (!ordered.contains(m)) {
                ordered.append(m);
            }
        }

        QFile reg(recomp_root + QDir::separator() + QStringLiteral("recomp_registration.c"));
        if (reg.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&reg);
            o << "/* auto-generated by suyu game export - DO NOT EDIT */\n"
                 "/* Lists this game's statically linked recompiled modules in NSO load\n"
                 "   order. Consumed by src/suyu_cmd/suyu.cpp. */\n"
                 "#include <stdint.h>\n\n"
                 "typedef void (*SuyuRecompBlockFn)(void*);\n\n";
            for (const auto& m : ordered) {
                o << "extern SuyuRecompBlockFn recomp_image_lookup_" << m << "(uint64_t);\n"
                  << "extern void recomp_image_set_base_" << m << "(uint64_t);\n"
                  << "extern uint64_t g_module_base_" << m << ";\n";
            }
            o << "\ntypedef struct {\n"
                 "    const char* name;\n"
                 "    SuyuRecompBlockFn (*lookup)(uint64_t);\n"
                 "    void (*set_base)(uint64_t);\n"
                 "} SuyuRecompStaticModule;\n\n"
                 "static const SuyuRecompStaticModule s_modules[] = {\n";
            for (const auto& m : ordered) {
                o << "    { \"" << m << "\", recomp_image_lookup_" << m
                  << ", recomp_image_set_base_" << m << " },\n";
            }
            o << "};\n\n"
                 "const SuyuRecompStaticModule* suyu_recomp_static_modules(unsigned* count) {\n"
                 "    *count = (unsigned)(sizeof(s_modules) / sizeof(s_modules[0]));\n"
                 "    return s_modules;\n"
                 "}\n";
            reg.close();
        }

        // Locate the suyu build tree this frontend was built from. The static
        // variant is an extra target inside it, so all of core/video_core/... is
        // already compiled and only the new target has to link.
        QString build_tree;
        QString source_tree;
        // The suyu tree needs a newer CMake than is typically first on PATH, so
        // reuse whichever one configured it.
        QString tree_cmake;
        {
            QDir up(QCoreApplication::applicationDirPath());
            for (int level = 0; level < 5; ++level) {
                const QString cache = up.absoluteFilePath(QStringLiteral("CMakeCache.txt"));
                if (QFile::exists(cache)) {
                    build_tree = up.absolutePath();
                    QFile cf(cache);
                    if (cf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream in(&cf);
                        while (!in.atEnd()) {
                            const QString line = in.readLine();
                            if (line.startsWith(QStringLiteral("CMAKE_HOME_DIRECTORY:"))) {
                                source_tree = line.section(QLatin1Char('='), 1);
                            } else if (line.startsWith(QStringLiteral("CMAKE_COMMAND:"))) {
                                tree_cmake = line.section(QLatin1Char('='), 1);
                            }
                        }
                    }
                    break;
                }
                if (!up.cdUp()) {
                    break;
                }
            }
        }

        const QString cmake = QFile::exists(tree_cmake)
                                  ? tree_cmake
                                  : QStandardPaths::findExecutable(QStringLiteral("cmake"));
        if (build_tree.isEmpty() || source_tree.isEmpty() || cmake.isEmpty()) {
            LOG_WARNING(Frontend,
                        "No suyu build tree found next to this executable — falling back to the "
                        "generic launcher; the export will use recompiled DLLs instead of a single "
                        "static executable");
        } else {
            status_label->setText(tr("Linking the single-file executable..."));
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            QString conf_log;
            QString link_log;

            // suyu requires a newer CMake than most systems have first on PATH.
            // The VS-bundled CMake is what actually configured this tree (via
            // vcvars64.bat) even when CMakeCache.txt's own CMAKE_COMMAND
            // record points at an older system-wide install (that record
            // reflects whichever cmake first touched the cache, not
            // necessarily the one capable of building it now) - so search
            // VS-bundled installs FIRST and only fall back to the
            // cache/PATH ones after.
            QStringList cmake_candidates;
            for (const auto& vs : QDir(QStringLiteral("C:/Program Files/Microsoft Visual Studio"))
                                     .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                for (const auto& ed :
                     QDir(QStringLiteral("C:/Program Files/Microsoft Visual Studio/") + vs)
                         .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                    cmake_candidates.append(
                        QStringLiteral("C:/Program Files/Microsoft Visual Studio/%1/%2/Common7/IDE/"
                                       "CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")
                            .arg(vs, ed));
                }
            }
            cmake_candidates.append(cmake);
            const QString path_cmake = QStandardPaths::findExecutable(QStringLiteral("cmake"));
            if (!path_cmake.isEmpty()) {
                cmake_candidates.append(path_cmake);
            }
            cmake_candidates.removeDuplicates();

            // Reconfiguring the tree outside a Developer Command Prompt (as
            // this QProcess launch is) leaves cl.exe/link.exe off PATH and
            // INCLUDE/LIB unset, so CMake's compiler-id detection falls back
            // to a "GENERIC" architecture and later steps that need to know
            // the target platform (e.g. the bundled-OpenSSL fetch) reject it.
            // Capture vcvars64.bat's environment once and apply it to both
            // the configure and build subprocesses.
            QProcessEnvironment vs_env = QProcessEnvironment::systemEnvironment();
            {
                const QString vcvars =
                    QStringLiteral("C:/Program Files/Microsoft Visual Studio/2022/Community/VC/"
                                   "Auxiliary/Build/vcvars64.bat");
                if (QFile::exists(vcvars)) {
                    QProcess env_proc;
                    QString out;
                    // `set`'s output for a Developer Command Prompt easily exceeds
                    // the OS pipe buffer (hundreds of vars, a huge PATH) - same
                    // undrained-pipe deadlock as the build subprocesses, so this
                    // uses the same drain-while-waiting helper rather than a bare
                    // waitForFinished()+readAllStandardOutput() after the fact.
                    RunProcessDrained(env_proc, QStringLiteral("cmd.exe"),
                                       {QStringLiteral("/c"), QStringLiteral("call"), vcvars,
                                        QStringLiteral("&&"), QStringLiteral("set")},
                                       &out);
                    for (const auto& line : out.split(QStringLiteral("\n"))) {
                        const int eq = line.indexOf(QLatin1Char('='));
                        if (eq > 0) {
                            vs_env.insert(line.left(eq).trimmed(),
                                          line.mid(eq + 1).trimmed());
                        }
                    }
                }
            }

            QString cmake_exe;
            int conf_rc = -1;
            QProcess conf;
            conf.setProcessEnvironment(vs_env);
            for (const auto& candidate : cmake_candidates) {
                if (!QFile::exists(candidate)) {
                    continue;
                }
                conf_rc = RunProcessDrained(
                    conf, candidate,
                    {QStringLiteral("-S"), source_tree, QStringLiteral("-B"), build_tree,
                     QStringLiteral("-DSUYU_CMD_RECOMP_DIR=") +
                         QDir::fromNativeSeparators(recomp_root)},
                    &conf_log);
                if (conf_rc == 0) {
                    cmake_exe = candidate;
                    break;
                }
                // Each candidate reuses `conf`, so without this only the last
                // failure's output survives to the summary log below.
                LOG_WARNING(Frontend, "cmake candidate {} failed to configure (rc={}):\n{}",
                            candidate.toStdString(), conf_rc,
                            conf_log.right(3000).toStdString());
            }
            int link_rc = -1;
            if (conf_rc == 0) {
                QProcess bld;
                bld.setProcessEnvironment(vs_env);
                link_rc = RunProcessDrained(bld, cmake_exe,
                                            {QStringLiteral("--build"), build_tree,
                                             QStringLiteral("--target"),
                                             QStringLiteral("suyu-cmd-static"),
                                             QStringLiteral("--config"), QStringLiteral("Release"),
                                             QStringLiteral("--parallel")},
                                            &link_log);
                if (link_rc != 0) {
                    LOG_ERROR(Frontend, "suyu-cmd-static failed to link:\n{}",
                              link_log.right(4000).toStdString());
                }
            } else {
                LOG_ERROR(Frontend, "cmake could not configure the static launcher:\n{}",
                          conf_log.right(4000).toStdString());
            }

            if (link_rc == 0) {
                // The target lands wherever suyu-cmd does; look in the usual spots.
                const QStringList candidates = {
                    build_tree + QStringLiteral("/bin/suyu-cmd-static.exe"),
                    build_tree + QStringLiteral("/bin/Release/suyu-cmd-static.exe"),
                    build_tree + QStringLiteral("/bin/suyu-cmd-static"),
                    QCoreApplication::applicationDirPath() +
                        QStringLiteral("/suyu-cmd-static.exe"),
                    QCoreApplication::applicationDirPath() + QStringLiteral("/suyu-cmd-static"),
                };
                for (const auto& c : candidates) {
                    if (!QFile::exists(c)) {
                        continue;
                    }
                    const QString dst_dir = cache_dir + QDir::separator() +
                                            QStringLiteral("launcher");
                    QDir().mkpath(dst_dir);
                    const QString dst =
                        dst_dir + QDir::separator() + QStringLiteral("static_launcher.exe");
                    QFile::remove(dst);
                    if (QFile::copy(c, dst)) {
                        LOG_INFO(Frontend, "Built single-file launcher from {}", c.toStdString());
                    }
                    break;
                }
            }
        }
    }

    // One-command build scripts.
    {
        QFile bw(recomp_root + QDir::separator() + QStringLiteral("build_native_windows.cmd"));
        if (bw.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&bw);
            o << "@echo off\r\n"
                 "rem Build all recompiled modules.\r\n"
                 "rem Auto-detect cmake from Visual Studio if not on PATH.\r\n"
                 "where cmake >nul 2>&1\r\n"
                 "if %errorlevel% neq 0 (\r\n"
                 "  set \"CMAKE_SEARCH=\"\r\n"
                 "  for /d %%V in (\"C:\\Program Files\\Microsoft Visual Studio\\2022\\*\") do (\r\n"
                 "    if exist \"%%V\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe\" (\r\n"
                 "      set \"CMAKE_SEARCH=%%V\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe\"\r\n"
                 "    )\r\n"
                 "  )\r\n"
                 "  if defined CMAKE_SEARCH (\r\n"
                 "    echo Using VS cmake: %CMAKE_SEARCH%\r\n"
                 "    set \"CMAKE=%CMAKE_SEARCH%\"\r\n"
                 "  ) else (\r\n"
                 "    echo ERROR: cmake not found. Install CMake or Visual Studio.\r\n"
                 "    pause & exit /b 1\r\n"
                 "  )\r\n"
                 ") else (\r\n"
                 "  set \"CMAKE=cmake\"\r\n"
                 ")\r\n"
                 "for /d %%M in (*) do (\r\n"
                 "  if exist \"%%M\\CMakeLists.txt\" (\r\n"
                 "    echo Building %%M ...\r\n"
                 "    \"%CMAKE%\" -S \"%%M\" -B \"%%M\\build\" && \"%CMAKE%\" --build \"%%M\\build\" --config Release\r\n"
                 "  )\r\n"
                 ")\r\n"
                 "echo Done.\r\n"
                 "pause\r\n";
            bw.close();
        }
        QFile bs(recomp_root + QDir::separator() + QStringLiteral("build_native_unix.sh"));
        if (bs.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&bs);
            o << "#!/bin/sh\n"
                 "# Build all recompiled modules.\n"
                 "cmake -S . -B build && cmake --build build\n";
            bs.close();
        }
    }

    // Write block map files for each module (binary format, debugging only -
    // see the note by dump_debug_artifacts above).
    // Format per entry: [u32 vaddr][u32 size][u32 instruction_count][u32 flags]
    for (const auto& mod : module_results) {
        if (!dump_debug_artifacts) {
            break;
        }
        const QString map_path = blockmap_dir + QDir::separator() + mod.name +
                                 QStringLiteral(".blockmap");
        QFile map_file(map_path);
        if (map_file.open(QIODevice::WriteOnly)) {
            // Header: magic "AOTB", version, block_count
            const u32 magic = 0x42544F41; // "AOTB"
            const u32 version = 1;
            const u32 block_count = static_cast<u32>(mod.blocks.size());
            map_file.write(reinterpret_cast<const char*>(&magic), 4);
            map_file.write(reinterpret_cast<const char*>(&version), 4);
            map_file.write(reinterpret_cast<const char*>(&block_count), 4);

            size_t map_blocks_written = 0;
            for (const auto& block : mod.blocks) {
                if (++map_blocks_written % 1000 == 0) {
                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                }
                map_file.write(reinterpret_cast<const char*>(&block.vaddr), 4);
                map_file.write(reinterpret_cast<const char*>(&block.size), 4);
                map_file.write(reinterpret_cast<const char*>(&block.instruction_count), 4);
                u32 flags = block.is_entry ? 1u : 0u;
                map_file.write(reinterpret_cast<const char*>(&flags), 4);
            }
            map_file.close();
        }
    }

    // Write the AOT manifest with real analysis results
    QFile manifest(manifest_path);
    if (manifest.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&manifest);
        out << "{\n";
        out << "  \"version\": 2,\n";
        out << "  \"requested_backend\": \"" << requested_backend_name << "\",\n";
        out << "  \"effective_backend\": \"" << effective_backend_name << "\",\n";
        out << "  \"full_scan\": " << (full_scan ? "true" : "false") << ",\n";
        out << "  \"total_modules\": " << module_results.size() << ",\n";
        out << "  \"total_blocks_analyzed\": " << total_blocks << ",\n";
        out << "  \"total_instructions\": " << total_instructions << ",\n";
        out << "  \"total_text_bytes\": " << total_text_bytes << ",\n";
        out << "  \"ir_blocks_serialized\": " << total_ir_blocks << ",\n";
        out << "  \"ir_translation_failures\": " << total_ir_failures << ",\n";
        out << "  \"host_machine_code_blocks\": 0,\n";
        out << "  \"recompiled_c_blocks\": " << recomp_total_blocks << ",\n";
        out << "  \"recompiled_project\": \"recompiled/<module>/ (buildable C, cross-platform CMake; "
               "generated units in <module>/src, build output in <module>/build)\",\n";
        out << "  \"native_build_scripts\": [\"recompiled/build_native_windows.cmd\", \"recompiled/build_native_unix.sh\"],\n";
        out << "  \"requires_runtime_codegen\": false,\n";
        out << "  \"modules\": [\n";
        for (size_t i = 0; i < module_results.size(); ++i) {
            const auto& mod = module_results[i];
            out << "    {\n";
            out << "      \"name\": \"" << mod.name << "\",\n";
            out << "      \"build_id\": \"" << mod.build_id_hex << "\",\n";
            out << "      \"text_vaddr\": " << mod.text_vaddr << ",\n";
            out << "      \"text_size\": " << mod.text_size << ",\n";
            out << "      \"rodata_vaddr\": " << mod.rodata_vaddr << ",\n";
            out << "      \"rodata_size\": " << mod.rodata_size << ",\n";
            out << "      \"data_vaddr\": " << mod.data_vaddr << ",\n";
            out << "      \"data_size\": " << mod.data_size << ",\n";
            out << "      \"blocks\": " << mod.total_blocks << ",\n";
            out << "      \"instructions\": " << mod.total_instructions << ",\n";
            out << "      \"project_directory\": \"recompiled/" << mod.name << "\",\n";
            out << "      \"sources_directory\": \"recompiled/" << mod.name << "/src\"";
            if (dump_debug_artifacts) {
                out << ",\n      \"blockmap_file\": \"debug/blockmaps/" << mod.name
                    << ".blockmap\",\n";
                out << "      \"ir_directory\": \"debug/ir/" << mod.name << "\",\n";
                out << "      \"guest_code_directory\": \"debug/code/" << mod.name << "\"";
            }
            out << "\n";
            out << "    }" << (i + 1 < module_results.size() ? "," : "") << "\n";
        }
        out << "  ],\n";
             out << "  \"comment\": \"Dynarmic A64 frontend export. suyu serializes translated IR, "
                 "raw guest code slices, and block maps as inputs for a future custom runtime/codegen "
                 "stage instead of bundling the existing frontend executable."
                 << (ballistic_requested
                         ? " Ballistic was requested but currently falls back to the Dynarmic export path until a distinct Ballistic serializer is wired."
                         : "")
                 << "\"\n";
        out << "}\n";
        manifest.close();
    }

    return cache_dir;
}

// ---------------------------------------------------------------------------
// Standalone packaging
// ---------------------------------------------------------------------------

// Where the AOT cache lives inside a finished package, per target platform.
// PackageNativeExport lays the package out; this has to agree with it exactly,
// because the export now generates the cache directly at this path instead of
// staging it elsewhere and copying it in.
static QString AotCacheDirFor(const QString& output_dir, const QString& game_name,
                              GameExportDialog::TargetPlatform platform) {
    switch (platform) {
    case GameExportDialog::TargetPlatform::Linux:
        return output_dir + QDir::separator() + game_name + QStringLiteral(".AppDir") +
               QDir::separator() + QStringLiteral("usr/bin") + QDir::separator() +
               QStringLiteral("aot_cache");
    case GameExportDialog::TargetPlatform::MacOS:
        return output_dir + QDir::separator() + game_name + QStringLiteral(".app") +
               QDir::separator() + QStringLiteral("Contents") + QDir::separator() +
               QStringLiteral("Resources") + QDir::separator() + QStringLiteral("aot_cache");
    case GameExportDialog::TargetPlatform::Windows:
    default:
        return output_dir + QDir::separator() + game_name + QDir::separator() +
               QStringLiteral("aot_cache");
    }
}

bool GameExportDialog::PackageNativeExport(const QString& rom_path, const QString& cache_dir,
                                           const QString& output_dir, const QString& game_name,
                                           TargetPlatform platform) {
    // The original ROM used to be copied into every package. It is not needed
    // there: the recompiled project carries the guest segments it executes in
    // <module>/data, and nothing in the package or in suyu ever opened the copy.
    // All it did was add the ROM's full size - several gigabytes for an XCI - to
    // an output that is otherwise tens of megabytes of C. The source is recorded
    // by path instead, so the export can still be traced back to it.
    const auto write_source_reference = [&rom_path](const QString& dir) {
        QFile ref(dir + QDir::separator() + QStringLiteral("game_source.txt"));
        if (!ref.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return;
        }
        QTextStream out(&ref);
        out << "Recompiled from: " << QDir::toNativeSeparators(rom_path) << "\n"
            << "The ROM is referenced, not bundled - the generated project runs from the guest\n"
            << "segments in aot_cache/recompiled/<module>/data and does not read this file.\n";
        ref.close();
    };

    switch (platform) {
    case TargetPlatform::Windows: {
        const QString pkg_dir = output_dir + QDir::separator() + game_name;
        if (!QDir().mkpath(pkg_dir)) {
            return false;
        }

        // A prior export into this same folder may have been "Build" format
        // and left a compiled launcher exe + its DLLs sitting in pkg_dir. If
        // this run is "Source" (or otherwise not producing a compiled
        // launcher), that stale exe is never touched by anything below - it
        // just sits there looking like part of the new export. Strip it so a
        // format switch on the same output folder doesn't leave orphaned
        // binaries next to freshly generated C source.
        if (!WantsCompiledOutput()) {
            QFile::remove(pkg_dir + QDir::separator() + game_name + QStringLiteral(".exe"));
            for (const char* dll : {"avcodec-61.dll", "avformat-61.dll", "avutil-59.dll",
                                     "dxcompiler.dll", "dxil.dll", "libcrypto.dll", "libssl.dll",
                                     "swresample-5.dll", "swscale-8.dll"}) {
                QFile::remove(pkg_dir + QDir::separator() + QString::fromLatin1(dll));
            }
        }

        // ── Extract exefs (NSO executables) → <pkg>/exefs/ ──────────────────────
        // This gives the package the same structure as Switch ROM viewers show.
        // suyu-cmd auto-detects exefs/main next to it and loads from there.
        const QString exefs_dst = pkg_dir + QStringLiteral("/exefs");
        auto exefs_vdir = ExtractExeFsFromRom(rom_path.toStdString());
        if (exefs_vdir) {
            DumpVirtualDir(exefs_vdir, exefs_dst);
        }

        // ── Extract romfs → <pkg>/exefs/romfs.bin ────────────────────────────────
        // DeconstructedRomDirectory loader looks for a .romfs file alongside the NSOs.
        // Romfs can be several GB — only extract if VFS gives it without copying the file.
        if (exefs_vdir) {
            auto romfs_vf = ExtractRomFsFromRom(rom_path.toStdString());
            if (romfs_vf) {
                // Always extract the decrypted romfs into the package so the
                // export is genuinely standalone: no original ROM file and no
                // keys required at runtime (only at export time, on this
                // machine, to read the source ROM once). Streamed in chunks -
                // romfs can be many GB and ReadAllBytes() would double the
                // package's peak memory use for no benefit.
                QFile rf(exefs_dst + QStringLiteral("/romfs.bin"));
                if (rf.open(QIODevice::WriteOnly)) {
                    constexpr u64 kChunk = 64ULL * 1024 * 1024;
                    const u64 total = romfs_vf->GetSize();
                    for (u64 off = 0; off < total; off += kChunk) {
                        const u64 len = std::min(kChunk, total - off);
                        const auto bytes = romfs_vf->ReadBytes(len, off);
                        rf.write(reinterpret_cast<const char*>(bytes.data()),
                                 static_cast<qint64>(bytes.size()));
                    }
                    rf.close();
                }
            }
        }

        // Bundle suyu-cmd.exe (renamed to the game name) and its runtime DLLs so the
        // package runs standalone — suyu-cmd provides the HLE+GPU+audio stack.
        const QString bin_dir = QCoreApplication::applicationDirPath();
        // Prefer the per-game build that has this game's recompiled modules
        // linked in: one file, no recompiled_*.dll beside it. The generic
        // suyu-cmd is the fallback for source-only exports and for machines
        // where the static link could not be produced.
        const QString static_launcher =
            cache_dir + QStringLiteral("/launcher/static_launcher.exe");
        const bool has_static_launcher = QFile::exists(static_launcher);

        // The generated C source and per-module build trees under aot_cache/
        // are compile-time-only: once the static launcher exists, everything
        // this package needs to run is already linked into that one exe.
        // Shipping the source tree alongside it (often hundreds of MB, plus
        // it visually screams "this is an emulator with a debug build in
        // it" rather than a native game) only makes sense for Source-format
        // exports, where the user asked for the C project instead of a
        // compiled binary.
        if (!has_static_launcher) {
            write_source_reference(pkg_dir);
            if (!CopyDirectoryUnlessInPlace(
                    cache_dir, pkg_dir + QDir::separator() + QStringLiteral("aot_cache"))) {
                return false;
            }
        }
        // has_static_launcher's aot_cache cleanup happens further down, after
        // static_launcher.exe has been copied out of it to its final path -
        // deleting cache_dir here would remove that file before the copy runs.
        // A Build export must never silently downgrade to the generic emulator.
        // That produces the misleading "game.exe + ROM" bundle which still
        // depends on recompiled DLLs (or falls back to JIT), rather than the
        // self-contained executable promised by this export mode.
        if (WantsCompiledOutput() && !has_static_launcher) {
            LOG_ERROR(Frontend, "Static recompiled launcher was not produced: {}",
                      static_launcher.toStdString());
            // Built explicitly rather than via QMessageBox::critical so the text
            // format can be pinned to plain text — Qt's auto rich-text detection
            // otherwise renders parts of the message as a styled/highlighted block.
            QMessageBox box(this);
            box.setIcon(QMessageBox::Critical);
            box.setWindowTitle(tr("Export Failed"));
            box.setTextFormat(Qt::PlainText);
            box.setText(tr("The static recompiled executable was not produced."));
            box.setInformativeText(
                tr("The export was stopped instead of packaging the generic emulator launcher. "
                   "Check the build log and ensure the recompiler modules compiled successfully."));
            box.setStandardButtons(QMessageBox::Ok);
            box.exec();
            return false;
        }
        const QString launcher_src = has_static_launcher
                                         ? static_launcher
                                         : bin_dir + QStringLiteral("/suyu-cmd.exe");
        const QString launcher_dst = pkg_dir + QDir::separator() + game_name + QStringLiteral(".exe");
        if (QFile::exists(launcher_src)) {
            QFile::remove(launcher_dst);
            QFile::copy(launcher_src, launcher_dst);

            // Embed the game's icon into the launcher exe via Windows resource update API.
            if (!game_icon_.isNull()) {
#ifdef _WIN32
                // Scale icon to 256x256 for best Explorer display quality
                const QPixmap icon256 = game_icon_.scaled(256, 256,
                    Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                    .copy(0, 0, 256, 256);
                const QByteArray icon_dib = MakeIconDib(icon256);
                if (!icon_dib.isEmpty()) {
#pragma pack(push,1)
                    struct GrpEntry { BYTE w,h,cc,res; WORD pl,bpp; DWORD sz; WORD id; };
                    struct GrpDir  { WORD reserved,type,count; GrpEntry e[1]; };
#pragma pack(pop)
                    GrpDir grp{};
                    grp.type=1; grp.count=1;
                    // w=h=0 signals 256x256 in ICO/GRPICONDIR convention
                    grp.e[0].w=0; grp.e[0].h=0;
                    grp.e[0].pl=1; grp.e[0].bpp=32;
                    grp.e[0].sz=(DWORD)icon_dib.size(); grp.e[0].id=1;
                    const std::wstring dstW = launcher_dst.toStdWString();
                    // FALSE = keep existing resources (manifests, version info, etc.)
                    HANDLE h = BeginUpdateResourceW(dstW.c_str(), FALSE);
                    if (h) {
                        const bool icon_ok = UpdateResourceW(h, RT_ICON, MAKEINTRESOURCEW(1),
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                            (LPVOID)icon_dib.data(), (DWORD)icon_dib.size()) != FALSE;
                        const bool group_ok = UpdateResourceW(h, RT_GROUP_ICON, MAKEINTRESOURCEW(1),
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                            (LPVOID)&grp, (DWORD)(sizeof(WORD)*3 + sizeof(GrpEntry)));
                        if (!icon_ok || !group_ok || !EndUpdateResourceW(h, FALSE)) {
                            LOG_WARNING(Frontend, "Failed to embed game icon in {}",
                                        launcher_dst.toStdString());
                        }
                    }
                }
#endif
            }

            // DLLs required by suyu-cmd (FFmpeg, DXC, OpenSSL — SDL3 is statically linked)
            static constexpr const char* kRuntimeDlls[] = {
                "avcodec-61.dll", "avformat-61.dll", "avutil-59.dll",
                "swresample-5.dll", "swscale-8.dll",
                "dxcompiler.dll", "dxil.dll",
                "libcrypto-3-x64.dll", "libssl-3-x64.dll",
                // fallback names used by some builds
                "libcrypto.dll", "libssl.dll",
            };
            for (const char* dll : kRuntimeDlls) {
                const QString src = bin_dir + QLatin1Char('/') + QLatin1String(dll);
                if (QFile::exists(src)) {
                    const QString dst = pkg_dir + QDir::separator() + QLatin1String(dll);
                    QFile::remove(dst);
                    QFile::copy(src, dst);
                }
            }
        }

        // No -g argument: the exe auto-detects exefs/main (and romfs.bin
        // alongside it) next to itself, both already extracted and decrypted
        // into this package above. Passing the original ROM path here would
        // bypass that and force suyu-cmd to re-open the encrypted source ROM
        // instead - needing keys and the original file present, exactly what
        // bundling exefs/romfs was meant to avoid.
        QFile bat(pkg_dir + QDir::separator() + QStringLiteral("launch.bat"));
        if (bat.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&bat);
            out << "@echo off\n";
            out << "\"" << game_name << ".exe\"\n";
            bat.close();
        }

        // cache_dir is pkg_dir/aot_cache itself (RunAotPrecompile generates
        // straight into the package, it was never copied in from elsewhere),
        // so once static_launcher.exe has been copied out to the package
        // root above, the whole generated-C build tree - per-module source,
        // object files, .lib artifacts, often several GB - is now dead
        // weight sitting in what's supposed to be a tidy, standalone game
        // folder. Delete it; a repeat export just recompiles (fast, thanks
        // to the /O1 + /MP codegen flags) rather than reusing this cache.
        if (has_static_launcher) {
            QDir(cache_dir).removeRecursively();
        } else {
            QDir(pkg_dir + QStringLiteral("/aot_cache/launcher")).removeRecursively();
        }

        // Mods/patches live beside the exe (see SetEdenPath(LoadDir) in
        // suyu_cmd/suyu.cpp); create it so the layout is discoverable.
        QDir().mkpath(pkg_dir + QStringLiteral("/mods"));

        QFile readme(pkg_dir + QDir::separator() + QStringLiteral("README_NATIVE_EXPORT.txt"));
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readme);
            out << "Recompiled native build — fully standalone, no ROM or keys needed to run\n\n";
            out << "Run: double-click launch.bat (or " << game_name << ".exe directly)\n\n";
            out << "This is the game itself, statically recompiled to x86 machine code and\n";
            out << "linked into " << game_name << ".exe alongside suyu's HLE/GPU/audio backend\n";
            out << "- no emulator install and no separate DLLs for the game code.\n\n";
            out << "What runs native vs emulated:\n";
            out << "- Native  : the game's own CPU code, translated ahead of time to C and\n";
            out << "            compiled into this exe. No instruction decoding at run time.\n";
            out << "- Emulated: system calls, OS services (filesystem, input, audio, sockets)\n";
            out << "            and the GPU, all served by suyu's HLE backend built into the\n";
            out << "            same exe. A console game cannot run without these.\n";
            out << "- Fallback: a small interpreter covers the few instructions the static\n";
            out << "            recompiler cannot translate yet (mostly rare SIMD forms) and\n";
            out << "            code only reachable through computed branches. It runs on\n";
            out << "            demand and hands control straight back; correctness never\n";
            out << "            depends on how much of the program it covers.\n\n";
            out << "Contents:\n";
            out << "- " << game_name << ".exe : the game (recompiled code + HLE/GPU backend, one file)\n";
            out << "- launch.bat      : one-click launcher\n";
            out << "- exefs/          : the game's own executables and data, extracted once at\n";
            out << "                    export time so no ROM or decryption keys are needed to run\n";
            out << "- *.dll           : runtime libraries (FFmpeg, Vulkan, OpenSSL)\n";
            out << "- mods/           : optional; drop <title_id>/<mod name>/ folders here\n";
            out << "- user/           : this game's own config, saves, and logs (not suyu's)\n\n";
            out << "Press F12 in-game for the debug panel (status, mods, folders).\n";
            readme.close();
        }

        return true;
    }

    case TargetPlatform::Linux: {
        const QString appdir =
            output_dir + QDir::separator() + game_name + QStringLiteral(".AppDir");
        const QString bin_dir = appdir + QDir::separator() + QStringLiteral("usr/bin");
        if (!QDir().mkpath(bin_dir)) {
            return false;
        }

        write_source_reference(bin_dir);

        // Copy AOT cache
        if (!CopyDirectoryUnlessInPlace(cache_dir,
                                    bin_dir + QDir::separator() + QStringLiteral("aot_cache"))) {
            return false;
        }

        QFile readme(appdir + QDir::separator() + QStringLiteral("README_NATIVE_EXPORT.txt"));
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readme);
            out << "Suyu native export artifact bundle\n\n";
            out << "Compiler artifacts are under usr/bin/aot_cache.\n";
            out << "No frontend runtime binary is bundled in this export.\n";
            readme.close();
        }

        return true;
    }

    case TargetPlatform::MacOS: {
        const QString app_bundle =
            output_dir + QDir::separator() + game_name + QStringLiteral(".app");
        const QString contents_dir = app_bundle + QDir::separator() + QStringLiteral("Contents");
        const QString res_dir = contents_dir + QDir::separator() + QStringLiteral("Resources");
        if (!QDir().mkpath(contents_dir) || !QDir().mkpath(res_dir)) {
            return false;
        }

        write_source_reference(res_dir);

        // Copy AOT cache
        if (!CopyDirectoryUnlessInPlace(cache_dir,
                                    res_dir + QDir::separator() + QStringLiteral("aot_cache"))) {
            return false;
        }

        QFile readme(contents_dir + QDir::separator() + QStringLiteral("README_NATIVE_EXPORT.txt"));
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readme);
            out << "Suyu native export artifact bundle\n\n";
            out << "Compiler artifacts are under Contents/Resources/aot_cache.\n";
            out << "No frontend runtime binary is bundled in this export.\n";
            readme.close();
        }

        return true;
    }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Locating already-built standalone recompiled executables
// ---------------------------------------------------------------------------

namespace {
constexpr char kOutputRootsKey[] = "recompile/output_roots";
} // namespace

QStringList GameExportDialog::RecompileOutputRoots() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("suyu"));
    QStringList roots = settings.value(QString::fromLatin1(kOutputRootsKey)).toStringList();

    // The dialog defaults to Downloads, and the test harness writes into an
    // aot_test_output next to the working directory, so both are worth
    // checking even before the user has ever completed an export.
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!downloads.isEmpty()) {
        roots.append(downloads);
    }
    roots.append(QDir::currentPath() + QDir::separator() + QStringLiteral("aot_test_output"));

    // In a development tree suyu runs out of build/bin, so the export root the
    // test harness writes to sits a couple of levels above the executable.
    QDir up(QCoreApplication::applicationDirPath());
    for (int level = 0; level < 4; ++level) {
        roots.append(up.absoluteFilePath(QStringLiteral("aot_test_output")));
        if (!up.cdUp()) {
            break;
        }
    }

    roots.removeDuplicates();
    return roots;
}

void GameExportDialog::RememberOutputRoot(const QString& dir) {
    if (dir.isEmpty()) {
        return;
    }
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("suyu"));
    QStringList roots = settings.value(QString::fromLatin1(kOutputRootsKey)).toStringList();
    roots.removeAll(dir);
    roots.prepend(dir);
    while (roots.size() > 8) {
        roots.removeLast();
    }
    settings.setValue(QString::fromLatin1(kOutputRootsKey), roots);
}

QStringList GameExportDialog::FindRecompiledExecutables(const QString& game_name,
                                                        const QString& rom_path) {
    // The export directory is named after the ROM's base name, which for a
    // library entry is usually but not always the display title.
    QStringList names;
    if (!game_name.isEmpty()) {
        names.append(game_name);
    }
    if (!rom_path.isEmpty()) {
        const QString base = QFileInfo(rom_path).completeBaseName();
        if (!base.isEmpty()) {
            names.append(base);
        }
    }
    names.removeDuplicates();

    // A package can be laid out for any of the three target platforms, and the
    // build directory is single- or multi-config depending on the generator.
    static const QStringList package_suffixes = {
        QStringLiteral("/aot_cache/recompiled"),
        QStringLiteral(".AppDir/usr/bin/aot_cache/recompiled"),
        QStringLiteral(".app/Contents/Resources/aot_cache/recompiled"),
    };
    static const QStringList exe_candidates = {
        QStringLiteral("build/Release/recompiled.exe"),
        QStringLiteral("build/Debug/recompiled.exe"),
        QStringLiteral("build/recompiled.exe"),
        QStringLiteral("build/recompiled"),
    };

    QStringList found;
    for (const QString& root : RecompileOutputRoots()) {
        for (const QString& name : names) {
            // New layout: <root>/<GameName>/<GameName>.exe (suyu-cmd launcher)
            const QString pkg_launcher = root + QDir::separator() + name +
                                         QDir::separator() + name +
#ifdef _WIN32
                                         QStringLiteral(".exe");
#else
                                         QString{};
#endif
            if (QFile::exists(pkg_launcher)) {
                found.prepend(QDir::toNativeSeparators(pkg_launcher));
            }

            // Legacy layout: deep in aot_cache/recompiled/<module>/build/
            for (const QString& suffix : package_suffixes) {
                const QString recomp_root = root + QDir::separator() + name + suffix;
                QDir dir(recomp_root);
                if (!dir.exists()) {
                    continue;
                }
                const QStringList modules =
                    dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
                for (const QString& mod : modules) {
                    for (const QString& rel : exe_candidates) {
                        const QString exe =
                            recomp_root + QDir::separator() + mod + QDir::separator() + rel;
                        const QFileInfo info(exe);
                        if (info.exists() && info.isFile()) {
                            found.append(QDir::toNativeSeparators(info.absoluteFilePath()));
                            break;
                        }
                    }
                }
            }
        }
    }
    found.removeDuplicates();
    return found;
}

// ---------------------------------------------------------------------------
// Main export entry point
// ---------------------------------------------------------------------------

void GameExportDialog::closeEvent(QCloseEvent* event) {
    if (export_in_progress) {
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}

void GameExportDialog::reject() {
    if (export_in_progress) {
        return;
    }
    QDialog::reject();
}

namespace {
// Holds `flag` for its lifetime and, on Windows, tells the OS the machine is
// busy. A Build export runs for tens of minutes with no user input, so the
// idle timer would otherwise be free to sleep the system out from under the
// child compilers mid-build. ES_DISPLAY_REQUIRED is deliberately not set: the
// screen may blank, only sleep is held off.
class ExportRunGuard {
public:
    explicit ExportRunGuard(bool& flag) : flag_{flag} {
        flag_ = true;
#ifdef _WIN32
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
#endif
    }
    ~ExportRunGuard() {
        flag_ = false;
#ifdef _WIN32
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
    }
    ExportRunGuard(const ExportRunGuard&) = delete;
    ExportRunGuard& operator=(const ExportRunGuard&) = delete;

private:
    bool& flag_;
};
} // namespace

/// Direct-from-ROM icon+title fallback for when the export ran without ever
/// matching a scanned library entry - a freshly downloaded ROM the library
/// hasn't indexed yet, or one outside the configured game directories
/// entirely. Without this, such an export silently kept the default suyu
/// icon and fell back to the filename for its title, since every other path
/// to a name/icon (OnSelectFromLibrary, the library_entries_ match in
/// OnExport) depends on the game already being in the scanned library.
/// Mirrors GameLibraryWorker::GetGameIcon (src/suyu/game_library.cpp).
static bool ReadIconAndTitleFromRom(Core::System& system, const QString& rom_path,
                                    QPixmap& out_icon, QString& out_title) {
    static const auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    auto file = vfs->OpenFile(rom_path.toStdString(), FileSys::OpenMode::Read);
    if (!file) {
        return false;
    }
    auto loader = Loader::GetLoader(system, file);
    if (!loader) {
        return false;
    }
    bool got_anything = false;
    std::vector<u8> icon_data;
    if (loader->ReadIcon(icon_data) == Loader::ResultStatus::Success && !icon_data.empty()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(icon_data.data(), static_cast<uint>(icon_data.size())) &&
            !pixmap.isNull()) {
            out_icon = pixmap;
            got_anything = true;
        }
    }
    std::string title;
    if (loader->ReadTitle(title) == Loader::ResultStatus::Success && !title.empty()) {
        out_title = QString::fromStdString(title);
        got_anything = true;
    }
    return got_anything;
}

void GameExportDialog::OnExport() {
    // Re-entry would run two exports over one cache directory. See the comment
    // on export_in_progress: processEvents() inside the export can deliver an
    // automation RPC that calls straight back in here.
    if (export_in_progress) {
        LOG_WARNING(Frontend, "Export already in progress; ignoring re-entrant request");
        return;
    }
    const ExportRunGuard run_guard{export_in_progress};

    const QString rom_path = rom_path_edit->text();
    const QString output_dir = output_path_edit->text();

    if (rom_path.isEmpty()) {
        QMessageBox::warning(this, tr("No ROM"), tr("Please set a ROM path first."));
        return;
    }
    if (output_dir.isEmpty()) {
        QMessageBox::warning(this, tr("No Output"),
                             tr("Please select an output directory first."));
        return;
    }
    if (!QFile::exists(rom_path)) {
        QMessageBox::warning(this, tr("ROM Not Found"),
                             tr("The specified ROM file does not exist."));
        return;
    }

    const auto platform =
        static_cast<TargetPlatform>(platform_combo->currentData().toInt());
    const auto backend =
        static_cast<RecompileBackend>(backend_combo->currentData().toInt());
    const bool include_save_data = include_save_data_checkbox->isChecked();
    const bool include_shader_cache = include_shader_cache_checkbox->isChecked();
    const bool include_custom_config = include_custom_config_checkbox->isChecked();
    const QFileInfo rom_info(rom_path);
    // Prefer the NACP/library title; fall back to filename if not found.
    QString game_name = rom_info.completeBaseName();
    bool matched_library_entry = false;
    for (const auto& entry : library_entries_) {
        if (QFileInfo(entry.path) == rom_info) {
            matched_library_entry = true;
            if (!entry.title.trimmed().isEmpty()) {
                game_name = entry.title.trimmed();
                // Strip characters that are illegal in Windows filenames
                static const QRegularExpression kIllegal(QStringLiteral("[\\\\/:*?\"<>|]"));
                game_name.replace(kIllegal, QStringLiteral("_"));
            }
            // OnSelectFromLibrary sets this already, but OnBrowseRom and a
            // hand-typed ROM path never do - only the "From Library" picker
            // called SetGameIcon, so any other way of choosing a ROM that
            // still matches a scanned library entry silently shipped with
            // suyu's own icon instead of the game's. Set it here too, once,
            // wherever the entry lookup already happens for the game name.
            if (!entry.icon.isNull()) {
                SetGameIcon(entry.icon);
            }
            break;
        }
    }
    // A ROM the library hasn't scanned yet (freshly downloaded, or outside
    // the configured game directories) never matches library_entries_ at
    // all, so the icon/title stayed at their defaults above. Read them
    // straight from the ROM's own control data instead of depending on the
    // library ever having indexed this file.
    if (!matched_library_entry || game_icon_.isNull()) {
        QPixmap rom_icon;
        QString rom_title;
        if (ReadIconAndTitleFromRom(system_, rom_path, rom_icon, rom_title)) {
            if (!rom_icon.isNull() && game_icon_.isNull()) {
                SetGameIcon(rom_icon);
            }
            if (!matched_library_entry && !rom_title.trimmed().isEmpty()) {
                game_name = rom_title.trimmed();
                static const QRegularExpression kIllegal(QStringLiteral("[\\\\/:*?\"<>|]"));
                game_name.replace(kIllegal, QStringLiteral("_"));
            }
        }
    }

    // Recorded before the run rather than after: even a half-finished export
    // leaves buildable output here, and this is how the library later finds it.
    RememberOutputRoot(output_dir);

    export_button->setEnabled(false);
    progress_bar->setVisible(true);
    progress_bar->setValue(0);
    status_label->setText(tr("Preparing AOT export..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (backend == RecompileBackend::Ballistic) {
        status_label->setText(tr("Ballistic export is not wired yet; using Dynarmic export artifacts for this run."));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    try {
    // Step 1: Create temporary working directories
    const QString work_dir = output_dir + QDir::separator() +
                             QStringLiteral(".aot_work_") + game_name;
    const QString exefs_work = work_dir + QDir::separator() + QStringLiteral("exefs");
    // The AOT cache is generated straight into the package it belongs in
    // rather than into the work area. It is by far the largest thing an export
    // produces - about 6 GB of C for Smash Ultimate - and staging it only to
    // copy it across meant writing 12 GB and holding both at once, which is
    // where large exports were failing during packaging.
    const QString cache_work = AotCacheDirFor(output_dir, game_name, platform);
    QDir().mkpath(exefs_work);
    QDir().mkpath(cache_work);
    progress_bar->setValue(5);

    // Step 2: ExeFS extraction is handled inside RunAotPrecompile via VFS.
    // For extracted directories, we copy them to the work area here.
    status_label->setText(tr("Scanning for ExeFS content..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QFileInfo rom_fi(rom_path);
    if (rom_fi.isDir()) {
        const QString exefs_sub = rom_path + QDir::separator() + QStringLiteral("exefs");
        if (QDir(exefs_sub).exists()) {
            if (!CopyDirectoryRecursive(exefs_sub, exefs_work)) {
                throw std::runtime_error("Failed to copy ExeFS staging directory");
            }
        } else {
            if (!CopyDirectoryRecursive(rom_path, exefs_work)) {
                throw std::runtime_error("Failed to copy ROM directory into export staging area");
            }
        }
    }
    // For packaged ROM files (NSP/XCI/NCA), the AOT step uses VFS to extract ExeFS directly.
    progress_bar->setValue(15);

    // Step 3: AOT pre-compilation
    status_label->setText(tr("Running AOT pre-compilation (%1)...")
                              .arg(backend == RecompileBackend::Ballistic
                                       ? QStringLiteral("Ballistic")
                                       : QStringLiteral("Dynarmic")));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QString cache_result = RunAotPrecompile(exefs_work, cache_work, backend, game_name);
    if (cache_result.isEmpty()) {
        status_label->setText(tr("AOT pre-compilation failed."));
        progress_bar->setValue(0);
        export_button->setEnabled(true);
        emit ExportFinished(false, {});
        return;
    }
    progress_bar->setValue(40);

    // Step 4: Package native export artifacts
    status_label->setText(tr("Packaging native export artifacts..."));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const bool pkg_ok = PackageNativeExport(rom_path, cache_work, output_dir, game_name, platform);
    if (!pkg_ok) {
        status_label->setText(tr("Packaging failed."));
        progress_bar->setValue(0);
        export_button->setEnabled(true);
        emit ExportFinished(false, {});
        return;
    }
    progress_bar->setValue(75);

    // Step 5: Bundle portable data (save, shader, config)
    if ((include_save_data || include_shader_cache || include_custom_config) &&
        rom_program_id != 0) {
        status_label->setText(tr("Bundling portable data..."));
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        // Determine the package root (platform-specific)
        QString pkg_root;
        switch (platform) {
        case TargetPlatform::Windows:
            pkg_root = output_dir + QDir::separator() + game_name;
            break;
        case TargetPlatform::Linux:
            pkg_root = output_dir + QDir::separator() + game_name + QStringLiteral(".AppDir");
            break;
        case TargetPlatform::MacOS:
            pkg_root = output_dir + QDir::separator() + game_name + QStringLiteral(".app");
            break;
        }

        if (!CopyPortableSupportData(rom_program_id, pkg_root, include_save_data,
                                     include_shader_cache, include_custom_config)) {
            throw std::runtime_error("Failed to bundle portable support data");
        }
    }
    progress_bar->setValue(90);

    // Step 6: Clean up working directory
    QDir(work_dir).removeRecursively();
    progress_bar->setValue(100);

    // Determine final output path for display
    QString final_path;
    switch (platform) {
    case TargetPlatform::Windows:
        final_path = output_dir + QDir::separator() + game_name;
        break;
    case TargetPlatform::Linux:
        final_path = output_dir + QDir::separator() + game_name + QStringLiteral(".AppDir");
        break;
    case TargetPlatform::MacOS:
        final_path = output_dir + QDir::separator() + game_name + QStringLiteral(".app");
        break;
    }

    export_button->setEnabled(true);
    status_label->setText(tr("Export completed: %1").arg(final_path));
    emit ExportFinished(true, final_path);

    if (WantsCompiledOutput()) {
        QMessageBox::information(
            this, tr("AOT Export Complete"),
            tr("Game exported and compiled to a standalone executable at:\n%1\n\n"
               "The package contains:\n"
               "- %2.exe — the recompiled game, statically linked with suyu's HLE/GPU backend "
               "(no separate DLLs, no emulator installation required)\n"
               "- mods/ — drop patch/mod folders here\n"
               "- user/ — this export's own config, save data, and logs (independent of suyu's)\n"
               "- Runtime DLLs (FFmpeg, Vulkan, OpenSSL) alongside the exe\n\n"
               "Just run %2.exe.")
                .arg(final_path, game_name));
    } else {
        QMessageBox::information(
            this, tr("AOT Export Complete"),
            tr("Game exported as C source to:\n%1\n\n"
               "The package contains:\n"
               "- Recompiled C source (buildable standalone PC executable)\n"
               "- Runtime with save/load support (save_data/ directory next to exe)\n"
               "- Bundled data segments (text, rodata, data)\n"
               "- Build scripts for Windows (.cmd) and Unix (.sh)\n\n"
               "Run build_native_windows.cmd (or build_native_unix.sh) in aot_cache/recompiled/ to compile.\n"
               "The resulting executable runs independently — no emulator required.\n\n"
               "(Choose \"Build\" instead of \"Source\" as the Export Format to have suyu compile "
               "this for you automatically.)")
                .arg(final_path));
    }
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception during game export: {}", e.what());
        export_button->setEnabled(true);
        progress_bar->setValue(0);
        status_label->setText(tr("Export failed."));
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("An error occurred during game export:\n%1")
                                  .arg(QString::fromUtf8(e.what())));
        emit ExportFinished(false, {});
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception during game export");
        export_button->setEnabled(true);
        progress_bar->setValue(0);
        status_label->setText(tr("Export failed."));
        QMessageBox::critical(this, tr("Export Failed"),
                              tr("An unexpected error occurred during game export."));
        emit ExportFinished(false, {});
    }
}
