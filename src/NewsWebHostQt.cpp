#include "NewsWebHost.h"

#include "AdBlock.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineClientHints>
#include <QWebEngineHistory>
#include <QWebEngineLoadingInfo>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineView>

namespace newsui {

namespace {

// JavaScript that sets the reader's hiding stylesheet (one rule per selector, so one bad selector doesn't void
// the rest).
QString hideScript(const QStringList &selectors, const char *id)
{
    QString css;
    for (const QString &s : selectors)
        css += s + "{display:none !important;}\n";
    return QStringLiteral("(function(){var s=document.getElementById(%1);if(!s){s=document.createElement('style');s.id=%1;"
                          "(document.head||document.documentElement).appendChild(s);}s.textContent=%2;})();")
        .arg(QString::fromUtf8(QJsonDocument(QJsonArray {QString::fromLatin1(id)}).toJson(QJsonDocument::Compact)).mid(1).chopped(1),
             QString::fromUtf8(QJsonDocument(QJsonArray {css}).toJson(QJsonDocument::Compact)).mid(1).chopped(1));
}

const char *const CollectScript =
    "(function(){var c={},i={},n=0;var els=document.querySelectorAll('[class],[id]');"
    "for(var k=0;k<els.length&&n<40000;k++){var e=els[k];if(e.id){i[e.id]=1;n++;}"
    "if(e.classList)for(var j=0;j<e.classList.length;j++){c[e.classList[j]]=1;n++;}}"
    "return [Object.keys(c),Object.keys(i)];})();";

// Qt WebEngine: an off-the-record profile of its own, the ad blocker's interceptor on its requests, and the
// cosmetic rules of each page injected when the document is ready (the page's own) and once it has loaded
// (the generic ones, from the classes and ids it carries; again a moment later for what scripts add).
class QtWebHost : public WebHost
{
public:
    explicit QtWebHost(QWidget *parent)
        : WebHost(parent)
    {
        auto *l = new QVBoxLayout(this);
        l->setContentsMargins(0, 0, 0, 0);
        m_view = new QWebEngineView(this);
        m_profile = new QWebEngineProfile(this);
        // The engine's own user agent, without the "QtWebEngine/x" token, the app's locale, and client hints that
        // name this application next to Chromium, as every browser names itself.
        static const QRegularExpression qtToken("\\s*QtWebEngine/\\S+");
        m_profile->setHttpUserAgent(m_profile->httpUserAgent().remove(qtToken));
        m_profile->setHttpAcceptLanguage(QLocale().name().replace('_', '-') + ",en;q=0.8");
        static const QRegularExpression chromeVersion("Chrome/(\\d+(?:\\.\\d+)*)");
        if (const QString version = chromeVersion.match(m_profile->httpUserAgent()).captured(1); !version.isEmpty()) {
            QWebEngineClientHints *hints = m_profile->clientHints();
            hints->setFullVersionList({{"Chromium", version}, {"Not=A?Brand", "24.0.0.0"}, {"Deepwater", version.section('.', 0, 0) + ".0.0.0"}});
            hints->setFullVersion(version);
        }
        m_interceptor = new adblock::Interceptor(this);
        m_profile->setUrlRequestInterceptor(m_interceptor);
        connect(m_interceptor, &adblock::Interceptor::blocked, this, [this] {
            m_blocked++;
            emit blockerChanged();
        });
        m_view->setPage(new QWebEnginePage(m_profile, m_view));
        l->addWidget(m_view);

        connect(m_view, &QWebEngineView::loadStarted, this, &WebHost::loadStarted);
        connect(m_view, &QWebEngineView::loadProgress, this, &WebHost::loadProgress);
        connect(m_view, &QWebEngineView::loadFinished, this, [this](bool ok) {
            emit loadFinished(ok);
            applyCosmetics();
            const int generation = m_generation;
            QTimer::singleShot(2500, this, [this, generation] {
                if (generation == m_generation && isVisible()) applyCosmetics();
            });
        });
        connect(m_view, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
            installEarlyHide(url);
            emit urlChanged(url);
        });
        connect(m_view->page(), &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo &info) {
            if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus && info.errorDomain() == QWebEngineLoadingInfo::HttpStatusCodeDomain)
                emit httpError(info.url(), info.errorCode());
        });
        connect(adblock::Blocker::instance(), &adblock::Blocker::readyChanged, this, [this] {
            emit blockerChanged();
            if (isVisible()) applyCosmetics();
        });
        connect(adblock::Blocker::instance(), &adblock::Blocker::stateChanged, this, &WebHost::blockerChanged);
    }

    void load(const QUrl &url) override
    {
        installEarlyHide(url);
        m_view->setUrl(url);
    }
    void loadBlank() override { m_view->setUrl(QUrl("about:blank")); }
    void back() override { m_view->back(); }
    bool canGoBack() const override { return m_view->history()->canGoBack(); }
    QUrl url() const override { return m_view->url(); }
    void focusContent() override { m_view->setFocus(); }
    QString engineName() const override { return "Qt WebEngine"; }

    BlockerState blockerState() const override
    {
        adblock::Blocker *b = adblock::Blocker::instance();
        if (!adblock::Blocker::available() || !b->isEnabled()) return BlockerState::Off;
        return b->ready() ? BlockerState::Active : BlockerState::Preparing;
    }
    int blockedCount() const override { return m_blocked; }
    void newPage() override
    {
        m_blocked = 0;
        m_generation++;
    }
    void runScript(const QString &script, std::function<void(const QString &)> done) override
    {
        m_view->page()->runJavaScript(script, [done](const QVariant &result) { done(result.toString()); });
    }
    void snapshot(std::function<void(const QImage &)> done) override { done(m_view->grab().toImage()); }

