#include "marketwidget.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>

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
    
    m_tickerTable->setColumnCount(4);
    m_tickerTable->setHorizontalHeaderLabels({"Symbol", "Price", "Change %", "Status"});
    m_tickerTable->horizontalHeader()->setStretchLastSection(true);
    m_tickerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tickerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tickerTable->setMaximumHeight(300);
    
    layout->addWidget(m_tickerTable);
    layout->addStretch();
    
    // Populate with demo data
    const QStringList symbols = {"AAPL", "GOOG", "MSFT", "AMZN", "TSLA", "NVDA", "META", "JPM", "BRK", "V"};
    m_tickerTable->setRowCount(symbols.size());
    
    for (int i = 0; i < symbols.size(); ++i) {
        m_tickerTable->setItem(i, 0, new QTableWidgetItem(symbols[i]));
        m_tickerTable->setItem(i, 1, new QTableWidgetItem("$0.00"));
        m_tickerTable->setItem(i, 2, new QTableWidgetItem("0.00%"));
        m_tickerTable->setItem(i, 3, new QTableWidgetItem("⬤ Idle"));
        m_rowMap[symbols[i]] = i;
    }
    
    connect(m_tickerTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        int row = m_tickerTable->currentRow();
        if (row >= 0) {
            QString symbol = m_tickerTable->item(row, 0)->text();
            emit stockSelected(symbol);
        }
    });
}

void MarketWidget::updatePrices(const QMap<QString, double> &prices)
{
    // This would be called periodically from log data
    // For now, it's a placeholder for future real-time updates
}

void MarketWidget::setSelectedStock(const QString &symbol)
{
    if (m_rowMap.contains(symbol)) {
        m_tickerTable->selectRow(m_rowMap[symbol]);
    }
}
