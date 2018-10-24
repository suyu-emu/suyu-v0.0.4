// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QAbstractListModel>
#include "video_core/debug_utils/debug_utils.h"

class BreakPointModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum {
        Role_IsEnabled = Qt::UserRole,
    };

    BreakPointModel(std::shared_ptr<Tegra::DebugContext> context, QObject* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void OnBreakPointHit(Tegra::DebugContext::Event event);
    void OnResumed();

private:
    static QString DebugContextEventToString(Tegra::DebugContext::Event event);

    std::weak_ptr<Tegra::DebugContext> context_weak;
    bool at_breakpoint;
    Tegra::DebugContext::Event active_breakpoint;
};
