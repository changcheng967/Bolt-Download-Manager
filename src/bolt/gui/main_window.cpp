// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/main_window.hpp>
#include <bolt/gui/download_widget.hpp>
#include <bolt/gui/speed_graph.hpp>
#include <bolt/gui/add_dialog.hpp>
#include <bolt/gui/settings_dialog.hpp>
#include <bolt/gui/about_dialog.hpp>
#include <bolt/gui/tray_icon.hpp>
#include <bolt/version.hpp>
#include <bolt/core/download_engine.hpp>

#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>
#include <QClipboard>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QAction>
#include <QPainter>
#include <QStyleFactory>
#include <QDir>
#include <QTabWidget>
#include <QStackedWidget>
#include <QMimeData>
#include <QDropEvent>

namespace bolt::gui {

namespace {

constexpr int UPDATE_INTERVAL_MS = 100;
constexpr int STATUS_UPDATE_INTERVAL_MS = 500;

// Modern styling for buttons
QString modern_button_style(bool dark = true) {
    if (dark) {
        return R"(
            QPushButton {
                background-color: #3a3a3a;
                border: 1px solid #555;
                border-radius: 4px;
                padding: 6px 16px;
                color: #eee;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #4a4a4a;
                border-color: #666;
            }
            QPushButton:pressed {
                background-color: #2a2a2a;
            }
            QPushButton:disabled {
                background-color: #2a2a2a;
                color: #666;
                border-color: #333;
            }
        )";
    }
    return R"(
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 6px 16px;
            color: #222;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        QPushButton:disabled {
            background-color: #f5f5f5;
            color: #999;
        }
    )";
}

QString modern_list_style(bool dark = true) {
    if (dark) {
        return R"(
            QListWidget {
                background-color: #2a2a2a;
                border: none;
                outline: none;
            }
            QListWidget::item {
                padding: 8px;
                border-bottom: 1px solid #3a3a3a;
            }
            QListWidget::item:selected {
                background-color: #2a5a8a;
                color: #fff;
            }
            QListWidget::item:hover {
                background-color: #333;
            }
        )";
    }
    return R"(
        QListWidget {
            background-color: #fff;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid #eee;
        }
        QListWidget::item:selected {
            background-color: #0078d4;
            color: #fff;
        }
        QListWidget::item:hover {
            background-color: #f5f5f5;
        }
    )";
}

} // namespace

//=============================================================================
// MainWindow
//=============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    DownloadEngine::global_init();

    setWindowTitle(QString("Bolt Download Manager %1").arg(bolt::version.to_string().c_str()));
    resize(1000, 650);
    setAcceptDrops(true);

    apply_theme();
    setup_ui();
    setup_menu_bar();
    setup_toolbar();
    setup_status_bar();
    connect_signals();

    // Start clipboard monitoring
    auto* clipboard = QApplication::clipboard();
    connect(clipboard, &QClipboard::changed, this, &MainWindow::on_clipboard_changed);

    // Status update timer
    auto* status_timer = new QTimer(this);
    connect(status_timer, &QTimer::timeout, this, &MainWindow::update_status);
    status_timer->start(STATUS_UPDATE_INTERVAL_MS);

    load_downloads();
}

MainWindow::~MainWindow() {
    save_downloads();
    DownloadEngine::global_cleanup();
}

