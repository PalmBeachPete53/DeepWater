// The JavaScript run on every custom indicator engine before a file is imported (CustomIndicators.cpp).
// The scripts are core/js/qtm-polyfills.js and core/js/qtm-runtime.js, compiled in as Qt resources (a single
// string literal over 16 KB doesn't build with MSVC).
#include "CustomIndicatorsJs.h"

#include <QFile>

namespace qtm::detail {

static QString readScript(const char *path)
{
    QFile f(QString::fromLatin1(path));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("%s is missing from the resources: custom indicators will fail", path);
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

const QString &jsPolyfills()
{
    static const QString script = readScript(":/qtm/qtm-polyfills.js");
    return script;
}

const QString &jsRuntime()
{
    static const QString script = readScript(":/qtm/qtm-runtime.js");
    return script;
}

} // namespace qtm::detail
