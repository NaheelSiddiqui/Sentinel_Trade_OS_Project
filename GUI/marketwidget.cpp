#include "marketwidget.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>

namespace {
const QStringList kSymbols = {
    "AAPL", "GOOG", "MSFT", "AMZN", "TSLA",
    "NVDA", "META", "JPM",  "BRK",  "V"
};
}  // namespace

MarketWidget::MarketWidget(QWidget *parent)
    : QWidget(parent), m_tickerTable(new QTableWidget(this))
{
    setupUI();
}

void MarketWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *title = new QLabel("Live Stock Prices", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);

    QLabel *hint = new QLabel("Click a row to view its order book.", this);
    hint->setStyleSheet("color: #666; font-size: 11px;");
    layout->addWidget(hint);

    m_tickerTable->setColumnCount(6);
    m_tickerTable->setHorizontalHeaderLabels(
        {"Symbol", "Price", "Change %", "High", "Low", "Volume"});
    m_tickerTable->horizontalHeader()->setStretchLastSection(true);
    m_tickerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tickerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tickerTable->verticalHeader()->setVisible(false);
    m_tickerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_tickerTable, /*stretch=*/1);

    // Pre-populate the table so a row exists for each symbol before any
    // snapshot has arrived. Real values get filled in on the first
    // updateMarket() call.
    m_tickerTable->setRowCount(kSymbols.size());
    for (int i = 0; i < kSymbols.size(); ++i) {
        m_tickerTable->setItem(i, 0, new QTableWidgetItem(kSymbols[i]));
        for (int c = 1; c < 6; ++c) {
            m_tickerTable->setItem(i, c, new QTableWidgetItem("—"));
        }
        m_rowMap[kSymbols[i]] = i;
    }

    connect(m_tickerTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_tickerTable->currentRow();
        if (row >= 0) {
            QString symbol = m_tickerTable->item(row, 0)->text();
            emit stockSelected(symbol);
        }
    });
}

void MarketWidget::updateMarket(const QMap<QString, MarketTick> &ticks)
{
    for (auto it = ticks.constBegin(); it != ticks.constEnd(); ++it) {
        const QString &sym = it.key();
        const MarketTick &t = it.value();

        // Add a row on the fly for any symbol we hadn't seen before.
        int row;
        if (m_rowMap.contains(sym)) {
            row = m_rowMap[sym];
        } else {
            row = m_tickerTable->rowCount();
            m_tickerTable->insertRow(row);
            m_tickerTable->setItem(row, 0, new QTableWidgetItem(sym));
            m_rowMap[sym] = row;
        }

        double priorPrice = m_lastPrice.value(sym, t.price);
        QColor trendColor = (t.price > priorPrice) ? QColor("#2d8659")
                          : (t.price < priorPrice) ? QColor("#b32121")
                                                   : QColor("#444");
        m_lastPrice[sym] = t.price;

        double changePct = (t.open > 0.0) ? (t.price - t.open) / t.open * 100.0 : 0.0;
        QColor changeColor = (changePct > 0.0) ? QColor("#2d8659")
                           : (changePct < 0.0) ? QColor("#b32121")
                                               : QColor("#444");

        auto setCell = [&](int col, const QString &text, const QColor &fg) {
            QTableWidgetItem *item = m_tickerTable->item(row, col);
            if (!item) {
                item = new QTableWidgetItem();
                m_tickerTable->setItem(row, col, item);
            }
            item->setText(text);
            item->setForeground(fg);
        };

        setCell(1, QString("$%1").arg(t.price, 0, 'f', 2),                trendColor);
        setCell(2, QString("%1%2%").arg(changePct >= 0 ? "+" : "")
                                   .arg(changePct, 0, 'f', 2),            changeColor);
        setCell(3, QString("$%1").arg(t.high, 0, 'f', 2),                 QColor("#444"));
        setCell(4, QString("$%1").arg(t.low,  0, 'f', 2),                 QColor("#444"));
        setCell(5, QString::number(t.volume),                             QColor("#444"));
    }
}

void MarketWidget::setSelectedStock(const QString &symbol)
{
    if (m_rowMap.contains(symbol)) {
        m_tickerTable->selectRow(m_rowMap[symbol]);
    }
}