void MainWindow::setup_ui() {
    // Create central widget
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);

    // Create splitter
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left side: Download list
    download_list_ = new QListWidget(this);
    download_list_->setMinimumWidth(280);
    download_list_->setMaximumWidth(400);
    download_list_->setStyleSheet(modern_list_style(dark_theme_));
    splitter->addWidget(download_list_);

    // Right side: Tab widget with details and graph
    tab_widget_ = new QTabWidget(this);
    tab_widget_->setStyleSheet(R"(
        QTabWidget::pane {
            border: none;
            background-color: #1e1e1e;
        }
        QTabBar::tab {
            padding: 10px 20px;
            background-color: #2a2a2a;
            color: #aaa;
            border: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #1e1e1e;
            color: #fff;
        }
        QTabBar::tab:hover:!selected {
            background-color: #333;
        }
    )");
    splitter->addWidget(tab_widget_);

    // Add "Details" tab with QStackedWidget to show selected download
    details_stack_ = new QStackedWidget(this);
    auto* placeholder = new QLabel("Select a download to view details");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #666; font-size: 14px;");
    details_stack_->addWidget(placeholder);  // Index 0 = placeholder
    tab_widget_->addTab(details_stack_, "Details");

    // Add "Speed Graph" tab
    speed_graph_ = new SpeedGraph(this);
    tab_widget_->addTab(speed_graph_, "Speed Graph");

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 700});

    main_layout->addWidget(splitter);
}

void MainWindow::setup_menu_bar() {
    // File menu
    auto* file_menu = menuBar()->addMenu("&File");

    auto* add_action = file_menu->addAction("&Add Download...");
    add_action->setShortcut(QKeySequence::New);
    connect(add_action, &QAction::triggered, this, &MainWindow::on_add_download);

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction("E&xit");
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &MainWindow::close);

    // Downloads menu
    auto* dl_menu = menuBar()->addMenu("&Downloads");

    auto* start_action = dl_menu->addAction("&Start");
    start_action->setShortcut(QKeySequence("Ctrl+S"));
    connect(start_action, &QAction::triggered, this, &MainWindow::on_start_selected);

    auto* pause_action = dl_menu->addAction("&Pause");
    pause_action->setShortcut(QKeySequence("Ctrl+P"));
    connect(pause_action, &QAction::triggered, this, &MainWindow::on_pause_selected);

    auto* resume_action = dl_menu->addAction("&Resume");
    resume_action->setShortcut(QKeySequence("Ctrl+R"));
    connect(resume_action, &QAction::triggered, this, &MainWindow::on_resume_selected);

    dl_menu->addSeparator();

    auto* cancel_action = dl_menu->addAction("&Cancel");
    connect(cancel_action, &QAction::triggered, this, &MainWindow::on_cancel_selected);

    auto* remove_action = dl_menu->addAction("&Remove");
    remove_action->setShortcut(QKeySequence::Delete);
    connect(remove_action, &QAction::triggered, this, &MainWindow::on_remove_download);

    dl_menu->addSeparator();

    auto* open_action = dl_menu->addAction("Open &Folder");
    connect(open_action, &QAction::triggered, this, &MainWindow::on_open_folder);

    // Tools menu
    auto* tools_menu = menuBar()->addMenu("&Tools");
    auto* settings_action = tools_menu->addAction("&Settings...");
    connect(settings_action, &QAction::triggered, this, &MainWindow::on_settings);

    // Help menu
    auto* help_menu = menuBar()->addMenu("&Help");
    auto* about_action = help_menu->addAction("&About...");
    connect(about_action, &QAction::triggered, this, &MainWindow::on_about);
}

