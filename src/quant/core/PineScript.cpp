// Pine Script via PineTS on Node.js: see PineScript.h. The protocol is documented at the top of
// third_party/pinets/deepwater-pinets.mjs.
#include "PineScript.h"

#include "Error.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutex>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QVersionNumber>

#include <algorithm>
#include <memory>

namespace qtm::pine {

namespace {

const char *const BUNDLE = "pinets.min.es.js";
const char *const BRIDGE = "deepwater-pinets.mjs";
const char *const PINETS_VERSION = "0.13.1";
const int MIN_NODE_MAJOR = 18;

QMutex g_mutex;   // the process-wide state below
QString g_runtimeDir;
QString g_nodePath;
bool g_nodeResolved = false;
bool g_installed = false;

bool settingsUsable()
{
    return QCoreApplication::instance() && !QCoreApplication::applicationName().isEmpty();
}

// Where Node.js may be, in order of preference. GUI apps don't get the shell's PATH on macOS,
// so the usual install locations are listed too.
QStringList nodeCandidates()
{
    QStringList out;
    const QString env = qEnvironmentVariable("DEEPWATER_NODE");
    if (!env.isEmpty()) out << env;
    if (settingsUsable()) {
        const QString saved = QSettings().value(QStringLiteral("pinets/node")).toString();
        if (!saved.isEmpty()) out << saved;
    }
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("node"));
    if (!onPath.isEmpty()) out << onPath;
    for (const char *p : { "/opt/homebrew/bin/node", "/usr/local/bin/node", "/opt/local/bin/node", "/usr/bin/node" })
        out << QString::fromLatin1(p);
    const QString home = QDir::homePath();
    const QDir nvm(home + QStringLiteral("/.nvm/versions/node"));
    QStringList versions = nvm.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(versions.begin(), versions.end(), [](const QString &a, const QString &b) {
        return QVersionNumber::fromString(a.mid(1)) > QVersionNumber::fromString(b.mid(1));
    });
    for (const QString &v : versions) out << nvm.filePath(v + QStringLiteral("/bin/node"));
    out << home + QStringLiteral("/.volta/bin/node")
        << home + QStringLiteral("/.local/share/fnm/aliases/default/bin/node")
        << home + QStringLiteral("/Library/Application Support/fnm/aliases/default/bin/node")
        << home + QStringLiteral("/.asdf/shims/node");
    return out;
}

QString resolveNode()
{
    QMutexLocker lock(&g_mutex);
    if (g_nodeResolved) return g_nodePath;
    for (const QString &c : nodeCandidates()) {
        const QFileInfo fi(c);
        if (fi.isFile() && fi.isExecutable()) {
            g_nodePath = fi.absoluteFilePath();
            break;
        }
    }
    g_nodeResolved = true;
    return g_nodePath;
}

QString noNodeMessage()
{
    return QStringLiteral("Node.js was not found (looked at the PATH, /opt/homebrew/bin, /usr/local/bin, nvm, volta and fnm). "
                          "Pine Script indicators run on PineTS, which needs Node.js %1 or newer: install it, or set the "
                          "`pinets/node` setting (or DEEPWATER_NODE) to the node binary.")
        .arg(MIN_NODE_MAJOR);
}

QString resolveRuntimeDir()
{
    QMutexLocker lock(&g_mutex);
    if (g_runtimeDir.isEmpty())
        g_runtimeDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/pinets");
    return g_runtimeDir;
}

// Copies a resource to the runtime folder unless the copy there is already identical.
bool installFile(const QString &dir, const char *name, QString &error)
{
    QFile res(QStringLiteral(":/pinets/") + QLatin1String(name));
    if (!res.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("The PineTS resource %1 is missing from this build").arg(QLatin1String(name));
        return false;
    }
    const QByteArray data = res.readAll();
    const QString target = dir + QLatin1Char('/') + QLatin1String(name);
    QFile have(target);
    if (have.open(QIODevice::ReadOnly) && have.size() == data.size() && have.readAll() == data) return true;
    QSaveFile out(target);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        error = QStringLiteral("Couldn't write %1: %2").arg(target, out.errorString());
        return false;
    }
    return true;
}

bool installRuntime(const QString &dir, QString &error)
{
    QMutexLocker lock(&g_mutex);
    if (g_installed) return true;
    if (!QDir().mkpath(dir)) {
        error = QStringLiteral("Couldn't create %1").arg(dir);
        return false;
    }
    if (!installFile(dir, BUNDLE, error) || !installFile(dir, BRIDGE, error)) return false;
    g_installed = true;
    return true;
}

// ---------------------------------------------------------------- the bridge process (one per thread)

struct Session {
    QProcess proc;
    int nextId = 1;
    bool ready = false;
    QString nodeVersion;
    QByteArray stderrTail;

    ~Session()
    {
        if (proc.state() == QProcess::NotRunning) return;
        proc.closeWriteChannel();   // the bridge exits when stdin closes
        if (!proc.waitForFinished(500)) {
            proc.kill();
            proc.waitForFinished(500);
        }
    }

