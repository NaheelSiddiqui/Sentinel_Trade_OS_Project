#include "mainwindow.h"
#include "marketwidget.h"
#include "orderbookwidget.h"
#include "tradelogwidget.h"
#include "systemwidget.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <QToolBar>
#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStyle>
#include <QFile>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <signal.h>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_tabWidget(new QTabWidget(this)),
      m_marketWidget(new MarketWidget(this)),
      m_orderBookWidget(new OrderBookWidget(this)),
      m_tradeLogWidget(new TradeLogWidget(this)),
      m_systemWidget(new SystemWidget(this)),
      m_logMonitor(new LogMonitor(this)),
      m_backend(new QProcess(this)),
      m_startAction(nullptr),
      m_stopAction(nullptr),
      m_restartAction(nullptr)
{
    // Merge stdout + stderr so a single signal carries everything.
    m_backend->setProcessChannelMode(QProcess::MergedChannels);

    setupUI();
    setupToolbar();
    setupConnections();
    startMonitoring();

    // Refresh UI every 500ms (parsed log state -> widgets).
    QTimer *refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, [this]() {
        onTradesUpdated();
        onSystemLogsUpdated();
        onStatisticsUpdated();
    });
    refreshTimer->start(500);

    // Auto-start the backend so the user gets a live view with zero clicks.
    QTimer::singleShot(0, this, &MainWindow::startBackend);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    setWindowTitle("SentinelTrade — Multi-threaded Stock Exchange Simulator");
    resize(1100, 750);

    m_tabWidget->addTab(m_marketWidget,    "Market");
    m_tabWidget->addTab(m_orderBookWidget, "Order Book");
    m_tabWidget->addTab(m_tradeLogWidget,  "Trades");
    m_tabWidget->addTab(m_systemWidget,    "System");

    setCentralWidget(m_tabWidget);

    m_orderBookWidget->setStock(m_selectedStock);
}

void MainWindow::setupToolbar()
{
    QToolBar *bar = addToolBar("Backend");
    bar->setMovable(false);

    m_startAction = bar->addAction(
        style()->standardIcon(QStyle::SP_MediaPlay), "Start");
    m_stopAction = bar->addAction(
        style()->standardIcon(QStyle::SP_MediaStop), "Stop");
    m_restartAction = bar->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload), "Restart");

    m_stopAction->setEnabled(false);
    m_restartAction->setEnabled(false);

    connect(m_startAction,   &QAction::triggered, this, &MainWindow::startBackend);
    connect(m_stopAction,    &QAction::triggered, this, &MainWindow::stopBackend);
    connect(m_restartAction, &QAction::triggered, this, &MainWindow::restartBackend);
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

    connect(m_backend, &QProcess::readyReadStandardOutput,
            this, &MainWindow::onBackendStdout);
    connect(m_backend, &QProcess::readyReadStandardError,
            this, &MainWindow::onBackendStderr);
    connect(m_backend, &QProcess::started,
            this, &MainWindow::onBackendStarted);
    connect(m_backend,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onBackendFinished);
    connect(m_backend, &QProcess::errorOccurred,
            this, &MainWindow::onBackendErrorOccurred);
}

void MainWindow::startMonitoring()
{
    // Tail logs in the backend working directory (== GUI/ when we spawn
    // the backend ourselves). MainWindow controls the working directory,
    // so these paths line up.
    QString dir = backendWorkingDir();
    m_logMonitor->startMonitoring(dir + "/trades.log", dir + "/system.log");
}

QString MainWindow::backendWorkingDir() const
{
    // Work in the GUI source dir (one level above the executable in the
    // standard `GUI/build/` layout). Logs land there and the log monitor
    // already watches them.
    QDir d(QCoreApplication::applicationDirPath());
    d.cdUp();
    return d.absolutePath();
}

QString MainWindow::locateBackendExecutable() const
{
    // Search the layouts we actually ship:
    //   GUI/build/sentinel_trade_gui   ->  ../../Backend/sentinel_trade
    //   GUI/sentinel_trade_gui         ->  ../Backend/sentinel_trade
    //   ./sentinel_trade               (user copied it next to the GUI)
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList names{
#ifdef Q_OS_WIN
        "sentinel_trade.exe"
#else
        "sentinel_trade"
#endif
    };
    const QStringList relRoots{
        appDir + "/../../Backend",
        appDir + "/../Backend",
        appDir + "/../../backend",
        appDir + "/../backend",
        appDir,
    };

    for (const QString &root : relRoots) {
        for (const QString &name : names) {
            QString candidate = QDir(root).absoluteFilePath(name);
            if (QFile::exists(candidate)) return candidate;
        }
    }
    return QString();
}

