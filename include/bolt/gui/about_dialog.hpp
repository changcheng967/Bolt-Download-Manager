// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QDialog>
#include <QLabel>

namespace bolt::gui {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog() override;

private:
    void setup_ui();
};

} // namespace bolt::gui
