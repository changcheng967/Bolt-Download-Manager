// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/tray_icon.hpp>
#include <QApplication>

namespace bolt::gui {

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent) {

    tray_icon_ = new QSystemTrayIcon(this);

    // Create icon (draw programmatically)
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw lightning bolt
    QPainterPath path;
    path.moveTo(16, 2);
    path.lineTo(8, 14);
    path.lineTo(14, 14);
    path.lineTo(12, 30);
    path.lineTo(24, 14);
    path.lineTo(18, 14);
    path.closeSubpath();

    QPen pen(QColor(0x00, 0x78, 0xd4), 2);
    painter.setPen(pen);
    painter.setBrush(QColor(0x00, 0xbc, 0xf2));
    painter.drawPath(path);

    painter.end();

    tray_icon_->setIcon(QIcon(pixmap));
    tray_icon_->setToolTip("Bolt Download Manager");

    setup_menu();
}

TrayIcon::~TrayIcon() = default;

void TrayIcon::setup_menu() {
    menu_ = new QMenu();

    action_show_ = menu_->addAction("Show Window");
    connect(action_show_, &QAction::triggered, this, &TrayIcon::show_window);

    menu_->addSeparator();

    action_quit_ = menu_->addAction("Quit");
    connect(action_quit_, &QAction::triggered, this, &TrayIcon::quit);

    tray_icon_->setContextMenu(menu_);
}

void TrayIcon::set_speed(std::uint64_t bps) {
    current_speed_ = bps;
    tray_icon_->setToolTip(format_tooltip());
}

void TrayIcon::set_active_count(std::uint32_t count) {
    active_downloads_ = count;
    tray_icon_->setToolTip(format_tooltip());
}

void TrayIcon::show() {
    tray_icon_->show();
}

void TrayIcon::hide() {
    tray_icon_->hide();
}

QString TrayIcon::format_tooltip() const {
    QString speed_str;
    if (current_speed_ >= 1024 * 1024) {
        speed_str = QString("%1 MB/s").arg(current_speed_ / (1024.0 * 1024), 0, 'f', 1);
    } else if (current_speed_ >= 1024) {
        speed_str = QString("%1 KB/s").arg(current_speed_ / 1024.0, 0, 'f', 0);
    } else {
        speed_str = QString("%1 B/s").arg(current_speed_);
    }

    return QString("Bolt Download Manager\nSpeed: %1\nActive: %2")
        .arg(speed_str)
        .arg(active_downloads_);
}

} // namespace bolt::gui
