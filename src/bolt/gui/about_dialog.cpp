// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/about_dialog.hpp>
#include <bolt/version.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>

namespace bolt::gui {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle("About Bolt Download Manager");
    setFixedSize(450, 300);
    setup_ui();
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::setup_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    // App icon placeholder
    auto* icon_label = new QLabel(this);
    icon_label->setMinimumSize(80, 80);
    icon_label->setStyleSheet(R"(
        QLabel {
            background-color: #0078d4;
            border-radius: 20px;
        }
    )");
    icon_label->setAlignment(Qt::AlignCenter);
    icon_label->setText("⚡");
    icon_label->setStyleSheet(R"(
        QLabel {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #0078d4, stop:1 #00bcf2);
            border-radius: 20px;
            font-size: 48px;
            color: #fff;
        }
    )");
    layout->addWidget(icon_label, 0, Qt::AlignCenter);

    layout->addSpacing(20);

    // App name
    auto* name_label = new QLabel("Bolt Download Manager", this);
    name_label->setStyleSheet("font-size: 22px; font-weight: bold; color: #fff;");
    name_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(name_label);

    auto* version_label = new QLabel(
        QString("Version %1").arg(version.to_string().c_str()), this);
    version_label->setStyleSheet("color: #888;");
    version_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(version_label);

    layout->addSpacing(10);

    // Author and copyright
    auto* author_label = new QLabel("Created by changcheng967", this);
    author_label->setStyleSheet("color: #aaa;");
    author_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(author_label);

    auto* copyright_label = new QLabel("© changcheng967 2026", this);
    copyright_label->setStyleSheet("color: #888; font-size: 10px;");
    copyright_label->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyright_label);

    layout->addSpacing(20);

    // Tagline
    auto* tagline = new QLabel(
        "Beats IDM in speed, UI, and architecture.\n"
        "Built with C++23, Qt 6, and libcurl.", this);
    tagline->setStyleSheet("color: #666; font-style: italic;");
    tagline->setAlignment(Qt::AlignCenter);
    layout->addWidget(tagline);

    layout->addStretch();

    // Close button
    auto* btn_close = new QPushButton("Close", this);
    btn_close->setStyleSheet(R"(
        QPushButton {
            background-color: #0078d4;
            border: none;
            border-radius: 4px;
            padding: 8px 32px;
            color: #fff;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #1084e8; }
    )");
    connect(btn_close, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(btn_close, 0, Qt::AlignCenter);
}

} // namespace bolt::gui
