// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <bolt/core/download_engine.hpp>
#include <QWidget>
#include <memory>

// Import DownloadProgress for use in this header
using bolt::core::DownloadProgress;
using bolt::core::DownloadEngine;
using bolt::core::DownloadState;

class QProgressBar;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;

namespace bolt::gui {

class SegmentWidget;

// Widget for displaying and controlling a single download
class DownloadWidget : public QWidget {
    Q_OBJECT

public:
    explicit DownloadWidget(std::uint32_t id, QWidget* parent = nullptr);
    ~DownloadWidget() override;

    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }

    void set_url(const QString& url);
    void set_output_path(const QString& path);

    // Getters
    [[nodiscard]] QString url() const noexcept { return url_; }
    [[nodiscard]] QString output_path() const noexcept { return output_path_; }
    [[nodiscard]] DownloadState state() const noexcept { return engine_->state(); }

    // Control methods
    void start();
    void pause();
    void resume();
    void cancel();

    // State queries
    [[nodiscard]] bool is_active() const;
    [[nodiscard]] std::uint64_t current_speed() const;

signals:
    void finished();
    void failed(const QString& error);
    void progress_changed(std::uint64_t downloaded, std::uint64_t total, std::uint64_t speed);

private slots:
    void update_ui();
    void on_progress_update(const DownloadProgress& progress);

private:
    void setup_ui();

    std::uint32_t id_;
    std::unique_ptr<DownloadEngine> engine_;
    DownloadProgress cached_progress_;  // Thread-safe progress cache

    // UI elements
    QLabel* label_filename_{nullptr};
    QLabel* label_url_{nullptr};
    QLabel* label_size_{nullptr};
    QLabel* label_speed_{nullptr};
    QLabel* label_eta_{nullptr};
    QProgressBar* progress_bar_{nullptr};

    // Segment display
    QWidget* segments_container_{nullptr};
    std::vector<SegmentWidget*> segment_widgets_;

    // Actions
    QPushButton* btn_start_{nullptr};
    QPushButton* btn_pause_{nullptr};
    QPushButton* btn_resume_{nullptr};
    QPushButton* btn_cancel_{nullptr};
    QPushButton* btn_remove_{nullptr};

    QString url_;
    QString output_path_;
};

} // namespace bolt::gui
