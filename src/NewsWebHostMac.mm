// The reader's web view on macOS: the system's WebKit (WKWebView, Safari's engine) hosted in a native child
// widget. The ad blocker is WebKit's own content blocking: Brave's lists converted by adblock-rust to WebKit's
// rule format (AdBlock.h), compiled by WKContentRuleListStore (cached by the store between launches, under an
// identifier that carries the lists' hash) and attached to the page's user content controller. WebKit applies
// them without reporting, so the shield shows "filters active" rather than a count.

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include "NewsWebHost.h"

#include "AdBlock.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPointer>
#include <QResizeEvent>
#include <QtDebug>

namespace newsui { class MacWebHost; }

@interface DWWebDelegate : NSObject <WKNavigationDelegate, WKUIDelegate>
@property (nonatomic, assign) newsui::MacWebHost *host;
@end

namespace newsui {

namespace {

constexpr int MaxRulesPerList = 150000;   // WebKit's limit per content rule list
constexpr int ChunkRules = 60000;          // blocking rules per list, under the limit with the exceptions repeated

QString fromNS(NSString *s) { return s ? QString::fromNSString(s) : QString(); }

} // namespace

class MacWebHost : public WebHost
{
public:
    explicit MacWebHost(QWidget *parent)
        : WebHost(parent)
    {
        // A native view of its own for the WKWebView, and only this one: without WA_DontCreateNativeAncestors Qt
        // would make the reader's panel and overlay native too, and they would stop compositing with the page.
        setAttribute(Qt::WA_DontCreateNativeAncestors);
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);
        connect(adblock::Blocker::instance(), &adblock::Blocker::contentRulesChanged, this, [this] { installRules(); });
        connect(adblock::Blocker::instance(), &adblock::Blocker::stateChanged, this, [this] {
            const bool on = adblock::Blocker::available() && adblock::Blocker::instance()->isEnabled();
            if (!on && m_rules != Rules::None) {
                if (m_controller) [m_controller removeAllContentRuleLists];
                m_rules = Rules::None;
                emit blockerChanged();
            } else if (on && m_rules == Rules::None) {
                installRules();
            }
        });
    }

    ~MacWebHost() override
    {
        if (m_web) {
            [m_web removeObserver:m_delegate forKeyPath:@"estimatedProgress"];
            [m_web removeObserver:m_delegate forKeyPath:@"URL"];
            [m_web removeObserver:m_delegate forKeyPath:@"canGoBack"];
            [m_web removeFromSuperview];
            m_web.navigationDelegate = nil;
            m_web.UIDelegate = nil;
        }
    }

    void load(const QUrl &url) override
    {
        ensureView();
        m_loadedWhileCompiling = m_rules == Rules::Compiling;
        [m_web loadRequest:[NSURLRequest requestWithURL:url.toNSURL()]];
    }
    void loadBlank() override
    {
        if (m_web) [m_web loadHTMLString:@"" baseURL:nil];
    }
    void back() override
    {
        if (m_web) [m_web goBack];
    }
    bool canGoBack() const override { return m_web && m_web.canGoBack; }
    QUrl url() const override { return m_web && m_web.URL ? QUrl::fromNSURL(m_web.URL) : QUrl(); }
    void focusContent() override
    {
        if (m_web) [m_web.window makeFirstResponder:m_web];
    }
    QString engineName() const override { return "WebKit"; }

    BlockerState blockerState() const override
    {
        if (!adblock::Blocker::available() || !adblock::Blocker::instance()->isEnabled()) return BlockerState::Off;
        return m_rules == Rules::Ready ? BlockerState::Active : BlockerState::Preparing;
    }
    int blockedCount() const override { return -1; }
    void newPage() override {}

    void runScript(const QString &script, std::function<void(const QString &)> done) override
    {
        [m_web evaluateJavaScript:script.toNSString() completionHandler:^(id result, NSError *) {
            done([result isKindOfClass:[NSString class]] ? QString::fromNSString((NSString *)result) : QString());
        }];
    }

