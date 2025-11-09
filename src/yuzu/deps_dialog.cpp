// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/deps_dialog.h"
#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QIcon>
#include <QPainter>
#include <QTableWidget>
#include <QTextEdit>
#include "dep_hashes.h"
#include "ui_deps_dialog.h"
#include <fmt/ranges.h>

DepsDialog::DepsDialog(QWidget* parent)
    : QDialog(parent)
    , ui{std::make_unique<Ui::DepsDialog>()}
{
    ui->setupUi(this);

    constexpr int rows = (int) Common::dep_hashes.size();
    ui->tableDeps->setRowCount(rows);

    QStringList labels;
    labels << tr("Dependency") << tr("Version");

    ui->tableDeps->setHorizontalHeaderLabels(labels);
    ui->tableDeps->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    ui->tableDeps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::Fixed);
    ui->tableDeps->horizontalHeader()->setMinimumSectionSize(200);

    for (int i = 0; i < rows; ++i) {
        const std::string name = Common::dep_names.at(i);
        const std::string sha = Common::dep_hashes.at(i);
        const std::string url = Common::dep_urls.at(i);

        std::string dependency = fmt::format("<a href=\"{}\">{}</a>", url, name);

        QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(dependency));
        QTableWidgetItem *shaItem = new QTableWidgetItem(QString::fromStdString(sha));

        ui->tableDeps->setItem(i, 0, nameItem);
        ui->tableDeps->setItem(i, 1, shaItem);
    }

    ui->tableDeps->setItemDelegateForColumn(0, new LinkItemDelegate(this));
}

DepsDialog::~DepsDialog() = default;

LinkItemDelegate::LinkItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void LinkItemDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    auto options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    QString html = index.data(Qt::DisplayRole).toString();
    doc.setHtml(html);

    options.text.clear();

    painter->save();
    painter->translate(options.rect.topLeft());
    doc.drawContents(painter, QRectF(0, 0, options.rect.width(), options.rect.height()));
    painter->restore();
}

QSize LinkItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}

bool LinkItemDelegate::editorEvent(QEvent *event,
                                   QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QString html = index.data(Qt::DisplayRole).toString();
            QTextDocument doc;
            doc.setHtml(html);
            doc.setTextWidth(option.rect.width());

            // this is kinda silly but it werks
            QAbstractTextDocumentLayout *layout = doc.documentLayout();

            QPoint pos = mouseEvent->pos() - option.rect.topLeft();
            int charPos = layout->hitTest(pos, Qt::ExactHit);

            if (charPos >= 0) {
                QTextCursor cursor(&doc);
                cursor.setPosition(charPos);

                QTextCharFormat format = cursor.charFormat();

                if (format.isAnchor()) {
                    QString href = format.anchorHref();
                    if (!href.isEmpty()) {
                        QDesktopServices::openUrl(QUrl(href));
                        return true;
                    }
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
