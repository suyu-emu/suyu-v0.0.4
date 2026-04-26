// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "suyu/programmer_environment.h"

// ---------------------------------------------------------------------------
// Stylesheet — Liquid Glass / Frutiger Aero dark theme
// ---------------------------------------------------------------------------
static QString BuildStyleSheet() {
    return QStringLiteral(
        /* ── Base ─────────────────────────────────────────────────────────── */
        "ProgrammerEnvironment {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgb(14,14,26), stop:1 rgb(9,9,18));"
        "  color: rgb(200,205,225);"
        "  font-family: 'Segoe UI', 'Arial';"
        "  font-size: 13px;"
        "}"

        /* ── Toolbar area ───────────────────────────────────────────────────── */
        "QWidget#toolbar_area {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(34,36,65,242), stop:0.5 rgba(24,26,50,242),"
        "    stop:1 rgba(18,20,42,242));"
        "  border-bottom: 1px solid rgba(100,115,215,0.32);"
        "}"

        /* ── Panel containers ───────────────────────────────────────────────── */
        "QWidget#left_panel, QWidget#center_panel,"
        "QWidget#game_view_widget, QWidget#right_panel {"
        "  background: rgba(18,20,40,220);"
        "  border: 1px solid rgba(100,115,215,0.22);"
        "  border-radius: 7px;"
        "}"
        "QWidget#game_view_content {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(10,11,24,255), stop:1 rgba(8,9,20,255));"
        "  border: none;"
        "}"

        /* ── Panel title labels ─────────────────────────────────────────────── */
        "QLabel#panel_title {"
        "  color: rgba(170,185,240,190);"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "  letter-spacing: 2px;"
        "  background: transparent;"
        "  padding: 3px 8px 3px 8px;"
        "}"
        "QLabel#app_title {"
        "  color: rgba(175,190,255,230);"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "  background: transparent;"
        "  padding: 0px 4px;"
        "}"
        "QLabel#status_label {"
        "  color: rgba(140,165,220,200);"
        "  font-size: 12px;"
        "  background: transparent;"
        "  padding: 0px 8px;"
        "}"

        /* ── Separator ──────────────────────────────────────────────────────── */
        "QFrame#vsep {"
        "  background: rgba(100,115,215,0.28);"
        "  border: none;"
        "  max-width: 1px;"
        "  min-width: 1px;"
        "}"

        /* ── Tree widget ────────────────────────────────────────────────────── */
        "QTreeWidget {"
        "  background: rgba(14,15,30,210);"
        "  alternate-background-color: rgba(16,18,35,200);"
        "  border: none;"
        "  border-radius: 0px;"
        "  color: rgba(190,200,230,220);"
        "  font-size: 13px;"
        "  outline: 0;"
        "  selection-background-color: rgba(137,180,250,0.26);"
        "  selection-color: rgba(220,228,255,255);"
        "}"
        "QTreeWidget::item {"
        "  padding: 3px 6px;"
        "  border-radius: 3px;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: rgba(137,180,250,0.12);"
        "}"
        "QTreeWidget::item:selected {"
        "  background: rgba(137,180,250,0.28);"
        "  border-left: 3px solid rgba(137,180,250,0.88);"
        "}"
        "QHeaderView::section {"
        "  background: rgba(18,20,42,200);"
        "  color: rgba(170,185,230,200);"
        "  border: none;"
        "  padding: 4px 6px;"
        "  font-size: 11px;"
        "}"

        /* ── Code editor ────────────────────────────────────────────────────── */
        "QTextEdit#code_editor {"
        "  background: rgba(11,11,21,250);"
        "  border: none;"
        "  color: rgba(205,214,244,255);"
        "  font-family: 'Consolas','Courier New',monospace;"
        "  font-size: 14px;"
        "  selection-background-color: rgba(98,115,152,175);"
        "  selection-color: rgba(255,255,255,245);"
        "}"

        /* ── Output / terminal text areas ───────────────────────────────────── */
        "QTextEdit#build_output, QTextEdit#problems_view,"
        "QTextEdit#terminal_view, QTextEdit#debug_console,"
        "QTextEdit#console_view, QTextEdit#hierarchy_view,"
        "QTextEdit#inspector_props {"
        "  background: rgba(10,11,22,215);"
        "  border: none;"
        "  color: rgba(185,198,225,220);"
        "  font-family: 'Consolas','Courier New',monospace;"
        "  font-size: 12px;"
        "}"

        /* ── Tab widgets ────────────────────────────────────────────────────── */
        "QTabWidget::pane {"
        "  background: transparent;"
        "  border: none;"
        "  border-top: 1px solid rgba(100,115,215,0.25);"
        "}"
        "QTabWidget::tab-bar { left: 4px; }"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: rgba(140,155,200,200);"
        "  padding: 6px 16px 5px 16px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  font-family: 'Segoe UI';"
        "  font-size: 12px;"
        "  min-width: 58px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: rgba(137,180,250,0.13);"
        "  color: rgba(218,228,255,255);"
        "  border-bottom: 2px solid rgba(137,180,250,0.92);"
        "  font-weight: 600;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: rgba(137,180,250,0.07);"
        "  color: rgba(190,210,245,230);"
        "  border-bottom: 2px solid rgba(137,180,250,0.28);"
        "}"
        "QTabBar::close-button {"
        "  image: none;"
        "  subcontrol-position: right;"
        "  margin: 2px;"
        "}"

        /* ── Scrollbars ─────────────────────────────────────────────────────── */
        "QScrollBar:vertical {"
        "  background: transparent; width: 7px; margin: 0; border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: rgba(137,180,250,0.28); border-radius: 3px; min-height: 22px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: rgba(137,180,250,0.55);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0; background: none;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: none;"
        "}"
        "QScrollBar:horizontal {"
        "  background: transparent; height: 7px; margin: 0; border: none;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: rgba(137,180,250,0.28); border-radius: 3px; min-width: 22px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: rgba(137,180,250,0.55);"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0; background: none;"
        "}"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        "  background: none;"
        "}"

        /* ── Splitter handles ───────────────────────────────────────────────── */
        "QSplitter::handle { background: rgba(255,255,255,0.04); }"
        "QSplitter::handle:hover { background: rgba(137,180,250,0.32); }"
        "QSplitter::handle:horizontal { width: 4px; }"
        "QSplitter::handle:vertical { height: 4px; }"

        /* ── Buttons (base) ─────────────────────────────────────────────────── */
        "QPushButton {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(52,56,92,222), stop:0.48 rgba(38,42,74,218),"
        "    stop:0.52 rgba(32,36,68,218), stop:1 rgba(28,32,60,225));"
        "  border: 1px solid rgba(105,122,205,0.42);"
        "  border-radius: 5px;"
        "  color: rgba(200,212,242,242);"
        "  padding: 5px 12px;"
        "  font-size: 12px;"
        "  font-family: 'Segoe UI';"
        "  min-width: 58px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(70,76,122,232), stop:1 rgba(48,54,100,232));"
        "  border: 1px solid rgba(137,180,250,0.66);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(26,28,56,232);"
        "  border: 1px solid rgba(137,180,250,0.42);"
        "}"
        "QPushButton:disabled {"
        "  color: rgba(100,110,148,150);"
        "  border-color: rgba(60,66,102,0.30);"
        "  background: rgba(24,26,50,180);"
        "}"

        /* ── Build button — warm red-orange ─────────────────────────────────── */
        "QPushButton#btn_build {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(218,98,78,222), stop:0.5 rgba(188,68,50,218),"
        "    stop:1 rgba(158,46,32,228));"
        "  border: 1px solid rgba(243,140,132,0.52);"
        "  color: rgba(255,236,232,252);"
        "  font-weight: 600;"
        "}"
        "QPushButton#btn_build:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(238,118,95,232), stop:1 rgba(198,78,58,232));"
        "  border: 1px solid rgba(243,140,132,0.88);"
        "}"
        "QPushButton#btn_build:pressed { background: rgba(158,48,36,242); }"
        "QPushButton#btn_build:disabled {"
        "  background: rgba(88,58,54,180);"
        "  border-color: rgba(148,98,88,0.30);"
        "  color: rgba(178,148,142,150);"
        "}"

        /* ── Debug button — cool blue ────────────────────────────────────────── */
        "QPushButton#btn_debug {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(78,138,218,222), stop:0.5 rgba(52,108,192,218),"
        "    stop:1 rgba(32,84,168,228));"
        "  border: 1px solid rgba(137,180,250,0.52);"
        "  color: rgba(218,235,255,252);"
        "  font-weight: 600;"
        "}"
        "QPushButton#btn_debug:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(98,158,242,232), stop:1 rgba(62,118,208,232));"
        "  border: 1px solid rgba(137,180,250,0.92);"
        "}"
        "QPushButton#btn_debug:pressed { background: rgba(38,88,172,242); }"
        "QPushButton#btn_debug:disabled {"
        "  background: rgba(48,74,114,180);"
        "  border-color: rgba(78,108,158,0.30);"
        "  color: rgba(138,165,208,150);"
        "}"

        /* ── Run button — vivid green ────────────────────────────────────────── */
        "QPushButton#btn_run {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(68,182,88,222), stop:0.5 rgba(46,152,66,218),"
        "    stop:1 rgba(28,122,46,228));"
        "  border: 1px solid rgba(166,227,161,0.52);"
        "  color: rgba(224,250,224,252);"
        "  font-weight: 600;"
        "}"
        "QPushButton#btn_run:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(88,208,108,232), stop:1 rgba(54,168,74,232));"
        "  border: 1px solid rgba(166,227,161,0.92);"
        "}"
        "QPushButton#btn_run:pressed { background: rgba(30,128,50,242); }"
        "QPushButton#btn_run:disabled {"
        "  background: rgba(44,84,50,180);"
        "  border-color: rgba(78,128,84,0.30);"
        "  color: rgba(138,185,140,150);"
        "}"

        /* ── Stop button — muted red ─────────────────────────────────────────── */
        "QPushButton#btn_stop {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(128,78,78,222), stop:1 rgba(98,52,52,228));"
        "  border: 1px solid rgba(200,130,130,0.42);"
        "  color: rgba(240,215,215,252);"
        "  font-weight: 600;"
        "}"
        "QPushButton#btn_stop:hover {"
        "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 rgba(158,98,98,232), stop:1 rgba(122,68,68,232));"
        "  border: 1px solid rgba(200,130,130,0.82);"
        "}"
        "QPushButton#btn_stop:pressed { background: rgba(98,48,48,242); }"

        /* ── ComboBox ────────────────────────────────────────────────────────── */
        "QComboBox {"
        "  background: rgba(28,30,58,212);"
        "  border: 1px solid rgba(105,122,205,0.42);"
        "  border-radius: 5px;"
        "  color: rgba(200,212,242,232);"
        "  padding: 4px 8px 4px 8px;"
        "  font-size: 12px;"
        "  min-width: 92px;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid rgba(137,180,250,0.66);"
        "}"
        "QComboBox::drop-down {"
        "  border: none; width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "  image: none;"
        "  border-left: 4px solid transparent;"
        "  border-right: 4px solid transparent;"
        "  border-top: 5px solid rgba(137,180,250,0.72);"
        "  margin-right: 6px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: rgba(20,22,46,248);"
        "  border: 1px solid rgba(105,122,205,0.52);"
        "  border-radius: 5px;"
        "  selection-background-color: rgba(137,180,250,0.32);"
        "  selection-color: rgba(218,228,255,255);"
        "  color: rgba(200,212,242,230);"
        "  outline: 0;"
        "}"

        /* ── Line edit (terminal input) ──────────────────────────────────────── */
        "QLineEdit {"
        "  background: rgba(10,12,22,200);"
        "  border: none;"
        "  border-top: 1px solid rgba(100,115,215,0.22);"
        "  color: rgba(200,225,175,232);"
        "  font-family: 'Consolas','Courier New',monospace;"
        "  font-size: 12px;"
        "  padding: 4px 8px;"
        "}"
        "QLineEdit:focus {"
        "  background: rgba(12,14,28,222);"
        "  border-top: 1px solid rgba(137,180,250,0.52);"
        "}"
    );
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
ProgrammerEnvironment::ProgrammerEnvironment(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumSize(920, 580);
    SetupUi();
}

ProgrammerEnvironment::~ProgrammerEnvironment() {
    if (build_process_ && build_process_->state() != QProcess::NotRunning) {
        build_process_->kill();
        build_process_->waitForFinished(2000);
    }
    if (run_process_ && run_process_->state() != QProcess::NotRunning) {
        run_process_->kill();
        run_process_->waitForFinished(2000);
    }
}

// ---------------------------------------------------------------------------
// SetupUi — full VS Code–like IDE layout
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::SetupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(4, 4, 4, 4);
    root_layout->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QWidget(this);
    toolbar->setObjectName(QStringLiteral("toolbar_area"));
    toolbar->setAttribute(Qt::WA_StyledBackground, true);
    toolbar->setFixedHeight(42);
    auto* tb_layout = new QHBoxLayout(toolbar);
    tb_layout->setContentsMargins(10, 0, 10, 0);
    tb_layout->setSpacing(6);

    // App title
    auto* app_title = new QLabel(QStringLiteral("Suyu Developer Suite"), toolbar);
    app_title->setObjectName(QStringLiteral("app_title"));
    tb_layout->addWidget(app_title);

    // Vertical separator helper
    auto addSep = [&]() {
        auto* sep = new QFrame(toolbar);
        sep->setObjectName(QStringLiteral("vsep"));
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setFixedHeight(22);
        tb_layout->addWidget(sep);
    };

    addSep();

    btn_open_project_ = new QPushButton(tr("Open..."), toolbar);
    btn_open_project_->setObjectName(QStringLiteral("btn_open_project"));
    btn_save_all_ = new QPushButton(tr("Save All"), toolbar);
    btn_save_all_->setObjectName(QStringLiteral("btn_save_all"));
    btn_compiler_options_ = new QPushButton(tr("Options"), toolbar);
    btn_compiler_options_->setObjectName(QStringLiteral("btn_compiler_options"));

    tb_layout->addWidget(btn_open_project_);
    tb_layout->addWidget(btn_save_all_);
    tb_layout->addWidget(btn_compiler_options_);

    addSep();

    btn_build_ = new QPushButton(tr("Build"), toolbar);
    btn_build_->setObjectName(QStringLiteral("btn_build"));
    btn_debug_ = new QPushButton(tr("Debug"), toolbar);
    btn_debug_->setObjectName(QStringLiteral("btn_debug"));
    btn_run_ = new QPushButton(tr("Run"), toolbar);
    btn_run_->setObjectName(QStringLiteral("btn_run"));
    btn_stop_ = new QPushButton(tr("Stop"), toolbar);
    btn_stop_->setObjectName(QStringLiteral("btn_stop"));
    btn_stop_->setEnabled(false);

    tb_layout->addWidget(btn_build_);
    tb_layout->addWidget(btn_debug_);
    tb_layout->addWidget(btn_run_);
    tb_layout->addWidget(btn_stop_);

    tb_layout->addStretch(1);

    build_config_ = new QComboBox(toolbar);
    build_config_->setObjectName(QStringLiteral("build_config"));
    build_config_->addItem(QStringLiteral("Debug"));
    build_config_->addItem(QStringLiteral("Release"));
    build_config_->addItem(QStringLiteral("RelWithDebInfo"));

    status_label_ = new QLabel(tr("Ready"), toolbar);
    status_label_->setObjectName(QStringLiteral("status_label"));

    tb_layout->addWidget(build_config_);
    tb_layout->addWidget(status_label_);

    root_layout->addWidget(toolbar);

    // ── Main vertical splitter (top content / bottom terminal) ────────────────
    auto* vsplit = new QSplitter(Qt::Vertical, this);
    vsplit->setChildrenCollapsible(false);

    // ── Top horizontal splitter (left / center / right) ──────────────────────
    auto* hsplit = new QSplitter(Qt::Horizontal, vsplit);
    hsplit->setChildrenCollapsible(false);

    // ── LEFT: file explorer ────────────────────────────────────────────────
    auto* left_panel = new QWidget(hsplit);
    left_panel->setObjectName(QStringLiteral("left_panel"));
    left_panel->setAttribute(Qt::WA_StyledBackground, true);
    left_panel->setMinimumWidth(160);
    auto* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(0);

    auto* left_title = new QLabel(tr("FILE EXPLORER"), left_panel);
    left_title->setObjectName(QStringLiteral("panel_title"));
    left_title->setFixedHeight(26);
    left_title->setAttribute(Qt::WA_StyledBackground, true);

    project_tree_ = new QTreeWidget(left_panel);
    project_tree_->setObjectName(QStringLiteral("project_tree"));
    project_tree_->setHeaderHidden(true);
    project_tree_->setRootIsDecorated(true);
    project_tree_->setExpandsOnDoubleClick(true);
    project_tree_->setAlternatingRowColors(true);

    left_layout->addWidget(left_title);
    left_layout->addWidget(project_tree_, 1);
    hsplit->addWidget(left_panel);

    // ── CENTER: code editor ────────────────────────────────────────────────
    auto* center_panel = new QWidget(hsplit);
    center_panel->setObjectName(QStringLiteral("center_panel"));
    center_panel->setAttribute(Qt::WA_StyledBackground, true);
    center_panel->setMinimumWidth(260);
    auto* center_layout = new QVBoxLayout(center_panel);
    center_layout->setContentsMargins(0, 0, 0, 0);
    center_layout->setSpacing(0);

    // File-tab title bar
    auto* center_titlebar = new QWidget(center_panel);
    center_titlebar->setObjectName(QStringLiteral("toolbar_area"));
    center_titlebar->setAttribute(Qt::WA_StyledBackground, true);
    center_titlebar->setFixedHeight(30);
    auto* ctb_layout = new QHBoxLayout(center_titlebar);
    ctb_layout->setContentsMargins(8, 0, 8, 0);
    ctb_layout->setSpacing(4);

    editor_title_ = new QLabel(tr("No file open"), center_titlebar);
    editor_title_->setObjectName(QStringLiteral("status_label"));
    ctb_layout->addWidget(editor_title_);
    ctb_layout->addStretch(1);

    code_editor_ = new QTextEdit(center_panel);
    code_editor_->setObjectName(QStringLiteral("code_editor"));
    code_editor_->setReadOnly(false);
    code_editor_->setTabStopDistance(28.0);
    code_editor_->setPlaceholderText(
        tr("Click a file in the explorer to open it."));

    center_layout->addWidget(center_titlebar);
    center_layout->addWidget(code_editor_, 1);
    hsplit->addWidget(center_panel);

    // ── RIGHT: vertical splitter (game view / inspector tabs) ─────────────
    auto* right_vsplit = new QSplitter(Qt::Vertical, hsplit);
    right_vsplit->setChildrenCollapsible(false);

    // Game view
    game_view_widget_ = new QWidget(right_vsplit);
    game_view_widget_->setObjectName(QStringLiteral("game_view_widget"));
    game_view_widget_->setAttribute(Qt::WA_StyledBackground, true);
    game_view_widget_->setMinimumHeight(130);
    auto* gv_layout = new QVBoxLayout(game_view_widget_);
    gv_layout->setContentsMargins(0, 0, 0, 0);
    gv_layout->setSpacing(0);

    auto* gv_header = new QWidget(game_view_widget_);
    gv_header->setObjectName(QStringLiteral("toolbar_area"));
    gv_header->setAttribute(Qt::WA_StyledBackground, true);
    gv_header->setFixedHeight(26);
    auto* gvh_layout = new QHBoxLayout(gv_header);
    gvh_layout->setContentsMargins(8, 0, 6, 0);
    gvh_layout->setSpacing(4);
    auto* gv_title_lbl = new QLabel(tr("GAME VIEW"), gv_header);
    gv_title_lbl->setObjectName(QStringLiteral("panel_title"));
    gvh_layout->addWidget(gv_title_lbl);
    gvh_layout->addStretch(1);
    auto* gv_dotbtn = new QPushButton(QStringLiteral("..."), gv_header);
    gv_dotbtn->setFixedSize(26, 20);
    gvh_layout->addWidget(gv_dotbtn);

    auto* gv_content = new QWidget(game_view_widget_);
    gv_content->setObjectName(QStringLiteral("game_view_content"));
    gv_content->setAttribute(Qt::WA_StyledBackground, true);
    auto* gvc_layout = new QVBoxLayout(gv_content);
    gvc_layout->setContentsMargins(8, 8, 8, 8);
    auto* gv_placeholder = new QLabel(
        tr("No game loaded\nRun a ROM to preview"), gv_content);
    gv_placeholder->setObjectName(QStringLiteral("status_label"));
    gv_placeholder->setAlignment(Qt::AlignCenter);
    gvc_layout->addWidget(gv_placeholder, 1, Qt::AlignCenter);

    gv_layout->addWidget(gv_header);
    gv_layout->addWidget(gv_content, 1);
    right_vsplit->addWidget(game_view_widget_);

    // Inspector / Hierarchy / Console tabs
    inspector_tabs_ = new QTabWidget(right_vsplit);
    inspector_tabs_->setObjectName(QStringLiteral("inspector_tabs"));
    inspector_tabs_->setMinimumHeight(100);

    inspector_props_ = new QTextEdit(inspector_tabs_);
    inspector_props_->setObjectName(QStringLiteral("inspector_props"));
    inspector_props_->setReadOnly(true);
    inspector_props_->setPlaceholderText(tr("No object selected"));
    inspector_tabs_->addTab(inspector_props_, tr("Inspector"));

    hierarchy_view_ = new QTextEdit(inspector_tabs_);
    hierarchy_view_->setObjectName(QStringLiteral("hierarchy_view"));
    hierarchy_view_->setReadOnly(true);
    hierarchy_view_->setPlaceholderText(tr("No hierarchy loaded"));
    inspector_tabs_->addTab(hierarchy_view_, tr("Hierarchy"));

    console_view_ = new QTextEdit(inspector_tabs_);
    console_view_->setObjectName(QStringLiteral("console_view"));
    console_view_->setReadOnly(true);
    inspector_tabs_->addTab(console_view_, tr("Console"));

    right_vsplit->addWidget(inspector_tabs_);
    right_vsplit->setSizes({260, 220});
    hsplit->addWidget(right_vsplit);

    // Horizontal split proportions: left 200 / center 520 / right 260
    hsplit->setSizes({200, 520, 260});
    vsplit->addWidget(hsplit);

    // ── BOTTOM: Problems / Output / Terminal / Debug Console ───────────────
    bottom_tabs_ = new QTabWidget(vsplit);
    bottom_tabs_->setObjectName(QStringLiteral("bottom_tabs"));
    bottom_tabs_->setMinimumHeight(90);

    // Problems
    problems_view_ = new QTextEdit(bottom_tabs_);
    problems_view_->setObjectName(QStringLiteral("problems_view"));
    problems_view_->setReadOnly(true);
    bottom_tabs_->addTab(problems_view_, tr("Problems"));

    // Output (build)
    build_output_ = new QTextEdit(bottom_tabs_);
    build_output_->setObjectName(QStringLiteral("build_output"));
    build_output_->setReadOnly(true);
    bottom_tabs_->addTab(build_output_, tr("Output"));

    // Terminal
    auto* term_widget = new QWidget(bottom_tabs_);
    term_widget->setAttribute(Qt::WA_StyledBackground, true);
    auto* term_layout = new QVBoxLayout(term_widget);
    term_layout->setContentsMargins(0, 0, 0, 0);
    term_layout->setSpacing(0);
    terminal_view_ = new QTextEdit(term_widget);
    terminal_view_->setObjectName(QStringLiteral("terminal_view"));
    terminal_view_->setReadOnly(true);
    terminal_input_ = new QLineEdit(term_widget);
    terminal_input_->setObjectName(QStringLiteral("terminal_input"));
    terminal_input_->setPlaceholderText(QStringLiteral("$ "));
    term_layout->addWidget(terminal_view_, 1);
    term_layout->addWidget(terminal_input_);
    bottom_tabs_->addTab(term_widget, tr("Terminal"));

    // Debug Console
    debug_console_ = new QTextEdit(bottom_tabs_);
    debug_console_->setObjectName(QStringLiteral("debug_console"));
    debug_console_->setReadOnly(true);
    bottom_tabs_->addTab(debug_console_, tr("Debug Console"));

    vsplit->addWidget(bottom_tabs_);
    vsplit->setSizes({560, 160});

    root_layout->addWidget(vsplit, 1);

    // ── Processes ─────────────────────────────────────────────────────────
    build_process_ = new QProcess(this);
    build_process_->setProcessChannelMode(QProcess::MergedChannels);
    run_process_ = new QProcess(this);

    // ── Signal connections ─────────────────────────────────────────────────
    connect(btn_build_,            &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnBuildClicked);
    connect(btn_debug_,            &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnDebugClicked);
    connect(btn_run_,              &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnRunClicked);
    connect(btn_stop_,             &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnStopClicked);
    connect(btn_open_project_,     &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnOpenProjectClicked);
    connect(btn_save_all_,         &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnSaveAllClicked);
    connect(btn_compiler_options_, &QPushButton::clicked,
            this, &ProgrammerEnvironment::OnCompilerOptionsClicked);
    connect(terminal_input_,       &QLineEdit::returnPressed,
            this, &ProgrammerEnvironment::OnTerminalInputSubmitted);
    connect(project_tree_,         &QTreeWidget::itemClicked,
            this, &ProgrammerEnvironment::OnFileSelected);

    connect(build_process_, &QProcess::readyReadStandardOutput, this, [this]() {
        AppendOutput(build_output_,
                     QString::fromUtf8(build_process_->readAllStandardOutput()),
                     QColor(0xa6, 0xad, 0xc8));
        bottom_tabs_->setCurrentWidget(build_output_);
    });

    connect(build_process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exit_code, QProcess::ExitStatus) {
                if (exit_code == 0) {
                    AppendOutput(build_output_,
                                 QStringLiteral("\n=== Build succeeded ===\n"),
                                 QColor(0xa6, 0xe3, 0xa1));
                    AppendOutput(problems_view_,
                                 QStringLiteral("[SUCCESS] Build completed without errors.\n"),
                                 QColor(0xa6, 0xe3, 0xa1));
                    status_label_->setText(tr("Build succeeded"));
                } else {
                    AppendOutput(build_output_,
                                 QStringLiteral("\n=== Build failed (exit %1) ===\n")
                                     .arg(exit_code),
                                 QColor(0xf3, 0x8b, 0xa8));
                    AppendOutput(problems_view_,
                                 QStringLiteral("[ERROR] Build failed with exit code %1.\n")
                                     .arg(exit_code),
                                 QColor(0xf3, 0x8b, 0xa8));
                    status_label_->setText(tr("Build failed"));
                }
                btn_build_->setEnabled(true);
                btn_debug_->setEnabled(true);
                btn_stop_->setEnabled(false);
            });

    connect(run_process_, &QProcess::started, this, [this]() {
        status_label_->setText(tr("Running..."));
        btn_stop_->setEnabled(true);
        btn_run_->setEnabled(false);
    });

    connect(run_process_,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exit_code, QProcess::ExitStatus) {
                btn_run_->setEnabled(true);
                btn_stop_->setEnabled(false);
                status_label_->setText(
                    exit_code == 0 ? tr("Done")
                                   : QStringLiteral("Exited (%1)").arg(exit_code));
            });

    // ── Apply stylesheet ───────────────────────────────────────────────────
    setStyleSheet(BuildStyleSheet());
}

