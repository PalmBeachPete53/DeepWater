#include "AiPage.h"
#include "ModelSlots.h"

#include "AiLocalView.h"
#include "AiOpencodeView.h"
#include "LocalChat.h"
#include "AiMlView.h"

#include "KeyedSources.h"
#include "Theme.h"

#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStorageInfo>
#include <QTableWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *dimLabel(const QString &text = {})
{
    auto *l = new QLabel(text);
    l->setObjectName("dim");
    l->setWordWrap(true);
    return l;
}

QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    return l;
}

QPushButton *segButton(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setObjectName("segButton");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QTableWidget *table(const QStringList &headers)
{
    auto *t = new QTableWidget(0, headers.size());
    t->setObjectName("aiTable");
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->verticalHeader()->setDefaultSectionSize(26);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->setFocusPolicy(Qt::NoFocus);
    t->horizontalHeader()->setStretchLastSection(false);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t->horizontalHeader()->setHighlightSections(false);
    return t;
}

QString compactNumber(qint64 n)
{
    const QLocale loc;
    if (n >= 1000000) return QString("%1M").arg(loc.toString(n / 1e6, 'f', 1));
    if (n >= 1000) return QString("%1k").arg(loc.toString(n / 1e3, 'f', 0));
    return loc.toString(n);
}

} // namespace

// ---------------------------------------------------------------- AiModelsView

