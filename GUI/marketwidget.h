#pragma once
#include <QWidget>
#include <QMap>
#include "logmonitor.h"

class QLabel;
class QTableWidget;

class MarketWidget : public QWidget {
    Q_OBJECT
public:
    explicit MarketWidget(QWidget *parent = nullptr);

    // Live data feed.
    void updateMarket(const QMap<QString, MarketTick> &ticks);

    void setSelectedStock(const QString &symbol);

signals:
    void stockSelected(const QString &symbol);

private:
    void setupUI();

    QTableWidget *m_tickerTable;
    QMap<QString, int>    m_rowMap;       // symbol -> row index
    QMap<QString, double> m_lastPrice;    // symbol -> prior tick price (for trend color)
};
