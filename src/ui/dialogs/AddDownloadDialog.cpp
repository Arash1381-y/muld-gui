#include "AddDownloadDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QUrl>
#include <QFileInfo>
#include <QVBoxLayout>

namespace muld_gui {

AddDownloadDialog::AddDownloadDialog(const QStringList& existingTargets, QWidget* parent)
    : QDialog(parent),
      m_urlEdit(nullptr),
      m_outputEdit(nullptr),
      m_filenameEdit(nullptr),
      m_speedLimitSpin(nullptr),
      m_okBtn(nullptr),
      m_errorLabel(nullptr),
      m_filenameTouchedByUser(false),
      m_existingTargets(existingTargets) {
    setWindowTitle(tr("Add Download"));
    setModal(true);
    setFixedWidth(520);
    setupUi();
}

void AddDownloadDialog::setupUi() {
    setStyleSheet(
        "QDialog { background:#252526; color:#cccccc; }"
        "QLabel { color:#858585; font:11px 'Segoe UI'; letter-spacing:0.4px; }"
        "QLineEdit, QSpinBox { background:#1e1e1e; color:#cccccc; border:1px solid #454545; padding:6px; }"
        "QSpinBox::up-button, QSpinBox::down-button { width:16px; border:none; }"
        "QPushButton { background:#3c3c3c; color:#cccccc; border:1px solid #3e3e42; padding:6px 14px; }"
        "QPushButton:hover { background:#505050; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto* urlLabel = new QLabel(tr("URL"), this);
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("https://example.com/file.zip"));
    root->addWidget(urlLabel);
    root->addWidget(m_urlEdit);

    auto* saveRow = new QHBoxLayout();
    saveRow->setSpacing(8);
    auto* savePathContainer = new QVBoxLayout();
    savePathContainer->setSpacing(4);
    auto* filenameContainer = new QVBoxLayout();
    filenameContainer->setSpacing(4);

    savePathContainer->addWidget(new QLabel(tr("Save Path"), this));
    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText(tr("/home/user/Downloads"));
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!defaultDir.isEmpty()) {
        m_outputEdit->setText(defaultDir);
    }
    savePathContainer->addWidget(m_outputEdit);

    filenameContainer->addWidget(new QLabel(tr("Filename"), this));
    m_filenameEdit = new QLineEdit(this);
    m_filenameEdit->setPlaceholderText(tr("Optional"));
    filenameContainer->addWidget(m_filenameEdit);

    saveRow->addLayout(savePathContainer, 1);
    saveRow->addLayout(filenameContainer, 1);
    root->addLayout(saveRow);

    auto* browseRow = new QHBoxLayout();
    browseRow->setSpacing(8);
    auto* browseButton = new QPushButton(tr("Browse..."), this);
    browseRow->addStretch(1);
    browseRow->addWidget(browseButton);
    root->addLayout(browseRow);

    root->addWidget(new QLabel(tr("Speed Limit (KB/s)"), this));
    m_speedLimitSpin = new QSpinBox(this);
    m_speedLimitSpin->setRange(0, 1000000);
    m_speedLimitSpin->setValue(0);
    m_speedLimitSpin->setSpecialValueText(tr("Unlimited"));
    m_speedLimitSpin->setSingleStep(256);
    m_speedLimitSpin->setAccelerated(true);
    root->addWidget(m_speedLimitSpin);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet("color:#e57373; font:11px 'Segoe UI';");
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    auto* cancelBtn = buttons->button(QDialogButtonBox::Cancel);
    if (m_okBtn) {
        m_okBtn->setText(tr("Start Download"));
        m_okBtn->setEnabled(false);
        m_okBtn->setStyleSheet("background:#0e639c; border:1px solid #1177bb;");
    }
    if (cancelBtn) {
        cancelBtn->setText(tr("Cancel"));
    }
    root->addWidget(buttons);

