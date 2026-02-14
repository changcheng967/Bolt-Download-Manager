// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/settings_dialog.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>

namespace bolt::gui {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle("Settings");
    resize(600, 450);

    auto* layout = new QVBoxLayout(this);

    tabs_ = new QTabWidget(this);
    tabs_->setStyleSheet(R"(
        QTabWidget::pane {
            border: none;
            background-color: #1e1e1e;
        }
        QTabBar::tab {
            padding: 10px 20px;
            background-color: #2a2a2a;
            color: #aaa;
            border: none;
        }
        QTabBar::tab:selected {
            background-color: #1e1e1e;
            color: #fff;
        }
    )");

    setup_general_tab();
    setup_connection_tab();
    setup_appearance_tab();

    layout->addWidget(tabs_);

    // Buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    auto* btn_ok = new QPushButton("OK", this);
    btn_ok->setStyleSheet(R"(
        QPushButton {
            background-color: #0078d4;
            border: none;
            border-radius: 4px;
            padding: 8px 24px;
            color: #fff;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #1084e8; }
    )");
    connect(btn_ok, &QPushButton::clicked, this, [this]() {
        apply_settings();
        accept();
    });
    button_layout->addWidget(btn_ok);

    auto* btn_cancel = new QPushButton("Cancel", this);
    btn_cancel->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 8px 24px;
            color: #ccc;
        }
        QPushButton:hover { background-color: #4a4a4a; }
    )");
    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(btn_cancel);

    layout->addLayout(button_layout);
}

SettingsDialog::~SettingsDialog() = default;

int SettingsDialog::max_concurrent_downloads() const {
    return spin_simultaneous_ ? spin_simultaneous_->value() : 3;
}

int SettingsDialog::max_segments() const {
    return spin_max_segments_ ? spin_max_segments_->value() : 16;
}

bool SettingsDialog::clipboard_monitor_enabled() const {
    return check_clipboard_monitor_ ? check_clipboard_monitor_->isChecked() : true;
}

bool SettingsDialog::dark_theme_enabled() const {
    return check_dark_theme_ ? check_dark_theme_->isChecked() : true;
}

void SettingsDialog::load_settings() {
    QSettings settings("changcheng967", "BoltDM");

    if (spin_simultaneous_) {
        spin_simultaneous_->setValue(settings.value("maxConcurrentDownloads", 3).toInt());
    }
    if (spin_max_segments_) {
        spin_max_segments_->setValue(settings.value("maxSegments", 16).toInt());
    }
    if (check_clipboard_monitor_) {
        check_clipboard_monitor_->setChecked(settings.value("clipboardMonitor", true).toBool());
    }
    if (check_dark_theme_) {
        check_dark_theme_->setChecked(settings.value("darkTheme", true).toBool());
    }
}

void SettingsDialog::save_settings() {
    QSettings settings("changcheng967", "BoltDM");

    if (spin_simultaneous_) {
        settings.setValue("maxConcurrentDownloads", spin_simultaneous_->value());
    }
    if (spin_max_segments_) {
        settings.setValue("maxSegments", spin_max_segments_->value());
    }
    if (check_clipboard_monitor_) {
        settings.setValue("clipboardMonitor", check_clipboard_monitor_->isChecked());
    }
    if (check_dark_theme_) {
        settings.setValue("darkTheme", check_dark_theme_->isChecked());
    }
}

void SettingsDialog::setup_general_tab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    auto* group = new QGroupBox("General", this);
    group->setStyleSheet(R"(
        QGroupBox {
            color: #ccc;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            margin-top: 12px;
            padding: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px;
        }
    )");

    auto* form = new QFormLayout(group);

    check_start_minimized_ = new QCheckBox("Start minimized to tray", this);
    check_start_minimized_->setStyleSheet("color: #ccc;");
    form->addRow("", check_start_minimized_);

    check_clipboard_monitor_ = new QCheckBox("Monitor clipboard for URLs", this);
    check_clipboard_monitor_->setStyleSheet("color: #ccc;");
    check_clipboard_monitor_->setChecked(true);
    form->addRow("", check_clipboard_monitor_);

    check_confirm_exit_ = new QCheckBox("Confirm when closing with active downloads", this);
    check_confirm_exit_->setStyleSheet("color: #ccc;");
    check_confirm_exit_->setChecked(true);
    form->addRow("", check_confirm_exit_);

    spin_simultaneous_ = new QSpinBox(this);
    spin_simultaneous_->setRange(1, 50);
    spin_simultaneous_->setValue(8);
    spin_simultaneous_->setStyleSheet(R"(
        QSpinBox {
            background-color: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 6px;
            color: #ccc;
        }
    )");
    form->addRow("Maximum simultaneous downloads:", spin_simultaneous_);

    layout->addWidget(group);
    layout->addStretch();
    tabs_->addTab(widget, "General");
}

void SettingsDialog::setup_connection_tab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    auto* group = new QGroupBox("Connection Settings", this);
    group->setStyleSheet(R"(
        QGroupBox {
            color: #ccc;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            margin-top: 12px;
            padding: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px;
        }
    )");

    auto* form = new QFormLayout(group);

    spin_max_segments_ = new QSpinBox(this);
    spin_max_segments_->setRange(1, 16);
    spin_max_segments_->setValue(8);
    spin_max_segments_->setStyleSheet(R"(
        QSpinBox {
            background-color: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 6px;
            color: #ccc;
        }
    )");
    form->addRow("Maximum segments per download:", spin_max_segments_);

    spin_connection_timeout_ = new QSpinBox(this);
    spin_connection_timeout_->setRange(10, 120);
    spin_connection_timeout_->setValue(30);
    spin_connection_timeout_->setSuffix(" s");
    spin_connection_timeout_->setStyleSheet(R"(
        QSpinBox {
            background-color: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 6px;
            color: #ccc;
        }
    )");
    form->addRow("Connection timeout:", spin_connection_timeout_);

    spin_retry_count_ = new QSpinBox(this);
    spin_retry_count_->setRange(0, 10);
    spin_retry_count_->setValue(3);
    spin_retry_count_->setStyleSheet(R"(
        QSpinBox {
            background-color: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 6px;
            color: #ccc;
        }
    )");
    form->addRow("Retry failed segments:", spin_retry_count_);

    check_http2_ = new QCheckBox("Use HTTP/2 multiplexing", this);
    check_http2_->setStyleSheet("color: #ccc;");
    check_http2_->setChecked(true);
    form->addRow("", check_http2_);

    layout->addWidget(group);
    layout->addStretch();
    tabs_->addTab(widget, "Connection");
}

void SettingsDialog::setup_appearance_tab() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    auto* group = new QGroupBox("Appearance", this);
    group->setStyleSheet(R"(
        QGroupBox {
            color: #ccc;
            border: 1px solid #3a3a3a;
            border-radius: 4px;
            margin-top: 12px;
            padding: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px;
        }
    )");

    auto* form = new QFormLayout(group);

    check_dark_theme_ = new QCheckBox("Use dark theme", this);
    check_dark_theme_->setStyleSheet("color: #ccc;");
    check_dark_theme_->setChecked(true);
    form->addRow("", check_dark_theme_);

    check_show_tray_ = new QCheckBox("Show system tray icon", this);
    check_show_tray_->setStyleSheet("color: #ccc;");
    check_show_tray_->setChecked(true);
    form->addRow("", check_show_tray_);

    layout->addWidget(group);
    layout->addStretch();
    tabs_->addTab(widget, "Appearance");
}

void SettingsDialog::apply_settings() {
    save_settings();
}

} // namespace bolt::gui
