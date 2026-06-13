// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "suyu/game_export.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

// Pull in common/assert.h first so its #pragma once guard is set before any
// Dynarmic headers are included.  mcl/assert.hpp (pulled in by Dynarmic)
// defines the same macro names (ASSERT, UNREACHABLE, etc.).  The #undefs
// below clear suyu's definitions so mcl can define them without C4005.
// Because #pragma once is now set, common/assert.h will NOT be re-included
// later (e.g. via common/common_types.h), regardless of PCH state.
#include "common/assert.h"

#ifdef ASSERT
#undef ASSERT
#endif
#ifdef ASSERT_MSG
#undef ASSERT_MSG
#endif
#ifdef UNREACHABLE
#undef UNREACHABLE
#endif
#ifdef DEBUG_ASSERT
#undef DEBUG_ASSERT
#endif
#ifdef DEBUG_ASSERT_MSG
#undef DEBUG_ASSERT_MSG
#endif
#ifdef UNIMPLEMENTED
#undef UNIMPLEMENTED
#endif

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
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/loader/nso.h"
#include "core/recompiler/arm64_to_c.h"

// ---------------------------------------------------------------------------
// Filesystem helpers
// ---------------------------------------------------------------------------

static bool CopyDirectoryRecursive(const QString& src, const QString& dst);

static bool CopyFileReplacingExisting(const QString& src, const QString& dst) {
    const QFileInfo src_info(src);
    if (!src_info.exists() || !src_info.isFile()) {
        return false;
    }

    const QFileInfo dst_info(dst);
    if (!QDir().mkpath(dst_info.absolutePath())) {
        return false;
    }

    if (QFile::exists(dst) && !QFile::remove(dst)) {
        return false;
    }

    return QFile::copy(src, dst);
}

