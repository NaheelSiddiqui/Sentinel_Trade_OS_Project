#include "orderbookwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLabel>
#include <QHeaderView>

OrderBookWidget::OrderBookWidget(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(new QLabel(this)),
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
    
    // Side-by-side layout for bid and ask
    QHBoxLayout *booksLayout = new QHBoxLayout();
    
    // Bid side
    QVBoxLayout *bidLayout = new QVBoxLayout();
    QLabel *bidLabel = new QLabel("BID (Buy Orders)", this);
    bidLabel->setStyleSheet("color: #2d8659; font-weight: bold;");
    bidLayout->addWidget(bidLabel);
    
    m_bidTable->setColumnCount(2);
    m_bidTable->setHorizontalHeaderLabels({"Price", "Qty"});
    m_bidTable->horizontalHeader()->setStretchLastSection(false);
    m_bidTable->setMaximumHeight(200);
    
    bidLayout->addWidget(m_bidTable);
    booksLayout->addLayout(bidLayout);
    
    // Ask side
    QVBoxLayout *askLayout = new QVBoxLayout();
    QLabel *askLabel = new QLabel("ASK (Sell Orders)", this);
    askLabel->setStyleSheet("color: #b32121; font-weight: bold;");
    askLayout->addWidget(askLabel);
    
    m_askTable->setColumnCount(2);
    m_askTable->setHorizontalHeaderLabels({"Price", "Qty"});
    m_askTable->horizontalHeader()->setStretchLastSection(false);
    m_askTable->setMaximumHeight(200);
    
    askLayout->addWidget(m_askTable);
    booksLayout->addLayout(askLayout);
    
    mainLayout->addLayout(booksLayout);
    
    m_spreadLabel->setText("Spread: $0.00");
    m_spreadLabel->setStyleSheet("color: #666; font-size: 12px;");
    mainLayout->addWidget(m_spreadLabel);
    
    mainLayout->addStretch();
}

void OrderBookWidget::setStock(const QString &symbol)
{
    m_titleLabel->setText(QString("Order Book — %1").arg(symbol));
}

void OrderBookWidget::updateOrderBook(const QStringList &bids, const QStringList &asks)
{
    m_bidTable->setRowCount(bids.size());
    for (int i = 0; i < bids.size(); ++i) {
        QStringList parts = bids[i].split(" ");
        if (parts.size() >= 2) {
            m_bidTable->setItem(i, 0, new QTableWidgetItem(parts[0]));
            m_bidTable->setItem(i, 1, new QTableWidgetItem(parts[1]));
        }
    }
    
    m_askTable->setRowCount(asks.size());
    for (int i = 0; i < asks.size(); ++i) {
        QStringList parts = asks[i].split(" ");
        if (parts.size() >= 2) {
            m_askTable->setItem(i, 0, new QTableWidgetItem(parts[0]));
            m_askTable->setItem(i, 1, new QTableWidgetItem(parts[1]));
        }
    }
}
