#pragma once
#include <QMainWindow>
#include <QProcess>
#include "logmonitor.h"

class QTabWidget;
class QAction;
class QCloseEvent;
class MarketWidget;
class OrderBookWidget;
class TradeLogWidget;
class SystemWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTradesUpdated();
    void onSystemLogsUpdated();
    void onStatisticsUpdated();
    void onMarketUpdated();
    void onStockSelected(const QString &symbol);

    // Backend process control
    void startBackend();
    void stopBackend();
    void restartBackend();
    void onBackendStdout();
    void onBackendStderr();
    void onBackendStarted();
    void onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBackendErrorOccurred(QProcess::ProcessError error);

private:
    void setupUI();
    void setupToolbar();
    void setupConnections();
    void startMonitoring();
    QString locateBackendExecutable() const;
    QString backendWorkingDir() const;

    QTabWidget       *m_tabWidget;
    MarketWidget     *m_marketWidget;
    OrderBookWidget  *m_orderBookWidget;
    TradeLogWidget   *m_tradeLogWidget;
    SystemWidget     *m_systemWidget;

    LogMonitor       *m_logMonitor;
    QProcess         *m_backend;
    QAction          *m_startAction;
    QAction          *m_stopAction;
    QAction          *m_restartAction;

    QString m_selectedStock = "AAPL";
};
