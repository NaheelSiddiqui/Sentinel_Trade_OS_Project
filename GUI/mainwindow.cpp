#include "mainwindow.h"
#include "marketwidget.h"
#include "orderbookwidget.h"
#include "tradelogwidget.h"
#include "systemwidget.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_tabWidget(new QTabWidget(this)),
      m_marketWidget(new MarketWidget(this)),
      m_orderBookWidget(new OrderBookWidget(this)),
      m_tradeLogWidget(new TradeLogWidget(this)),
      m_systemWidget(new SystemWidget(this)),
      m_logMonitor(new LogMonitor(this))
{
    setupUI();
    setupConnections();
    startMonitoring();
    
    // Refresh UI every 500ms
    QTimer *refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        onTradesUpdated();
        onSystemLogsUpdated();
        onStatisticsUpdated();
    });
    refreshTimer->start(500);
}

void MainWindow::setupUI()
{
    setWindowTitle("SentinelTrade — Multi-threaded Stock Exchange Simulator");
    setWindowIcon(QIcon());
    resize(1000, 700);
    
    m_tabWidget->addTab(m_marketWidget, "Market");
    m_tabWidget->addTab(m_orderBookWidget, "Order Book");
    m_tabWidget->addTab(m_tradeLogWidget, "Trades");
    m_tabWidget->addTab(m_systemWidget, "System");
    
    setCentralWidget(m_tabWidget);
    
    // Set initial stock
    m_orderBookWidget->setStock(m_selectedStock);
}

void MainWindow::setupConnections()
{
    connect(m_marketWidget, &MarketWidget::stockSelected,
            this, &MainWindow::onStockSelected);
    
    connect(m_logMonitor, &LogMonitor::tradesUpdated,
            this, &MainWindow::onTradesUpdated);
    
    connect(m_logMonitor, &LogMonitor::systemLogsUpdated,
            this, &MainWindow::onSystemLogsUpdated);
    
    connect(m_logMonitor, &LogMonitor::statisticsUpdated,
            this, &MainWindow::onStatisticsUpdated);
}

void MainWindow::startMonitoring()
{
    // Look for log files in the parent directory
    QString logDir = "../";
    m_logMonitor->startMonitoring(logDir + "trades.log", logDir + "system.log");
}

void MainWindow::onTradesUpdated()
{
    auto trades = m_logMonitor->getRecentTrades(50);
    m_tradeLogWidget->updateTrades(trades);
}

void MainWindow::onSystemLogsUpdated()
{
    auto logs = m_logMonitor->getRecentSystemLogs(100);
    m_systemWidget->updateSystemLogs(logs);
}

void MainWindow::onStatisticsUpdated()
{
    m_systemWidget->updateStatistics(
        m_logMonitor->getTotalTrades(),
        m_logMonitor->getTotalVolume());
}

void MainWindow::onStockSelected(const QString &symbol)
{
    m_selectedStock = symbol;
    m_orderBookWidget->setStock(symbol);
}