void MainWindow::setup_toolbar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setStyleSheet(R"(
        QToolBar {
            background-color: #2a2a2a;
            border: none;
            spacing: 4px;
            padding: 4px;
        }
        QToolBar::separator {
            width: 16px;
            background-color: #3a3a3a;
        }
    )");

    // Add buttons
    btn_add_ = new QPushButton("Add", this);
    btn_add_->setStyleSheet(modern_button_style(dark_theme_));
    connect(btn_add_, &QPushButton::clicked, this, &MainWindow::on_add_download);
    toolbar->addWidget(btn_add_);

    toolbar->addSeparator();

    btn_start_ = new QPushButton("Start", this);
    btn_start_->setStyleSheet(modern_button_style(dark_theme_));
    btn_start_->setEnabled(false);
    connect(btn_start_, &QPushButton::clicked, this, &MainWindow::on_start_selected);
    toolbar->addWidget(btn_start_);

    btn_pause_ = new QPushButton("Pause", this);
    btn_pause_->setStyleSheet(modern_button_style(dark_theme_));
    btn_pause_->setEnabled(false);
    connect(btn_pause_, &QPushButton::clicked, this, &MainWindow::on_pause_selected);
    toolbar->addWidget(btn_pause_);

    btn_resume_ = new QPushButton("Resume", this);
    btn_resume_->setStyleSheet(modern_button_style(dark_theme_));
    btn_resume_->setEnabled(false);
    connect(btn_resume_, &QPushButton::clicked, this, &MainWindow::on_resume_selected);
    toolbar->addWidget(btn_resume_);

    btn_cancel_ = new QPushButton("Cancel", this);
    btn_cancel_->setStyleSheet(modern_button_style(dark_theme_));
    btn_cancel_->setEnabled(false);
    connect(btn_cancel_, &QPushButton::clicked, this, &MainWindow::on_cancel_selected);
    toolbar->addWidget(btn_cancel_);

    toolbar->addSeparator();

    btn_remove_ = new QPushButton("Remove", this);
    btn_remove_->setStyleSheet(modern_button_style(dark_theme_));
    btn_remove_->setEnabled(false);
    connect(btn_remove_, &QPushButton::clicked, this, &MainWindow::on_remove_download);
    toolbar->addWidget(btn_remove_);

    toolbar->addSeparator();

    auto* btn_settings = new QPushButton("Settings", this);
    btn_settings->setStyleSheet(modern_button_style(dark_theme_));
    connect(btn_settings, &QPushButton::clicked, this, &MainWindow::on_settings);
    toolbar->addWidget(btn_settings);
}

void MainWindow::setup_status_bar() {
    status_downloads_ = new QLabel("Downloads: 0", this);
    status_speed_ = new QLabel("Speed: 0 B/s", this);
    status_active_ = new QLabel("Active: 0", this);

    statusBar()->setStyleSheet(R"(
        QStatusBar {
            background-color: #2a2a2a;
            color: #ccc;
            border-top: 1px solid #3a3a3a;
        }
    )");

    statusBar()->addWidget(status_downloads_);
    statusBar()->addPermanentWidget(status_active_);
    statusBar()->addPermanentWidget(status_speed_);
}

void MainWindow::connect_signals() {
    connect(download_list_, &QListWidget::itemSelectionChanged,
            this, [this]() {
                auto items = download_list_->selectedItems();
                if (!items.isEmpty()) {
                    on_download_selected(items.first());
                } else {
                    btn_start_->setEnabled(false);
                    btn_pause_->setEnabled(false);
                    btn_resume_->setEnabled(false);
                    btn_cancel_->setEnabled(false);
                    btn_remove_->setEnabled(false);
                }
            });

    connect(download_list_, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item) {
                if (item) {
                    auto* widget = get_download(item->data(Qt::UserRole).toUInt());
                    if (widget) {
                        // Show details in tab
                    }
                }
            });
}

void MainWindow::apply_theme() {
    QString qss;
    if (dark_theme_) {
        qss = R"(
            QMainWindow {
                background-color: #1e1e1e;
                color: #ccc;
            }
            QWidget {
                background-color: #1e1e1e;
                color: #ccc;
            }
            QMenuBar {
                background-color: #2a2a2a;
                color: #ccc;
                border-bottom: 1px solid #3a3a3a;
            }
            QMenuBar::item:selected {
                background-color: #3a3a3a;
            }
            QMenu {
                background-color: #2a2a2a;
                color: #ccc;
                border: 1px solid #3a3a3a;
            }
            QMenu::item:selected {
                background-color: #3a3a3a;
            }
        )";
    }
    setStyleSheet(qss);
}

