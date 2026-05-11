#pragma once
#include <QWidget>
#include "logmonitor.h"

class QTableWidget;
class QLabel;

class OrderBookWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderBookWidget(QWidget *parent = nullptr);

    void setStock(const QString &symbol);

    // Render aggregated ladder for the current symbol.
    void updateLadder(const BookLadder &ladder, double lastPrice);

private:
    void setupUI();

    QString       m_symbol;
    QLabel       *m_titleLabel;
    QLabel       *m_summaryLabel;
    QTableWidget *m_bidTable;
    QTableWidget *m_askTable;
    QLabel       *m_spreadLabel;
};