    void snapshot(std::function<void(const QImage &)> done) override
    {
        if (!m_web) {
            done(QImage());
            return;
        }
        WKSnapshotConfiguration *config = [WKSnapshotConfiguration new];
        [m_web takeSnapshotWithConfiguration:config completionHandler:^(NSImage *image, NSError *) {
            QImage out;
            if (image) {
                // Through PNG: Qt 6 has no CGImage → QImage conversion.
                NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithData:image.TIFFRepresentation];
                NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
                if (png) out = QImage::fromData(QByteArray::fromNSData(png), "png");
            }
            done(out);
        }];
    }

    // Called by the delegate.
    void onStarted()
    {
        emit loadStarted();
        emit loadProgress(0);
    }
    void onProgress(double fraction) { emit loadProgress(int(fraction * 100)); }
    void onFinished(bool ok) { emit loadFinished(ok); }
    void onUrlChanged() { emit urlChanged(url()); }
    void onMainResponse(NSURL *nsurl, int status)
    {
        const QUrl u = QUrl::fromNSURL(nsurl);
        qInfo().noquote() << QStringLiteral("webkit: %1 answered HTTP %2").arg(u.host()).arg(status);
        if (status >= 400) emit httpError(u, status);
    }

protected:
    void resizeEvent(QResizeEvent *e) override
    {
        WebHost::resizeEvent(e);
        if (m_web) m_web.frame = NSMakeRect(0, 0, width(), height());
    }

private:
    enum class Rules { None, Compiling, Ready };

    void ensureView()
    {
        if (m_web)
            return;
        WKWebViewConfiguration *config = [WKWebViewConfiguration new];
        config.applicationNameForUserAgent = @"Deepwater/0.1";
        config.websiteDataStore = [WKWebsiteDataStore defaultDataStore];
        config.preferences.javaScriptCanOpenWindowsAutomatically = NO;
        m_controller = config.userContentController;
        m_web = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, width(), height()) configuration:config];
        m_web.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        m_web.allowsBackForwardNavigationGestures = YES;
        m_delegate = [DWWebDelegate new];
        m_delegate.host = this;
        m_web.navigationDelegate = m_delegate;
        m_web.UIDelegate = m_delegate;
        [m_web addObserver:m_delegate forKeyPath:@"estimatedProgress" options:NSKeyValueObservingOptionNew context:nullptr];
        [m_web addObserver:m_delegate forKeyPath:@"URL" options:NSKeyValueObservingOptionNew context:nullptr];
        [m_web addObserver:m_delegate forKeyPath:@"canGoBack" options:NSKeyValueObservingOptionNew context:nullptr];
        NSView *host = (__bridge NSView *)reinterpret_cast<void *>(winId());
        [host addSubview:m_web];
        installRules();
    }

    // Compiles (or finds in the store) the content rule lists of the current rules and attaches them.
    void installRules()
    {
        adblock::Blocker *b = adblock::Blocker::instance();
        if (!m_controller || !adblock::Blocker::available() || !b->isEnabled())
            return;
        const QByteArray json = b->contentRules();
        const QString id = b->contentRulesId();
        if (json.isEmpty() || id.isEmpty() || (id == m_rulesId && m_rules != Rules::None))
            return;
        m_rulesId = id;
        m_rules = Rules::Compiling;
        m_rulesTimer.start();
        emit blockerChanged();
        [m_controller removeAllContentRuleLists];

        // Blocking rules in chunks, each followed by every exception ("ignore-previous-rules" only reaches back
        // inside its own list).
        const QJsonArray all = QJsonDocument::fromJson(json).array();
        QJsonArray blocking, exceptions;
        for (const QJsonValue &v : all) {
            if (v.toObject().value("action").toObject().value("type").toString() == "ignore-previous-rules")
                exceptions.append(v);
            else
                blocking.append(v);
        }
        // Central banks carry no ads, and a generic hiding rule catches the whole of federalreserve.gov's page, leaving
        // it white: the lists don't apply to the sites the Desk's notes open.
        exceptions.append(QJsonObject {{"trigger", QJsonObject {{"url-filter", ".*"},
                                                                {"if-domain", QJsonArray {"*federalreserve.gov", "*ecb.europa.eu", "*bankofengland.co.uk", "*bis.org"}}}},
                                       {"action", QJsonObject {{"type", "ignore-previous-rules"}}}});
        const int perChunk = std::max(1000, std::min(ChunkRules, MaxRulesPerList - int(exceptions.size())));
        QVector<QByteArray> chunks;
        for (int start = 0; start < blocking.size(); start += perChunk) {
            QJsonArray chunk;
            for (int i = start; i < blocking.size() && i < start + perChunk; ++i) chunk.append(blocking[i]);
            for (const QJsonValue &v : exceptions) chunk.append(v);
            chunks.push_back(QJsonDocument(chunk).toJson(QJsonDocument::Compact));
        }
        qInfo().noquote() << QStringLiteral("webkit: %1 content rules (%2 exceptions) in %3 list(s), id %4")
                                 .arg(all.size()).arg(exceptions.size()).arg(chunks.size()).arg(id);
        m_pending = chunks.size();
        if (m_pending == 0) {
            m_rules = Rules::Ready;
            emit blockerChanged();
            return;
        }
        WKContentRuleListStore *store = [WKContentRuleListStore defaultStore];
        const QString prefix = "deepwater-v2-" + id + "-";   // v2: with the central banks' exception
        QPointer<MacWebHost> self(this);
        for (int i = 0; i < chunks.size(); ++i) {
            NSString *identifier = (prefix + QString::number(i)).toNSString();
            NSString *source = [[NSString alloc] initWithData:chunks[i].toNSData() encoding:NSUTF8StringEncoding];
            auto attach = ^(WKContentRuleList *list, NSError *error) {
                if (!self || self->m_rulesId != id) return;
                if (list) {
                    [self->m_controller addContentRuleList:list];
                } else {
                    qWarning().noquote() << "webkit: content rules failed:" << fromNS(error.localizedDescription);
                }
                if (--self->m_pending == 0) {
                    self->m_rules = Rules::Ready;
                    qInfo().noquote() << QStringLiteral("webkit: content rules ready after %1 ms").arg(self->m_rulesTimer.elapsed());
                    emit self->blockerChanged();
                    if (self->m_loadedWhileCompiling && self->m_web) {
                        self->m_loadedWhileCompiling = false;
                        [self->m_web reload];
                    }
                }
            };
            [store lookUpContentRuleListForIdentifier:identifier completionHandler:^(WKContentRuleList *found, NSError *) {
                if (found) {
                    attach(found, nil);
                } else {
                    [store compileContentRuleListForIdentifier:identifier encodedContentRuleList:source completionHandler:attach];
                }
            }];
        }
        // Lists of earlier rule sets are dropped from the store.
        [store getAvailableContentRuleListIdentifiers:^(NSArray<NSString *> *identifiers) {
            for (NSString *ident in identifiers) {
                const QString s = fromNS(ident);
                if (s.startsWith("deepwater-") && !s.startsWith(prefix))
                    [store removeContentRuleListForIdentifier:ident completionHandler:^(NSError *) {}];
            }
        }];
    }

    WKWebView *m_web = nil;
    WKUserContentController *m_controller = nil;
    DWWebDelegate *m_delegate = nil;
    Rules m_rules = Rules::None;
    QString m_rulesId;
    QElapsedTimer m_rulesTimer;
    int m_pending = 0;
    bool m_loadedWhileCompiling = false;
};

