#pragma once

#include <QImage>
#include <QUrl>
#include <QWidget>

#include <functional>

namespace newsui {

// What the article reader needs from a web view, whichever engine renders it. On macOS it is the system's WebKit
// (WKWebView, the engine of Safari): some sites' bot protections only accept the network fingerprint of a real
// browser engine, and nasdaq.com's Akamai refuses Qt WebEngine while it lets Safari through. Elsewhere it is
// Qt WebEngine. Both carry the ad blocker (AdBlock.h): WebKit through content rules compiled from Brave's lists
// (network blocking and element hiding alike), WebEngine through a request interceptor and injected hiding rules.
class WebHost : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
    // The engine for this platform (DEEPWATER_WEB_ENGINE=qt forces Qt WebEngine, for comparisons).
    static WebHost *create(QWidget *parent);
    // The host currently shown, if any: the --screenshot flag composites its snapshot (native views don't paint
    // through QWidget::grab).
    static WebHost *visibleHost();

    virtual void load(const QUrl &url) = 0;
    virtual void loadBlank() = 0;   // stops whatever plays
    virtual void back() = 0;
    virtual bool canGoBack() const = 0;
    virtual QUrl url() const = 0;
    virtual void focusContent() = 0;
    virtual QString engineName() const = 0;   // "WebKit", "Qt WebEngine"

    // The ad blocker as seen from this engine, and the requests blocked on the page since newPage() (-1 when
    // the engine doesn't tell: WebKit applies its content rules without reporting).
    enum class BlockerState { Off, Preparing, Active };
    virtual BlockerState blockerState() const = 0;
    virtual int blockedCount() const = 0;
    virtual void newPage() = 0;

    virtual void snapshot(std::function<void(const QImage &)> done) = 0;

signals:
    void loadStarted();
    void loadProgress(int percent);
    void loadFinished(bool ok);
    void urlChanged(const QUrl &url);
    void httpError(const QUrl &url, int status);   // the main document answered 4xx / 5xx
    void blockerChanged();

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
};

WebHost *createQtWebHost(QWidget *parent);
#ifdef Q_OS_MACOS
WebHost *createMacWebHost(QWidget *parent);
#endif

} // namespace newsui
