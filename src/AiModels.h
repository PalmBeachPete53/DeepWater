#pragma once

#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

class QNetworkReply;

// Models from the Hugging Face Hub (search, a repository's files, downloads with resume) and the models kept
// on this computer, under <app data>/models/<org>/<name>/. A Hub token (Sources › AI, optional) opens gated
// repositories.
class HuggingFaceHub : public QObject
{
    Q_OBJECT
public:
    struct Model {
        QString id;          // "Qwen/Qwen2.5-0.5B-Instruct-GGUF"
        QString task;        // pipeline tag
        QString library;
        qint64 downloads = 0;
        int likes = 0;
        bool gated = false;
        QDateTime modified;
    };
    struct File {
        QString name;   // path in the repository
        qint64 size = -1;
        QString sha256; // of LFS files, from the Hub: checked after the download
    };
    struct Download {
        QString model;
        QString file;
        QString sha256;
        qint64 received = 0;
        qint64 total = -1;
        double speed = 0;   // bytes per second
        QString error;
        bool queued = false;    // waiting for a slot: a few at a time, so each file lands as soon as it can
        bool verifying = false; // the SHA-256 is being computed
        bool verified = false;  // it matched
        bool done = false;
    };

    explicit HuggingFaceHub(QObject *parent = nullptr);

    static QString tokenId() { return "huggingface"; }
    static QString modelsRoot();

    void search(const QString &query);
    void listFiles(const QString &modelId);
    // Queues (or resumes) the download of one file into the models folder; `sha256` is checked at the end when given.
    void download(const QString &modelId, const QString &file, qint64 size, const QString &sha256 = QString());
    // How many files download at once: the line is shared, so more in parallel only delays each one.
    static constexpr int MaxParallel = 2;
    void cancel(const QString &modelId, const QString &file);
    const QVector<Download> &downloads() const { return m_downloads; }
    bool isDownloading(const QString &modelId, const QString &file) const;
    QUrl probeUrl() const { return QUrl("https://huggingface.co/api/models?limit=1"); }

signals:
    void searchDone(const QString &query, const QVector<HuggingFaceHub::Model> &models, const QString &error);
    void filesDone(const QString &modelId, const QVector<HuggingFaceHub::File> &files, const QString &error);
    void downloadChanged();   // progress, completion or failure of any download
    void downloadFinished(const QString &modelId, const QString &file, bool ok);

private:
    struct Active {
        QNetworkReply *reply = nullptr;
        QFile *out = nullptr;
        qint64 startedAt = 0;   // ms
        qint64 resumedFrom = 0;
    };
    QNetworkRequest request(const QUrl &url) const;
    Download *find(const QString &modelId, const QString &file);
    void start(const QString &modelId, const QString &file);
    void startQueued();
    void finishDownload(const QString &modelId, const QString &file, const QString &error);
    void verify(const QString &modelId, const QString &file, const QString &path, const QString &sha256);

    QNetworkAccessManager m_net;
    QVector<Download> m_downloads;
    QHash<QString, Active> m_active;   // "model|file"
};

// The models on this computer.
class LocalModels : public QObject
{
    Q_OBJECT
public:
    struct Entry {
        QString id;            // "org/name"
        QStringList files;
        qint64 bytes = 0;
        QDateTime modified;
    };
    explicit LocalModels(QObject *parent = nullptr);

    QVector<Entry> scan() const;
    bool remove(const QString &id);
    bool removeFile(const QString &id, const QString &file);
    QString folder(const QString &id) const;
    static QString formatBytes(qint64 bytes);
};
