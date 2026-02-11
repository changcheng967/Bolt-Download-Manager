// Copyright (c) 2026 changcheng967. All rights reserved.

#include <bolt/gui/add_dialog.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>
#include <QUrl>

namespace bolt::gui {

AddDialog::AddDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle("Add Download");
    setMinimumWidth(500);
    setup_ui();
}

AddDialog::~AddDialog() = default;

void AddDialog::setup_ui() {
    auto* layout = new QVBoxLayout(this);

    // URL input
    auto* form_layout = new QFormLayout();

    edit_url_ = new QLineEdit(this);
    edit_url_->setPlaceholderText("https://example.com/file.zip");
    edit_url_->setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 8px;
            color: #ccc;
        }
        QLineEdit:focus {
            border-color: #0078d4;
        }
    )");
    connect(edit_url_, &QLineEdit::textChanged, this, &AddDialog::validate_url);
    form_layout->addRow("URL:", edit_url_);

    edit_filename_ = new QLineEdit(this);
    edit_filename_->setPlaceholderText("Auto-detect from URL");
    edit_filename_->setStyleSheet(edit_url_->styleSheet());
    form_layout->addRow("Save as:", edit_filename_);

    // Save path
    auto* path_layout = new QHBoxLayout();
    edit_save_path_ = new QLineEdit(this);
    edit_save_path_->setText(QDir::homePath());
    edit_save_path_->setStyleSheet(edit_url_->styleSheet());
    path_layout->addWidget(edit_save_path_);

    btn_browse_ = new QPushButton("Browse...", this);
    btn_browse_->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 6px 16px;
            color: #eee;
        }
        QPushButton:hover { background-color: #4a4a4a; }
    )");
    connect(btn_browse_, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(
            this, "Select Save Folder", edit_save_path_->text());
        if (!path.isEmpty()) {
            edit_save_path_->setText(path);
        }
    });
    path_layout->addWidget(btn_browse_);

    form_layout->addRow("Save to folder:", path_layout);

    // File info placeholder
    label_file_info_ = new QLabel("Enter URL to see file information", this);
    label_file_info_->setStyleSheet("color: #888; font-size: 11px; padding: 8px;");
    form_layout->addRow("", label_file_info_);

    layout->addLayout(form_layout);

    // Buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->addStretch();

    btn_ok_ = new QPushButton("Download", this);
    btn_ok_->setEnabled(false);
    btn_ok_->setStyleSheet(R"(
        QPushButton {
            background-color: #0078d4;
            border: none;
            border-radius: 4px;
            padding: 8px 24px;
            color: #fff;
            font-weight: bold;
        }
        QPushButton:hover { background-color: #1084e8; }
        QPushButton:disabled { background-color: #3a3a3a; color: #666; }
    )");
    connect(btn_ok_, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(btn_ok_);

    btn_cancel_ = new QPushButton("Cancel", this);
    btn_cancel_->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 8px 24px;
            color: #ccc;
        }
        QPushButton:hover { background-color: #4a4a4a; }
    )");
    connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
    button_layout->addWidget(btn_cancel_);

    layout->addLayout(button_layout);
}

void AddDialog::validate_url() {
    QString url = edit_url_->text().trimmed();
    bool valid = url.startsWith("http://") || url.startsWith("https://");

    btn_ok_->setEnabled(valid);

    if (valid && edit_filename_->text().isEmpty()) {
        // Auto-fill filename from URL
        QUrl qurl(url);
        QString filename = qurl.fileName();
        if (!filename.isEmpty()) {
            edit_filename_->setText(filename);
        }
        label_file_info_->setText(QString("Filename: %1").arg(filename));
    } else if (!valid) {
        label_file_info_->setText("Enter a valid URL");
    }
}

void AddDialog::set_url(const QString& url) {
    edit_url_->setText(url);
    validate_url();
}

std::pair<QString, QString> AddDialog::get_result() const {
    QString full_path = QDir(edit_save_path_->text()).filePath(edit_filename_->text());
    return {edit_url_->text(), full_path};
}

void AddDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    edit_url_->setFocus();
}

} // namespace bolt::gui