void MainWindow::add_download(const QString& url, const QString& save_path) {
    auto id = next_download_id_++;

    auto* widget = new DownloadWidget(id, this);
    widget->set_url(url);
    if (!save_path.isEmpty()) {
        widget->set_output_path(save_path);
    }

    downloads_[id] = widget;

    auto* item = new QListWidgetItem();
    item->setText(QString("%1. %2").arg(id).arg(url));
    item->setData(Qt::UserRole, id);
    download_list_->addItem(item);

    download_list_->setCurrentItem(item);

    // Auto-start if under concurrent limit
    if (active_download_count() < max_concurrent_downloads_) {
        widget->start();
    }

    // Connect finished signal
    connect(widget, &DownloadWidget::finished, this, [this, id]() {
        on_download_finished(id);
    });
}

DownloadWidget* MainWindow::get_download(std::uint32_t id) const {
    auto it = downloads_.find(id);
    return it != downloads_.end() ? it->second : nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!tray_icon_) {
        event->accept();
        return;
    }

    // Minimize to tray instead
    hide();
    event->ignore();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    for (const auto& url : urls) {
        if (url.isLocalFile()) {
            continue; // Skip local files
        }
        add_download(url.toString());
    }
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange &&
        isMinimized() && tray_icon_) {
        QTimer::singleShot(0, this, [this]() { hide(); });
    }
}

void MainWindow::on_add_download() {
    if (!add_dialog_) {
        add_dialog_ = std::make_unique<AddDialog>(this);
    }

    if (add_dialog_->exec() == QDialog::Accepted) {
        auto [url, path] = add_dialog_->get_result();
        add_download(url, path);
    }
}

void MainWindow::on_remove_download() {
    auto* item = download_list_->currentItem();
    if (!item) return;

    auto id = item->data(Qt::UserRole).toUInt();
    auto* widget = get_download(id);
    if (widget) {
        downloads_.erase(id);
        delete widget;
    }

    delete item;
    update_status();
}

void MainWindow::on_start_selected() {
    auto* item = download_list_->currentItem();
    if (!item) return;

    auto* widget = get_download(item->data(Qt::UserRole).toUInt());
    if (widget) {
        widget->start();
    }
}

void MainWindow::on_pause_selected() {
    auto* item = download_list_->currentItem();
    if (!item) return;

    auto* widget = get_download(item->data(Qt::UserRole).toUInt());
    if (widget) {
        widget->pause();
    }
}

void MainWindow::on_resume_selected() {
    auto* item = download_list_->currentItem();
    if (!item) return;

    auto* widget = get_download(item->data(Qt::UserRole).toUInt());
    if (widget) {
        widget->resume();
    }
}

void MainWindow::on_cancel_selected() {
    auto* item = download_list_->currentItem();
    if (!item) return;

    auto* widget = get_download(item->data(Qt::UserRole).toUInt());
    if (widget) {
        widget->cancel();
    }
}

void MainWindow::on_open_folder() {
    // Open the download folder in explorer
}

void MainWindow::on_settings() {
    if (!settings_dialog_) {
        settings_dialog_ = std::make_unique<SettingsDialog>(this);
    }
    settings_dialog_->exec();
}

void MainWindow::on_about() {
    if (!about_dialog_) {
        about_dialog_ = std::make_unique<AboutDialog>(this);
    }
    about_dialog_->exec();
}

void MainWindow::on_download_selected(QListWidgetItem* item) {
    if (!item) return;

    btn_start_->setEnabled(true);
    btn_pause_->setEnabled(true);
    btn_resume_->setEnabled(true);
    btn_cancel_->setEnabled(true);
    btn_remove_->setEnabled(true);

    auto id = item->data(Qt::UserRole).toUInt();
    auto* widget = get_download(id);
    if (widget) {
        // Show download widget in details stack
        int idx = details_stack_->indexOf(widget);
        if (idx == -1) {
            // Not yet added, add it
            idx = details_stack_->addWidget(widget);
        }
        details_stack_->setCurrentIndex(idx);
    }
}

