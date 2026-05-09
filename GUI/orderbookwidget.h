#pragma once
#include <QWidget>

class QTableWidget;
class QLabel;

class OrderBookWidget : public QWidget {
    Q_OBJECT
public:
    explicit OrderBookWidget(QWidget *parent = nullptr);
    
    void setStock(const QString &symbol);
    void updateOrderBook(const QStringList &bids, const QStringList &asks);

private:
    void setupUI();
    
    QLabel *m_titleLabel;
    QTableWidget *m_bidTable;
    QTableWidget *m_askTable;
    QLabel *m_spreadLabel;
};