// ---------------------------------------------------------------------------
// Public setters
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::SetProjectRoot(const QString& path) {
    project_root_ = path;
    project_tree_->clear();
    if (path.isEmpty())
        return;

    QFileInfo fi(path);
    auto* root = new QTreeWidgetItem(project_tree_);
    root->setText(0, fi.fileName());
    root->setData(0, Qt::UserRole, path);
    PopulateTree(root, path, 0);
    root->setExpanded(true);

    status_label_->setText(
        QStringLiteral("Project: %1").arg(fi.fileName()));

    // Sync hierarchy panel
    hierarchy_view_->clear();
    AppendOutput(hierarchy_view_,
                 QStringLiteral("Project root: %1\n").arg(path),
                 QColor(0x89, 0xb4, 0xfa));
}

void ProgrammerEnvironment::SetEmulatorPath(const QString& path) {
    emulator_path_ = path;
}

void ProgrammerEnvironment::SetRomPath(const QString& path) {
    rom_path_ = path;
}

// ---------------------------------------------------------------------------
// PopulateTree
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::PopulateTree(QTreeWidgetItem* parent,
                                         const QString& path, int depth) {
    if (depth >= kMaxTreeDepth)
        return;

    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::DirsFirst | QDir::Name);

    const QStringList skip{QStringLiteral("build"), QStringLiteral(".git"),
                            QStringLiteral("node_modules"),
                            QStringLiteral(".vs"),   QStringLiteral("__pycache__")};

    for (const QFileInfo& fi : dir.entryInfoList()) {
        if (fi.isHidden())
            continue;
        if (fi.isDir() && skip.contains(fi.fileName()))
            continue;

        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, fi.fileName());
        item->setData(0, Qt::UserRole, fi.absoluteFilePath());

        if (fi.isDir())
            PopulateTree(item, fi.absoluteFilePath(), depth + 1);
    }
}

