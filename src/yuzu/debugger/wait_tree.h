// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <QAbstractItemModel>
#include <QDockWidget>
#include <QTreeView>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

class EmuThread;

namespace Kernel {
class HandleTable;
class ReadableEvent;
class WaitObject;
class Thread;
} // namespace Kernel

class WaitTreeThread;

class WaitTreeItem : public QObject {
    Q_OBJECT
public:
    WaitTreeItem();
    ~WaitTreeItem() override;

    virtual bool IsExpandable() const;
    virtual std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const;
    virtual QString GetText() const = 0;
    virtual QColor GetColor() const;

    void Expand();
    WaitTreeItem* Parent() const;
    const std::vector<std::unique_ptr<WaitTreeItem>>& Children() const;
    std::size_t Row() const;
    static std::vector<std::unique_ptr<WaitTreeThread>> MakeThreadItemList();

private:
    std::size_t row;
    bool expanded = false;
    WaitTreeItem* parent = nullptr;
    std::vector<std::unique_ptr<WaitTreeItem>> children;
};

class WaitTreeText : public WaitTreeItem {
    Q_OBJECT
public:
    explicit WaitTreeText(QString text);
    ~WaitTreeText() override;

    QString GetText() const override;

private:
    QString text;
};

class WaitTreeExpandableItem : public WaitTreeItem {
    Q_OBJECT
public:
    WaitTreeExpandableItem();
    ~WaitTreeExpandableItem() override;

    bool IsExpandable() const override;
};

class WaitTreeMutexInfo : public WaitTreeExpandableItem {
    Q_OBJECT
public:
    explicit WaitTreeMutexInfo(VAddr mutex_address, const Kernel::HandleTable& handle_table);
    ~WaitTreeMutexInfo() override;

    QString GetText() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;

private:
    VAddr mutex_address;
    u32 mutex_value;
    Kernel::Handle owner_handle;
    Kernel::SharedPtr<Kernel::Thread> owner;
};

class WaitTreeCallstack : public WaitTreeExpandableItem {
    Q_OBJECT
public:
    explicit WaitTreeCallstack(const Kernel::Thread& thread);
    ~WaitTreeCallstack() override;

    QString GetText() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;

private:
    const Kernel::Thread& thread;
};

class WaitTreeWaitObject : public WaitTreeExpandableItem {
    Q_OBJECT
public:
    explicit WaitTreeWaitObject(const Kernel::WaitObject& object);
    ~WaitTreeWaitObject() override;

    static std::unique_ptr<WaitTreeWaitObject> make(const Kernel::WaitObject& object);
    QString GetText() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;

protected:
    const Kernel::WaitObject& object;
};

class WaitTreeObjectList : public WaitTreeExpandableItem {
    Q_OBJECT
public:
    WaitTreeObjectList(const std::vector<Kernel::SharedPtr<Kernel::WaitObject>>& list,
                       bool wait_all);
    ~WaitTreeObjectList() override;

    QString GetText() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;

private:
    const std::vector<Kernel::SharedPtr<Kernel::WaitObject>>& object_list;
    bool wait_all;
};

class WaitTreeThread : public WaitTreeWaitObject {
    Q_OBJECT
public:
    explicit WaitTreeThread(const Kernel::Thread& thread);
    ~WaitTreeThread() override;

    QString GetText() const override;
    QColor GetColor() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;
};

class WaitTreeEvent : public WaitTreeWaitObject {
    Q_OBJECT
public:
    explicit WaitTreeEvent(const Kernel::ReadableEvent& object);
    ~WaitTreeEvent() override;
};

class WaitTreeThreadList : public WaitTreeExpandableItem {
    Q_OBJECT
public:
    explicit WaitTreeThreadList(const std::vector<Kernel::SharedPtr<Kernel::Thread>>& list);
    ~WaitTreeThreadList() override;

    QString GetText() const override;
    std::vector<std::unique_ptr<WaitTreeItem>> GetChildren() const override;

private:
    const std::vector<Kernel::SharedPtr<Kernel::Thread>>& thread_list;
};

class WaitTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit WaitTreeModel(QObject* parent = nullptr);
    ~WaitTreeModel() override;

    QVariant data(const QModelIndex& index, int role) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    void ClearItems();
    void InitItems();

private:
    std::vector<std::unique_ptr<WaitTreeThread>> thread_items;
};

class WaitTreeWidget : public QDockWidget {
    Q_OBJECT

public:
    explicit WaitTreeWidget(QWidget* parent = nullptr);
    ~WaitTreeWidget() override;

public slots:
    void OnDebugModeEntered();
    void OnDebugModeLeft();

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

private:
    QTreeView* view;
    WaitTreeModel* model;
};