    connect(browseButton, &QPushButton::clicked, this, &AddDownloadDialog::browseOutputDir);
    connect(m_urlEdit, &QLineEdit::textChanged, this, &AddDownloadDialog::onUrlChanged);
    connect(m_filenameEdit, &QLineEdit::textChanged, this, &AddDownloadDialog::onFilenameEdited);
    connect(m_outputEdit, &QLineEdit::textChanged, this, &AddDownloadDialog::onTargetEdited);
    connect(m_filenameEdit, &QLineEdit::textChanged, this, &AddDownloadDialog::onTargetEdited);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddDownloadDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AddDownloadDialog::reject);
}

QString AddDownloadDialog::url() const {
    return m_urlEdit ? m_urlEdit->text().trimmed() : QString();
}

QString AddDownloadDialog::outputDir() const {
    return m_outputEdit ? m_outputEdit->text().trimmed() : QString();
}

QString AddDownloadDialog::filename() const {
    return m_filenameEdit ? m_filenameEdit->text().trimmed() : QString();
}

std::size_t AddDownloadDialog::speedLimitBytesPerSec() const {
    if (!m_speedLimitSpin) {
        return 0;
    }
    return static_cast<std::size_t>(m_speedLimitSpin->value()) * 1024ull;
}

void AddDownloadDialog::browseOutputDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), outputDir());
    if (!dir.isEmpty() && m_outputEdit) {
        m_outputEdit->setText(dir);
    }
    validateTarget();
}

void AddDownloadDialog::onUrlChanged(const QString& text) {
    validateTarget();

    if (!m_filenameEdit) {
        return;
    }

    const QString currentFilename = m_filenameEdit->text().trimmed();
    const bool canAutofill = !m_filenameTouchedByUser ||
                             currentFilename.isEmpty() ||
                             currentFilename == m_lastSuggestedFilename;
    if (!canAutofill) {
        return;
    }

    const QString trimmedUrl = text.trimmed();
    QString suggested;
    if (!trimmedUrl.isEmpty()) {
        const QUrl parsed(trimmedUrl);
        suggested = QFileInfo(parsed.path()).fileName();
    }

    if (suggested.isEmpty()) {
        m_lastSuggestedFilename.clear();
        m_filenameEdit->clear();
        return;
    }

    m_lastSuggestedFilename = suggested;
    m_filenameEdit->setText(suggested);
    m_filenameTouchedByUser = false;
    validateTarget();
}

void AddDownloadDialog::onFilenameEdited(const QString& text) {
    if (text.trimmed().isEmpty() || text.trimmed() == m_lastSuggestedFilename) {
        m_filenameTouchedByUser = false;
        return;
    }
    m_filenameTouchedByUser = true;
    validateTarget();
}

void AddDownloadDialog::onTargetEdited() {
    validateTarget();
}

void AddDownloadDialog::validateTarget() {
    const QString urlValue = url();
    const QString baseDir = outputDir().trimmed();
    const QString fileName = filename().trimmed();

    QString error;
    if (urlValue.isEmpty()) {
        error = tr("URL is required.");
    } else if (baseDir.isEmpty()) {
        error = tr("Save path is required.");
    } else {
        QString finalPath = baseDir;
        if (!fileName.isEmpty()) {
            finalPath = QDir(baseDir).filePath(fileName);
        }
        const QString normalized = QFileInfo(finalPath).absoluteFilePath();

        if (m_existingTargets.contains(normalized)) {
            error = tr("This destination is already in the list.");
        } else if (QFileInfo::exists(normalized)) {
            error = tr("This destination path already exists.");
        } else if (!fileName.isEmpty()) {
            const QFileInfo dirInfo(baseDir);
            if (dirInfo.isFile() && dirInfo.fileName() == fileName) {
                error = tr("Filename is repeated in save path.");
            }
        }
    }

    if (m_errorLabel) {
        m_errorLabel->setVisible(!error.isEmpty());
        m_errorLabel->setText(error);
    }
    if (m_okBtn) {
        m_okBtn->setEnabled(error.isEmpty() && !urlValue.isEmpty());
    }
}

}  // namespace muld_gui
