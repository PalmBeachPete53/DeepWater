#pragma once

#include <QColor>

#include <QAbstractButton>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>
#include <QVector>
#include <QWidget>

#include <functional>

class AisStream;
class BalanceOfPayments;
class CotData;
class CotReport;
class EconomySectors;
class BankCredit;
class BlsIndicators;
class BrazilIndicators;
class ArgentinaIndicators;
class SouthAfricaIndicators;
class SingaporeIndicators;
class FredIndicators;
class EiaIndicators;
class BeaIndicators;
class BanxicoIndicators;
class EcosIndicators;
class EvdsIndicators;
class SurveyHeadlines;
class ComtradeProducts;
class CommodityBenchmarks;
class EurostatIndicators;
class ImfConsumerPrices;
class OecdIndicators;
class CorporateCalendar;
class EconomicCalendar;
class MarketSessions;
class FuturesQuotes;
class GlobalDebt;
class HousePriceRatios;
class PropertyCredit;
class FxData;
class PolicyRates;
class PortWatch;
namespace supply { class SupplyChain; }
class TradeFlows;
class VesselStore;
class WorldEconomy;
class WorldPolicyRates;
class QButtonGroup;
class QComboBox;
class QLabel;
class QScrollArea;
class QStackedWidget;
class SourceFlowView;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
namespace qtm { class Backend; }

// Sources page: a downloaded data set of the map or forex pages, seen through the methods they share.
struct SourceDataset {
    std::function<QDateTime()> fetchedAt;
    std::function<QDateTime()> changedAt;   // the last time it had something new; empty: not told apart from fetchedAt
    std::function<qint64()> checkEvery;     // its period when that depends on the moment; empty: everySecs
    bool onDemand = false;                  // read when a page needs it, not on a period
    std::function<QString()> error;
    std::function<bool()> fetching;
    std::function<void()> refresh;
    qint64 everySecs = 0;
    QUrl probeUrl;
    // A source whose request carries the user's key: the probe, rebuilt at each test (probeUrl was built with the
    // key of the moment the page opened, empty before the key was first saved: FRED then answered HTTP 400).
    std::function<QUrl()> probe;
    QString keyId;      // a source that needs a user's key: the settings id of the key ("keys/<keyId>")
    QString keyHint;    // where to get one
};
struct SourceTest {
    bool running = false;
    bool done = false;
    bool ok = false;
    double ms = 0;
    QString error;
    QDateTime at;
};

// On / off switch of a source.
class SourceSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit SourceSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(36, 20); }

protected:
    void paintEvent(QPaintEvent *) override;
};

// The Sources page: every data source the app downloads from, what it feeds, its state, and its
// controls. Turning a source off stops its requests: the map and forex data sets keep their last
// copy saved on disk, and the Charts page moves on to the next provider in its fallback chain. The
// choice is kept in the settings ("sources/disabled") and applied at startup (applySaved).
// API keys (aisstream, Alpaca, Polygon) are entered here.
class SourcesPage : public QWidget
{
    Q_OBJECT
public:
    struct Targets {
        AisStream *ais = nullptr;
        VesselStore *vessels = nullptr;
        PortWatch *portWatch = nullptr;
        supply::SupplyChain *supply = nullptr;
        FxData *fx = nullptr;
        PolicyRates *policyRates = nullptr;
        CotData *cot = nullptr;
        CotReport *cotReport = nullptr;
        WorldEconomy *economy = nullptr;
        WorldPolicyRates *worldRates = nullptr;
        BalanceOfPayments *bop = nullptr;
        TradeFlows *trade = nullptr;
        EconomySectors *sectors = nullptr;
        PropertyCredit *propertyCredit = nullptr;
        HousePriceRatios *housePrices = nullptr;
        GlobalDebt *globalDebt = nullptr;
        BankCredit *bankCredit = nullptr;
        OecdIndicators *oecd = nullptr;
        EurostatIndicators *eurostat = nullptr;
        ImfConsumerPrices *imfPrices = nullptr;
        BlsIndicators *bls = nullptr;
        BrazilIndicators *brazil = nullptr;
        ArgentinaIndicators *argentina = nullptr;
        SouthAfricaIndicators *southAfrica = nullptr;
        SingaporeIndicators *singapore = nullptr;
        FredIndicators *fred = nullptr;
        EiaIndicators *eia = nullptr;
        BeaIndicators *bea = nullptr;
        BanxicoIndicators *banxico = nullptr;
        EcosIndicators *ecos = nullptr;
        EvdsIndicators *evds = nullptr;
        SurveyHeadlines *surveys = nullptr;
        ComtradeProducts *comtrade = nullptr;
        FuturesQuotes *futures = nullptr;
        CommodityBenchmarks *benchmarks = nullptr;
        EconomicCalendar *economicCalendar = nullptr;
        CorporateCalendar *corporateCalendar = nullptr;
        MarketSessions *marketSessions = nullptr;
        qtm::Backend *backend = nullptr;
        bool aisKeyFromEnv = false;   // AISSTREAM_API_KEY is set: the key can't be edited here
    };