// ---------------------------------------------------------------------------
// OnFileSelected
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnFileSelected() {
    auto* item = project_tree_->currentItem();
    if (!item)
        return;

    const QString path = item->data(0, Qt::UserRole).toString();
    QFileInfo fi(path);

    if (fi.isDir()) {
        item->setExpanded(!item->isExpanded());
        return;
    }

    constexpr qint64 kMaxBytes = 2 * 1024 * 1024;
    if (fi.size() > kMaxBytes) {
        status_label_->setText(tr("File too large to display"));
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        status_label_->setText(tr("Cannot open file"));
        return;
    }

    QTextStream ts(&f);
    code_editor_->setPlainText(ts.readAll());
    current_file_path_ = path;
    editor_title_->setText(fi.fileName());
    status_label_->setText(
        QStringLiteral("%1  (%2 KB)")
            .arg(fi.fileName())
            .arg(fi.size() / 1024));

    // Update inspector with file info
    const QString info =
        QStringLiteral("Name:     %1\nPath:     %2\nSize:     %3 bytes\n"
                        "Modified: %4\n")
            .arg(fi.fileName())
            .arg(fi.absoluteFilePath())
            .arg(fi.size())
            .arg(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    inspector_props_->setPlainText(info);
    inspector_tabs_->setCurrentWidget(inspector_props_);
}

// ---------------------------------------------------------------------------
// OnBuildClicked / OnDebugClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnBuildClicked() {
    debug_mode_ = false;
    StartBuildProcess();
}