WebHost *createMacWebHost(QWidget *parent)
{
    return new MacWebHost(parent);
}

} // namespace newsui

@implementation DWWebDelegate

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    if (self.host) self.host->onStarted();
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    if (self.host) self.host->onFinished(true);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (self.host) self.host->onFinished(false);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (self.host) self.host->onFinished(false);
}

- (void)webView:(WKWebView *)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse
                      decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler
{
    if (self.host && navigationResponse.forMainFrame && [navigationResponse.response isKindOfClass:[NSHTTPURLResponse class]])
        self.host->onMainResponse(navigationResponse.response.URL, int(((NSHTTPURLResponse *)navigationResponse.response).statusCode));
    decisionHandler(WKNavigationResponsePolicyAllow);
}

// Links that want a new window open in the same view.
- (WKWebView *)webView:(WKWebView *)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration
               forNavigationAction:(WKNavigationAction *)navigationAction
                    windowFeatures:(WKWindowFeatures *)windowFeatures
{
    if (!navigationAction.targetFrame.isMainFrame)
        [webView loadRequest:navigationAction.request];
    return nil;
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (!self.host) return;
    WKWebView *web = (WKWebView *)object;
    if ([keyPath isEqualToString:@"estimatedProgress"])
        self.host->onProgress(web.estimatedProgress);
    else if ([keyPath isEqualToString:@"URL"] || [keyPath isEqualToString:@"canGoBack"])
        self.host->onUrlChanged();
}

@end
