// qtm_server_run: the C++ backend on its own, like `node server.js`, for the live comparison with
// Node and for trying the examples in quant-terminal/examples. Data goes to a temporary folder
// unless --data is given; the custom folder is seeded with the bundled example indicator.
//   qtm_server_run [--port 3777] [--data DIR]

#include "../../../src/quant/server/Server.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    int port = 3777;
    QString data;
    for (int i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == "--port") port = args[i + 1].toInt();
        if (args[i] == "--data") data = args[i + 1];
    }
    QTemporaryDir tmp;
    if (data.isEmpty()) data = tmp.path();
    const QString custom = QDir(data).filePath("custom");
    QDir().mkpath(custom);
    QFile::copy(QStringLiteral(QTM_QT_ROOT "/custom/ema-ribbon-squeeze.js"), QDir(custom).filePath("ema-ribbon-squeeze.js"));
    qtm::Backend backend(QDir(data).filePath("data"), custom);
    backend.loadCustomIndicators();
    if (!backend.startHttp(quint16(port), 20)) return 1;
    backend.startAlerts();
    std::printf("listening on %d\n", backend.httpPort());
    std::fflush(stdout);
    return app.exec();
}
