// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDir>
#include <QString>
#include "qt_common/qt_common.h"
#include <quazip.h>
#include <zlib.h>

/** This is a modified version of JlCompress **/
namespace QtCommon::Compress {

class Options {
public:
    /**
         * The enum values refer to the comments in the open function of the quazipfile.h file.
         *
         * The value is represented by two hexadecimal characters,
         * the left character indicating the compression method,
         * and the right character indicating the compression level.
         *
         * method == 0 indicates that the file is not compressed but rather stored as is.
         * method == 8(Z_DEFLATED) indicates that zlib compression is used.
         *
         * A higher value of level indicates a smaller size of the compressed file,
         * although it also implies more time consumed during the compression process.
         */
    enum CompressionStrategy
    {
        /// Storage without compression
        Storage  = 0x00, // Z_NO_COMPRESSION 0
        /// The fastest compression speed
        Fastest  = 0x81, // Z_BEST_SPEED 1
        /// Relatively fast compression speed
        Faster   = 0x83,
        /// Standard compression speed and ratio
        Standard = 0x86,
        /// Better compression ratio
        Better   = 0x87,
        /// The best compression ratio
        Best     = 0x89, // Z_BEST_COMPRESSION 9
        /// The default compression strategy, according to the open function of quazipfile.h,
        /// the value of method is Z_DEFLATED, and the value of level is Z_DEFAULT_COMPRESSION -1 (equals lvl 6)
        Default  = 0xff
    };

public:
    explicit Options(const CompressionStrategy& strategy)
        : m_compressionStrategy(strategy) {}

    explicit Options(const QDateTime& dateTime = QDateTime(), const CompressionStrategy& strategy = Default)
        : m_dateTime(dateTime), m_compressionStrategy(strategy) {}

    QDateTime getDateTime() const {
        return m_dateTime;
    }

    void setDateTime(const QDateTime &dateTime) {
        m_dateTime = dateTime;
    }

    CompressionStrategy getCompressionStrategy() const {
        return m_compressionStrategy;
    }

    int getCompressionMethod() const {
        return m_compressionStrategy != Default ? m_compressionStrategy >> 4 : Z_DEFLATED;
    }

    int getCompressionLevel() const {
        return m_compressionStrategy != Default ? m_compressionStrategy & 0x0f : Z_DEFAULT_COMPRESSION;
    }

    void setCompressionStrategy(const CompressionStrategy &strategy) {
        m_compressionStrategy = strategy;
    }

private:
    // If set, used as last modified on file inside the archive.
    // If compressing a directory, used for all files.
    QDateTime m_dateTime;
    CompressionStrategy m_compressionStrategy;
};

/**
 * @brief Compress an entire directory and report its progress.
 * @param fileCompressed Destination file
 * @param dir            The directory to compress
 * @param options        Compression level, etc
 * @param callback       Callback that takes in two std::size_t (total, progress) and returns false if the current operation should be cancelled.
 */
bool compressDir(QString fileCompressed,
                 QString dir,
                 const Options& options = Options(),
                 QtCommon::QtProgressCallback callback = {});

// Internal //
bool compressSubDir(QuaZip *zip,
                    QString dir,
                    QString origDir,
                    const Options &options,
                    std::size_t total,
                    std::size_t &progress,
                    QtCommon::QtProgressCallback callback);

bool compressFile(QuaZip *zip,
                  QString fileName,
                  QString fileDest,
                  const Options &options,
                  std::size_t total,
                  std::size_t &progress,
                  QtCommon::QtProgressCallback callback);

bool copyData(QIODevice &inFile,
              QIODevice &outFile,
              std::size_t total,
              std::size_t &progress,
              QtCommon::QtProgressCallback callback);

// Extract //

/**
 * @brief Extract a zip file and report its progress.
 * @param fileCompressed Compressed file
 * @param dir            The directory to push the results to
 * @param callback       Callback that takes in two std::size_t (total, progress) and returns false if the current operation should be cancelled.
 */
QStringList extractDir(QString fileCompressed, QString dir, QtCommon::QtProgressCallback callback = {});

// Internal //
QStringList extractDir(QuaZip &zip, const QString &dir, QtCommon::QtProgressCallback callback);

bool extractFile(QuaZip *zip,
                 QString fileName,
                 QString fileDest,
                 std::size_t total,
                 std::size_t &progress,
                 QtCommon::QtProgressCallback callback);

bool removeFile(QStringList listFile);

}
