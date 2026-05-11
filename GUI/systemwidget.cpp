#include "systemwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollBar>

SystemWidget::SystemWidget(QWidget *parent)
    : QWidget(parent),
      m_statsLabel(new QLabel(this)),
      m_backendStatus(new QLabel(this)),
      m_logView(new QTextEdit(this)),
      m_consoleView(new QPlainTextEdit(this)),
      m_tradeProgress(new QProgressBar(this))
{
    setupUI();
}

void SystemWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // ── Header row: title + backend status pill ──────────────────────────
    QHBoxLayout *headerRow = new QHBoxLayout();
    QLabel *title = new QLabel("System Status & Logs", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    headerRow->addWidget(title);
    headerRow->addStretch();

    m_backendStatus->setText("Backend: stopped");
    m_backendStatus->setStyleSheet(
        "padding: 3px 10px; border-radius: 10px;"
        "background: #888; color: white; font-weight: bold;");
    headerRow->addWidget(m_backendStatus);
    layout->addLayout(headerRow);

    // ── Statistics card ──────────────────────────────────────────────────
    m_statsLabel->setStyleSheet(
        "background: #f5f5f5; padding: 10px; border-radius: 4px; font-family: monospace;");
    layout->addWidget(m_statsLabel);

    // ── Trade progress bar ───────────────────────────────────────────────
    QHBoxLayout *progressLayout = new QHBoxLayout();
    progressLayout->addWidget(new QLabel("Trade progress:"));
    m_tradeProgress->setMaximum(1000);
    m_tradeProgress->setValue(0);
    progressLayout->addWidget(m_tradeProgress);
    layout->addLayout(progressLayout);

    // ── POSIX system-call log (parsed from system.log) ───────────────────
    QLabel *logTitle = new QLabel("POSIX System Call Log", this);
    logTitle->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(logTitle);

    m_logView->setReadOnly(true);
    m_logView->setFont(QFont("Courier", 9));
    m_logView->setMaximumHeight(220);
    layout->addWidget(m_logView);

    // ── Live backend stdout / stderr ─────────────────────────────────────
    QLabel *consoleTitle = new QLabel("Backend Live Output (stdout/stderr)", this);
    consoleTitle->setStyleSheet("font-weight: bold; margin-top: 10px;");
    layout->addWidget(consoleTitle);

    m_consoleView->setReadOnly(true);
    m_consoleView->setFont(QFont("Courier", 9));
    m_consoleView->setMaximumBlockCount(5000);  // cap memory
    m_consoleView->setStyleSheet(
        "background: #1e1e1e; color: #d4d4d4; border-radius: 4px; padding: 6px;");
    layout->addWidget(m_consoleView, /*stretch=*/1);
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
    m_tradeProgress->setValue(qMin(totalTrades, 1000));
}

void SystemWidget::appendBackendOutput(const QString &chunk)
{
    // appendPlainText already inserts a newline; trim a trailing one to
    // avoid double-spacing on chunks that end in '\n'.
    QString text = chunk;
    if (text.endsWith('\n')) text.chop(1);
    if (text.isEmpty()) return;
    m_consoleView->appendPlainText(text);
    m_consoleView->verticalScrollBar()->setValue(
        m_consoleView->verticalScrollBar()->maximum());
}

void SystemWidget::clearBackendOutput()
{
    m_consoleView->clear();
}

void SystemWidget::setBackendStatus(const QString &text, const QString &cssColor)
{
    m_backendStatus->setText(text);
    m_backendStatus->setStyleSheet(QString(
        "padding: 3px 10px; border-radius: 10px;"
        "background: %1; color: white; font-weight: bold;").arg(cssColor));
}
