// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "compress.h"
#include "quazipfileinfo.h"

#include <QDirIterator>
#include <quazipfile.h>

/** This is a modified version of JlCompress **/
namespace QtCommon::Compress {

bool compressDir(QString fileCompressed,
                 QString dir,
                 const Options &options,
                 QtCommon::QtProgressCallback callback)
{
    // Create zip
    QuaZip zip(fileCompressed);
    QDir().mkpath(QFileInfo(fileCompressed).absolutePath());
    if (!zip.open(QuaZip::mdCreate)) {
        QFile::remove(fileCompressed);
        return false;
    }

    // See how big the overall fs structure is
    // good approx. of total progress
    // TODO(crueter): QDirListing impl... or fs::recursive_dir_iterator
    QDirIterator iter(dir,
                      QDir::NoDotAndDotDot | QDir::Hidden | QDir::Files,
                      QDirIterator::Subdirectories);

    std::size_t total = 0;
    while (iter.hasNext()) {
        total += iter.nextFileInfo().size();
    }

    std::size_t progress = 0;
    callback(total, progress);

    // Add the files and subdirectories
    if (!compressSubDir(&zip, dir, dir, options, total, progress, callback)) {
        QFile::remove(fileCompressed);
        return false;
    }

    // Close zip
    zip.close();
    if (zip.getZipError() != 0) {
        QFile::remove(fileCompressed);
        return false;
    }

    return true;
}

bool compressSubDir(QuaZip *zip,
                    QString dir,
                    QString origDir,
                    const Options &options,
                    std::size_t total,
                    std::size_t &progress,
                    QtProgressCallback callback)
{
    // zip: object where to add the file
    // dir: current real directory
    // origDir: original real directory
    // (path(dir)-path(origDir)) = path inside the zip object

    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdCreate && zip->getMode() != QuaZip::mdAppend
        && zip->getMode() != QuaZip::mdAdd)
        return false;

    QDir directory(dir);
    if (!directory.exists())
        return false;


    QDir origDirectory(origDir);
    if (dir != origDir) {
        QuaZipFile dirZipFile(zip);
        std::unique_ptr<QuaZipNewInfo> qzni;
        qzni = std::make_unique<QuaZipNewInfo>(origDirectory.relativeFilePath(dir)
                                                   + QLatin1String("/"),
                                               dir);
        if (!dirZipFile.open(QIODevice::WriteOnly, *qzni, nullptr, 0, 0)) {
            return false;
        }
        dirZipFile.close();
    }

    // For each subfolder
    QFileInfoList subfiles = directory.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot
                                                     | QDir::Hidden | QDir::Dirs);
    for (const auto &file : std::as_const(subfiles)) {
        if (!compressSubDir(
                zip, file.absoluteFilePath(), origDir, options, total, progress, callback)) {
            return false;
        }
    }

    // For each file in directory
    QFileInfoList files = directory.entryInfoList(QDir::Hidden | QDir::Files);
    for (const auto &file : std::as_const(files)) {
        // If it's not a file or it's the compressed file being created
        if (!file.isFile() || file.absoluteFilePath() == zip->getZipName())
            continue;

        // Create relative name for the compressed file
        QString filename = origDirectory.relativeFilePath(file.absoluteFilePath());

        // Compress the file
        if (!compressFile(zip, file.absoluteFilePath(), filename, options, total, progress, callback)) {
            return false;
        }
    }

    return true;
}

bool compressFile(QuaZip *zip,
                  QString fileName,
                  QString fileDest,
                  const Options &options,
                  std::size_t total,
                  std::size_t &progress,
                  QtCommon::QtProgressCallback callback)
{
    // zip: object where to add the file
    // fileName: real file name
    // fileDest: file name inside the zip object

    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdCreate && zip->getMode() != QuaZip::mdAppend
        && zip->getMode() != QuaZip::mdAdd)
        return false;

    QuaZipFile outFile(zip);
    if (options.getDateTime().isNull()) {
        if (!outFile.open(QIODevice::WriteOnly,
                          QuaZipNewInfo(fileDest, fileName),
                          nullptr,
                          0,
                          options.getCompressionMethod(),
                          options.getCompressionLevel()))
            return false;
    } else {
        if (!outFile.open(QIODevice::WriteOnly,
                          QuaZipNewInfo(fileDest, fileName),
                          nullptr,
                          0,
                          options.getCompressionMethod(),
                          options.getCompressionLevel()))
            return false;
    }

    QFileInfo input(fileName);
    if (quazip_is_symlink(input)) {
        // Not sure if we should use any specialized codecs here.
        // After all, a symlink IS just a byte array. And
        // this is mostly for Linux, where UTF-8 is ubiquitous these days.
        QString path = quazip_symlink_target(input);
        QString relativePath = input.dir().relativeFilePath(path);
        outFile.write(QFile::encodeName(relativePath));
    } else {
        QFile inFile;
        inFile.setFileName(fileName);
        if (!inFile.open(QIODevice::ReadOnly)) {
            return false;
        }
        if (!copyData(inFile, outFile, total, progress, callback) || outFile.getZipError() != UNZ_OK) {
            return false;
        }
        inFile.close();
    }

    outFile.close();
    return outFile.getZipError() == UNZ_OK;
}

