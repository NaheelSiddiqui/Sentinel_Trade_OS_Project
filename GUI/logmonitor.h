#pragma once
#include <QString>
#include <QVector>
#include <QMap>
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

// Live ticker snapshot for one stock (parsed from book.json).
struct MarketTick {
    double price = 0.0;
    double open  = 0.0;
    double high  = 0.0;
    double low   = 0.0;
    long   volume = 0;
};

// One aggregated level of the order book (price × cumulative qty).
struct BookLevel {
    double price = 0.0;
    int    qty   = 0;
};

struct BookLadder {
    QVector<BookLevel> bids;   // sorted desc by price
    QVector<BookLevel> asks;   // sorted asc by price
};

class LogMonitor : public QObject {
    Q_OBJECT
public:
    explicit LogMonitor(QObject *parent = nullptr);
    ~LogMonitor();

    void startMonitoring(const QString &tradeLogPath,
                         const QString &systemLogPath,
                         const QString &bookSnapshotPath = QString());

    // Wipe cached trades / system entries and re-parse from disk. Useful
    // when the backend is restarted and its log files are truncated.
    void resync();

    QVector<TradeEntry> getRecentTrades(int count = 50);
    QVector<SystemEntry> getRecentSystemLogs(int count = 100);

    QMap<QString, MarketTick> getMarketTicks() const { return m_marketTicks; }
    BookLadder getBook(const QString &symbol) const { return m_books.value(symbol); }
    QStringList getKnownSymbols() const { return m_marketTicks.keys(); }

    int getTotalTrades() const { return m_totalTrades; }
    int getTotalVolume() const { return m_totalVolume; }

signals:
    void tradesUpdated();
    void systemLogsUpdated();
    void statisticsUpdated();
    void marketUpdated();   // book.json reloaded

private slots:
    void onTradeLogChanged(const QString &path);
    void onSystemLogChanged(const QString &path);
    void onBookSnapshotChanged(const QString &path);

private:
    void parseTradeLogs();
    void parseSystemLogs();
    void parseBookSnapshot();

    QString m_tradeLogPath;
    QString m_systemLogPath;
    QString m_bookSnapshotPath;
    QVector<TradeEntry>  m_trades;
    QVector<SystemEntry> m_systemLogs;
    QMap<QString, MarketTick> m_marketTicks;
    QMap<QString, BookLadder> m_books;
    QFileSystemWatcher *m_watcher;

    int m_totalTrades = 0;
    int m_totalVolume = 0;
};
