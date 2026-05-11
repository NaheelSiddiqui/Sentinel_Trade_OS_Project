#pragma once
#include <QWidget>
#include "logmonitor.h"

class QTextEdit;
class QPlainTextEdit;
class QLabel;
class QProgressBar;

class SystemWidget : public QWidget {
    Q_OBJECT
public:
    explicit SystemWidget(QWidget *parent = nullptr);

    void updateSystemLogs(const QVector<SystemEntry> &logs);
    void updateStatistics(int totalTrades, int totalVolume);

    // Backend process I/O — fed from MainWindow's QProcess.
    void appendBackendOutput(const QString &chunk);
    void clearBackendOutput();
    void setBackendStatus(const QString &text, const QString &cssColor);

private:
    void setupUI();

    QLabel         *m_statsLabel;
    QLabel         *m_backendStatus;
    QTextEdit      *m_logView;
    QPlainTextEdit *m_consoleView;
    QProgressBar   *m_tradeProgress;
};
