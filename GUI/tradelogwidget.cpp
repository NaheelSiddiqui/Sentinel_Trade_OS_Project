#include "tradelogwidget.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QLabel>
#include <QHeaderView>

TradeLogWidget::TradeLogWidget(QWidget *parent)
    : QWidget(parent),
      m_tradeTable(new QTableWidget(this)),
      m_statsLabel(new QLabel(this))
{
    setupUI();
}

void TradeLogWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *title = new QLabel("Recent Trade Executions", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    
    m_tradeTable->setColumnCount(6);
    m_tradeTable->setHorizontalHeaderLabels({
        "Time", "Symbol", "Qty", "Price", "Buyer", "Seller"
    });
    m_tradeTable->horizontalHeader()->setStretchLastSection(true);
    m_tradeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tradeTable->setMaximumHeight(400);
    
    layout->addWidget(m_tradeTable);
    
    m_statsLabel->setStyleSheet("color: #666; font-size: 12px;");
    layout->addWidget(m_statsLabel);
    
    layout->addStretch();
}

void TradeLogWidget::updateTrades(const QVector<TradeEntry> &trades)
{
    m_tradeTable->setRowCount(trades.size());
    
    for (int i = 0; i < trades.size(); ++i) {
        const TradeEntry &trade = trades[i];
        
        m_tradeTable->setItem(i, 0, new QTableWidgetItem(
            trade.timestamp.toString("HH:mm:ss")));
        m_tradeTable->setItem(i, 1, new QTableWidgetItem(trade.symbol));
        m_tradeTable->setItem(i, 2, new QTableWidgetItem(QString::number(trade.quantity)));
        m_tradeTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(trade.price, 'f', 2)));
        m_tradeTable->setItem(i, 4, new QTableWidgetItem(
            QString("T%1").arg(trade.buyerId)));
        m_tradeTable->setItem(i, 5, new QTableWidgetItem(
            QString("T%1").arg(trade.sellerId)));
    }
    
    // Update stats
    int totalQty = 0;
    for (const auto &t : trades) totalQty += t.quantity;
    m_statsLabel->setText(QString("Total: %1 trades | %2 shares")
        .arg(trades.size()).arg(totalQty));
}
