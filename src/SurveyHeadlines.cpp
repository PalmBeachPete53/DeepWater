#include "SurveyHeadlines.h"

#include "CalendarData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>

#include <cmath>
#include <iterator>

namespace {

// How a calendar event maps to a survey figure: a pattern on its title, the series, the publisher.
struct Rule {
    const char *pattern;
    SurveyHeadlines::Series series;
    const char *publisher;
};
const Rule Rules[] = {
    // Purchasing managers' indexes: S&P Global's, with their local sponsors, and the ISM's.
    {"ISM Manufacturing PMI", SurveyHeadlines::ManufacturingPmi, "ism"},
    {"ISM Services PMI", SurveyHeadlines::ServicesPmi, "ism"},
    {"ISM Non-Manufacturing PMI", SurveyHeadlines::ServicesPmi, "ism"},
    {"NBS Manufacturing PMI", SurveyHeadlines::ManufacturingPmi, "nbs"},
    {"NBS Non Manufacturing PMI", SurveyHeadlines::ServicesPmi, "nbs"},
    {"Manufacturing PMI", SurveyHeadlines::ManufacturingPmi, "spglobal"},
    {"Services PMI", SurveyHeadlines::ServicesPmi, "spglobal"},
    {"Composite PMI", SurveyHeadlines::CompositePmi, "spglobal"},
    // Business climate and confidence.
    {"Ifo Business Climate", SurveyHeadlines::BusinessClimate, "ifo"},
    {"NAB Business Confidence", SurveyHeadlines::BusinessClimate, "nab"},
    {"ANZ Business Confidence", SurveyHeadlines::BusinessClimate, "anz"},
    {"CBI Business Optimism", SurveyHeadlines::BusinessClimate, "cbi"},
    {"CBI Industrial Trends", SurveyHeadlines::BusinessClimate, "cbi"},
    {"NFIB Business Optimism", SurveyHeadlines::BusinessClimate, "nfib"},
    {"NZIER Business Confidence", SurveyHeadlines::BusinessClimate, "nzier"},
    // Economic sentiment.
    {"ZEW Economic Sentiment", SurveyHeadlines::EconomicSentiment, "zew"},
    {"Sentix Investor Confidence", SurveyHeadlines::EconomicSentiment, "sentix"},
    // Consumer sentiment.
    {"Michigan Consumer Sentiment", SurveyHeadlines::ConsumerSentiment, "michigan"},
    {"CB Consumer Confidence", SurveyHeadlines::ConsumerSentiment, "conferenceboard"},
    {"GfK Consumer Confidence", SurveyHeadlines::ConsumerSentiment, "gfk"},
    {"Westpac Consumer Confidence", SurveyHeadlines::ConsumerSentiment, "westpac"},
    {"ANZ Roy Morgan Consumer Confidence", SurveyHeadlines::ConsumerSentiment, "anz"},
    // Private payrolls.
    {"ADP Employment Change", SurveyHeadlines::PrivatePayrolls, "adp"},
    {"ADP Nonfarm Employment Change", SurveyHeadlines::PrivatePayrolls, "adp"},
};

// Titles that look like a survey but aren't the headline figure (flash estimates are, final ones replace them).
bool skipped(const QString &title)
{
    static const QRegularExpression rx("Prices|Employment Index|New Orders|Output|Expectations|Current Conditions|"
                                       "Current Assessment|Inflation Expectations|5-Year|Prel\\b|Weekly|\\bChange\\b|"
                                       "Capacity|Job Ads|Leading Index|Distributive",
                                       QRegularExpression::CaseInsensitiveOption);
    // "ADP Employment Change" is the headline; "Westpac Consumer Confidence Change" and "ADP … Weekly" aren't.
    if (title.contains("ADP Employment Change", Qt::CaseInsensitive) && !title.contains("Weekly", Qt::CaseInsensitive))
        return false;
    return rx.match(title).hasMatch() && !title.contains("Flash", Qt::CaseInsensitive);
}

} // namespace

SurveyHeadlines::SurveyHeadlines(QObject *parent)
    : CountrySeriesSet("survey_headlines", 6 * 3600, SeriesCount, parent)
{
    loadCached();
}

const QVector<SurveyHeadlines::Publisher> &SurveyHeadlines::publishers()
{
    // Rebuilt when the language changes (the first call comes before the translator is installed).
    static QVector<Publisher> list;
    static QString language;
    if (!list.isEmpty() && language == QLocale().name())
        return list;
    language = QLocale().name();
    list = {
        {"spglobal", "S&P Global", "pmi.spglobal.com", tr("Purchasing managers' indexes (manufacturing, services, composite) of some 40 economies, with HCOB, au Jibun Bank, Caixin, CIPS and Judo Bank")},
        {"ism", "Institute for Supply Management", "ismworld.org", tr("ISM manufacturing and services PMIs (United States)")},
        {"nbs", "National Bureau of Statistics of China", "stats.gov.cn", tr("Official manufacturing and non-manufacturing PMIs (China)")},
        {"ifo", "ifo Institute", "ifo.de", tr("Ifo business climate (Germany)")},
        {"zew", "ZEW", "zew.de", tr("ZEW economic sentiment (Germany, euro area)")},
        {"gfk", "GfK", "gfk.com", tr("Consumer confidence (Germany, United Kingdom)")},
        {"michigan", "University of Michigan", "sca.isr.umich.edu", tr("Consumer sentiment (United States)")},
        {"conferenceboard", "The Conference Board", "conference-board.org", tr("Consumer confidence (United States)")},
        {"sentix", "Sentix", "sentix.de", tr("Investor confidence (euro area)")},
        {"nab", "National Australia Bank", "business.nab.com.au", tr("Business confidence (Australia)")},
        {"westpac", "Westpac · Melbourne Institute", "westpac.com.au", tr("Consumer confidence (Australia)")},
        {"anz", "ANZ", "anz.com", tr("Business confidence (New Zealand), consumer confidence with Roy Morgan (Australia)")},
        {"nzier", "NZIER", "nzier.org.nz", tr("Quarterly business opinion (New Zealand)")},
        {"cbi", "Confederation of British Industry", "cbi.org.uk", tr("Industrial trends and business optimism (United Kingdom)")},
        {"nfib", "NFIB", "nfib.com", tr("Small business optimism (United States)")},
        {"adp", "ADP", "adpemploymentreport.com", tr("Private payrolls (United States)")},
    };
    return list;
}

