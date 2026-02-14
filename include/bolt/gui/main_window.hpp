// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/download_engine.hpp>
#include <QMainWindow>
#include <memory>
#include <map>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QSystemTrayIcon;
class QMenu;
class QProgressBar;
class QTabWidget;

namespace bolt::gui {

class DownloadWidget;
class SpeedGraph;
class AddDialog;
class SettingsDialog;
class AboutDialog;
class TrayIcon;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Add a new download
    void add_download(const QString& url, const QString& save_path = {});

    // Get download widget by ID
    [[nodiscard]] DownloadWidget* get_download(std::uint32_t id) const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void on_add_download();
    void on_remove_download();
    void on_start_selected();
    void on_pause_selected();
    void on_resume_selected();
    void on_cancel_selected();
    void on_open_folder();
    void on_settings();
    void on_about();
    void on_download_selected(QListWidgetItem* item);
    void on_download_finished(std::uint32_t id);

    void on_clipboard_changed();

private:
    void setup_ui();
    void setup_menu_bar();
    void setup_toolbar();
    void setup_status_bar();
    void connect_signals();
    void apply_theme();

    // Update overall status
    void update_status();

    // Auto-save downloads
    void save_downloads();
    void load_downloads();

    // UI Components
    QListWidget* download_list_{nullptr};
    QTabWidget* tab_widget_{nullptr};

    // Action buttons
    QPushButton* btn_add_{nullptr};
    QPushButton* btn_start_{nullptr};
    QPushButton* btn_pause_{nullptr};
    QPushButton* btn_resume_{nullptr};
    QPushButton* btn_cancel_{nullptr};
    QPushButton* btn_remove_{nullptr};

    // Status bar
    QLabel* status_downloads_{nullptr};
    QLabel* status_speed_{nullptr};
    QLabel* status_active_{nullptr};

    // Tray icon
    std::unique_ptr<TrayIcon> tray_icon_;

    // Dialogs
    std::unique_ptr<AddDialog> add_dialog_;
    std::unique_ptr<SettingsDialog> settings_dialog_;
    std::unique_ptr<AboutDialog> about_dialog_;

    // Downloads map
    std::map<std::uint32_t, DownloadWidget*> downloads_;
    std::uint32_t next_download_id_{1};

    // Download queue settings
    std::uint32_t max_concurrent_downloads_{3};

    // Queue management
    void check_queue();
    [[nodiscard]] std::uint32_t active_download_count() const;

    // Clipboard monitoring
    QString last_clipboard_text_;

    // Theme
    bool dark_theme_{true};
};

} // namespace bolt::gui
