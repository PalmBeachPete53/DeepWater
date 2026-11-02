#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QVector>

struct FxPair {
    QString base;    // e.g. EUR in EUR/USD
    QString quote;   // e.g. USD
    QString label() const { return base + '/' + quote; }
};

// Daily FX reference rates (European Central Bank, served by frankfurter.dev,
// no key). One fix per business day around 16:00 CET: suited to history and
// correlations, not intraday. Cached on disk and refreshed every 6 hours.
class FxData : public QObject
{
    Q_OBJECT
public:
    explicit FxData(QObject *parent = nullptr);

    const QVector<FxPair> &pairs() const { return m_pairs; }
    const QVector<QDate> &dates() const { return m_dates; }
    // Rates of one pair aligned with dates(), oldest first.
    QVector<double> series(int pair) const;
    double latest(int pair) const;
    double changePct(int pair) const;   // last fix vs the previous one

    // Pearson correlation matrix of daily log returns over the last `returns`
    // observations (fewer if history is shorter). Also reports the count used.
    QVector<QVector<double>> correlation(int returns, int *used = nullptr) const;

    QDateTime fetchedAt() const { return m_fetchedAt; }
    QString errorString() const { return m_error; }
    void refreshIfStale();

    // Sources page: a source turned off makes no requests (its last cached copy stays in use);
    // refreshNow() downloads at once whatever the age of the data.
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    void refreshNow() { fetch(); }
    bool isFetching() const { return m_fetching; }

signals:
    void updated();

private:
    void fetch();
    bool parse(const QByteArray &json);
    void loadCache();

    QNetworkAccessManager m_net;
    QVector<FxPair> m_pairs;
    QVector<QDate> m_dates;
    QHash<QString, QVector<double>> m_perUsd;   // currency -> units per 1 USD, aligned with m_dates
    QDateTime m_fetchedAt;
    QString m_error;
    bool m_fetching = false;
    bool m_enabled = true;
    QTimer m_retryTimer;
};