AiModelsView::AiModelsView(QWidget *parent)
    : QWidget(parent)
    , m_hub(new HuggingFaceHub(this))
    , m_local(new LocalModels(this))
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);

    // The description and the token state go in the page's header, above its navigation bar (AiPage).
    m_header = new QWidget;
    auto *hv = new QVBoxLayout(m_header);
    hv->setContentsMargins(0, 0, 0, 0);
    hv->setSpacing(8);
    hv->addWidget(dimLabel(tr("The models kept on this computer, and the Hugging Face Hub to get more: search a model, pick its "
                              "files (a GGUF quantisation, the safetensors weights…) and download them into the app's models "
                              "folder; downloads resume where they stopped. A Hub token (Sources › AI) opens gated repositories.")));
    m_tokenState = dimLabel();
    hv->addWidget(m_tokenState);

    // Search
    auto *searchRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setObjectName("keyEdit");
    m_search->setPlaceholderText(tr("Search the Hub: qwen2.5 gguf, mistral, bge-m3, whisper…"));
    m_search->setClearButtonEnabled(true);
    searchRow->addWidget(m_search, 1);
    auto *go = segButton(tr("Search"));
    searchRow->addWidget(go);
    v->addLayout(searchRow);
    m_resultsNote = dimLabel();
    v->addWidget(m_resultsNote);

    auto *split = new QSplitter(Qt::Horizontal);
    auto *left = new QWidget;
    auto *lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(6);
    lv->addWidget(sectionLabel(tr("Results")));
    m_results = table({tr("Model"), tr("Task"), tr("Downloads"), tr("Likes"), tr("Updated")});
    m_results->setColumnWidth(1, 150);
    m_results->setColumnWidth(2, 100);
    m_results->setColumnWidth(3, 70);
    m_results->setColumnWidth(4, 100);
    m_results->setMinimumHeight(240);
    lv->addWidget(m_results, 1);
    split->addWidget(left);

    auto *right = new QWidget;
    auto *rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(6);
    m_filesTitle = sectionLabel(tr("Files"));
    rv->addWidget(m_filesTitle);
    m_files = table({tr("File"), tr("Size")});
    m_files->setColumnWidth(1, 90);
    m_files->setSelectionMode(QAbstractItemView::NoSelection);
    m_files->setMinimumHeight(240);
    rv->addWidget(m_files, 1);
    auto *fileButtons = new QHBoxLayout;
    m_downloadButton = segButton(tr("Download the ticked files"));
    m_downloadButton->setEnabled(false);
    m_selectionSize = new QLabel;
    m_selectionSize->setObjectName("sourceMeta");
    m_openHub = segButton(tr("Open on huggingface.co"));
    m_openHub->setEnabled(false);
    fileButtons->addWidget(m_downloadButton);
    fileButtons->addWidget(m_selectionSize);
    fileButtons->addSpacing(8);
    fileButtons->addWidget(m_openHub);
    fileButtons->addStretch(1);
    rv->addLayout(fileButtons);
    split->addWidget(right);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    v->addWidget(split, 3);

    // Downloads
    m_downloadsTitle = sectionLabel(tr("Downloads"));
    v->addWidget(m_downloadsTitle);
    m_downloadsLayout = new QVBoxLayout;
    m_downloadsLayout->setSpacing(4);
    v->addLayout(m_downloadsLayout);
    m_downloadsTitle->setVisible(false);

    // Installed
    v->addWidget(sectionLabel(tr("On this computer")));
    m_installedNote = dimLabel();
    v->addWidget(m_installedNote);
    m_installed = table({tr("Model"), tr("Files"), tr("Size"), tr("Updated")});
    m_installed->setColumnWidth(1, 70);
    m_installed->setColumnWidth(2, 90);
    m_installed->setColumnWidth(3, 130);
    m_installed->setMinimumHeight(130);
    v->addWidget(m_installed, 2);
    auto *installedButtons = new QHBoxLayout;
    m_openFolder = segButton(tr("Open the folder"));
    m_remove = segButton(tr("Delete"));
    installedButtons->addWidget(m_openFolder);
    installedButtons->addWidget(m_remove);
    installedButtons->addStretch(1);
    v->addLayout(installedButtons);
    // Slots: a model dragged from the list above gets the skills of its slot, and nothing else.
    m_installed->setDragEnabled(true);
    m_installed->setDragDropMode(QAbstractItemView::DragOnly);
    m_installed->setToolTip(tr("Drag a model into a slot below to give it skills"));
    m_slots = new ModelSlotsView;   // placed by AiPage, under both parts of Models

    connect(go, &QPushButton::clicked, this, &AiModelsView::search);
    connect(m_search, &QLineEdit::returnPressed, this, &AiModelsView::search);
    connect(m_hub, &HuggingFaceHub::searchDone, this, [this](const QString &, const QVector<HuggingFaceHub::Model> &models, const QString &error) {
        showResults(models, error);
    });
    connect(m_hub, &HuggingFaceHub::filesDone, this, &AiModelsView::showFiles);
    connect(m_results, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row < 0 || row >= m_models.size())
            return;
        m_currentModel = m_models[row].id;
        m_filesTitle->setText(tr("Files · %1").arg(m_currentModel).toUpper());
        m_files->setRowCount(0);
        m_downloadButton->setEnabled(false);
        m_selectionSize->clear();
        m_openHub->setEnabled(true);
        m_hub->listFiles(m_currentModel);
    });
    connect(m_openHub, &QPushButton::clicked, this, [this] {
        if (!m_currentModel.isEmpty())
            QDesktopServices::openUrl(QUrl("https://huggingface.co/" + m_currentModel));
    });
    connect(m_downloadButton, &QPushButton::clicked, this, &AiModelsView::downloadSelected);
    connect(m_files, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *) { updateSelectionSize(); });
    connect(m_hub, &HuggingFaceHub::downloadChanged, this, &AiModelsView::refreshDownloads);
    connect(m_hub, &HuggingFaceHub::downloadFinished, this, [this](const QString &, const QString &, bool) { refreshInstalled(); });
    connect(m_openFolder, &QPushButton::clicked, this, [this] {
        const int row = m_installed->currentRow();
        const QString id = row >= 0 ? m_installed->item(row, 0)->text() : QString();
        QDesktopServices::openUrl(QUrl::fromLocalFile(id.isEmpty() ? HuggingFaceHub::modelsRoot() : m_local->folder(id)));
    });
    connect(m_remove, &QPushButton::clicked, this, &AiModelsView::removeInstalled);
    refreshInstalled();
    updateTokenState();
}

void AiModelsView::showEvent(QShowEvent *e)
{
    updateTokenState();
    refreshInstalled();
    QWidget::showEvent(e);
}