    explicit SourcesPage(const Targets &targets, QWidget *parent = nullptr);

    static bool isEnabled(const QString &id);
    // Dev flag --search: fills the search box (screenshots).
    void setSearch(const QString &text);
    // The sub-tabs: 0 the list of sources, 1 the diagram of the data (Data). Kept in "sources/tab".
    void showTab(int index);
    void showDisc(const QString &pin);   // dev flag --sources-disc: the diagram's disc view
    // Turns off every source the settings say is off (call once the targets exist).
    static void applySaved(const Targets &targets);

signals:
    void aisKeyChanged(const QString &key);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    enum class Kind { Dataset, Stream, Market, Key, Local, Catalogue, Publisher };
    using Dataset = SourceDataset;
    using Test = SourceTest;
    struct Row {
        QString id;
        Kind kind;
        Dataset dataset;
        QLabel *dot = nullptr;
        QLabel *status = nullptr;
        QLabel *detail = nullptr;
        QPushButton *test = nullptr;
        QPushButton *refresh = nullptr;
        SourceSwitch *toggle = nullptr;
        QWidget *keyBox = nullptr;
        QLineEdit *key = nullptr;
        QLineEdit *secret = nullptr;
        QComboBox *feed = nullptr;
        QPushButton *save = nullptr;
        Test test_;
        enum State { Neutral, Good, Bad, Off } state = Neutral;   // for the summary line
        QColor color;   // of the status, as last applied: a stylesheet is re-polished even when unchanged
        QWidget *card = nullptr;
        QString publisher;   // Kind::Publisher: the survey publisher's id
        QString search;      // name, provider, host and feeds, lower case: what the search box matches
        QString name, provider, section, host;   // as shown, for the Data diagram
        int catalogue = -1;  // index in OfficialSources for a catalogue source
    };
    // A section of the page: its heading and the ids of its rows (hidden together when a search leaves none).
    struct Section {
        QLabel *label = nullptr;
        QStringList rowIds;
        QString title;
    };
    // A section of the catalogue (one kind of body): collapsed until opened, its cards built on demand for the
    // sources the filters keep.
    struct CatalogueSection {
        int kind = 0;
        QPushButton *header = nullptr;
        QWidget *body = nullptr;
        QVBoxLayout *layout = nullptr;
        QPushButton *more = nullptr;
        bool expanded = false;
        QVector<int> matches;   // sources the filters keep, in catalogue order
        int shown = 0;          // how many of them have a card laid out
    };
    struct Info {
        QString id;
        Kind kind;
        QString name, provider, host, feeds;
    };

    QWidget *buildRow(const Info &info);
    QWidget *buildRow(const Info &info, const Dataset &dataset);
    QWidget *buildCatalogueRow(int source);
    void buildCatalogueSections(QVBoxLayout *l);
    void applyFilter();
    void fillCatalogueSection(CatalogueSection &section, int limit);
    void setEnabled(const QString &id, bool on);
    void runTest(Row &row);
    void saveKeys(Row &row);
    void loadKeys();
    void refresh();
    void refreshHealth();
    void updateRow(Row &row);
    void updateRow(Row &row, const QStringList &disabled);   // with the disabled sources, read once per refresh
    void updateSummary();
    void updateFlow();
    // "Show in the list" from the diagram: the list keeps only these rows until the chip is closed.
    void showOnly(const QString &title, const QStringList &ids);
    // "Show in the list" on a group of the catalogue: the List with that section open, every access.
    void showCatalogueKind(int kind);
    QString age(const QDateTime &t) const;
    QString every(qint64 secs) const;

    Targets m_t;
    QVector<Row> m_rows;
    QHash<QString, int> m_index;
    QVector<Section> m_sections;
    QVector<CatalogueSection> m_catalogue;
    QLineEdit *m_search = nullptr;
    QComboBox *m_country = nullptr;
    QComboBox *m_access = nullptr;
    QJsonObject m_health;          // Charts backend /api/health "providers"
    bool m_healthLoaded = false;
    int m_imports = -1;            // CSV files imported in the Charts page
    QJsonObject m_feedConfig;      // feeds.json
    QLabel *m_summary = nullptr;
    QButtonGroup *m_tabs = nullptr;
    QStackedWidget *m_stack = nullptr;
    QScrollArea *m_listScroll = nullptr;
    SourceFlowView *m_flow = nullptr;
    QSet<QString> m_only;          // the rows the diagram sent to the list; empty: every row
    QPushButton *m_onlyChip = nullptr;
    QVector<QWidget *> m_catalogueIntro;   // the catalogue's heading and hint, hidden with m_only
    QNetworkAccessManager m_net;
    QTimer m_tick;                 // the shown states, once a second while the page is visible
    int m_ticks = 0;
};