    void drainStderr()
    {
        const QByteArray e = proc.readAllStandardError();
        if (e.isEmpty()) return;
        stderrTail += e;
        if (stderrTail.size() > 4000) stderrTail = stderrTail.right(4000);
    }

    QString stopped()
    {
        drainStderr();
        const QString tail = QString::fromUtf8(stderrTail).trimmed();
        return QStringLiteral("PineTS (Node.js) stopped%1").arg(tail.isEmpty() ? QString() : QStringLiteral(": ") + tail.right(600));
    }

    // Sends one request and waits for its reply.
    QJsonObject exchange(QJsonObject req, int timeoutMs)
    {
        const int id = nextId++;
        req.insert(QStringLiteral("id"), id);
        proc.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n');
        QElapsedTimer t;
        t.start();
        for (;;) {
            while (proc.canReadLine()) {
                const QByteArray line = proc.readLine();
                const QJsonObject r = QJsonDocument::fromJson(line).object();
                if (r.value(QStringLiteral("id")).toInt(-1) == id) return r;
            }
            drainStderr();
            if (proc.state() != QProcess::Running) throw Error(stopped());
            const qint64 left = timeoutMs - t.elapsed();
            if (left <= 0) {
                proc.kill();
                proc.waitForFinished(500);
                ready = false;
                throw Error(QStringLiteral("PineTS did not answer within %1 s; the script was stopped").arg(timeoutMs / 1000));
            }
            proc.waitForReadyRead(int(std::min<qint64>(left, 250)));
        }
    }
};

thread_local std::unique_ptr<Session> tlsSession;

Session &session()
{
    if (tlsSession && tlsSession->ready && tlsSession->proc.state() == QProcess::Running) return *tlsSession;
    tlsSession.reset();
    const QString node = resolveNode();
    if (node.isEmpty()) throw Error(noNodeMessage());
    const QString dir = resolveRuntimeDir();
    QString error;
    if (!installRuntime(dir, error)) throw Error(error);

    auto s = std::make_unique<Session>();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("NODE_NO_WARNINGS"), QStringLiteral("1"));
    s->proc.setProcessEnvironment(env);
    s->proc.setProgram(node);
    s->proc.setArguments({ dir + QLatin1Char('/') + QLatin1String(BRIDGE) });
    s->proc.setWorkingDirectory(dir);
    s->proc.start();
    if (!s->proc.waitForStarted(10000))
        throw Error(QStringLiteral("Couldn't start Node.js (%1): %2").arg(node, s->proc.errorString()));
    QJsonObject hello;
    try {
        hello = s->exchange(QJsonObject { { QStringLiteral("cmd"), QStringLiteral("hello") } }, 30000);
    } catch (const Error &e) {
        throw Error(QStringLiteral("PineTS didn't start with Node.js %1: %2").arg(node, e.message()));
    }
    if (!hello.value(QStringLiteral("ok")).toBool())
        throw Error(QStringLiteral("PineTS didn't start: %1").arg(hello.value(QStringLiteral("error")).toString()));
    s->nodeVersion = hello.value(QStringLiteral("node")).toString();
    const QVersionNumber v = QVersionNumber::fromString(s->nodeVersion.mid(s->nodeVersion.startsWith(QLatin1Char('v')) ? 1 : 0));
    if (v.majorVersion() < MIN_NODE_MAJOR)
        throw Error(QStringLiteral("Node.js %1 at %2 is too old for PineTS: %3 or newer is needed").arg(s->nodeVersion, node).arg(MIN_NODE_MAJOR));
    s->ready = true;
    tlsSession = std::move(s);

    // The main thread's process goes with the application, before the thread_local would.
    static bool postRoutine = false;
    if (!postRoutine && QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
        postRoutine = true;
        qAddPostRoutine([] { tlsSession.reset(); });
    }
    return *tlsSession;
}

QJsonObject request(const QJsonObject &req, int timeoutMs)
{
    const QJsonObject r = session().exchange(req, timeoutMs);
    if (!r.value(QStringLiteral("ok")).toBool()) throw Error(r.value(QStringLiteral("error")).toString());
    return r;
}

QJsonValue jsonOrUndefined(const QJsonObject &o, const QString &key)
{
    return o.contains(key) ? o.value(key) : QJsonValue(QJsonValue::Undefined);
}

Input inputFrom(const QJsonObject &o)
{
    Input in;
    in.id = o.value(QStringLiteral("id")).toString();
    in.varId = o.value(QStringLiteral("varId")).toString();
    in.title = o.value(QStringLiteral("title")).toString(o.value(QStringLiteral("name")).toString());
    in.type = o.value(QStringLiteral("type")).toString();
    in.defval = o.value(QStringLiteral("defval"));
    in.options = o.value(QStringLiteral("options")).toArray();
    in.minval = jsonOrUndefined(o, QStringLiteral("minval"));
    in.maxval = jsonOrUndefined(o, QStringLiteral("maxval"));
    in.step = jsonOrUndefined(o, QStringLiteral("step"));
    in.group = o.value(QStringLiteral("group")).toString();
    in.tooltip = o.value(QStringLiteral("tooltip")).toString();
    return in;
}

