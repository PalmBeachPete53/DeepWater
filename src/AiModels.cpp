#include "AiModels.h"

#include "KeyedSources.h"

#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QPointer>
#include <QtConcurrent>

// ---------------------------------------------------------------- HuggingFaceHub

HuggingFaceHub::HuggingFaceHub(QObject *parent)
    : QObject(parent)
{
}

QString HuggingFaceHub::modelsRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
}

QNetworkRequest HuggingFaceHub::request(const QUrl &url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    const QString token = sourceKey(tokenId());
    if (!token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    return req;
}

void HuggingFaceHub::search(const QString &query)
{
    QUrl url("https://huggingface.co/api/models");
    QUrlQuery q;
    q.addQueryItem("search", query);
    q.addQueryItem("limit", "30");
    q.addQueryItem("sort", "downloads");
    q.addQueryItem("direction", "-1");
    url.setQuery(q);
    QNetworkRequest req = request(url);
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query] {
        reply->deleteLater();
        QVector<Model> models;
        if (reply->error() != QNetworkReply::NoError) {
            emit searchDone(query, models, reply->errorString());
            return;
        }
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).array()) {
            const QJsonObject o = v.toObject();
            Model m;
            m.id = o.value("id").toString(o.value("modelId").toString());
            m.task = o.value("pipeline_tag").toString();
            m.library = o.value("library_name").toString();
            m.downloads = qint64(o.value("downloads").toDouble());
            m.likes = o.value("likes").toInt();
            m.gated = o.value("gated").toBool() || o.value("gated").isString();
            m.modified = QDateTime::fromString(o.value("lastModified").toString(o.value("createdAt").toString()), Qt::ISODateWithMs);
            if (!m.id.isEmpty())
                models.push_back(m);
        }
        emit searchDone(query, models, QString());
    });
}

void HuggingFaceHub::listFiles(const QString &modelId)
{
    QNetworkRequest req = request(QUrl("https://huggingface.co/api/models/" + modelId + "?blobs=true"));
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, modelId] {
        reply->deleteLater();
        QVector<File> files;
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit filesDone(modelId, files,
                           status == 401 || status == 403 ? tr("Gated repository: a Hub token that accepted its terms is needed (Sources › AI)")
                                                           : reply->errorString());
            return;
        }
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).object().value("siblings").toArray()) {
            const QJsonObject o = v.toObject();
            File f;
            f.name = o.value("rfilename").toString();
            f.size = o.contains("size") ? qint64(o.value("size").toDouble()) : -1;
            f.sha256 = o.value("lfs").toObject().value("sha256").toString();
            if (!f.name.isEmpty())
                files.push_back(f);
        }
        emit filesDone(modelId, files, QString());
    });
}

HuggingFaceHub::Download *HuggingFaceHub::find(const QString &modelId, const QString &file)
{
    for (Download &d : m_downloads)
        if (d.model == modelId && d.file == file)
            return &d;
    return nullptr;
}

bool HuggingFaceHub::isDownloading(const QString &modelId, const QString &file) const
{
    return m_active.contains(modelId + '|' + file);
}

void HuggingFaceHub::download(const QString &modelId, const QString &file, qint64 size, const QString &sha256)
{
    const QString key = modelId + '|' + file;
    if (m_active.contains(key))
        return;
    Download *d = find(modelId, file);
    if (!d) {
        m_downloads.push_back({modelId, file});
        d = &m_downloads.last();
    }
    if (d->queued)
        return;
    *d = {modelId, file};
    d->sha256 = sha256;
    d->total = size;
    d->queued = true;
    emit downloadChanged();
    startQueued();
}

void HuggingFaceHub::startQueued()
{
    for (Download &d : m_downloads) {
        if (m_active.size() >= MaxParallel)
            return;
        if (d.queued) {
            d.queued = false;
            start(d.model, d.file);
        }
    }
}