void ProgrammerEnvironment::OnDebugClicked() {
    debug_mode_ = true;
    StartBuildProcess();
}

void ProgrammerEnvironment::StartBuildProcess() {
    if (project_root_.isEmpty()) {
        QMessageBox::warning(this, tr("No Project"),
                             tr("Open a project folder first."));
        return;
    }

    btn_build_->setEnabled(false);
    btn_debug_->setEnabled(false);
    btn_stop_->setEnabled(true);
    build_output_->clear();
    problems_view_->clear();
    status_label_->setText(tr("Building..."));
    bottom_tabs_->setCurrentWidget(build_output_);

    const QString build_type = debug_mode_
        ? QStringLiteral("Debug")
        : (build_config_ ? build_config_->currentText()
                         : QStringLiteral("Release"));

    const QDir project_dir(project_root_);
    const QString build_subdir = project_root_ + QStringLiteral("/build");

    QDir().mkpath(build_subdir);

    AppendOutput(build_output_,
                 QStringLiteral("=== CMake Configure (%1) ===\n").arg(build_type),
                 QColor(0x89, 0xb4, 0xfa));

    if (QFile::exists(project_root_ + QStringLiteral("/CMakeLists.txt"))) {
        auto* configure = new QProcess(this);
        configure->setWorkingDirectory(build_subdir);
        configure->setProcessChannelMode(QProcess::MergedChannels);

        connect(configure, &QProcess::readyReadStandardOutput, this,
                [this, configure]() {
                    AppendOutput(build_output_,
                                 QString::fromUtf8(configure->readAllStandardOutput()),
                                 QColor(0xa6, 0xad, 0xc8));
                });

        connect(configure,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, configure, build_subdir, build_type](int exit_code,
                                                             QProcess::ExitStatus) {
                    configure->deleteLater();
                    if (exit_code != 0) {
                        AppendOutput(build_output_,
                                     QStringLiteral("\n=== Configure failed (exit %1) ===\n")
                                         .arg(exit_code),
                                     QColor(0xf3, 0x8b, 0xa8));
                        btn_build_->setEnabled(true);
                        btn_debug_->setEnabled(true);
                        btn_stop_->setEnabled(false);
                        status_label_->setText(tr("Configure failed"));
                        return;
                    }

                    AppendOutput(build_output_,
                                 QStringLiteral("\n=== CMake Build (%1) ===\n").arg(build_type),
                                 QColor(0x89, 0xb4, 0xfa));
                    build_process_->setWorkingDirectory(build_subdir);
                    build_process_->start(
                        QStringLiteral("cmake"),
                        {QStringLiteral("--build"), QStringLiteral("."),
                         QStringLiteral("--parallel")});
                });

        configure->start(
            QStringLiteral("cmake"),
            {QStringLiteral(".."), QStringLiteral("-G"), QStringLiteral("Ninja"),
             QStringLiteral("-DCMAKE_BUILD_TYPE=") + build_type});
    } else {
        AppendOutput(build_output_,
                     QStringLiteral("=== Make Build ===\n"),
                     QColor(0x89, 0xb4, 0xfa));
        build_process_->setWorkingDirectory(project_root_);
        build_process_->start(QStringLiteral("make"), {QStringLiteral("-j")});
    }
}

