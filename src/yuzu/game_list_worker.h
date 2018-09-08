// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <QList>
#include <QObject>
#include <QRunnable>
#include <QString>

#include "common/common_types.h"

class QStandardItem;

namespace FileSys {
class NCA;
class VfsFilesystem;
} // namespace FileSys

/**
 * Asynchronous worker object for populating the game list.
 * Communicates with other threads through Qt's signal/slot system.
 */
class GameListWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    GameListWorker(
        std::shared_ptr<FileSys::VfsFilesystem> vfs, QString dir_path, bool deep_scan,
        const std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list);
    ~GameListWorker() override;

    /// Starts the processing of directory tree information.
    void run() override;

    /// Tells the worker that it should no longer continue processing. Thread-safe.
    void Cancel();

signals:
    /**
     * The `EntryReady` signal is emitted once an entry has been prepared and is ready
     * to be added to the game list.
     * @param entry_items a list with `QStandardItem`s that make up the columns of the new entry.
     */
    void EntryReady(QList<QStandardItem*> entry_items);

    /**
     * After the worker has traversed the game directory looking for entries, this signal is emitted
     * with a list of folders that should be watched for changes as well.
     */
    void Finished(QStringList watch_list);

private:
    void AddInstalledTitlesToGameList();
    void FillControlMap(const std::string& dir_path);
    void AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion = 0);

    std::shared_ptr<FileSys::VfsFilesystem> vfs;
    std::map<u64, std::shared_ptr<FileSys::NCA>> nca_control_map;
    QStringList watch_list;
    QString dir_path;
    bool deep_scan;
    const std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list;
    std::atomic_bool stop_processing;
};
