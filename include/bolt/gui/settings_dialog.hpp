// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include <QTabWidget>

namespace bolt::gui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void apply_settings();

private:
    void setup_general_tab();
    void setup_connection_tab();
    void setup_appearance_tab();

    QTabWidget* tabs_{nullptr};

    // General
    QCheckBox* check_start_minimized_{nullptr};
    QCheckBox* check_clipboard_monitor_{nullptr};
    QCheckBox* check_confirm_exit_{nullptr};
    QSpinBox* spin_simultaneous_{nullptr};

    // Connection
    QSpinBox* spin_max_segments_{nullptr};
    QSpinBox* spin_connection_timeout_{nullptr};
    QSpinBox* spin_retry_count_{nullptr};
    QCheckBox* check_http2_{nullptr};

    // Appearance
    QCheckBox* check_dark_theme_{nullptr};
    QCheckBox* check_show_tray_{nullptr};
};

} // namespace bolt::gui
