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

    // What a file of a model is. Weights the app may one day load are data-only formats (GGUF, safetensors, ONNX);
    // pickle-based formats (.bin, .pt, .pth, .ckpt, .pkl, .joblib, .h5/.keras with their Lambda layers) and Python
    // files can run code when loaded: they are never ticked by default and Deepwater never loads them.
    enum class FileKind { Weights, Code, Other };
    static FileKind fileKind(const QString &file);
    // Whether the local engine can run the model: it reads GGUF files only.
    static bool runnable(const QStringList &files);
    // A file as it was when a slot took the model: its SHA-256, and its size and date to see quickly that it changed.
    struct Fingerprint {
        QString sha256;
        qint64 size = 0;
        qint64 modified = 0;   // ms since the epoch
    };
    // The fingerprints of every file of a model (the SHA-256 of a multi-gigabyte file takes seconds: call it off the
    // UI thread), or only their sizes and dates (cheap).
    static QHash<QString, Fingerprint> fingerprint(const QString &id);
    static QHash<QString, Fingerprint> stat(const QString &id);
    bool removeFile(const QString &id, const QString &file);
    QString folder(const QString &id) const;
    static QString formatBytes(qint64 bytes);
};
