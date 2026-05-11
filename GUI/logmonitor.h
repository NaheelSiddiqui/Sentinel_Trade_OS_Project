#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>

struct TradeEntry {
    QString symbol;
    int quantity;
    double price;
    int buyerId;
    int sellerId;
    QDateTime timestamp;
};

struct SystemEntry {
    QString level;  // INFO, WARN, ERROR, TRADE
    QString message;
    QDateTime timestamp;
};

class LogMonitor : public QObject {
    Q_OBJECT
public:
    explicit LogMonitor(QObject *parent = nullptr);
    ~LogMonitor();

    void startMonitoring(const QString &tradeLogPath, const QString &systemLogPath);

    // Wipe cached trades / system entries and re-parse from disk. Useful
    // when the backend is restarted and its log files are truncated.
    void resync();

    QVector<TradeEntry> getRecentTrades(int count = 50);
    QVector<SystemEntry> getRecentSystemLogs(int count = 100);
    
    int getTotalTrades() const { return m_totalTrades; }
    int getTotalVolume() const { return m_totalVolume; }

signals:
    void tradesUpdated();
    void systemLogsUpdated();
    void statisticsUpdated();

private slots:
    void onTradeLogChanged(const QString &path);
    void onSystemLogChanged(const QString &path);

private:
    void parseTradeLogs();
    void parseSystemLogs();

    QString m_tradeLogPath;
    QString m_systemLogPath;
    QVector<TradeEntry> m_trades;
    QVector<SystemEntry> m_systemLogs;
    QFileSystemWatcher *m_watcher;
    
    int m_totalTrades = 0;
    int m_totalVolume = 0;
};
