// Copyright (c) 2026 changcheng967. All rights reserved.

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <memory>

namespace bolt::gui {

// Dialog for adding new downloads
class AddDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddDialog(QWidget* parent = nullptr);
    ~AddDialog() override;

    // Get dialog result
    [[nodiscard]] std::pair<QString, QString> get_result() const;

    // Set initial URL (for clipboard monitoring)
    void set_url(const QString& url);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setup_ui();
    void validate_url();

    QLineEdit* edit_url_{nullptr};
    QLineEdit* edit_filename_{nullptr};
    QLineEdit* edit_save_path_{nullptr};
    QPushButton* btn_browse_{nullptr};
    QPushButton* btn_ok_{nullptr};
    QPushButton* btn_cancel_{nullptr};
    QLabel* label_file_info_{nullptr};
};

} // namespace bolt::gui
