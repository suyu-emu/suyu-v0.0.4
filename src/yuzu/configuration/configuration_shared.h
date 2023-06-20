// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <memory>
#include <QString>
#include <QWidget>
#include <qobjectdefs.h>

class QObject;

namespace ConfigurationShared {

class Tab : public QWidget {
    Q_OBJECT

public:
    explicit Tab(std::shared_ptr<std::forward_list<Tab*>> group, QWidget* parent = nullptr);
    ~Tab();

    virtual void ApplyConfiguration() = 0;
    virtual void SetConfiguration() = 0;
};

} // namespace ConfigurationShared
