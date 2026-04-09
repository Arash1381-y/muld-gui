#pragma once

#include <QDialog>
#include <QStringList>
#include <cstddef>

class QLineEdit;
class QSpinBox;

class QPushButton;
class QLabel;

namespace muld_gui {

class AddDownloadDialog : public QDialog {

Q_OBJECT

public:

explicit AddDownloadDialog(const QStringList& existingTargets, QWidget* parent = nullptr);

QString url() const;

QString outputDir() const;

QString filename() const;

std::size_t speedLimitBytesPerSec() const;

private slots:

void browseOutputDir();

void onUrlChanged(const QString& text);
void onFilenameEdited(const QString& text);
void onTargetEdited();

private:

void setupUi();

void validateTarget();

QLineEdit* m_urlEdit;

QLineEdit* m_outputEdit;

QLineEdit* m_filenameEdit;
QSpinBox* m_speedLimitSpin;

QPushButton* m_okBtn;
QLabel* m_errorLabel;
QString m_lastSuggestedFilename;
bool m_filenameTouchedByUser;
QStringList m_existingTargets;

};

} // namespace muld_gui