void AiModelsView::updateTokenState()
{
    m_tokenState->setText(sourceKey(HuggingFaceHub::tokenId()).isEmpty()
                              ? tr("No Hub token: public repositories only. Enter one in Sources › AI for gated models.")
                              : tr("Hub token saved: gated repositories you accepted the terms of are available."));
}

void AiModelsView::search()
{
    const QString q = m_search->text().trimmed();
    if (q.isEmpty())
        return;
    m_resultsNote->setText(tr("Searching…"));
    m_hub->search(q);
}

void AiModelsView::searchFor(const QString &query)
{
    m_search->setText(query);
    m_openFirst = true;
    search();
}

void AiModelsView::downloadFile(const QString &modelId, const QString &file)
{
    // Through the files list, so the size and the SHA-256 come along as for a ticked file.
    m_currentModel = modelId;
    m_devDownload = file;
    m_hub->listFiles(modelId);
}

void AiModelsView::showResults(const QVector<HuggingFaceHub::Model> &models, const QString &error)
{
    m_models = models;
    m_results->setRowCount(0);
    if (!error.isEmpty()) {
        m_resultsNote->setText(tr("Hub error: %1").arg(error));
        return;
    }
    m_resultsNote->setText(tr("%n model(s), most downloaded first. Click one to see its files.", nullptr, models.size()));
    const QLocale loc;
    m_results->setRowCount(models.size());
    for (int i = 0; i < models.size(); ++i) {
        const HuggingFaceHub::Model &m = models[i];
        auto item = [&](int col, const QString &text, int align = Qt::AlignLeft) {
            auto *it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::Alignment(align) | Qt::AlignVCenter);
            m_results->setItem(i, col, it);
        };
        item(0, m.id + (m.gated ? tr(" (gated)") : QString()));
        item(1, m.task);
        item(2, compactNumber(m.downloads), Qt::AlignRight);
        item(3, compactNumber(m.likes), Qt::AlignRight);
        item(4, m.modified.isValid() ? loc.toString(m.modified.date(), QLocale::ShortFormat) : QString());
    }
    if (m_openFirst && !models.isEmpty()) {
        m_openFirst = false;
        m_results->setCurrentCell(0, 0);
    }
}

void AiModelsView::showFiles(const QString &modelId, const QVector<HuggingFaceHub::File> &files, const QString &error)
{
    if (modelId != m_currentModel)
        return;
    m_currentFiles = files;
    m_files->setRowCount(0);
    if (!error.isEmpty()) {
        m_resultsNote->setText(error);
        return;
    }
    m_files->setRowCount(files.size());
    const QVector<LocalModels::Entry> installed = m_local->scan();
    QStringList have;
    for (const LocalModels::Entry &e : installed)
        if (e.id == modelId)
            have = e.files;
    qint64 total = 0;
    for (int i = 0; i < files.size(); ++i) {
        const HuggingFaceHub::File &f = files[i];
        const LocalModels::FileKind kind = LocalModels::fileKind(f.name);
        const bool code = kind == LocalModels::FileKind::Code;
        auto *name = new QTableWidgetItem(f.name + (have.contains(f.name) ? tr(" · on this computer") : QString())
                                          + (code ? tr(" · can run code") : QString()));
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        // The weights in data-only formats are ticked, the small files (README, config) too since they're cheap;
        // already-there files aren't, nor those that can run code when loaded (pickle, Python), which are marked.
        const bool weight = kind == LocalModels::FileKind::Weights;
        name->setCheckState(have.contains(f.name) || code ? Qt::Unchecked
                            : weight || f.size < 5 * 1024 * 1024 ? Qt::Checked : Qt::Unchecked);
        if (code) {
            name->setForeground(Theme::current().Down);
            name->setToolTip(tr("This format can run code when it is loaded (a pickle file or Python code). Deepwater "
                                "never loads it, and the local engine reads GGUF files only."));
        }
        m_files->setItem(i, 0, name);
        auto *size = new QTableWidgetItem(f.size >= 0 ? LocalModels::formatBytes(f.size) : QString::fromUtf8("—"));
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_files->setItem(i, 1, size);
        if (f.size > 0) total += f.size;
    }
    m_downloadButton->setEnabled(!files.isEmpty());
    m_resultsNote->setText(tr("%1: %n file(s), %2 in all. Tick what to download.", nullptr, files.size())
                               .arg(modelId, LocalModels::formatBytes(total)));
    updateSelectionSize();
    if (!m_devDownload.isEmpty()) {
        for (const HuggingFaceHub::File &f : files)
            if (f.name == m_devDownload)
                m_hub->download(modelId, f.name, f.size, f.sha256);
        m_devDownload.clear();
    }
}