// ---------------------------------------------------------------------------
// OnRunClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnRunClicked() {
    const QString exe = emulator_path_.isEmpty()
                            ? QCoreApplication::applicationFilePath()
                            : emulator_path_;

    if (rom_path_.isEmpty()) {
        AppendOutput(build_output_,
                     QStringLiteral("No ROM path set. Use SetRomPath() first.\n"),
                     QColor(0xfa, 0xb3, 0x87));
        bottom_tabs_->setCurrentWidget(build_output_);
        return;
    }

    run_process_->start(exe, {rom_path_});
}

// ---------------------------------------------------------------------------
// OnStopClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnStopClicked() {
    if (build_process_ && build_process_->state() != QProcess::NotRunning) {
        build_process_->kill();
        AppendOutput(build_output_,
                     QStringLiteral("\n=== Build killed by user ===\n"),
                     QColor(0xfa, 0xb3, 0x87));
        btn_build_->setEnabled(true);
        btn_debug_->setEnabled(true);
    }
    if (run_process_ && run_process_->state() != QProcess::NotRunning) {
        run_process_->kill();
    }
    btn_stop_->setEnabled(false);
    btn_run_->setEnabled(true);
    status_label_->setText(tr("Stopped"));
}

// ---------------------------------------------------------------------------
// OnOpenProjectClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnOpenProjectClicked() {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Open Project Folder"),
        project_root_.isEmpty() ? QDir::homePath() : project_root_,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty())
        SetProjectRoot(dir);
}