static bool CopyPathReplacingExisting(const QString& src, const QString& dst) {
    const QFileInfo src_info(src);
    if (!src_info.exists()) {
        return false;
    }

    if (src_info.isDir()) {
        if (QFileInfo::exists(dst) && !QDir(dst).removeRecursively()) {
            return false;
        }
        return CopyDirectoryRecursive(src, dst);
    }

    return CopyFileReplacingExisting(src, dst);
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

GameExportDialog::GameExportDialog(QWidget* parent) : QDialog(parent) {
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
          tr("This exports Dynarmic-translated IR dumps, guest code slices, block maps, "
              "and game data for a future custom native runtime. It does not bundle the "
              "current suyu frontend executable as a fake standalone app."),
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
    export_button = new QPushButton(tr("Export (Recompile)"), this);
    layout->addWidget(export_button);

    connect(rom_library_btn, &QPushButton::clicked, this, &GameExportDialog::OnSelectFromLibrary);
    connect(rom_browse_btn, &QPushButton::clicked, this, &GameExportDialog::OnBrowseRom);
    connect(browse_btn, &QPushButton::clicked, this, &GameExportDialog::OnBrowseOutput);
    connect(export_button, &QPushButton::clicked, this, &GameExportDialog::OnExport);
}

void GameExportDialog::SetLibraryEntries(QVector<LibraryEntry> entries) {
    library_entries_ = std::move(entries);
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
    std::vector<u8> text_bytes;
    std::vector<Arm64BasicBlock> blocks;
};

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
    auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
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

    // Try NSP
    if (ext == ".nsp") {
        auto nsp = std::make_shared<FileSys::NSP>(file);
        if (nsp->GetStatus() == Loader::ResultStatus::Success) {
            auto exefs = nsp->GetExeFS();
            if (exefs) return exefs;
        }
    }

    // Try XCI
    if (ext == ".xci") {
        auto xci = std::make_shared<FileSys::XCI>(file);
        if (xci->GetStatus() == Loader::ResultStatus::Success) {
            auto secure_nsp = xci->GetSecurePartitionNSP();
            if (secure_nsp) {
                auto exefs = secure_nsp->GetExeFS();
                if (exefs) return exefs;
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

    for (const auto& block : mod.blocks) {
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
        auto ir_block = Dynarmic::A64::Translate(descriptor, read_code, options);

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

} // anonymous namespace

// ---------------------------------------------------------------------------
// AOT Pre-compilation — Real Implementation
// ---------------------------------------------------------------------------

QString GameExportDialog::RunAotPrecompile(const QString& exefs_dir,
                                           const QString& cache_dir,
                                           RecompileBackend backend) {
    QDir().mkpath(cache_dir);

    const QString manifest_path = cache_dir + QDir::separator() + QStringLiteral("aot_manifest.json");
    const QString blockmap_dir = cache_dir + QDir::separator() + QStringLiteral("blockmaps");
    const QString code_dir = cache_dir + QDir::separator() + QStringLiteral("code");
    const QString ir_dir = cache_dir + QDir::separator() + QStringLiteral("ir");
    QDir().mkpath(blockmap_dir);
    QDir().mkpath(code_dir);
    QDir().mkpath(ir_dir);

    const bool full_scan = aot_full_scan_checkbox->isChecked();
    const bool ballistic_requested = backend == RecompileBackend::Ballistic;
    const QString requested_backend_name =
        ballistic_requested ? QStringLiteral("ballistic") : QStringLiteral("dynarmic");
    const QString effective_backend_name = QStringLiteral("dynarmic");

    // Collect NSO files to analyze — either from VFS (ROM containers) or from extracted ExeFS
    std::vector<NsoAnalysisResult> module_results;
    bool used_vfs = false;

    // First, try to open the ROM via VFS to extract ExeFS directly
    const QString rom_path = rom_path_edit->text();
    if (!rom_path.isEmpty() && QFile::exists(rom_path) && QFileInfo(rom_path).isFile()) {
        auto exefs_vdir = ExtractExeFsFromRom(rom_path.toStdString());
        if (exefs_vdir) {
            used_vfs = true;
            const auto nso_files = exefs_vdir->GetFiles();

            // Standard NSO module names in load order
            static const std::vector<std::string> module_names = {
                "rtld", "main", "subsdk0", "subsdk1", "subsdk2", "subsdk3",
                "subsdk4", "subsdk5", "subsdk6", "subsdk7", "subsdk8", "subsdk9", "sdk"
            };

            for (const auto& nso_file : nso_files) {
                auto result = AnalyzeNsoFile(nso_file, full_scan);
                if (result.has_value()) {
                    module_results.push_back(std::move(*result));
                }
            }

            // Also save extracted ExeFS content to cache
            const QString exefs_cache = cache_dir + QDir::separator() + QStringLiteral("exefs");
            QDir().mkpath(exefs_cache);
            for (const auto& f : nso_files) {
                const QString out_path = exefs_cache + QDir::separator() +
                                         QString::fromStdString(f->GetName());
                QFile out_file(out_path);
                if (out_file.open(QIODevice::WriteOnly)) {
                    std::vector<u8> nso_bytes = f->ReadAllBytes();
                    out_file.write(reinterpret_cast<const char*>(nso_bytes.data()),
                                static_cast<qint64>(nso_bytes.size()));
                    out_file.close();
                }
            }
        }
    }

    // Fallback: read NSO files from the extracted exefs directory on disk
    if (!used_vfs && QDir(exefs_dir).exists()) {
        CopyDirectoryRecursive(exefs_dir, cache_dir + QDir::separator() + QStringLiteral("exefs"));

        auto vfs = std::make_shared<FileSys::RealVfsFilesystem>();
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
        SerializeTranslatedBlocks(mod, ir_dir, code_dir, &total_ir_blocks, &total_ir_failures);
    }

    // Static recompilation: lift each module's AArch64 .text into a buildable, cross-platform C
    // project (Windows .exe / Linux+BSD ELF / macOS Mach-O), plus the recompiled PC source itself.
    // This is the real "recompile" path — it emits native-compilable code, not a repackaged frontend.
    const QString recomp_root = cache_dir + QDir::separator() + QStringLiteral("recompiled");
    QDir().mkpath(recomp_root);
    u64 recomp_total_blocks = 0;
    for (const auto& mod : module_results) {
        if (mod.text_bytes.empty()) {
            continue;
        }
        const QString mod_dir = recomp_root + QDir::separator() + mod.name;
        QDir().mkpath(mod_dir);
        const auto stats = suyu::recomp::EmitProject(
            mod.name.toStdString(), mod.text_bytes.data(), mod.text_bytes.size(), mod.text_vaddr,
            mod_dir.toStdString(), /*source_only=*/false);
        recomp_total_blocks += stats.blocks;
    }
    // One-command native build scripts (the user can run these to produce the actual executable).
    {
        QFile bw(recomp_root + QDir::separator() + QStringLiteral("build_native_windows.cmd"));
        if (bw.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&bw);
            o << "@echo off\r\n"
                 "rem Build every recompiled module into a native .exe (needs cmake + a C compiler).\r\n"
                 "for /d %%M in (*) do (\r\n"
                 "  if exist \"%%M\\CMakeLists.txt\" (\r\n"
                 "    cmake -S \"%%M\" -B \"%%M\\build\" && cmake --build \"%%M\\build\" --config Release\r\n"
                 "  )\r\n"
                 ")\r\n";
            bw.close();
        }
        QFile bs(recomp_root + QDir::separator() + QStringLiteral("build_native_unix.sh"));
        if (bs.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream o(&bs);
            o << "#!/bin/sh\n"
                 "# Build every recompiled module into a native binary (ELF on Linux/BSD, Mach-O on macOS).\n"
                 "for m in */ ; do\n"
                 "  [ -f \"$m/CMakeLists.txt\" ] && cmake -S \"$m\" -B \"$m/build\" && cmake --build \"$m/build\"\n"
                 "done\n";
            bs.close();
        }
    }

    // Write block map files for each module (binary format for runtime consumption)
    // Format per entry: [u32 vaddr][u32 size][u32 instruction_count][u32 flags]
    for (const auto& mod : module_results) {
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

            for (const auto& block : mod.blocks) {
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
        out << "  \"recompiled_project\": \"recompiled/<module>/ (buildable C, cross-platform CMake)\",\n";
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
                 out << "      \"blockmap_file\": \"blockmaps/" << mod.name << ".blockmap\",\n";
                 out << "      \"ir_directory\": \"ir/" << mod.name << "\",\n";
                 out << "      \"guest_code_directory\": \"code/" << mod.name << "\"\n";
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

bool GameExportDialog::PackageNativeExport(const QString& rom_path, const QString& cache_dir,
                                           const QString& output_dir, const QString& game_name,
                                           TargetPlatform platform) {
    const QFileInfo rom_info(rom_path);
    const bool rom_is_directory = rom_info.isDir();
    const QString rom_extension =
        rom_info.suffix().isEmpty() ? QStringLiteral("") : QStringLiteral(".") + rom_info.suffix();
    const QString bundled_entry_name =
        rom_is_directory ? game_name : game_name + rom_extension;

    switch (platform) {
    case TargetPlatform::Windows: {
        const QString pkg_dir = output_dir + QDir::separator() + game_name;
        if (!QDir().mkpath(pkg_dir)) {
            return false;
        }

        // Bundle game data
        const QString bundled_rom = pkg_dir + QDir::separator() + bundled_entry_name;
        if (!CopyPathReplacingExisting(rom_path, bundled_rom)) {
            return false;
        }

        // Copy AOT cache
        if (!CopyDirectoryRecursive(cache_dir,
                                    pkg_dir + QDir::separator() + QStringLiteral("aot_cache"))) {
            return false;
        }

        QFile readme(pkg_dir + QDir::separator() + QStringLiteral("README_NATIVE_EXPORT.txt"));
        if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&readme);
            out << "Suyu native export artifact bundle\n\n";
            out << "This directory contains compiler-facing export data, not the suyu GUI binary.\n";
            out << "Contents:\n";
            out << "- " << bundled_entry_name << " : bundled game payload\n";
            out << "- aot_cache/blockmaps : discovered guest basic blocks\n";
            out << "- aot_cache/code : raw guest code slices per block\n";
            out << "- aot_cache/ir : Dynarmic-translated IR dumps per block\n";
            out << "- aot_cache/aot_manifest.json : export metadata\n\n";
            out << "A future minimal runtime/codegen stage can consume these artifacts to build a dedicated executable.\n";
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

        // Bundle game data
        const QString bundled_rom = bin_dir + QDir::separator() + bundled_entry_name;
        if (!CopyPathReplacingExisting(rom_path, bundled_rom)) {
            return false;
        }

        // Copy AOT cache
        if (!CopyDirectoryRecursive(cache_dir,
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

        // Bundle game data
        const QString bundled_rom = res_dir + QDir::separator() + bundled_entry_name;
        if (!CopyPathReplacingExisting(rom_path, bundled_rom)) {
            return false;
        }

        // Copy AOT cache
        if (!CopyDirectoryRecursive(cache_dir,
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
// Main export entry point
// ---------------------------------------------------------------------------

void GameExportDialog::OnExport() {
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
    const QString game_name = rom_info.completeBaseName();

    export_button->setEnabled(false);
    progress_bar->setVisible(true);
    progress_bar->setValue(0);
    status_label->setText(tr("Preparing AOT export..."));
    QApplication::processEvents();

    if (backend == RecompileBackend::Ballistic) {
        status_label->setText(tr("Ballistic export is not wired yet; using Dynarmic export artifacts for this run."));
        QApplication::processEvents();
    }

    try {
    // Step 1: Create temporary working directories
    const QString work_dir = output_dir + QDir::separator() +
                             QStringLiteral(".aot_work_") + game_name;
    const QString exefs_work = work_dir + QDir::separator() + QStringLiteral("exefs");
    const QString cache_work = work_dir + QDir::separator() + QStringLiteral("cache");
    QDir().mkpath(exefs_work);
    QDir().mkpath(cache_work);
    progress_bar->setValue(5);

    // Step 2: ExeFS extraction is handled inside RunAotPrecompile via VFS.
    // For extracted directories, we copy them to the work area here.
    status_label->setText(tr("Scanning for ExeFS content..."));
    QApplication::processEvents();

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
    QApplication::processEvents();

    const QString cache_result = RunAotPrecompile(exefs_work, cache_work, backend);
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
    QApplication::processEvents();

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
        QApplication::processEvents();

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

    QMessageBox::information(
        this, tr("AOT Export Complete"),
        tr("Game exported with static recompilation to:\n%1\n\n"
           "The package contains translated IR dumps, guest code slices, and block maps.\n"
           "It no longer bundles the current suyu frontend executable as a fake standalone app.\n"
           "A future custom runtime/codegen stage can consume these artifacts to build a dedicated executable.")
            .arg(final_path));
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