void AiModelsView::updateSelectionSize()
{
    // What the ticked files weigh, and whether the disk has room for them.
    qint64 bytes = 0;
    int count = 0, unknown = 0;
    for (int i = 0; i < m_files->rowCount() && i < m_currentFiles.size(); ++i) {
        if (!m_files->item(i, 0) || m_files->item(i, 0)->checkState() != Qt::Checked)
            continue;
        ++count;
        if (m_currentFiles[i].size > 0)
            bytes += m_currentFiles[i].size;
        else
            ++unknown;
    }
    if (!count) {
        m_selectionSize->setText(tr("Nothing ticked"));
        m_downloadButton->setEnabled(false);
        return;
    }
    m_downloadButton->setEnabled(true);
    QString text = tr("%n file(s), %1 on disk", nullptr, count).arg(LocalModels::formatBytes(bytes));
    if (unknown)
        text += " " + tr("(+%n of unknown size)", nullptr, unknown);
    const QStorageInfo disk(HuggingFaceHub::modelsRoot());
    if (disk.isValid()) {
        const qint64 free = disk.bytesAvailable();
        text += " · " + tr("%1 free").arg(LocalModels::formatBytes(free));
        if (bytes > free)
            text += " · " + tr("not enough room");
    }
    m_selectionSize->setText(text);
    m_selectionSize->setStyleSheet(disk.isValid() && bytes > disk.bytesAvailable()
                                       ? QString("color: %1;").arg(Theme::current().Down.name())
                                       : QString());
}

