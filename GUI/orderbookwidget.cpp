#include "orderbookwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QHeaderView>

OrderBookWidget::OrderBookWidget(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(new QLabel(this)),
      m_summaryLabel(new QLabel(this)),
      m_bidTable(new QTableWidget(this)),
      m_askTable(new QTableWidget(this)),
      m_spreadLabel(new QLabel(this))
{
    setupUI();
}

void OrderBookWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_titleLabel->setText("Order Book");
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    mainLayout->addWidget(m_titleLabel);

    m_summaryLabel->setStyleSheet("color: #666; font-size: 11px;");
    m_summaryLabel->setText("Last: —    Aggregated by price; top 5 levels per side.");
    mainLayout->addWidget(m_summaryLabel);

    auto buildLadderTable = [this](QTableWidget *tbl) {
        tbl->setColumnCount(2);
        tbl->setHorizontalHeaderLabels({"Price", "Qty"});
        tbl->horizontalHeader()->setStretchLastSection(true);
        tbl->verticalHeader()->setVisible(false);
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setSelectionMode(QAbstractItemView::NoSelection);
        tbl->setFocusPolicy(Qt::NoFocus);
        tbl->setShowGrid(false);
    };

    QHBoxLayout *booksLayout = new QHBoxLayout();

    // ── Bid side ─────────────────────────────────────────────────────────
    QVBoxLayout *bidLayout = new QVBoxLayout();
    QLabel *bidLabel = new QLabel("BID (Buy Orders) — sorted desc", this);
    bidLabel->setStyleSheet("color: #2d8659; font-weight: bold;");
    bidLayout->addWidget(bidLabel);
    buildLadderTable(m_bidTable);
    bidLayout->addWidget(m_bidTable);
    booksLayout->addLayout(bidLayout);

    // ── Ask side ─────────────────────────────────────────────────────────
    QVBoxLayout *askLayout = new QVBoxLayout();
    QLabel *askLabel = new QLabel("ASK (Sell Orders) — sorted asc", this);
    askLabel->setStyleSheet("color: #b32121; font-weight: bold;");
    askLayout->addWidget(askLabel);
    buildLadderTable(m_askTable);
    askLayout->addWidget(m_askTable);
    booksLayout->addLayout(askLayout);

    mainLayout->addLayout(booksLayout, /*stretch=*/1);

    m_spreadLabel->setText("Spread: —");
    m_spreadLabel->setStyleSheet(
        "background: #f5f5f5; padding: 6px; border-radius: 4px;"
        "font-family: monospace;");
    mainLayout->addWidget(m_spreadLabel);
}

void OrderBookWidget::setStock(const QString &symbol)
{
    m_symbol = symbol;
    m_titleLabel->setText(QString("Order Book — %1").arg(symbol));
}

void OrderBookWidget::updateLadder(const BookLadder &ladder, double lastPrice)
{
    // ── Bid table ────────────────────────────────────────────────────────
    m_bidTable->setRowCount(ladder.bids.size());
    int totalBidQty = 0;
    for (int i = 0; i < ladder.bids.size(); ++i) {
        const BookLevel &lv = ladder.bids[i];
        auto *priceItem = new QTableWidgetItem(QString("$%1").arg(lv.price, 0, 'f', 2));
        auto *qtyItem   = new QTableWidgetItem(QString::number(lv.qty));
        priceItem->setForeground(QColor("#2d8659"));
        m_bidTable->setItem(i, 0, priceItem);
        m_bidTable->setItem(i, 1, qtyItem);
        totalBidQty += lv.qty;
    }

    // ── Ask table ────────────────────────────────────────────────────────
    m_askTable->setRowCount(ladder.asks.size());
    int totalAskQty = 0;
    for (int i = 0; i < ladder.asks.size(); ++i) {
        const BookLevel &lv = ladder.asks[i];
        auto *priceItem = new QTableWidgetItem(QString("$%1").arg(lv.price, 0, 'f', 2));
        auto *qtyItem   = new QTableWidgetItem(QString::number(lv.qty));
        priceItem->setForeground(QColor("#b32121"));
        m_askTable->setItem(i, 0, priceItem);
        m_askTable->setItem(i, 1, qtyItem);
        totalAskQty += lv.qty;
    }

    // ── Summary line: best bid / best ask / spread / last trade ─────────
    QString bestBid = ladder.bids.isEmpty()
        ? "—" : QString("$%1").arg(ladder.bids.first().price, 0, 'f', 2);
    QString bestAsk = ladder.asks.isEmpty()
        ? "—" : QString("$%1").arg(ladder.asks.first().price, 0, 'f', 2);

    QString spreadText;
    if (!ladder.bids.isEmpty() && !ladder.asks.isEmpty()) {
        double spread = ladder.asks.first().price - ladder.bids.first().price;
        spreadText = QString("$%1").arg(spread, 0, 'f', 2);
    } else {
        spreadText = "—";
    }

    QString lastText = (lastPrice > 0.0)
        ? QString("$%1").arg(lastPrice, 0, 'f', 2) : QString("—");

    m_summaryLabel->setText(QString(
        "Last: %1    Aggregated by price; top 5 levels per side.").arg(lastText));
    m_spreadLabel->setText(QString(
        "Best Bid: %1   |   Best Ask: %2   |   Spread: %3   "
        "|   Resting Qty:  bids=%4  asks=%5")
        .arg(bestBid, bestAsk, spreadText)
        .arg(totalBidQty).arg(totalAskQty));
}
