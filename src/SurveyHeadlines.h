#pragma once

#include "DomesticFinance.h"

// The headline figures of the private surveys (purchasing managers' indexes, business climate and sentiment
// surveys, private payrolls), which their publishers release for free and whose series are paid: read from the
// economic calendar (TradingView, no key) over the last weeks, the latest actual of each survey and country.
// Each figure keeps its publisher, and a publisher can be turned off in Sources.
class SurveyHeadlines : public CountrySeriesSet
{
    Q_OBJECT
public:
    // The series are the KeyIndicators ids of the survey figures (see KeyIndicators::Id).
    enum Series {
        ManufacturingPmi, ServicesPmi, CompositePmi, BusinessClimate, EconomicSentiment, ConsumerSentiment,
        PrivatePayrolls, SeriesCount
    };
    struct Publisher {
        QString id;     // "spglobal"
        QString name;   // "S&P Global"
        QString host;
        QString about;  // what it publishes
    };

    explicit SurveyHeadlines(QObject *parent = nullptr);

    static const QVector<Publisher> &publishers();
    // The publisher of a country's figure, or empty.
    QString publisherOf(const QString &iso3, int series) const;
    // Figures of a publisher in the data (for its row in Sources).
    int figureCount(const QString &publisherId) const;
    void setPublisherEnabled(const QString &publisherId, bool on);
    bool isPublisherEnabled(const QString &publisherId) const { return !m_off.contains(publisherId); }
    // The series of a country, empty when its publisher is off.
    QMap<double, double> enabledSeries(const QString &iso3, int series) const;

protected:
    QList<QUrl> urls() const override;
    void prepare(QNetworkRequest &request) const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
    bool parse(const QByteArray &body) override;

private:
    QHash<QString, QString> m_publisher;   // "ISO3/k" -> publisher id
    QSet<QString> m_off;
};
