// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/speed_graph.hpp>
#include <QVBoxLayout>
#include <QTimer>

namespace bolt::gui {

SpeedGraph::SpeedGraph(QWidget* parent)
    : QWidget(parent) {

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setContentsMargins(0, 0, 0, 0);

    setup_chart();

    // Auto-update timer
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        // Add current time sample (speed would come from actual downloads)
        // For now, add placeholder
        // add_sample(current_total_speed);
    });
    timer->start(1000);
}

SpeedGraph::~SpeedGraph() = default;

void SpeedGraph::setup_chart() {
    chart_ = new QChart();
    chart_->setMargins({0, 0, 0, 0});
    chart_->setBackgroundRoundness(0);

    // Series for speed data
    series_ = new QLineSeries();
    series_->setUseOpenGL(true);
    QPen pen(QColor(0x00, 0x78, 0xd4));
    pen.setWidth(2);
    series_->setPen(pen);
    chart_->addSeries(series_);

    // X axis (time)
    axis_x_ = new QValueAxis();
    axis_x_->setRange(0, static_cast<double>(max_samples_));
    axis_x_->setLabelsVisible(false);
    axis_x_->setGridLineVisible(false);
    axis_x_->setLineVisible(false);
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    series_->attachAxis(axis_x_);

    // Y axis (speed)
    axis_y_ = new QValueAxis();
    axis_y_->setRange(0, 10);  // Will auto-scale
    axis_y_->setLabelFormat("%.1f");
    axis_y_->setGridLinePen(QPen(QColor(80, 80, 80), 1, Qt::DashLine));
    axis_y_->setLinePen(QPen(QColor(120, 120, 120), 1));
    axis_y_->setTitleText("Speed (MB/s)");
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    series_->attachAxis(axis_y_);

    // Chart view
    chart_view_ = new QChartView(chart_, this);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setRubberBand(QChartView::NoRubberBand);

    // Dark theme styling
    chart_->setBackgroundBrush(QBrush(QColor(30, 30, 30)));
    chart_->setTitleBrush(QBrush(QColor(200, 200, 200)));
    axis_y_->setLabelsColor(QColor(150, 150, 150));
    axis_y_->setTitleBrush(QBrush(QColor(180, 180, 180)));

    layout()->addWidget(chart_view_);
}

void SpeedGraph::add_sample(std::uint64_t speed_bps) {
    samples_.push_back(speed_bps);
    if (samples_.size() > max_samples_) {
        samples_.pop_front();
    }

    // Update max speed for auto-scaling
    if (speed_bps > max_speed_seen_) {
        max_speed_seen_ = speed_bps;
        // Update Y axis range (convert B/s to MB/s)
        double max_mb = static_cast<double>(max_speed_seen_) / (1024.0 * 1024.0);
        axis_y_->setRange(0, max_mb * 1.1);
    }

    // Update series
    series_->clear();
    double max_mb = static_cast<double>(max_speed_seen_) / (1024.0 * 1024.0);
    if (max_mb == 0) max_mb = 1.0;

    for (std::size_t i = 0; i < samples_.size(); ++i) {
        double mbps = static_cast<double>(samples_[i]) / (1024.0 * 1024.0);
        series_->append(static_cast<double>(i), mbps);
    }
}

void SpeedGraph::reset() {
    samples_.clear();
    series_->clear();
    max_speed_seen_ = 0;
    axis_y_->setRange(0, 10);
}

void SpeedGraph::set_max_samples(std::size_t count) {
    max_samples_ = count;
    axis_x_->setRange(0, static_cast<double>(max_samples_));
}

void SpeedGraph::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Chart will auto-resize via layout
}

} // namespace bolt::gui
