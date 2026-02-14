// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>

namespace bolt::gui {

class MainWindow;

// System tray icon with live speed display
class TrayIcon : public QObject {
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    void set_speed(std::uint64_t bps);
    void set_active_count(std::uint32_t count);
    void show();
    void hide();
    void show_message(const QString& title, const QString& message);

signals:
    void show_window();
    void quit();

private:
    void setup_menu();
    [[nodiscard]] QString format_tooltip() const;

    QSystemTrayIcon* tray_icon_{nullptr};
    QMenu* menu_{nullptr};
    QAction* action_show_{nullptr};
    QAction* action_quit_{nullptr};

    std::uint64_t current_speed_{0};
    std::uint32_t active_downloads_{0};
};

} // namespace bolt::gui
