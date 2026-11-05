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
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

// The AI page: the models on this computer and the Hugging Face Hub to get more (Models › Local models), the
// Claude settings and opencode (Models › Integration), with the slots under both; a conversation with a model run here (Local,
// AiLocalView, over LocalChat), and machine learning on the app's data (AiMlView). Keys and tokens are entered in Sources › AI.

class AiModelsView : public QWidget
{
    Q_OBJECT
public:
    explicit AiModelsView(QWidget *parent = nullptr);
    // Dev flags --ai-search / --ai-download: run a search and open its first result, download one file (screenshots).
    void searchFor(const QString &query);
    void downloadFile(const QString &modelId, const QString &file);
    class ModelSlotsView *slotsView() const { return m_slots; }
    // The description and the Hub token state, shown by AiPage above the Models navigation bar.
    QWidget *header() const { return m_header; }

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

    QWidget *m_header;
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
    class ModelSlotsView *m_slots;
    QLabel *m_installedNote;
    QPushButton *m_openFolder;
    QPushButton *m_remove;
    QVector<HuggingFaceHub::Model> m_models;
    QString m_currentModel;
    QVector<HuggingFaceHub::File> m_currentFiles;
    bool m_openFirst = false;
    QString m_devDownload;   // --ai-download: the file to download once the repository's files (and SHA) are known
};

// AI › Models › Integration: the Claude model the app calls and its effort (ai/model, ai/effort), and the key state;
// then opencode, which reads Deepwater's data through MCP (AiOpencodeView, McpServer). Each can be dragged into a slot.
class AiClaudeView : public QWidget
{
    Q_OBJECT
public:
    explicit AiClaudeView(class ModelSlotsView *slotsView, QWidget *parent = nullptr);
    // The description and the key state, shown by AiPage above the Models navigation bar.
    QWidget *header() const { return m_header; }

protected:
    void showEvent(QShowEvent *) override;

private:
    void updateKeyState();
    void setModels(const QVector<ClaudeClient::ModelInfo> &models, const QString &error);

    QWidget *m_header;
    ClaudeClient *m_claude;
    QComboBox *m_model;
    QComboBox *m_effort;
    QLabel *m_keyState;
    QLabel *m_status;
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
    void runLocalPrompt(const QString &prompt);
    // The conversation with the local model, shared by AI › Local and the side panel (AiDock).
    class LocalChat *chat() const { return m_chat; }

private:
    // Models › Local models (0) or Integration (1, Claude).
    void showModelsPart(int index);

    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
    QButtonGroup *m_modelsTabs;
    AiModelsView *m_models;
    AiClaudeView *m_claude;
    class AiLocalView *m_localView;
    class LocalChat *m_chat;
    class AiMlView *m_ml;
};
