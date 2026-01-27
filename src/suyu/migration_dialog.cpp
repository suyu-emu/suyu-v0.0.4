#include "migration_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

MigrationDialog::MigrationDialog(
    QWidget *parent)
    : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_text = new QLabel(this);
    m_boxes = new QVBoxLayout;
    m_buttons = new QHBoxLayout;

    layout->addWidget(m_text, 1);
    layout->addLayout(m_boxes, 1);
    layout->addLayout(m_buttons, 1);
}

MigrationDialog::~MigrationDialog() {
    m_boxes->deleteLater();
    m_buttons->deleteLater();
}

void MigrationDialog::setText(
    const QString &text)
{
    m_text->setText(text);
}

void MigrationDialog::addBox(
    QWidget *box)
{
    m_boxes->addWidget(box);

}

QAbstractButton *MigrationDialog::addButton(
    const QString &text, const bool reject)
{
    QAbstractButton *button = new QPushButton(this);
    button->setText(text);
    m_buttons->addWidget(button, 1);

    connect(button, &QAbstractButton::clicked, this, [this, button, reject]() {
        m_clickedButton = button;

        if (reject) {
            this->reject();
        } else {
            this->accept();
        }
    });
    return button;
}

QAbstractButton *MigrationDialog::clickedButton() const
{
    return m_clickedButton;
}