// ---------------------------------------------------------------------------
// OnSaveAllClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnSaveAllClicked() {
    if (current_file_path_.isEmpty()) {
        status_label_->setText(tr("Nothing to save"));
        return;
    }

    QFile f(current_file_path_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not write to %1").arg(current_file_path_));
        return;
    }

    QTextStream ts(&f);
    ts << code_editor_->toPlainText();
    status_label_->setText(
        QStringLiteral("Saved: %1").arg(QFileInfo(current_file_path_).fileName()));
    AppendOutput(build_output_,
                 QStringLiteral("[INFO] Saved %1\n").arg(current_file_path_),
                 QColor(0xa6, 0xe3, 0xa1));
}

// ---------------------------------------------------------------------------
// OnCompilerOptionsClicked
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnCompilerOptionsClicked() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Compiler Options"));
    dlg->setMinimumSize(420, 280);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(BuildStyleSheet() +
                       QStringLiteral("QDialog { background: rgb(14,14,26); }"));

    auto* layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto addRow = [&](const QString& label_text, const QString& value) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label_text, dlg);
        lbl->setObjectName(QStringLiteral("panel_title"));
        lbl->setFixedWidth(140);
        auto* edit = new QLineEdit(value, dlg);
        row->addWidget(lbl);
        row->addWidget(edit, 1);
        layout->addLayout(row);
    };

    addRow(tr("Build Type:"),
           build_config_ ? build_config_->currentText()
                         : QStringLiteral("Release"));
    addRow(tr("Generator:"), QStringLiteral("Ninja"));
    addRow(tr("CMake Args:"), QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"));
    addRow(tr("CMake Path:"), QStringLiteral("cmake"));
    addRow(tr("Make Path:"), QStringLiteral("make"));

    layout->addStretch(1);

    auto* btn_ok = new QPushButton(tr("Close"), dlg);
    btn_ok->setObjectName(QStringLiteral("btn_run"));
    connect(btn_ok, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(btn_ok, 0, Qt::AlignRight);

    dlg->exec();
}

// ---------------------------------------------------------------------------
// OnTerminalInputSubmitted
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::OnTerminalInputSubmitted() {
    const QString cmd = terminal_input_->text().trimmed();
    if (cmd.isEmpty())
        return;

    AppendOutput(terminal_view_,
                 QStringLiteral("$ %1\n").arg(cmd),
                 QColor(0x89, 0xdc, 0xeb));

    terminal_input_->clear();
    bottom_tabs_->setCurrentWidget(
        bottom_tabs_->widget(2)); // Terminal tab index

    auto* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    if (!project_root_.isEmpty())
        proc->setWorkingDirectory(project_root_);

    connect(proc, &QProcess::readyReadStandardOutput, this,
            [this, proc]() {
                AppendOutput(terminal_view_,
                             QString::fromUtf8(proc->readAllStandardOutput()),
                             QColor(0xca, 0xd3, 0xf5));
            });

    connect(proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int exit_code, QProcess::ExitStatus) {
                if (exit_code != 0) {
                    AppendOutput(terminal_view_,
                                 QStringLiteral("[exit %1]\n").arg(exit_code),
                                 QColor(0xf3, 0x8b, 0xa8));
                }
                proc->deleteLater();
            });

#ifdef Q_OS_WIN
    proc->start(QStringLiteral("cmd.exe"),
                {QStringLiteral("/C"), cmd});
#else
    proc->start(QStringLiteral("/bin/sh"),
                {QStringLiteral("-c"), cmd});
#endif
}

// ---------------------------------------------------------------------------
// AppendOutput — shared helper
// ---------------------------------------------------------------------------
void ProgrammerEnvironment::AppendOutput(QTextEdit* target,
                                          const QString& text,
                                          const QColor& color) {
    if (!target)
        return;
    target->setTextColor(color);
    target->insertPlainText(text);
    QScrollBar* sb = target->verticalScrollBar();
    if (sb)
        sb->setValue(sb->maximum());
}
