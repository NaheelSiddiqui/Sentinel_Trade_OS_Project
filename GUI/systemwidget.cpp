#include "systemwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>

SystemWidget::SystemWidget(QWidget *parent)
    : QWidget(parent),
      m_statsLabel(new QLabel(this)),
      m_logView(new QTextEdit(this)),
      m_tradeProgress(new QProgressBar(this))
{
    setupUI();
}

void SystemWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *title = new QLabel("System Status & Logs", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    
    // Statistics
    m_statsLabel->setStyleSheet(
        "background: #f5f5f5; padding: 10px; border-radius: 4px; font-family: monospace;");
    layout->addWidget(m_statsLabel);
    
    // Progress
    QHBoxLayout *progressLayout = new QHBoxLayout();
    progressLayout->addWidget(new QLabel("Trade progress:"));
    m_tradeProgress->setMaximum(1000);
    m_tradeProgress->setValue(0);
    progressLayout->addWidget(m_tradeProgress);
    layout->addLayout(progressLayout);
    
    // Log view
    QLabel *logTitle = new QLabel("POSIX System Call Log", this);
    logTitle->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(logTitle);
    
    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Courier", 9));
    m_logView->setMaximumHeight(300);
    layout->addWidget(m_logView);
    
    layout->addStretch();
}

void SystemWidget::updateSystemLogs(const QVector<SystemEntry> &logs)
{
    QString html;
    
    for (const auto &entry : logs) {
        QString color = "#666";
        if (entry.level == "ERROR") color = "#b32121";
        else if (entry.level == "WARN") color = "#cc7700";
        else if (entry.level == "TRADE") color = "#2d8659";
        else if (entry.level == "INFO") color = "#0066cc";
        
        html += QString("<span style='color: %1'>[%2] [%3] %4</span><br/>")
            .arg(color)
            .arg(entry.timestamp.toString("HH:mm:ss"))
            .arg(entry.level)
            .arg(entry.message);
    }
    
    m_logView->setHtml(html);
    // Auto-scroll to bottom
    m_logView->verticalScrollBar()->setValue(
        m_logView->verticalScrollBar()->maximum());
}

void SystemWidget::updateStatistics(int totalTrades, int totalVolume)
{
    QString stats = QString(
        "Total Trades Executed: %1\n"
        "Total Volume (shares):  %2\n"
        "Matching Engine Status: ACTIVE\n"
        "Semaphore Capacity:     10 concurrent traders\n"
        "Thread Pool:            20 trader threads")
        .arg(totalTrades)
        .arg(totalVolume);
    
    m_statsLabel->setText(stats);
    
    // Update progress (out of 1000 trades for demo)
    m_tradeProgress->setValue(qMin(totalTrades, 1000));
}