bool copyData(QIODevice &inFile,
              QIODevice &outFile,
              std::size_t total,
              std::size_t &progress,
              QtProgressCallback callback)
{
    while (!inFile.atEnd()) {
        char buf[4096];
        qint64 readLen = inFile.read(buf, 4096);
        if (readLen <= 0)
            return false;
        if (outFile.write(buf, readLen) != readLen)
            return false;

        progress += readLen;
        if (!callback(total, progress)) {
            return false;
        }
    }
    return true;
}

QStringList extractDir(QString fileCompressed, QString dir, QtCommon::QtProgressCallback callback)
{
    // Open zip
    QuaZip zip(fileCompressed);
    return extractDir(zip, dir, callback);
}

QStringList extractDir(QuaZip &zip, const QString &dir, QtCommon::QtProgressCallback callback)
{
    if (!zip.open(QuaZip::mdUnzip)) {
        return QStringList();
    }
    QString cleanDir = QDir::cleanPath(dir);
    QDir directory(cleanDir);
    QString absCleanDir = directory.absolutePath();
    if (!absCleanDir.endsWith(QLatin1Char('/'))) // It only ends with / if it's the FS root.
        absCleanDir += QLatin1Char('/');
    QStringList extracted;
    if (!zip.goToFirstFile()) {
        return QStringList();
    }

    std::size_t total = 0;
    for (const QuaZipFileInfo64 &info : zip.getFileInfoList64()) {
        total += info.uncompressedSize;
    }

    std::size_t progress = 0;
    callback(total, progress);

    do {
        QString name = zip.getCurrentFileName();
        QString absFilePath = directory.absoluteFilePath(name);
        QString absCleanPath = QDir::cleanPath(absFilePath);
        if (!absCleanPath.startsWith(absCleanDir))
            continue;
        if (!extractFile(&zip, QLatin1String(""), absFilePath, total, progress, callback)) {
            removeFile(extracted);
            return QStringList();
        }
        extracted.append(absFilePath);
    } while (zip.goToNextFile());

    // Close zip
    zip.close();
    if (zip.getZipError() != 0) {
        removeFile(extracted);
        return QStringList();
    }

    return extracted;
}

bool extractFile(QuaZip *zip,
                 QString fileName,
                 QString fileDest,
                 std::size_t total,
                 std::size_t &progress,
                 QtCommon::QtProgressCallback callback)
{
    // zip: object where to add the file
    // filename: real file name
    // fileincompress: file name of the compressed file

    if (!zip)
        return false;
    if (zip->getMode() != QuaZip::mdUnzip)
        return false;

    if (!fileName.isEmpty())
        zip->setCurrentFile(fileName);
    QuaZipFile inFile(zip);
    if (!inFile.open(QIODevice::ReadOnly) || inFile.getZipError() != UNZ_OK)
        return false;

    // Check existence of resulting file
    QDir curDir;
    if (fileDest.endsWith(QLatin1String("/"))) {
        if (!curDir.mkpath(fileDest)) {
            return false;
        }
    } else {
        if (!curDir.mkpath(QFileInfo(fileDest).absolutePath())) {
            return false;
        }
    }

    QuaZipFileInfo64 info;
    if (!zip->getCurrentFileInfo(&info))
        return false;

    QFile::Permissions srcPerm = info.getPermissions();
    if (fileDest.endsWith(QLatin1String("/")) && QFileInfo(fileDest).isDir()) {
        if (srcPerm != 0) {
            QFile(fileDest).setPermissions(srcPerm);
        }
        return true;
    }

    if (info.isSymbolicLink()) {
        QString target = QFile::decodeName(inFile.readAll());
        return QFile::link(target, fileDest);
    }

    // Open resulting file
    QFile outFile;
    outFile.setFileName(fileDest);
    if (!outFile.open(QIODevice::WriteOnly))
        return false;

    // Copy data
    if (!copyData(inFile, outFile, total, progress, callback) || inFile.getZipError() != UNZ_OK) {
        outFile.close();
        removeFile(QStringList(fileDest));
        return false;
    }
    outFile.close();

    // Close file
    inFile.close();
    if (inFile.getZipError() != UNZ_OK) {
        removeFile(QStringList(fileDest));
        return false;
    }

    if (srcPerm != 0) {
        outFile.setPermissions(srcPerm);
    }
    return true;
}

bool removeFile(QStringList listFile)
{
    bool ret = true;
    // For each file
    for (int i = 0; i < listFile.count(); i++) {
        // Remove
        ret = ret && QFile::remove(listFile.at(i));
    }
    return ret;
}

} // namespace QtCommon::Compress
