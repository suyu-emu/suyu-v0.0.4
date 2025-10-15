// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDir>
#include <QString>
#include <JlCompress.h>
#include "qt_common/qt_common.h"

/** This is a modified version of JlCompress **/
namespace QtCommon::Compress {

/**
 * @brief Compress an entire directory and report its progress.
 * @param fileCompressed Destination file
 * @param dir            The directory to compress
 * @param options        Compression level, etc
 * @param callback       Callback that takes in two std::size_t (total, progress) and returns false if the current operation should be cancelled.
 */
bool compressDir(QString fileCompressed,
                 QString dir,
                 const JlCompress::Options& options = JlCompress::Options(),
                 QtCommon::QtProgressCallback callback = {});

// Internal //
bool compressSubDir(QuaZip *zip,
                    QString dir,
                    QString origDir,
                    const JlCompress::Options &options,
                    std::size_t total,
                    std::size_t &progress,
                    QtCommon::QtProgressCallback callback);

bool compressFile(QuaZip *zip,
                  QString fileName,
                  QString fileDest,
                  const JlCompress::Options &options,
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
