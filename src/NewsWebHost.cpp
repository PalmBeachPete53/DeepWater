#include "NewsWebHost.h"

#include <QPointer>

namespace newsui {

namespace {
QPointer<WebHost> s_visible;
}

WebHost *WebHost::create(QWidget *parent)
{
#ifdef Q_OS_MACOS
    if (qEnvironmentVariable("DEEPWATER_WEB_ENGINE") != "qt")
        return createMacWebHost(parent);
#endif
    return createQtWebHost(parent);
}

WebHost *WebHost::visibleHost()
{
    return s_visible && s_visible->isVisible() ? s_visible.data() : nullptr;
}

void WebHost::showEvent(QShowEvent *)
{
    s_visible = this;
}

void WebHost::hideEvent(QHideEvent *)
{
    if (s_visible == this)
        s_visible = nullptr;
}

} // namespace newsui
