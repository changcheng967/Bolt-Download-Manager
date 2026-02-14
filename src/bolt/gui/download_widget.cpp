// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/download_widget.hpp>
#include <bolt/gui/segment_widget.hpp>

#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>

namespace bolt::gui {

namespace {
constexpr int UPDATE_INTERVAL_MS = 100;
}

DownloadWidget::DownloadWidget(std::uint32_t id, QWidget* parent)
    : QWidget(parent)
    , id_(id)
    , engine_(std::make_unique<DownloadEngine>()) {

    setup_ui();

    // Set up progress callback
    engine_->callback([this](const DownloadProgress& p) {
        // Cache progress for UI updates (called from download thread)
        cached_progress_ = p;
    });

    // Update timer
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DownloadWidget::update_ui);
    timer->start(UPDATE_INTERVAL_MS);
}

DownloadWidget::~DownloadWidget() = default;

void DownloadWidget::setup_ui() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);

    // Filename and status row
    auto* top_layout = new QHBoxLayout();
    label_filename_ = new QLabel("Ready", this);
    label_filename_->setStyleSheet("font-size: 14px; font-weight: bold; color: #fff;");
    top_layout->addWidget(label_filename_);
    top_layout->addStretch();
    main_layout->addLayout(top_layout);

    // URL
    label_url_ = new QLabel(this);
    label_url_->setStyleSheet("color: #888; font-size: 11px;");
    label_url_->setWordWrap(true);
    main_layout->addWidget(label_url_);

    // Progress bar
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setRange(0, 10000);
    progress_bar_->setValue(0);
    progress_bar_->setTextVisible(true);
    progress_bar_->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background-color: #333;
            border-radius: 3px;
            height: 8px;
        }
        QProgressBar::chunk {
            background-color: #0078d4;
            border-radius: 3px;
        }
    )");
    main_layout->addWidget(progress_bar_);

    // Info row
    auto* info_layout = new QHBoxLayout();
    label_size_ = new QLabel("--", this);
    label_size_->setStyleSheet("color: #aaa; font-size: 11px;");
    info_layout->addWidget(label_size_);

    info_layout->addStretch();

    label_speed_ = new QLabel("Speed: --", this);
    label_speed_->setStyleSheet("color: #00ff00; font-size: 12px; font-weight: bold;");
    info_layout->addWidget(label_speed_);

    info_layout->addSpacing(20);

    label_eta_ = new QLabel("ETA: --", this);
    label_eta_->setStyleSheet("color: #aaa; font-size: 11px;");
    info_layout->addWidget(label_eta_);

    main_layout->addLayout(info_layout);

    // Segments area
    segments_container_ = new QWidget(this);
    segments_container_->setMaximumHeight(80);
    segments_container_->setStyleSheet("background-color: #252525; border-radius: 4px;");
    main_layout->addWidget(segments_container_);

    // Button row
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    btn_start_ = new QPushButton("Start", this);
    btn_start_->setStyleSheet(R"(
        QPushButton {
            background-color: #2a7a2a;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            color: #fff;
            font-weight: 500;
        }
        QPushButton:hover { background-color: #3a8a3a; }
        QPushButton:pressed { background-color: #1a6a1a; }
    )");
    connect(btn_start_, &QPushButton::clicked, this, &DownloadWidget::start);
    button_layout->addWidget(btn_start_);

    btn_pause_ = new QPushButton("Pause", this);
    btn_pause_->setStyleSheet(R"(
        QPushButton {
            background-color: #7a6a2a;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            color: #fff;
            font-weight: 500;
        }
        QPushButton:hover { background-color: #8a7a3a; }
        QPushButton:pressed { background-color: #6a5a1a; }
    )");
    btn_pause_->setEnabled(false);
    connect(btn_pause_, &QPushButton::clicked, this, &DownloadWidget::pause);
    button_layout->addWidget(btn_pause_);

    btn_resume_ = new QPushButton("Resume", this);
    btn_resume_->setStyleSheet(R"(
        QPushButton {
            background-color: #2a7a5a;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            color: #fff;
            font-weight: 500;
        }
        QPushButton:hover { background-color: #3a8a6a; }
        QPushButton:pressed { background-color: #1a6a4a; }
    )");
    btn_resume_->setEnabled(false);
    connect(btn_resume_, &QPushButton::clicked, this, &DownloadWidget::resume);
    button_layout->addWidget(btn_resume_);

    btn_cancel_ = new QPushButton("Cancel", this);
    btn_cancel_->setStyleSheet(R"(
        QPushButton {
            background-color: #7a2a2a;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            color: #fff;
            font-weight: 500;
        }
        QPushButton:hover { background-color: #8a3a3a; }
        QPushButton:pressed { background-color: #6a1a1a; }
    )");
    btn_cancel_->setEnabled(false);
    connect(btn_cancel_, &QPushButton::clicked, this, &DownloadWidget::cancel);
    button_layout->addWidget(btn_cancel_);

    main_layout->addLayout(button_layout);
}

void DownloadWidget::set_url(const QString& url) {
    url_ = url;
    label_url_->setText(url);

    // Set URL in engine
    auto result = engine_->set_url(url.toStdString());
    if (result) {
        // Extract filename from URL or use engine's filename
        QString filename = QString::fromStdString(engine_->filename());
        if (filename.isEmpty()) {
            QFileInfo info(url.split('/').last());
            filename = info.fileName();
        }
        if (filename.isEmpty()) {
            filename = "download";
        }
        label_filename_->setText(filename);
    } else {
        label_filename_->setText("Invalid URL");
    }
}

void DownloadWidget::set_output_path(const QString& path) {
    output_path_ = path;
    engine_->output_path(path.toStdString());
}

void DownloadWidget::start() {
    if (url_.isEmpty()) return;

    // Set default output path if not specified
    if (output_path_.isEmpty()) {
        QString download_dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        QString filename = QString::fromStdString(engine_->filename());
        if (filename.isEmpty()) {
            QFileInfo info(url_.split('/').last());
            filename = info.fileName();
        }
        output_path_ = download_dir + "/" + filename;
        engine_->output_path(output_path_.toStdString());
    }

    // Ensure directory exists
    QFileInfo info(output_path_);
    QDir().mkpath(info.absolutePath());

    auto err = engine_->start();
    if (err) {
        label_filename_->setText(QString("Error: %1").arg(err.message().c_str()));
        return;
    }

    label_filename_->setText(QString("Downloading: %1").arg(QString::fromStdString(engine_->filename())));
    btn_start_->setEnabled(false);
    btn_pause_->setEnabled(true);
    btn_cancel_->setEnabled(true);
}

void DownloadWidget::pause() {
    engine_->pause();
    btn_pause_->setEnabled(false);
    btn_resume_->setEnabled(true);
    label_filename_->setText("Paused");
}

void DownloadWidget::resume() {
    auto err = engine_->resume();
    if (!err) {
        btn_pause_->setEnabled(true);
        btn_resume_->setEnabled(false);
        label_filename_->setText(QString("Downloading: %1").arg(QString::fromStdString(engine_->filename())));
    }
}

void DownloadWidget::cancel() {
    engine_->cancel();
    btn_start_->setEnabled(true);
    btn_pause_->setEnabled(false);
    btn_resume_->setEnabled(false);
    btn_cancel_->setEnabled(false);
    label_filename_->setText("Cancelled");
}

bool DownloadWidget::is_active() const {
    auto state = engine_->state();
    return state == DownloadState::downloading ||
           state == DownloadState::preparing ||
           state == DownloadState::completing;
}

std::uint64_t DownloadWidget::current_speed() const {
    return cached_progress_.speed_bps;
}

void DownloadWidget::update_ui() {
    auto state = engine_->state();

    // Update from cached progress
    if (state == DownloadState::downloading || state == DownloadState::completing) {
        on_progress_update(cached_progress_);
    }

    // Handle state changes
    if (state == DownloadState::completed) {
        label_filename_->setText("Completed!");
        btn_start_->setEnabled(false);
        btn_pause_->setEnabled(false);
        btn_resume_->setEnabled(false);
        btn_cancel_->setEnabled(false);
        progress_bar_->setValue(10000);
        emit finished();
    } else if (state == DownloadState::failed) {
        label_filename_->setText("Failed");
        btn_start_->setEnabled(true);
        btn_pause_->setEnabled(false);
        btn_resume_->setEnabled(false);
        btn_cancel_->setEnabled(false);
    }
}

void DownloadWidget::on_progress_update(const DownloadProgress& progress) {
    // Update progress bar
    progress_bar_->setValue(static_cast<int>(progress.percent * 100));

    // Update speed label
    QString speed_str;
    if (progress.speed_bps >= 1024 * 1024) {
        speed_str = QString("%1 MB/s").arg(progress.speed_bps / (1024.0 * 1024), 0, 'f', 2);
    } else if (progress.speed_bps >= 1024) {
        speed_str = QString("%1 KB/s").arg(progress.speed_bps / 1024.0, 0, 'f', 1);
    } else {
        speed_str = QString("%1 B/s").arg(progress.speed_bps);
    }
    label_speed_->setText(QString("Speed: %1").arg(speed_str));

    std::uint64_t downloaded = progress.downloaded_bytes;
    std::uint64_t total = progress.total_bytes;

    QString size_str;
    if (total >= 1024 * 1024 * 1024) {
        size_str = QString("%1 / %2 GB")
            .arg(downloaded / (1024.0 * 1024 * 1024), 0, 'f', 2)
            .arg(total / (1024.0 * 1024 * 1024), 0, 'f', 2);
    } else if (total >= 1024 * 1024) {
        size_str = QString("%1 / %2 MB")
            .arg(downloaded / (1024.0 * 1024), 0, 'f', 1)
            .arg(total / (1024.0 * 1024), 0, 'f', 1);
    } else {
        size_str = QString("%1 / %2 KB")
            .arg(downloaded / 1024.0, 0, 'f', 0)
            .arg(total / 1024.0, 0, 'f', 0);
    }
    label_size_->setText(size_str);

    // Update ETA
    if (progress.eta_seconds > 0) {
        int secs = static_cast<int>(progress.eta_seconds);
        if (secs >= 3600) {
            label_eta_->setText(QString("ETA: %1h %2m").arg(secs / 3600).arg((secs % 3600) / 60));
        } else if (secs >= 60) {
            label_eta_->setText(QString("ETA: %1m %2s").arg(secs / 60).arg(secs % 60));
        } else {
            label_eta_->setText(QString("ETA: %1s").arg(secs));
        }
    } else {
        label_eta_->setText("ETA: --");
    }
}

} // namespace bolt::gui
