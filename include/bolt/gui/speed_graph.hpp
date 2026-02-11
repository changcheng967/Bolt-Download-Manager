// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QWidget>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <deque>
#include <cstdint>

namespace bolt::gui {

// Real-time speed graph using Qt Charts
class SpeedGraph : public QWidget {
    Q_OBJECT

public:
    explicit SpeedGraph(QWidget* parent = nullptr);
    ~SpeedGraph() override;

    void add_sample(std::uint64_t speed_bps);
    void reset();
    void set_max_samples(std::size_t count);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setup_chart();

    QtCharts::QChart* chart_{nullptr};
    QtCharts::QChartView* chart_view_{nullptr};
    QtCharts::QLineSeries* series_{nullptr};
    QtCharts::QValueAxis* axis_x_{nullptr};
    QtCharts::QValueAxis* axis_y_{nullptr};

    std::deque<std::uint64_t> samples_;
    std::size_t max_samples_{300};  // 5 minutes at 1 sample per second

    std::uint64_t max_speed_seen_{0};
};

} // namespace bolt::gui