private:
    void installEarlyHide(const QUrl &url)
    {
        if (!url.scheme().startsWith("http"))
            return;
        QWebEngineScriptCollection &scripts = m_view->page()->scripts();
        for (const QWebEngineScript &s : scripts.find("dw-adblock-early"))
            scripts.remove(s);
        adblock::Blocker *b = adblock::Blocker::instance();
        if (!b->ready() || !b->isEnabled())
            return;
        const adblock::Cosmetic cos = b->cosmeticFor(url);
        if (cos.hide.isEmpty())
            return;
        QWebEngineScript script;
        script.setName("dw-adblock-early");
        script.setInjectionPoint(QWebEngineScript::DocumentReady);
        script.setWorldId(QWebEngineScript::ApplicationWorld);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(hideScript(cos.hide, "dw-adblock-early-style"));
        scripts.insert(script);
    }

    void applyCosmetics()
    {
        adblock::Blocker *b = adblock::Blocker::instance();
        if (!b->ready() || !b->isEnabled())
            return;
        const QUrl url = m_view->url();
        if (!url.scheme().startsWith("http"))
            return;
        const adblock::Cosmetic cos = b->cosmeticFor(url);
        const int generation = m_generation;
        QWebEnginePage *page = m_view->page();
        page->runJavaScript(QString::fromLatin1(CollectScript), QWebEngineScript::ApplicationWorld,
                            [this, generation, cos, url, page](const QVariant &result) {
                                if (generation != m_generation || m_view->page() != page || m_view->url() != url)
                                    return;
                                const QVariantList lists = result.toList();
                                QStringList selectors = cos.hide;
                                if (!cos.generichide && lists.size() == 2)
                                    selectors += adblock::Blocker::instance()->hiddenSelectors(lists[0].toStringList(), lists[1].toStringList(),
                                                                                               cos.exceptions);
                                selectors.removeDuplicates();
                                if (!selectors.isEmpty())
                                    page->runJavaScript(hideScript(selectors, "dw-adblock-style"), QWebEngineScript::ApplicationWorld);
                            });
    }

    QWebEngineView *m_view;
    QWebEngineProfile *m_profile;
    adblock::Interceptor *m_interceptor;
    int m_blocked = 0;
    int m_generation = 0;
};

} // namespace

WebHost *createQtWebHost(QWidget *parent)
{
    return new QtWebHost(parent);
}

} // namespace newsui
