// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cmath>
#include <QPainter>
#include "yuzu/util/util.h"
#ifdef _WIN32
#include <windows.h>
#include "common/fs/file.h"
#endif

QFont GetMonospaceFont() {
    QFont font(QStringLiteral("monospace"));
    // Automatic fallback to a monospace font on on platforms without a font called "monospace"
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    return font;
}

QString ReadableByteSize(qulonglong size) {
    static constexpr std::array units{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    if (size == 0) {
        return QStringLiteral("0");
    }

    const int digit_groups = std::min(static_cast<int>(std::log10(size) / std::log10(1024)),
                                      static_cast<int>(units.size()));
    return QStringLiteral("%L1 %2")
        .arg(size / std::pow(1024, digit_groups), 0, 'f', 1)
        .arg(QString::fromUtf8(units[digit_groups]));
}

QPixmap CreateCirclePixmapFromColor(const QColor& color) {
    QPixmap circle_pixmap(16, 16);
    circle_pixmap.fill(Qt::transparent);
    QPainter painter(&circle_pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(color);
    painter.setBrush(color);
    painter.drawEllipse({circle_pixmap.width() / 2.0, circle_pixmap.height() / 2.0}, 7.0, 7.0);
    return circle_pixmap;
}

bool SaveIconToFile(const std::string_view path, const QImage& image) {
#if defined(WIN32)
#pragma pack(push, 2)
    struct IconDir {
        WORD id_reserved;
        WORD id_type;
        WORD id_count;
    };

    struct IconDirEntry {
        BYTE width;
        BYTE height;
        BYTE color_count;
        BYTE reserved;
        WORD planes;
        WORD bit_count;
        DWORD bytes_in_res;
        DWORD image_offset;
    };
#pragma pack(pop)

    QImage source_image = image.convertToFormat(QImage::Format_RGB32);
    constexpr int bytes_per_pixel = 4;
    const int image_size = source_image.width() * source_image.height() * bytes_per_pixel;

    BITMAPINFOHEADER info_header{};
    info_header.biSize = sizeof(BITMAPINFOHEADER), info_header.biWidth = source_image.width(),
    info_header.biHeight = source_image.height() * 2, info_header.biPlanes = 1,
    info_header.biBitCount = bytes_per_pixel * 8, info_header.biCompression = BI_RGB;

    const IconDir icon_dir{.id_reserved = 0, .id_type = 1, .id_count = 1};
    const IconDirEntry icon_entry{.width = static_cast<BYTE>(source_image.width()),
                                  .height = static_cast<BYTE>(source_image.height() * 2),
                                  .color_count = 0,
                                  .reserved = 0,
                                  .planes = 1,
                                  .bit_count = bytes_per_pixel * 8,
                                  .bytes_in_res =
                                      static_cast<DWORD>(sizeof(BITMAPINFOHEADER) + image_size),
                                  .image_offset = sizeof(IconDir) + sizeof(IconDirEntry)};

    Common::FS::IOFile icon_file(path, Common::FS::FileAccessMode::Write,
                                 Common::FS::FileType::BinaryFile);
    if (!icon_file.IsOpen()) {
        return false;
    }

    if (!icon_file.Write(icon_dir)) {
        return false;
    }
    if (!icon_file.Write(icon_entry)) {
        return false;
    }
    if (!icon_file.Write(info_header)) {
        return false;
    }

    for (int y = 0; y < image.height(); y++) {
        const auto* line = source_image.scanLine(source_image.height() - 1 - y);
        std::vector<u8> line_data(source_image.width() * bytes_per_pixel);
        std::memcpy(line_data.data(), line, line_data.size());
        if (!icon_file.Write(line_data)) {
            return false;
        }
    }
    icon_file.Close();

    return true;
#else
    return false;
#endif
}
