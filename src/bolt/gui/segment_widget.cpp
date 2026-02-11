// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/segment_widget.hpp>
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>

namespace bolt::gui {

SegmentWidget::SegmentWidget(std::uint32_t id, QWidget* parent)
    : QWidget(parent)
    , id_(id) {
    setMinimumSize(60, 40);
}

void SegmentWidget::set_progress(std::uint64_t downloaded, std::uint64_t total) {
    downloaded_ = downloaded;
    total_ = total;
    update();
}

void SegmentWidget::set_state(int state) {
    state_ = state;
    update();
}

void SegmentWidget::set_speed(std::uint64_t bps) {
    speed_ = bps;
    update();
}

void SegmentWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    auto rect = this->rect();
    int w = rect.width();
    int h = rect.height();

    // Background
    painter.fillRect(rect, QColor(0x2a, 0x2a, 0x2a));

    // Progress bar
    double percent = total_ > 0 ? static_cast<double>(downloaded_) / static_cast<double>(total_) : 0.0;
    int filled_w = static_cast<int>(w * percent);

    QColor fill_color;
    switch (state_) {
        case 0: fill_color = QColor(0x55, 0x55, 0x55); break;  // pending
        case 1: fill_color = QColor(0x00, 0x78, 0xd4); break;  // downloading
        case 2: fill_color = QColor(0x00, 0xaa, 0x00); break;  // completed
        case 3: fill_color = QColor(0xaa, 0x00, 0x00); break;  // failed
        default: fill_color = QColor(0x55, 0x55, 0x55);
    }

    painter.fillRect(0, 0, filled_w, h / 2, fill_color);

    // Gradient effect for active downloads
    if (state_ == 1) {
        QLinearGradient grad(0, h / 2, filled_w, h / 2);
        grad.setColorAt(0, QColor(0x00, 0x78, 0xd4));
        grad.setColorAt(1, QColor(0x20, 0x98, 0xf4));
        painter.fillRect(0, 0, filled_w, h / 2, grad);
    }

    // Border
    painter.setPen(QColor(0x44, 0x44, 0x44));
    painter.drawRect(rect.adjusted(0, 0, -1, -1));

    // Segment number
    painter.setPen(QColor(0xcc, 0xcc, 0xcc));
    QFont font = painter.font();
    font.setPixelSize(10);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, QString("#%1").arg(id_));

    // Speed text below
    if (state_ == 1 && speed_ > 0) {
        QString speed_str;
        if (speed_ >= 1024 * 1024) {
            speed_str = QString("%1M").arg(speed_ / (1024 * 1024));
        } else if (speed_ >= 1024) {
            speed_str = QString("%1K").arg(speed_ / 1024);
        } else {
            speed_str = QString("%1").arg(speed_);
        }

        painter.setPen(QColor(0x88, 0xff, 0x88));
        painter.drawText(QRect(0, h / 2, w, h / 2), Qt::AlignCenter, speed_str);
    }

    // Percent text
    painter.setPen(QColor(0xaa, 0xaa, 0xaa));
    painter.drawText(rect.adjusted(0, 0, 0, -h/4),
                     Qt::AlignBottom | Qt::AlignHCenter,
                     QString("%1%").arg(static_cast<int>(percent * 100)));
}

} // namespace bolt::gui
