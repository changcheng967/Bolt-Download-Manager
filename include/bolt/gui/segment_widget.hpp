// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QWidget>
#include <cstdint>

class QPainter;
class QStyleOption;

namespace bolt::gui {

// Visual representation of a single download segment
class SegmentWidget : public QWidget {
    Q_OBJECT

public:
    explicit SegmentWidget(std::uint32_t id, QWidget* parent = nullptr);

    void set_progress(std::uint64_t downloaded, std::uint64_t total);
    void set_state(int state);  // SegmentState enum value
    void set_speed(std::uint64_t bps);

    [[nodiscard]] std::uint32_t id() const noexcept { return id_; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::uint32_t id_;
    std::uint64_t downloaded_{0};
    std::uint64_t total_{0};
    std::uint64_t speed_{0};
    int state_{0};  // 0=pending, 1=downloading, 2=completed, 3=failed
};

} // namespace bolt::gui