QList<QUrl> SurveyHeadlines::urls() const
{
    // The last ten weeks: every monthly survey has been released at least once, most twice.
    const QDate today = QDate::currentDate();
    // TradingView answers at most 2000 events, the earliest of the range, about six weeks of all these countries.
    return {EconomicCalendar::rangeUrl(today.addDays(-70), today.addDays(-46)), EconomicCalendar::rangeUrl(today.addDays(-46), today.addDays(-23)),
            EconomicCalendar::rangeUrl(today.addDays(-23), today.addDays(1))};
}

void SurveyHeadlines::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Origin", "https://www.tradingview.com");
}

QByteArray SurveyHeadlines::joinParts(const QList<QByteArray> &parts) const
{
    // The compact form, plus a line per figure naming its publisher ("# ISO3 k publisher").
    QByteArray out = header();
    QVector<EconomicCalendar::Event> events;
    for (const QByteArray &part : parts)
        events += EconomicCalendar::parseEvents(QJsonDocument::fromJson(part).object().value("result").toArray());
    // The rule each event matches, and for each country and series the publisher whose rule comes first (the
    // Conference Board over Michigan for US consumer sentiment, the ISM over S&P Global for US PMIs).
    struct Match {
        const EconomicCalendar::Event *event;
        int rule;
    };
    QVector<Match> matches;
    QHash<QString, int> chosen;   // "ISO3/k" -> rule index
    for (const EconomicCalendar::Event &e : events) {
        if (!std::isfinite(e.actual) || skipped(e.title))
            continue;
        const QString iso3 = e.country == "EU" ? QString() : iso3For(e.country);
        if (iso3.isEmpty())
            continue;
        for (int r = 0; r < int(std::size(Rules)); ++r) {
            if (!e.title.contains(QLatin1String(Rules[r].pattern), Qt::CaseInsensitive))
                continue;
            matches.push_back({&e, r});
            const QString key = iso3 + '/' + QString::number(Rules[r].series);
            if (!chosen.contains(key) || r < chosen.value(key))
                chosen.insert(key, r);
            break;
        }
    }
    QHash<QString, QString> publisher;
    for (const Match &m : matches) {
        const EconomicCalendar::Event &e = *m.event;
        const Rule &rule = Rules[m.rule];
        const QString iso3 = iso3For(e.country);
        const QString key = iso3 + '/' + QString::number(rule.series);
        if (chosen.value(key) != m.rule)
            continue;
        // The month the figure covers: the one named in the period ("Sep", "Nov" for a forward-looking survey
        // released in October), else the release month. A named month far ahead of the release is last year's.
        QDate month = e.time.date();
        if (e.period.size() >= 3) {
            const QDate named = QLocale(QLocale::English).toDate(e.period.left(3), "MMM");
            if (named.isValid()) {
                const int ahead = named.month() - month.month();
                month = QDate(ahead > 2 ? month.year() - 1 : ahead < -9 ? month.year() + 1 : month.year(), named.month(), 1);
            }
        }
        publisher.insert(key, rule.publisher);
        addLine(out, iso3, rule.series, month.year() + (month.month() - 0.5) / 12.0, e.actual);
    }
    for (auto it = publisher.begin(); it != publisher.end(); ++it)
        out += "# " + it.key().toLatin1() + ' ' + it.value().toLatin1() + '\n';
    return out;
}

bool SurveyHeadlines::parse(const QByteArray &body)
{
    m_publisher.clear();
    for (const QByteArray &line : body.split('\n'))
        if (line.startsWith("# ")) {
            const QList<QByteArray> f = line.mid(2).split(' ');
            if (f.size() == 2)
                m_publisher.insert(QString::fromLatin1(f[0]), QString::fromLatin1(f[1]));
        }
    return CountrySeriesSet::parse(body);
}

QString SurveyHeadlines::publisherOf(const QString &iso3, int series) const
{
    return m_publisher.value(iso3 + '/' + QString::number(series));
}

int SurveyHeadlines::figureCount(const QString &publisherId) const
{
    int n = 0;
    for (const QString &p : m_publisher)
        n += p == publisherId;
    return n;
}

void SurveyHeadlines::setPublisherEnabled(const QString &publisherId, bool on)
{
    if (on)
        m_off.remove(publisherId);
    else
        m_off.insert(publisherId);
    emit updated();
}

QMap<double, double> SurveyHeadlines::enabledSeries(const QString &iso3, int series) const
{
    const QString publisher = publisherOf(iso3, series);
    if (publisher.isEmpty() || m_off.contains(publisher))
        return {};
    return this->series(iso3, series);
}
