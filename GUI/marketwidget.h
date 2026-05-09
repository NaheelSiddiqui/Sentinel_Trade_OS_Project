#pragma once
#include <QWidget>
#include <QMap>

class QLabel;
class QTableWidget;

class MarketWidget : public QWidget {
    Q_OBJECT
public:
    explicit MarketWidget(QWidget *parent = nullptr);
    
    void updatePrices(const QMap<QString, double> &prices);
    void setSelectedStock(const QString &symbol);
    
signals:
    void stockSelected(const QString &symbol);

private:
    void setupUI();
    
    QTableWidget *m_tickerTable;
    QMap<QString, int> m_rowMap;  // symbol -> row
};