void MainWindow::on_download_finished(std::uint32_t id) {
    auto* widget = get_download(id);
    if (widget) {
        // Show notification
        if (tray_icon_) {
            tray_icon_->show_message("Download Complete",
                QString("Download %1 has finished").arg(id));
        }
    }

    // Start next queued download
    check_queue();
}

std::uint32_t MainWindow::active_download_count() const {
    std::uint32_t count = 0;
    for (const auto& [id, widget] : downloads_) {
        if (widget && widget->is_active()) {
            ++count;
        }
    }
    return count;
}

void MainWindow::check_queue() {
    // Count active downloads
    auto active = active_download_count();

    // If below max, start queued downloads
    if (active >= max_concurrent_downloads_) return;

    // Find queued (not started) downloads and start them
    for (auto& [id, widget] : downloads_) {
        if (widget && !widget->is_active()) {
            // This download is not active, check if it's queued (has URL but not started)
            widget->start();
            ++active;
            if (active >= max_concurrent_downloads_) break;
        }
    }
}

void MainWindow::on_clipboard_changed() {
    auto* clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    // Check if URL was copied
    if (text != last_clipboard_text_ &&
        (text.startsWith("http://") || text.startsWith("https://"))) {
        last_clipboard_text_ = text;

        // Check if URL looks like a file (has common file extensions)
        bool looks_like_file = text.contains(".zip", Qt::CaseInsensitive) ||
                               text.contains(".exe", Qt::CaseInsensitive) ||
                               text.contains(".msi", Qt::CaseInsensitive) ||
                               text.contains(".rar", Qt::CaseInsensitive) ||
                               text.contains(".7z", Qt::CaseInsensitive) ||
                               text.contains(".mp4", Qt::CaseInsensitive) ||
                               text.contains(".mkv", Qt::CaseInsensitive) ||
                               text.contains(".mp3", Qt::CaseInsensitive) ||
                               text.contains(".iso", Qt::CaseInsensitive) ||
                               text.contains(".bin", Qt::CaseInsensitive) ||
                               text.contains(".pdf", Qt::CaseInsensitive);

        if (looks_like_file) {
            // Show Add Download dialog pre-filled with URL
            if (!add_dialog_) {
                add_dialog_ = std::make_unique<AddDialog>(this);
            }
            add_dialog_->set_url(text);

            if (add_dialog_->exec() == QDialog::Accepted) {
                auto [url, path] = add_dialog_->get_result();
                add_download(url, path);
            }
        }
    }
}

void MainWindow::update_status() {
    std::uint32_t total = 0;
    std::uint32_t active = 0;
    std::uint64_t total_speed = 0;

    for (const auto& [id, widget] : downloads_) {
        ++total;
        if (widget->is_active()) {
            ++active;
            total_speed += widget->current_speed();
        }
    }

    status_downloads_->setText(QString("Downloads: %1").arg(total));
    status_active_->setText(QString("Active: %1").arg(active));

    // Format speed
    QString speed_str;
    if (total_speed >= 1024 * 1024 * 1024) {
        speed_str = QString("%1 GB/s").arg(total_speed / (1024.0 * 1024 * 1024), 0, 'f', 2);
    } else if (total_speed >= 1024 * 1024) {
        speed_str = QString("%1 MB/s").arg(total_speed / (1024.0 * 1024), 0, 'f', 2);
    } else if (total_speed >= 1024) {
        speed_str = QString("%1 KB/s").arg(total_speed / 1024.0, 0, 'f', 1);
    } else {
        speed_str = QString("%1 B/s").arg(total_speed);
    }
    status_speed_->setText(QString("Speed: %1").arg(speed_str));
}

void MainWindow::save_downloads() {
    // Save download queue to disk
}

void MainWindow::load_downloads() {
    // Load download queue from disk
}

} // namespace bolt::gui
