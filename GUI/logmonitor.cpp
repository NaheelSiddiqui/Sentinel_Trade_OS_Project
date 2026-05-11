#include "logmonitor.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

LogMonitor::LogMonitor(QObject *parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
        if      (path == m_tradeLogPath)       onTradeLogChanged(path);
        else if (path == m_systemLogPath)      onSystemLogChanged(path);
        else if (path == m_bookSnapshotPath)   onBookSnapshotChanged(path);
    });
}

LogMonitor::~LogMonitor() = default;

void LogMonitor::startMonitoring(const QString &tradeLogPath,
                                 const QString &systemLogPath,
                                 const QString &bookSnapshotPath)
{
    m_tradeLogPath = tradeLogPath;
    m_systemLogPath = systemLogPath;
    m_bookSnapshotPath = bookSnapshotPath;

    m_watcher->addPath(tradeLogPath);
    m_watcher->addPath(systemLogPath);
    if (!bookSnapshotPath.isEmpty()) m_watcher->addPath(bookSnapshotPath);

    parseTradeLogs();
    parseSystemLogs();
    parseBookSnapshot();
}

void LogMonitor::resync()
{
    auto rehook = [this](const QString &p) {
        if (p.isEmpty()) return;
        m_watcher->removePath(p);
        m_watcher->addPath(p);
    };
    rehook(m_tradeLogPath);
    rehook(m_systemLogPath);
    rehook(m_bookSnapshotPath);

    parseTradeLogs();
    parseSystemLogs();
    parseBookSnapshot();

    emit tradesUpdated();
    emit systemLogsUpdated();
    emit statisticsUpdated();
    emit marketUpdated();
}

void LogMonitor::onTradeLogChanged(const QString &)
{
    parseTradeLogs();
    emit tradesUpdated();
    emit statisticsUpdated();
}

void LogMonitor::onSystemLogChanged(const QString &)
{
    parseSystemLogs();
    emit systemLogsUpdated();
}

void LogMonitor::onBookSnapshotChanged(const QString &path)
{
    parseBookSnapshot();
    // QFileSystemWatcher drops the watch when the file is replaced via
    // rename — re-add it so we keep receiving notifications.
    if (!m_watcher->files().contains(path))
        m_watcher->addPath(path);
    emit marketUpdated();
}

void LogMonitor::parseTradeLogs()
{
    m_trades.clear();
    m_totalTrades = 0;
    m_totalVolume = 0;

    QFile file(m_tradeLogPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
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

void LogMonitor::parseBookSnapshot()
{
    if (m_bookSnapshotPath.isEmpty()) return;

    QFile file(m_bookSnapshotPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray bytes = file.readAll();
    file.close();
    if (bytes.isEmpty()) return;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject root = doc.object();

    // ── stocks: array of {symbol, price, open, high, low, volume} ────────
    QMap<QString, MarketTick> ticks;
    const QJsonArray stocks = root.value("stocks").toArray();
    for (const auto &v : stocks) {
        QJsonObject s = v.toObject();
        MarketTick t;
        t.price  = s.value("price").toDouble();
        t.open   = s.value("open").toDouble();
        t.high   = s.value("high").toDouble();
        t.low    = s.value("low").toDouble();
        t.volume = static_cast<long>(s.value("volume").toDouble());
        ticks.insert(s.value("symbol").toString(), t);
    }
    m_marketTicks = ticks;

    // ── books: object keyed by symbol -> {bids:[…], asks:[…]} ────────────
    QMap<QString, BookLadder> books;
    QJsonObject booksObj = root.value("books").toObject();
    for (auto it = booksObj.begin(); it != booksObj.end(); ++it) {
        BookLadder ladder;
        QJsonObject sym = it.value().toObject();
        for (const auto &b : sym.value("bids").toArray()) {
            QJsonObject o = b.toObject();
            BookLevel lv;
            lv.price = o.value("price").toDouble();
            lv.qty   = o.value("qty").toInt();
            ladder.bids.append(lv);
        }
        for (const auto &a : sym.value("asks").toArray()) {
            QJsonObject o = a.toObject();
            BookLevel lv;
            lv.price = o.value("price").toDouble();
            lv.qty   = o.value("qty").toInt();
            ladder.asks.append(lv);
        }
        books.insert(it.key(), ladder);
    }
    m_books = books;
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
