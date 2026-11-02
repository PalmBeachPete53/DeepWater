#pragma once

#include <QJsonArray>
#include <QHash>
#include <QWidget>

#include "AiModels.h"
#include "ClaudeClient.h"
#include "MlData.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QVBoxLayout;

// The AI page: the models on this computer and the Hugging Face Hub to get more (Models), and a conversation
// with Claude through the Anthropic API (Claude). Keys and tokens are entered in Sources › AI.

class AiModelsView : public QWidget
{
    Q_OBJECT
public:
    explicit AiModelsView(QWidget *parent = nullptr);
    // Dev flags --ai-search / --ai-download: run a search and open its first result, download one file (screenshots).
    void searchFor(const QString &query);
    void downloadFile(const QString &modelId, const QString &file);

protected:
    void showEvent(QShowEvent *) override;

private:
    void search();
    void showResults(const QVector<HuggingFaceHub::Model> &models, const QString &error);
    void showFiles(const QString &modelId, const QVector<HuggingFaceHub::File> &files, const QString &error);
    void downloadSelected();
    void updateSelectionSize();
    void refreshDownloads();
    void refreshInstalled();
    void removeInstalled();
    void updateTokenState();

    HuggingFaceHub *m_hub;
    LocalModels *m_local;
    QLineEdit *m_search;
    QLabel *m_tokenState;
    QLabel *m_resultsNote;
    QTableWidget *m_results;
    QLabel *m_filesTitle;
    QTableWidget *m_files;
    QPushButton *m_downloadButton;
    QLabel *m_selectionSize;   // what the ticked files weigh
    QPushButton *m_openHub;
    QVBoxLayout *m_downloadsLayout;
    QLabel *m_downloadsTitle;
    struct DownloadRow {
        QWidget *row;
        QProgressBar *bar;
        QLabel *label;
        QPushButton *cancel;
    };
    QHash<QString, DownloadRow> m_downloadRows;
    QTableWidget *m_installed;
    QLabel *m_installedNote;
    QPushButton *m_openFolder;
    QPushButton *m_remove;
    QVector<HuggingFaceHub::Model> m_models;
    QString m_currentModel;
    QVector<HuggingFaceHub::File> m_currentFiles;
    bool m_openFirst = false;
    QString m_devDownload;   // --ai-download: the file to download once the repository's files (and SHA) are known
};

class AiClaudeView : public QWidget
{
    Q_OBJECT
public:
    explicit AiClaudeView(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;

private:
    void sendMessage();
    void appendBubble(const QString &role, const QString &text);
    void updateKeyState();
    void setModels(const QVector<ClaudeClient::ModelInfo> &models, const QString &error);

    ClaudeClient *m_claude;
    QComboBox *m_model;
    QComboBox *m_effort;
    QLineEdit *m_system;
    QLabel *m_keyState;
    QTextEdit *m_transcript;
    QPlainTextEdit *m_input;
    QPushButton *m_send;
    QPushButton *m_stop;
    QPushButton *m_clear;
    QLabel *m_status;
    QJsonArray m_messages;   // the conversation, as the API takes it
    QString m_pending;       // the answer being streamed
    QString m_pendingThinking;
    bool m_modelsFromApi = false;
};

class AiPage : public QWidget
{
    Q_OBJECT
public:
    explicit AiPage(const MlDataExporter::Sources &mlSources, QWidget *parent = nullptr);
    void showSection(int index);
    void searchFor(const QString &query);
    void downloadFile(const QString &modelId, const QString &file);
    void exportMlData();
    void trainMl(const QString &file, const QString &target);

private:
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
    AiModelsView *m_models;
    class AiMlView *m_ml;
};
