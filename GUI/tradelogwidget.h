#pragma once
#include <QWidget>
#include "logmonitor.h"

class QTableWidget;
class QLabel;

class TradeLogWidget : public QWidget {
    Q_OBJECT
public:
    explicit TradeLogWidget(QWidget *parent = nullptr);
    
    void updateTrades(const QVector<TradeEntry> &trades);

private:
    void setupUI();
    
    QTableWidget *m_tradeTable;
    QLabel *m_statsLabel;
};
