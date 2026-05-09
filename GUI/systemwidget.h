#pragma once
#include <QWidget>
#include "logmonitor.h"

class QTextEdit;
class QLabel;
class QProgressBar;

class SystemWidget : public QWidget {
    Q_OBJECT
public:
    explicit SystemWidget(QWidget *parent = nullptr);
    
    void updateSystemLogs(const QVector<SystemEntry> &logs);
    void updateStatistics(int totalTrades, int totalVolume);

private:
    void setupUI();
    
    QLabel *m_statsLabel;
    QTextEdit *m_logView;
    QProgressBar *m_tradeProgress;
};