void MainWindow::startBackend()
{
    if (m_backend->state() != QProcess::NotRunning) return;

    QString exe = locateBackendExecutable();
    if (exe.isEmpty()) {
        m_systemWidget->setBackendStatus("Backend: not found", "#b32121");
        m_systemWidget->appendBackendOutput(
            "[GUI] Cannot find sentinel_trade executable. "
            "Build it first (`cd Backend && make`) and place it where the "
            "GUI can find it (e.g. ../Backend/ relative to the GUI exe).");
        QMessageBox::warning(this, "Backend not found",
            "Could not locate the sentinel_trade executable.\n\n"
            "Build it with:  cd Backend && make");
        return;
    }

    QString wd = backendWorkingDir();
    m_backend->setWorkingDirectory(wd);

    m_systemWidget->clearBackendOutput();
    m_systemWidget->appendBackendOutput(
        QString("[GUI] Launching: %1\n[GUI] Working dir: %2").arg(exe, wd));
    m_systemWidget->setBackendStatus("Backend: starting…", "#cc7700");

    m_backend->start(exe, QStringList{});
}

void MainWindow::stopBackend()
{
    if (m_backend->state() == QProcess::NotRunning) return;

    m_systemWidget->appendBackendOutput("[GUI] Sending shutdown signal…");
    m_systemWidget->setBackendStatus("Backend: stopping…", "#cc7700");

    // The backend installs a SIGINT handler that prints a final report
    // and exits cleanly. terminate() sends SIGTERM on Unix and posts
    // WM_CLOSE on Windows, which is close enough for our purposes.
#ifdef Q_OS_UNIX
    ::kill(static_cast<pid_t>(m_backend->processId()), SIGINT);
#else
    m_backend->terminate();
#endif

    if (!m_backend->waitForFinished(3000)) {
        m_systemWidget->appendBackendOutput(
            "[GUI] Backend did not exit in 3s — killing it.");
        m_backend->kill();
        m_backend->waitForFinished(1000);
    }
}

void MainWindow::restartBackend()
{
    stopBackend();
    m_logMonitor->resync();  // log files have been truncated by O_TRUNC
    startBackend();
}

void MainWindow::onBackendStarted()
{
    m_systemWidget->setBackendStatus("Backend: running", "#2d8659");
    m_startAction->setEnabled(false);
    m_stopAction->setEnabled(true);
    m_restartAction->setEnabled(true);

    // The backend just truncated its log files (O_TRUNC) — drop the
    // cached state so we don't show stale rows from a previous run.
    m_logMonitor->resync();
}

void MainWindow::onBackendStdout()
{
    QByteArray data = m_backend->readAllStandardOutput();
    if (!data.isEmpty())
        m_systemWidget->appendBackendOutput(QString::fromLocal8Bit(data));
}

void MainWindow::onBackendStderr()
{
    QByteArray data = m_backend->readAllStandardError();
    if (!data.isEmpty())
        m_systemWidget->appendBackendOutput(QString::fromLocal8Bit(data));
}

void MainWindow::onBackendFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_systemWidget->appendBackendOutput(
        QString("[GUI] Backend exited (code=%1, status=%2)")
            .arg(exitCode)
            .arg(exitStatus == QProcess::NormalExit ? "normal" : "crashed"));
    m_systemWidget->setBackendStatus("Backend: stopped", "#888");
    m_startAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_restartAction->setEnabled(false);
}

void MainWindow::onBackendErrorOccurred(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
        case QProcess::FailedToStart: msg = "FailedToStart"; break;
        case QProcess::Crashed:       msg = "Crashed";       break;
        case QProcess::Timedout:      msg = "Timedout";      break;
        case QProcess::WriteError:    msg = "WriteError";    break;
        case QProcess::ReadError:     msg = "ReadError";     break;
        default:                      msg = "UnknownError";  break;
    }
    m_systemWidget->appendBackendOutput("[GUI] QProcess error: " + msg);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Don't leave an orphan backend behind.
    if (m_backend->state() != QProcess::NotRunning) {
        stopBackend();
    }
    event->accept();
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
