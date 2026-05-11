#include "logmonitor.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

LogMonitor::LogMonitor(QObject *parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
        if (path == m_tradeLogPath) onTradeLogChanged(path);
        else if (path == m_systemLogPath) onSystemLogChanged(path);
    });
}

LogMonitor::~LogMonitor() = default;

void LogMonitor::startMonitoring(const QString &tradeLogPath, const QString &systemLogPath)
{
    m_tradeLogPath = tradeLogPath;
    m_systemLogPath = systemLogPath;

    m_watcher->addPath(tradeLogPath);
    m_watcher->addPath(systemLogPath);

    parseTradeLogs();
    parseSystemLogs();
}

void LogMonitor::resync()
{
    // Re-add paths in case the file was deleted+recreated (which loses the
    // QFileSystemWatcher hook), then re-parse from scratch.
    if (!m_tradeLogPath.isEmpty()) {
        m_watcher->removePath(m_tradeLogPath);
        m_watcher->addPath(m_tradeLogPath);
    }
    if (!m_systemLogPath.isEmpty()) {
        m_watcher->removePath(m_systemLogPath);
        m_watcher->addPath(m_systemLogPath);
    }
    parseTradeLogs();
    parseSystemLogs();
    emit tradesUpdated();
    emit systemLogsUpdated();
    emit statisticsUpdated();
}

void LogMonitor::onTradeLogChanged(const QString &path)
{
    parseTradeLogs();
    emit tradesUpdated();
    emit statisticsUpdated();
}

void LogMonitor::onSystemLogChanged(const QString &path)
{
    parseSystemLogs();
    emit systemLogsUpdated();
}

void LogMonitor::parseTradeLogs()
{
    m_trades.clear();
    m_totalTrades = 0;
    m_totalVolume = 0;
    
    QFile file(m_tradeLogPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    
    QTextStream in(&file);
    // Pattern: [YYYY-MM-DD HH:MM:SS] [TRADE] MATCH AAPL qty=10 price=$100.50 buyer=T1 seller=T2
    QRegularExpression re(R"(\[([^\]]+)\].*MATCH\s+(\w+)\s+qty=(\d+)\s+price=\$([0-9.]+)\s+buyer=T(\d+)\s+seller=T(\d+))");
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            TradeEntry trade;
            trade.timestamp = QDateTime::fromString(match.captured(1), "yyyy-MM-dd HH:mm:ss");
            trade.symbol = match.captured(2);
            trade.quantity = match.captured(3).toInt();
            trade.price = match.captured(4).toDouble();
            trade.buyerId = match.captured(5).toInt();
            trade.sellerId = match.captured(6).toInt();
            
            m_trades.append(trade);
            m_totalTrades++;
            m_totalVolume += trade.quantity;
        }
    }
    file.close();
}

void LogMonitor::parseSystemLogs()
{
    m_systemLogs.clear();
    
    QFile file(m_systemLogPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    
    QTextStream in(&file);
    // Pattern: [YYYY-MM-DD HH:MM:SS] [LEVEL] message
    QRegularExpression re(R"(\[([^\]]+)\]\s+\[([^\]]+)\]\s+(.+))");
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            SystemEntry entry;
            entry.timestamp = QDateTime::fromString(match.captured(1), "yyyy-MM-dd HH:mm:ss");
            entry.level = match.captured(2);
            entry.message = match.captured(3);
            m_systemLogs.append(entry);
        }
    }
    file.close();
}

QVector<TradeEntry> LogMonitor::getRecentTrades(int count)
{
    int start = qMax(0, m_trades.size() - count);
    return m_trades.mid(start);
}

QVector<SystemEntry> LogMonitor::getRecentSystemLogs(int count)
{
    int start = qMax(0, m_systemLogs.size() - count);
    return m_systemLogs.mid(start);
}
