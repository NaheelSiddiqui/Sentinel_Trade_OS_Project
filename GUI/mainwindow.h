#pragma once
#include <QMainWindow>
#include "logmonitor.h"

class QTabWidget;
class MarketWidget;
class OrderBookWidget;
class TradeLogWidget;
class SystemWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onTradesUpdated();
    void onSystemLogsUpdated();
    void onStatisticsUpdated();
    void onStockSelected(const QString &symbol);

private:
    void setupUI();
    void setupConnections();
    void startMonitoring();
    
    QTabWidget *m_tabWidget;
    MarketWidget *m_marketWidget;
    OrderBookWidget *m_orderBookWidget;
    TradeLogWidget *m_tradeLogWidget;
    SystemWidget *m_systemWidget;
    
    LogMonitor *m_logMonitor;
    QString m_selectedStock = "AAPL";
};
