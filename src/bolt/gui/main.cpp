// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/main_window.hpp>
#include <bolt/version.hpp>

#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace {

void apply_default_style() {
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Dark palette
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30, 30, 30));
    dark.setColor(QPalette::WindowText, QColor(220, 220, 220));
    dark.setColor(QPalette::Base, QColor(25, 25, 25));
    dark.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
    dark.setColor(QPalette::ToolTipBase, QColor(30, 30, 30));
    dark.setColor(QPalette::ToolTipText, QColor(220, 220, 220));
    dark.setColor(QPalette::Text, QColor(220, 220, 220));
    dark.setColor(QPalette::Button, QColor(45, 45, 45));
    dark.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    dark.setColor(QPalette::BrightText, QColor(255, 0, 0));
    dark.setColor(QPalette::Link, QColor(42, 130, 218));
    dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, QColor(30, 30, 30));

    QApplication::setPalette(dark);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("BoltDM");
    app.setApplicationDisplayName("Bolt Download Manager");
    app.setApplicationVersion(bolt::version.to_string().c_str());
    app.setOrganizationName("changcheng967");

    apply_default_style();

    // Create main window
    bolt::gui::MainWindow window;
    window.show();

    return app.exec();
}