void HuggingFaceHub::start(const QString &modelId, const QString &file)
{
    const QString key = modelId + '|' + file;
    Download *d = find(modelId, file);
    const QString dir = modelsRoot() + '/' + modelId + '/' + QFileInfo(file).path();
    QDir().mkpath(dir);
    const QString target = modelsRoot() + '/' + modelId + '/' + file;
    auto *out = new QFile(target + ".part", this);
    if (!out->open(QIODevice::WriteOnly | QIODevice::Append)) {
        d->error = tr("Can't write %1").arg(target);
        d->done = true;
        out->deleteLater();
        emit downloadChanged();
        return;
    }
    Active a;
    a.out = out;
    a.resumedFrom = out->size();
    a.startedAt = QDateTime::currentMSecsSinceEpoch();
    QNetworkRequest req = request(QUrl("https://huggingface.co/" + modelId + "/resolve/main/" + file));
    req.setTransferTimeout(120000);
    if (a.resumedFrom > 0)
        req.setRawHeader("Range", "bytes=" + QByteArray::number(a.resumedFrom) + "-");
    a.reply = m_net.get(req);
    // Read in large pieces: the UI thread is woken less often and the disk gets bigger writes.
    a.reply->setReadBufferSize(8 * 1024 * 1024);
    m_active.insert(key, a);
    d->received = a.resumedFrom;
    QNetworkReply *reply = a.reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, key, reply] {
        if (Active *act = m_active.contains(key) ? &m_active[key] : nullptr) {
            // A server that ignores the range answers 200 with the whole file: start over.
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 200 && act->resumedFrom > 0) {
                act->out->resize(0);
                act->resumedFrom = 0;
            }
            act->out->write(reply->readAll());
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, modelId, file, key](qint64 got, qint64 total) {
        Download *dl = find(modelId, file);
        const Active *act = m_active.contains(key) ? &m_active[key] : nullptr;
        if (!dl || !act)
            return;
        dl->received = act->resumedFrom + got;
        if (total > 0)
            dl->total = act->resumedFrom + total;
        const double secs = (QDateTime::currentMSecsSinceEpoch() - act->startedAt) / 1000.0;
        dl->speed = secs > 0.5 ? got / secs : 0;
        emit downloadChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, modelId, file, key, reply, target] {
        reply->deleteLater();
        QString error;
        if (reply->error() == QNetworkReply::OperationCanceledError)
            error = tr("Cancelled");
        else if (reply->error() != QNetworkReply::NoError)
            error = reply->errorString();
        if (Active *act = m_active.contains(key) ? &m_active[key] : nullptr) {
            act->out->write(reply->readAll());
            act->out->close();
            if (error.isEmpty()) {
                QFile::remove(target);
                if (!act->out->rename(target))
                    error = tr("Can't rename the downloaded file");
            }
            act->out->deleteLater();
        }
        m_active.remove(key);
        const QString sha = find(modelId, file) ? find(modelId, file)->sha256 : QString();
        if (error.isEmpty() && !sha.isEmpty())
            verify(modelId, file, target, sha);
        else
            finishDownload(modelId, file, error);
        startQueued();
    });
    emit downloadChanged();
}

void HuggingFaceHub::verify(const QString &modelId, const QString &file, const QString &path, const QString &sha256)
{
    // The hash of a multi-gigabyte file takes seconds: computed on the thread pool.
    if (Download *d = find(modelId, file)) {
        d->verifying = true;
        emit downloadChanged();
    }
    QPointer<HuggingFaceHub> self(this);
    QtConcurrent::run([path] {
        QFile f(path);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!f.open(QIODevice::ReadOnly) || !hash.addData(&f))
            return QString();
        return QString::fromLatin1(hash.result().toHex());
    }).then(this, [self, modelId, file, sha256](const QString &got) {
        if (!self)
            return;
        if (Download *d = self->find(modelId, file)) {
            d->verifying = false;
            d->verified = got == sha256;
        }
        self->finishDownload(modelId, file, got == sha256 ? QString() : tr("The file's SHA-256 doesn't match the Hub's: download it again"));
    });
}

void HuggingFaceHub::finishDownload(const QString &modelId, const QString &file, const QString &error)
{
    if (Download *d = find(modelId, file)) {
        d->done = true;
        d->error = error;
        if (error.isEmpty())
            d->total = d->received;
    }
    emit downloadFinished(modelId, file, error.isEmpty());
    emit downloadChanged();
}

void HuggingFaceHub::cancel(const QString &modelId, const QString &file)
{
    const QString key = modelId + '|' + file;
    if (const auto it = m_active.constFind(key); it != m_active.constEnd())
        it->reply->abort();   // the finished handler keeps the .part for a later resume
}

// ---------------------------------------------------------------- LocalModels

LocalModels::LocalModels(QObject *parent)
    : QObject(parent)
{
}

QString LocalModels::folder(const QString &id) const
{
    return HuggingFaceHub::modelsRoot() + '/' + id;
}

QVector<LocalModels::Entry> LocalModels::scan() const
{
    QVector<Entry> out;
    const QDir root(HuggingFaceHub::modelsRoot());
    for (const QFileInfo &org : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        for (const QFileInfo &repo : QDir(org.filePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            Entry e;
            e.id = org.fileName() + '/' + repo.fileName();
            QDirIterator it(repo.filePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo f(it.next());
                if (f.suffix() == "part")
                    continue;   // an unfinished download
                e.files << QDir(repo.filePath()).relativeFilePath(f.filePath());
                e.bytes += f.size();
                if (f.lastModified() > e.modified)
                    e.modified = f.lastModified();
            }
            if (!e.files.isEmpty())
                out.push_back(e);
        }
    return out;
}

bool LocalModels::remove(const QString &id)
{
    QDir dir(folder(id));
    if (!dir.exists())
        return false;
    const bool ok = dir.removeRecursively();
    QDir(HuggingFaceHub::modelsRoot()).rmdir(id.section('/', 0, 0));   // the organisation folder, when empty
    return ok;
}

bool LocalModels::removeFile(const QString &id, const QString &file)
{
    return QFile::remove(folder(id) + '/' + file);
}

QString LocalModels::formatBytes(qint64 bytes)
{
    const QLocale loc;
    if (bytes >= 1024LL * 1024 * 1024)
        return QObject::tr("%1 GB").arg(loc.toString(bytes / (1024.0 * 1024 * 1024), 'f', 2));
    if (bytes >= 1024 * 1024)
        return QObject::tr("%1 MB").arg(loc.toString(bytes / (1024.0 * 1024), 'f', 1));
    if (bytes >= 1024)
        return QObject::tr("%1 KB").arg(loc.toString(bytes / 1024.0, 'f', 0));
    return QObject::tr("%1 B").arg(bytes);
}