QStringList stringsOf(const QJsonArray &a)
{
    QStringList out;
    out.reserve(a.size());
    for (const QJsonValue &v : a) out << (v.isString() ? v.toString() : QString());
    return out;
}

Plot plotFrom(const QJsonObject &o, int bars)
{
    Plot p;
    auto str = [&](const char *k) {
        const QJsonValue v = o.value(QLatin1String(k));
        return v.isString() ? v.toString() : QString();
    };
    p.key = str("key");
    p.title = str("title");
    p.style = str("style");
    p.color = str("color");
    p.linewidth = o.value(QStringLiteral("linewidth")).toInt();
    p.overlay = o.value(QStringLiteral("overlay")).toBool(true);
    p.location = str("location");
    p.shape = str("shape");
    p.ch = str("char");
    p.text = str("text");
    const QJsonArray values = o.value(QStringLiteral("values")).toArray();
    if (p.isMarks()) {
        p.flags.resize(bars);
        for (int i = 0; i < bars; ++i) p.flags[i] = i < values.size() && values[i].toBool();
    } else {
        p.values.resize(bars);
        for (int i = 0; i < bars; ++i) {
            const QJsonValue v = i < values.size() ? values[i] : QJsonValue();
            p.values[i] = v.isDouble() ? v.toDouble() : NaN;
        }
    }
    p.colors = stringsOf(o.value(QStringLiteral("colors")).toArray());
    p.texts = stringsOf(o.value(QStringLiteral("texts")).toArray());
    return p;
}

} // namespace

bool available(QString *reason)
{
    try {
        session();
        return true;
    } catch (const Error &e) {
        if (reason) *reason = e.message();
        return false;
    }
}

QString nodePath()
{
    return resolveNode();
}

void setNodePath(const QString &path)
{
    if (settingsUsable()) {
        if (path.isEmpty()) QSettings().remove(QStringLiteral("pinets/node"));
        else QSettings().setValue(QStringLiteral("pinets/node"), path);
    }
    QMutexLocker lock(&g_mutex);
    g_nodeResolved = false;
    g_nodePath.clear();
}

QString nodeVersion()
{
    return tlsSession ? tlsSession->nodeVersion : QString();
}

QString pinetsVersion()
{
    return QLatin1String(PINETS_VERSION);
}

QString runtimeDir()
{
    return resolveRuntimeDir();
}

void setRuntimeDir(const QString &dir)
{
    QMutexLocker lock(&g_mutex);
    g_runtimeDir = dir;
    g_installed = false;
}

QString transpile(const QString &source)
{
    const QJsonObject r = request(QJsonObject { { QStringLiteral("cmd"), QStringLiteral("transpile") }, { QStringLiteral("source"), source } }, 60000);
    return r.value(QStringLiteral("code")).toString();
}

Result run(const QString &source, const Bars &bars, const QJsonObject &inputs, const Options &options)
{
    QJsonArray rows;
    for (const Bar &b : bars)
        rows.append(QJsonArray { b.time, b.open, b.high, b.low, b.close, b.volume });
    QJsonObject req {
        { QStringLiteral("cmd"), QStringLiteral("run") },
        { QStringLiteral("source"), source },
        { QStringLiteral("bars"), rows },
        { QStringLiteral("inputs"), inputs },
    };
    if (!options.tickerId.isEmpty()) req.insert(QStringLiteral("tickerId"), options.tickerId);
    if (!options.timeframe.isEmpty()) req.insert(QStringLiteral("timeframe"), options.timeframe);
    if (!options.timezone.isEmpty()) req.insert(QStringLiteral("timezone"), options.timezone);
    const QJsonObject r = request(req, options.timeoutMs);

    Result out;
    out.declaration.type = r.value(QStringLiteral("declaration")).toString();
    out.declaration.version = r.value(QStringLiteral("version")).toInt();
    const QJsonObject decl = r.value(QStringLiteral("indicator")).toObject();
    out.declaration.title = decl.value(QStringLiteral("title")).toString();
    out.declaration.shorttitle = decl.value(QStringLiteral("shorttitle")).toString();
    out.declaration.overlay = decl.value(QStringLiteral("overlay")).toBool();
    out.declaration.props = decl;
    for (const QJsonValue &v : r.value(QStringLiteral("inputs")).toArray()) out.inputs << inputFrom(v.toObject());
    for (const QJsonValue &v : r.value(QStringLiteral("plots")).toArray()) out.plots << plotFrom(v.toObject(), bars.size());
    out.warnings = stringsOf(r.value(QStringLiteral("warnings")).toArray());
    return out;
}

} // namespace qtm::pine