void AiModelsView::downloadSelected()
{
    QStringList code;
    for (int i = 0; i < m_files->rowCount() && i < m_currentFiles.size(); ++i)
        if (m_files->item(i, 0)->checkState() == Qt::Checked
            && LocalModels::fileKind(m_currentFiles[i].name) == LocalModels::FileKind::Code)
            code << m_currentFiles[i].name;
    if (!code.isEmpty()
        && QMessageBox::question(this, tr("Files that can run code"),
                                 tr("%1 can run code when loaded. Deepwater never loads them, but another program "
                                    "opening them could. Download them anyway?").arg(code.join(", ")),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    for (int i = 0; i < m_files->rowCount() && i < m_currentFiles.size(); ++i)
        if (m_files->item(i, 0)->checkState() == Qt::Checked)
            m_hub->download(m_currentModel, m_currentFiles[i].name, m_currentFiles[i].size, m_currentFiles[i].sha256);
}

void AiModelsView::refreshDownloads()
{
    const QVector<HuggingFaceHub::Download> &list = m_hub->downloads();
    m_downloadsTitle->setVisible(!list.isEmpty());
    const QLocale loc;
    for (const HuggingFaceHub::Download &d : list) {
        const QString key = d.model + '|' + d.file;
        if (!m_downloadRows.contains(key)) {
            DownloadRow r;
            r.row = new QWidget;
            auto *h = new QHBoxLayout(r.row);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(10);
            r.label = new QLabel;
            r.label->setObjectName("sourceMeta");
            r.label->setMinimumWidth(360);
            r.bar = new QProgressBar;
            r.bar->setObjectName("qtProgress");
            r.bar->setFixedHeight(8);
            r.bar->setTextVisible(false);
            r.cancel = segButton(tr("Cancel"));
            const QString model = d.model, file = d.file;
            connect(r.cancel, &QPushButton::clicked, this, [this, model, file] { m_hub->cancel(model, file); });
            h->addWidget(r.label);
            h->addWidget(r.bar, 1);
            h->addWidget(r.cancel);
            m_downloadsLayout->addWidget(r.row);
            m_downloadRows.insert(key, r);
        }
        DownloadRow &r = m_downloadRows[key];
        QString state;
        if (d.done)
            state = !d.error.isEmpty() ? d.error : d.verified ? tr("done · SHA-256 checked") : tr("done");
        else if (d.verifying)
            state = tr("checking the SHA-256…");
        else if (d.queued)
            state = tr("waiting (%1 at a time)").arg(HuggingFaceHub::MaxParallel);
        else if (d.total > 0)
            state = tr("%1 of %2 · %3/s").arg(LocalModels::formatBytes(d.received), LocalModels::formatBytes(d.total),
                                              LocalModels::formatBytes(qint64(d.speed)));
        else
            state = tr("%1 · %2/s").arg(LocalModels::formatBytes(d.received), LocalModels::formatBytes(qint64(d.speed)));
        r.label->setText(QString("%1 / %2 · %3").arg(d.model, d.file, state));
        if (d.queued) {
            r.bar->setRange(0, 1000);
            r.bar->setValue(0);
        } else if (d.verifying) {
            r.bar->setRange(0, 0);
        } else if (d.total > 0) {
            r.bar->setRange(0, 1000);
            r.bar->setValue(int(1000.0 * d.received / d.total));
        } else {
            r.bar->setRange(0, 0);
        }
        if (d.done) {
            r.bar->setRange(0, 1000);
            r.bar->setValue(d.error.isEmpty() ? 1000 : 0);
        }
        r.cancel->setVisible(!d.done);
    }
}

void AiModelsView::refreshInstalled()
{
    const QVector<LocalModels::Entry> list = m_local->scan();
    const QLocale loc;
    m_installed->setRowCount(list.size());
    qint64 total = 0;
    for (int i = 0; i < list.size(); ++i) {
        const LocalModels::Entry &e = list[i];
        total += e.bytes;
        auto item = [&](int col, const QString &text, int align = Qt::AlignLeft) {
            auto *it = new QTableWidgetItem(text);
            it->setTextAlignment(Qt::Alignment(align) | Qt::AlignVCenter);
            it->setToolTip(e.files.join('\n'));
            m_installed->setItem(i, col, it);
        };
        item(0, e.id);
        item(1, QString::number(e.files.size()), Qt::AlignRight);
        item(2, LocalModels::formatBytes(e.bytes), Qt::AlignRight);
        item(3, loc.toString(e.modified, QLocale::ShortFormat));
    }
    m_slots->setInstalled(list);
    m_installedNote->setText(list.isEmpty()
                                 ? tr("No model yet. They go to %1.").arg(HuggingFaceHub::modelsRoot())
                                 : tr("%n model(s), %1, in %2.", nullptr, list.size()).arg(LocalModels::formatBytes(total), HuggingFaceHub::modelsRoot()));
}

void AiModelsView::removeInstalled()
{
    const int row = m_installed->currentRow();
    if (row < 0)
        return;
    const QString id = m_installed->item(row, 0)->text();
    const QString size = m_installed->item(row, 2)->text();
    if (QMessageBox::question(this, tr("Delete the model"), tr("Delete %1 (%2) from this computer?").arg(id, size),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_local->remove(id);
    refreshInstalled();
}

// ---------------------------------------------------------------- AiClaudeView

AiClaudeView::AiClaudeView(ModelSlotsView *slotsView, QWidget *parent)
    : QWidget(parent)
    , m_claude(new ClaudeClient(this))
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    // The description goes in the page's header, above its navigation bar (AiPage); Claude, then opencode, below it.
    m_header = new QWidget;
    auto *hv = new QVBoxLayout(m_header);
    hv->setContentsMargins(0, 0, 0, 0);
    hv->setSpacing(8);
    hv->addWidget(dimLabel(tr("Models and agents outside the app: Claude, which the app calls through the Anthropic API, and "
                              "opencode, which reads Deepwater's data through MCP. Drag one into a slot below to give it skills.")));

    v->addWidget(sectionLabel(tr("Claude")));
    v->addWidget(modelslots::integrationChip(modelslots::ClaudeId, slotsView));
    v->addWidget(dimLabel(tr("Claude through the Anthropic API: the model the app calls and how deep it thinks (Effort; the "
                             "model thinks adaptively). The models list comes from the API once a key is saved. The key is "
                             "entered in Sources › AI.")));
    m_keyState = dimLabel();
    v->addWidget(m_keyState);

    auto *controls = new QHBoxLayout;
    controls->setSpacing(8);
    m_model = new QComboBox;
    m_model->setObjectName("sourceCombo");
    m_model->setMinimumWidth(220);
    m_effort = new QComboBox;
    m_effort->setObjectName("sourceCombo");
    for (const char *e : {"low", "medium", "high", "xhigh", "max"})
        m_effort->addItem(QString::fromLatin1(e), QString::fromLatin1(e));
    auto *refreshModels = segButton(tr("Refresh the models"));
    controls->addWidget(new QLabel(tr("Model")));
    controls->addWidget(m_model);
    controls->addWidget(new QLabel(tr("Effort")));
    controls->addWidget(m_effort);
    controls->addWidget(refreshModels);
    controls->addStretch(1);
    v->addLayout(controls);
    m_status = dimLabel();
    v->addWidget(m_status);
    v->addSpacing(14);
    v->addWidget(sectionLabel(tr("opencode")));
    v->addWidget(new AiOpencodeView(slotsView));

    const QString effort = QSettings().value("ai/effort", "medium").toString();
    if (const int i = m_effort->findData(effort); i >= 0)
        m_effort->setCurrentIndex(i);
    setModels(ClaudeClient::knownModels(), QString());

    connect(refreshModels, &QPushButton::clicked, m_claude, &ClaudeClient::listModels);
    connect(m_claude, &ClaudeClient::modelsListed, this, &AiClaudeView::setModels);
    connect(m_effort, &QComboBox::currentIndexChanged, this, [this] { QSettings().setValue("ai/effort", m_effort->currentData().toString()); });
    connect(m_model, &QComboBox::currentIndexChanged, this, [this] {
        if (m_model->currentIndex() >= 0) QSettings().setValue("ai/model", m_model->currentData().toString());
    });
    updateKeyState();
}

void AiClaudeView::showEvent(QShowEvent *e)
{
    updateKeyState();
    if (!m_modelsFromApi && m_claude->hasKey())
        m_claude->listModels();
    QWidget::showEvent(e);
}

void AiClaudeView::updateKeyState()
{
    m_keyState->setText(m_claude->hasKey() ? tr("API key saved (Sources › AI).")
                                           : tr("No API key: enter one in Sources › AI to use Claude."));
}

void AiClaudeView::setModels(const QVector<ClaudeClient::ModelInfo> &models, const QString &error)
{
    const QString wanted = m_model->currentIndex() >= 0 ? m_model->currentData().toString()
                                                        : QSettings().value("ai/model", ClaudeClient::defaultModel()).toString();
    m_model->blockSignals(true);
    m_model->clear();
    for (const ClaudeClient::ModelInfo &m : models)
        m_model->addItem(m.name.isEmpty() ? m.id : m.name, m.id);
    int index = m_model->findData(wanted);
    if (index < 0) index = m_model->findData(ClaudeClient::defaultModel());
    m_model->setCurrentIndex(std::max(0, index));
    m_model->blockSignals(false);
    m_modelsFromApi = error.isEmpty() && m_claude->hasKey();
    m_status->setText(error.isEmpty() ? QString() : tr("Couldn't list the models: %1").arg(error));
    m_status->setVisible(!error.isEmpty());   // no empty line between Claude and opencode
}

// ---------------------------------------------------------------- AiPage

AiPage::AiPage(const MlDataExporter::Sources &mlSources, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("aiPage");
    setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new qtm::kit::WrapBarLayout(sub, 2);
    sh->setContentsMargins(6, 0, 12, 0);
    m_tabs = new QButtonGroup(this);
    const QStringList names {tr("Models"), tr("Local"), tr("Machine learning")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(names[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    v->addWidget(sub);
    m_stack = new QStackedWidget;
    m_models = new AiModelsView;
    m_claude = new AiClaudeView(m_models->slotsView());
    {
        // Models: the header (title, the description of the part shown, its navigation bar), then the models of this
        // computer and the Hub (Local models) or the Claude settings (Integration), and the slots under both. Scrolls
        // when the window is too short for the Hub's tables, the models here and the slots together.
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *section = new QWidget;
        section->setObjectName("aiModelsSection");
        section->setAttribute(Qt::WA_StyledBackground);
        auto *sv = new QVBoxLayout(section);
        sv->setContentsMargins(32, 22, 32, 24);
        sv->setSpacing(8);
        auto *title = new QLabel(tr("Models"));
        title->setObjectName("title");
        sv->addWidget(title);
        sv->addWidget(m_models->header());
        sv->addWidget(m_claude->header());
        auto *nav = new QWidget;
        nav->setObjectName("aiModelsNav");
        nav->setAttribute(Qt::WA_StyledBackground);
        nav->setFixedHeight(34);
        auto *nh = new qtm::kit::WrapBarLayout(nav, 4);
        nh->setContentsMargins(0, 0, 0, 0);
        m_modelsTabs = new QButtonGroup(this);
        const QStringList parts {tr("Local models"), tr("Integration")};
        for (int i = 0; i < parts.size(); ++i) {
            auto *b = new QToolButton;
            b->setObjectName("calSubTab");
            b->setText(parts[i].toUpper());
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setFixedHeight(34);
            m_modelsTabs->addButton(b, i);
            nh->addWidget(b);
        }
        sv->addSpacing(4);
        sv->addWidget(nav);
        sv->addSpacing(8);
        // The part not shown is hidden (not stacked), so Integration doesn't keep the Hub's height above the slots.
        sv->addWidget(m_models, 1);
        sv->addWidget(m_claude);
        sv->addSpacing(14);
        sv->addWidget(m_models->slotsView());
        sv->addStretch(0);
        scroll->setWidget(section);
        m_stack->addWidget(scroll);
        connect(m_modelsTabs, &QButtonGroup::idClicked, this, &AiPage::showModelsPart);
        showModelsPart(std::clamp(QSettings().value("ai/modelsPart", 0).toInt(), 0, 1));
    }
    m_chat = new LocalChat(m_models->slotsView(), mlSources, this);
    m_localView = new AiLocalView(m_chat);
    // Short window: the conversation, its controls and the journal together scroll under the section tabs.
    auto *localScroll = new QScrollArea;
    localScroll->setWidgetResizable(true);
    localScroll->setFrameShape(QFrame::NoFrame);
    localScroll->setWidget(m_localView);
    m_stack->addWidget(localScroll);
    m_ml = new AiMlView(mlSources);
    m_stack->addWidget(m_ml);
    v->addWidget(m_stack, 1);
    connect(m_tabs, &QButtonGroup::idClicked, this, &AiPage::showSection);
    showSection(std::clamp(QSettings().value("ai/section", 0).toInt(), 0, m_stack->count() - 1));
}

void AiPage::showModelsPart(int index)
{
    m_modelsTabs->button(index)->setChecked(true);
    m_models->setVisible(index == 0);
    m_claude->setVisible(index == 1);
    m_models->header()->setVisible(index == 0);
    m_claude->header()->setVisible(index == 1);
    QSettings().setValue("ai/modelsPart", index);
}

void AiPage::searchFor(const QString &query)
{
    showSection(0);
    showModelsPart(0);
    m_models->searchFor(query);
}

void AiPage::exportMlData()
{
    showSection(2);
    m_ml->exportNow();
}

void AiPage::trainMl(const QString &file, const QString &target)
{
    showSection(2);
    m_ml->trainNow(file, target);
}

void AiPage::downloadFile(const QString &modelId, const QString &file)
{
    showSection(0);
    showModelsPart(0);
    m_models->downloadFile(modelId, file);
}

void AiPage::showSection(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("ai/section", index);
}

void AiPage::runLocalPrompt(const QString &prompt)
{
    showSection(1);
    m_chat->runPrompt(prompt);
}
