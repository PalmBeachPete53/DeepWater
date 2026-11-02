#include "AiMlView.h"

#include "AiModels.h"
#include "Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
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

QComboBox *combo()
{
    auto *c = new QComboBox;
    c->setObjectName("sourceCombo");
    return c;
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
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    t->horizontalHeader()->setHighlightSections(false);
    return t;
}

QTableWidgetItem *cell(const QString &text, Qt::Alignment align = Qt::AlignLeft)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(align | Qt::AlignVCenter);
    return it;
}

QString frameworkId(int index)
{
    static const char *ids[] = {"sklearn", "lightgbm", "xgboost", "torch"};
    return QString::fromLatin1(ids[std::clamp(index, 0, 3)]);
}

QString defaultParams(const QString &framework)
{
    if (framework == "sklearn") return "{\"n_estimators\": 300, \"max_depth\": 0}";
    if (framework == "lightgbm") return "{\"n_estimators\": 500, \"learning_rate\": 0.03, \"num_leaves\": 31, \"early_stopping\": 50}";
    if (framework == "xgboost") return "{\"n_estimators\": 500, \"learning_rate\": 0.03, \"max_depth\": 6, \"early_stopping\": 50}";
    return "{\"hidden\": \"64,32\", \"epochs\": 200, \"learning_rate\": 0.001, \"batch_size\": 256, \"dropout\": 0.1, \"early_stopping\": 20}";
}

QString metricsText(const QJsonObject &m)
{
    const QLocale loc;
    QStringList parts;
    auto add = [&](const char *key, const QString &label, int decimals) {
        if (m.contains(key) && m.value(key).isDouble())
            parts << label + ' ' + loc.toString(m.value(key).toDouble(), 'f', decimals);
    };
    add("rmse", "RMSE", 4);
    add("mae", "MAE", 4);
    add("r2", "R²", 3);
    add("direction_hit", AiMlView::tr("direction"), 3);
    add("accuracy", AiMlView::tr("accuracy"), 3);
    add("f1", "F1", 3);
    add("auc", "AUC", 3);
    return parts.join(" · ");
}

} // namespace

AiMlView::AiMlView(const MlDataExporter::Sources &sources, QWidget *parent)
    : QWidget(parent)
    , m_env(new MlEnvironment(this))
    , m_data(new MlDataExporter(sources, this))
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(32, 22, 32, 24);
    v->setSpacing(8);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    auto *title = new QLabel(tr("Machine learning"));
    title->setObjectName("title");
    v->addWidget(title);
    v->addWidget(dimLabel(tr("Train models on Deepwater's data without leaving the app: scikit-learn, LightGBM, XGBoost and PyTorch "
                             "run in a private Python environment the app creates in its data folder. Export the data, pick a data set, "
                             "a target and a framework, train, and read the metrics; every run keeps its model, its settings and its "
                             "feature importances.")));

    // ---- Environment
    v->addWidget(sectionLabel(tr("Python environment")));
    auto *envRow = new QHBoxLayout;
    envRow->setSpacing(8);
    m_envState = new QLabel;
    m_envState->setObjectName("sourceStatus");
    envRow->addWidget(m_envState);
    envRow->addStretch(1);
    envRow->addWidget(new QLabel(tr("Interpreter")));
    m_interpreter = combo();
    m_interpreter->setMinimumWidth(320);
    for (const MlEnvironment::Interpreter &i : MlEnvironment::interpreters())
        m_interpreter->addItem(QString("Python %1 · %2").arg(i.version, i.path), i.path);
    if (const int idx = m_interpreter->findData(m_env->interpreter()); idx >= 0)
        m_interpreter->setCurrentIndex(idx);
    envRow->addWidget(m_interpreter);
    m_create = segButton(tr("Create the environment"));
    m_check = segButton(tr("Check"));
    m_cancel = segButton(tr("Cancel"));
    envRow->addWidget(m_create);
    envRow->addWidget(m_check);
    envRow->addWidget(m_cancel);
    v->addLayout(envRow);
    m_versions = dimLabel();
    v->addWidget(m_versions);
    m_log = new QPlainTextEdit;
    m_log->setObjectName("aiLog");
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setFixedHeight(150);
    v->addWidget(m_log);

    // ---- Data
    v->addWidget(sectionLabel(tr("Data")));
    v->addWidget(dimLabel(tr("CSV files in %1: the country series of every data set (long form), the IMF's annual indicators, policy "
                             "rates, exchange rates, the COT report and ten years of daily prices of the symbols below (Yahoo).")
                              .arg(MlDataExporter::dir())));
    auto *dataRow = new QHBoxLayout;
    m_symbols = new QLineEdit(MlDataExporter::symbols().join(", "));
    m_symbols->setObjectName("keyEdit");
    m_symbols->setPlaceholderText(tr("Symbols for daily prices, comma-separated"));
    dataRow->addWidget(m_symbols, 1);
    m_export = segButton(tr("Export Deepwater's data"));
    dataRow->addWidget(m_export);
    v->addLayout(dataRow);
    m_filesNote = dimLabel();
    v->addWidget(m_filesNote);
    m_files = table({tr("File"), tr("Rows"), tr("Size"), tr("Written")});
    m_files->setColumnWidth(1, 90);
    m_files->setColumnWidth(2, 90);
    m_files->setColumnWidth(3, 150);
    m_files->setFixedHeight(190);
    v->addWidget(m_files);

    // ---- Training
    v->addWidget(sectionLabel(tr("Training")));
    auto *form = new QGridLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    int row = 0;
    auto field = [&](const QString &label, QWidget *w, int col) {
        auto *l = new QLabel(label);
        l->setObjectName("dim");
        form->addWidget(l, row, col * 2);
        form->addWidget(w, row, col * 2 + 1);
    };
    m_framework = combo();
    m_framework->addItems({"scikit-learn", "LightGBM", "XGBoost", "PyTorch"});
    m_model = combo();
    m_task = combo();
    m_task->addItem(tr("Regression"), "regression");
    m_task->addItem(tr("Classification"), "classification");
    field(tr("Framework"), m_framework, 0);
    field(tr("Model"), m_model, 1);
    field(tr("Task"), m_task, 2);
    ++row;
    m_dataset = combo();
    m_target = combo();
    m_derived = combo();
    m_derived->addItem(tr("The column itself"), "none");
    m_derived->addItem(tr("Next period's return, %"), "next_return");
    m_derived->addItem(tr("Next period up or down (1 / 0)"), "next_up");
    m_derived->addItem(tr("Next period's value"), "next_value");
    m_derived->addItem(tr("Next period's change"), "next_change");
    field(tr("Data set"), m_dataset, 0);
    field(tr("Target column"), m_target, 1);
    field(tr("Target"), m_derived, 2);
    ++row;
    m_lags = new QSpinBox;
    m_lags->setRange(0, 60);
    m_lags->setToolTip(tr("Lagged copies of every feature, for time series"));
    m_drop = new QLineEdit;
    m_drop->setObjectName("keyEdit");
    m_drop->setPlaceholderText(tr("Columns to leave out, comma-separated"));
    m_testSize = new QDoubleSpinBox;
    m_testSize->setRange(0.05, 0.5);
    m_testSize->setSingleStep(0.05);
    m_testSize->setValue(0.2);
    field(tr("Lags"), m_lags, 0);
    field(tr("Leave out"), m_drop, 1);
    field(tr("Test share"), m_testSize, 2);
    ++row;
    m_params = new QLineEdit;
    m_params->setObjectName("keyEdit");
    form->addWidget(dimLabel(tr("Hyper-parameters")), row, 0);
    form->addWidget(m_params, row, 1, 1, 5);
    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);
    form->setColumnStretch(5, 1);
    v->addLayout(form);
    auto *trainRow = new QHBoxLayout;
    m_train = new QPushButton(tr("Train"));
    m_train->setObjectName("primaryButton");
    m_train->setCursor(Qt::PointingHandCursor);
    m_stop = segButton(tr("Stop"));
    m_stop->setEnabled(false);
    m_progress = new QProgressBar;
    m_progress->setObjectName("qtProgress");
    m_progress->setFixedHeight(8);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    trainRow->addWidget(m_train);
    trainRow->addWidget(m_stop);
    trainRow->addWidget(m_progress, 1);
    v->addLayout(trainRow);
    m_result = new QLabel;
    m_result->setObjectName("sourceStatus");
    m_result->setWordWrap(true);
    v->addWidget(m_result);
    m_importances = table({tr("Feature"), tr("Importance")});
    m_importances->setColumnWidth(1, 110);
    m_importances->setFixedHeight(190);
    v->addWidget(m_importances);

    // ---- Runs
    v->addWidget(sectionLabel(tr("Runs")));
    v->addWidget(dimLabel(tr("Every training, with its model, in %1. Double-click a run to open its folder.").arg(MlEnvironment::runsDir())));
    m_runs = table({tr("Run"), tr("Framework"), tr("Data"), tr("Target"), tr("Metrics"), tr("Rows"), tr("Time")});
    m_runs->setColumnWidth(1, 90);
    m_runs->setColumnWidth(2, 170);
    m_runs->setColumnWidth(3, 150);
    m_runs->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_runs->setColumnWidth(5, 70);
    m_runs->setColumnWidth(6, 70);
    m_runs->setMinimumHeight(220);
    v->addWidget(m_runs, 1);
    auto *runButtons = new QHBoxLayout;
    m_openRun = segButton(tr("Open the run's folder"));
    runButtons->addWidget(m_openRun);
    runButtons->addStretch(1);
    v->addLayout(runButtons);

    // ---- Wiring
    connect(m_env, &MlEnvironment::log, this, &AiMlView::appendLog);
    connect(m_env, &MlEnvironment::stateChanged, this, &AiMlView::updateEnvironment);
    connect(m_env, &MlEnvironment::progress, this, [this](int done, int total) {
        if (total > 0) m_progress->setValue(int(1000.0 * done / total));
    });
    connect(m_env, &MlEnvironment::result, this, &AiMlView::showResult);
    connect(m_env, &MlEnvironment::finished, this, [this](bool) {
        if (m_training) {
            m_training = false;
            m_train->setEnabled(m_env->state() == MlEnvironment::State::Ready);
            m_stop->setEnabled(false);
            refreshRuns();
        }
        updateEnvironment();
    });
    connect(m_interpreter, &QComboBox::currentIndexChanged, this, [this] {
        m_env->setInterpreter(m_interpreter->currentData().toString());
        updateEnvironment();
    });
    connect(m_create, &QPushButton::clicked, m_env, &MlEnvironment::create);
    connect(m_check, &QPushButton::clicked, m_env, &MlEnvironment::check);
    connect(m_cancel, &QPushButton::clicked, m_env, &MlEnvironment::cancel);
    connect(m_data, &MlDataExporter::log, this, &AiMlView::appendLog);
    connect(m_data, &MlDataExporter::finished, this, [this] {
        m_export->setEnabled(true);
        refreshFiles();
    });
    connect(m_export, &QPushButton::clicked, this, &AiMlView::exportNow);
    connect(m_symbols, &QLineEdit::editingFinished, this, [this] {
        QStringList list;
        for (const QString &s : m_symbols->text().split(',', Qt::SkipEmptyParts))
            if (!s.trimmed().isEmpty()) list << s.trimmed().toUpper();
        MlDataExporter::setSymbols(list);
    });
    connect(m_framework, &QComboBox::currentIndexChanged, this, &AiMlView::updateFrameworkForm);
    connect(m_dataset, &QComboBox::currentIndexChanged, this, &AiMlView::refreshColumns);
    connect(m_train, &QPushButton::clicked, this, &AiMlView::train);
    connect(m_stop, &QPushButton::clicked, m_env, &MlEnvironment::cancel);
    connect(m_runs, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(MlEnvironment::runsDir() + '/' + m_runs->item(r, 0)->text()));
    });
    connect(m_openRun, &QPushButton::clicked, this, [this] {
        const int r = m_runs->currentRow();
        QDesktopServices::openUrl(QUrl::fromLocalFile(r >= 0 ? MlEnvironment::runsDir() + '/' + m_runs->item(r, 0)->text()
                                                              : MlEnvironment::runsDir()));
    });

    QSettings settings;
    m_framework->setCurrentIndex(settings.value("ml/framework", 1).toInt());
    updateFrameworkForm();
    updateEnvironment();
    refreshFiles();
    refreshRuns();
}

void AiMlView::showEvent(QShowEvent *e)
{
    updateEnvironment();
    QWidget::showEvent(e);
}

void AiMlView::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void AiMlView::updateEnvironment()
{
    const Palette &pal = Theme::current();
    QString text;
    QColor color = pal.TextDim;
    switch (m_env->state()) {
    case MlEnvironment::State::NoInterpreter:
        text = tr("No Python 3 on this computer");
        color = pal.Down;
        break;
    case MlEnvironment::State::NotCreated:
        text = tr("Environment not created yet");
        break;
    case MlEnvironment::State::Installing:
        text = tr("Installing…");
        color = pal.Accent;
        break;
    case MlEnvironment::State::Ready:
        text = tr("Ready");
        color = pal.Up;
        break;
    case MlEnvironment::State::Broken:
        text = tr("Incomplete: %1").arg(m_env->lastError());
        color = pal.Down;
        break;
    }
    m_envState->setText(QString::fromUtf8("● ") + text);
    m_envState->setStyleSheet(QString("color: %1;").arg(color.name()));
    const QJsonObject v = m_env->versions();
    if (!v.isEmpty()) {
        QStringList parts;
        for (const char *name : {"python", "scikit-learn", "lightgbm", "xgboost", "torch", "numpy", "pandas"})
            if (!v.value(QLatin1String(name)).isNull())
                parts << QString("%1 %2").arg(name, v.value(QLatin1String(name)).toString());
        if (v.value("torch_mps").toBool()) parts << tr("PyTorch on the Apple GPU (MPS)");
        m_versions->setText(parts.join(" · ") + "\n" + MlEnvironment::venvDir());
    } else {
        m_versions->setText(MlEnvironment::venvDir());
    }
    const bool busy = m_env->isBusy();
    m_create->setText(m_env->state() == MlEnvironment::State::Ready || m_env->state() == MlEnvironment::State::Broken
                          ? tr("Update the packages") : tr("Create the environment"));
    m_create->setEnabled(!busy && m_env->state() != MlEnvironment::State::NoInterpreter);
    m_check->setEnabled(!busy && m_env->state() != MlEnvironment::State::NotCreated && m_env->state() != MlEnvironment::State::NoInterpreter);
    m_cancel->setEnabled(busy);
    m_train->setEnabled(!busy && m_env->state() == MlEnvironment::State::Ready && m_dataset->count() > 0);
}

void AiMlView::refreshFiles()
{
    const QVector<MlDataExporter::File> files = MlDataExporter::files();
    const QLocale loc;
    m_files->setRowCount(files.size());
    const QString current = m_dataset->currentText();
    m_dataset->blockSignals(true);
    m_dataset->clear();
    qint64 total = 0;
    for (int i = 0; i < files.size(); ++i) {
        const MlDataExporter::File &f = files[i];
        total += f.bytes;
        m_files->setItem(i, 0, cell(f.name));
        m_files->setItem(i, 1, cell(loc.toString(f.rows), Qt::AlignRight));
        m_files->setItem(i, 2, cell(LocalModels::formatBytes(f.bytes), Qt::AlignRight));
        m_files->setItem(i, 3, cell(loc.toString(f.written, QLocale::ShortFormat)));
        m_dataset->addItem(f.name);
    }
    if (const int idx = m_dataset->findText(current); idx >= 0)
        m_dataset->setCurrentIndex(idx);
    m_dataset->blockSignals(false);
    m_filesNote->setText(files.isEmpty() ? tr("No export yet.") : tr("%n file(s), %1.", nullptr, files.size()).arg(LocalModels::formatBytes(total)));
    refreshColumns();
    updateEnvironment();
}

void AiMlView::refreshColumns()
{
    const QString current = m_target->currentText();
    m_target->clear();
    if (m_dataset->currentIndex() < 0)
        return;
    const QStringList cols = MlDataExporter::columns(m_dataset->currentText());
    m_target->addItems(cols);
    int idx = m_target->findText(current);
    if (idx < 0) idx = m_target->findText("close");
    if (idx < 0) idx = m_target->findText("value");
    if (idx < 0) idx = m_target->count() - 1;
    m_target->setCurrentIndex(std::max(0, idx));
    // Prices: the natural setting is tomorrow's return with a few lags; the other files: the column itself.
    const bool prices = m_dataset->currentText().startsWith("prices_");
    m_derived->setCurrentIndex(prices ? 1 : 0);
    m_lags->setValue(prices ? 5 : 0);
    m_drop->setText(prices ? "open,high,low" : QString());
}

void AiMlView::updateFrameworkForm()
{
    const QString fw = frameworkId(m_framework->currentIndex());
    m_model->clear();
    if (fw == "sklearn") {
        m_model->addItem(tr("Random forest"), "rf");
        m_model->addItem(tr("Gradient boosting"), "gb");
        m_model->addItem(tr("Linear (ridge / logistic)"), "linear");
    } else if (fw == "torch") {
        m_model->addItem(tr("Multi-layer perceptron"), "mlp");
    } else {
        m_model->addItem(tr("Gradient-boosted trees"), "");
    }
    m_params->setText(defaultParams(fw));
    QSettings().setValue("ml/framework", m_framework->currentIndex());
}

void AiMlView::exportNow()
{
    if (m_data->isBusy())
        return;
    m_export->setEnabled(false);
    m_data->exportAll();
}

void AiMlView::trainNow(const QString &file, const QString &target)
{
    refreshFiles();
    if (const int i = m_dataset->findText(file); i >= 0) m_dataset->setCurrentIndex(i);
    refreshColumns();
    if (const int i = m_target->findText(target); i >= 0) m_target->setCurrentIndex(i);
    if (m_env->isBusy()) {
        // The environment check that runs at launch: train once it is done.
        connect(m_env, &MlEnvironment::finished, this, [this](bool) { train(); }, Qt::SingleShotConnection);
        return;
    }
    train();
}

void AiMlView::train()
{
    if (m_training || m_env->state() != MlEnvironment::State::Ready || m_dataset->currentIndex() < 0 || m_target->currentIndex() < 0)
        return;
    const QString fw = frameworkId(m_framework->currentIndex());
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    m_runDir = MlEnvironment::runsDir() + '/' + stamp + '-' + fw;
    QStringList args {"--framework", fw, "--task", m_task->currentData().toString(),
                      "--data", MlDataExporter::dir() + '/' + m_dataset->currentText(), "--target", m_target->currentText(),
                      "--make-target", m_derived->currentData().toString(), "--lags", QString::number(m_lags->value()),
                      "--test-size", QString::number(m_testSize->value()), "--params", m_params->text().trimmed().isEmpty() ? "{}" : m_params->text().trimmed(),
                      "--out", m_runDir};
    if (!m_model->currentData().toString().isEmpty())
        args << "--model" << m_model->currentData().toString();
    if (!m_drop->text().trimmed().isEmpty())
        args << "--drop" << m_drop->text().trimmed().remove(' ');
    m_training = true;
    m_progress->setValue(0);
    m_result->clear();
    m_importances->setRowCount(0);
    m_train->setEnabled(false);
    m_stop->setEnabled(true);
    appendLog(tr("Training %1 on %2 → %3").arg(fw, m_dataset->currentText(), m_target->currentText()));
    if (!m_env->run("train.py", args, m_runDir)) {
        m_training = false;
        m_train->setEnabled(true);
        m_stop->setEnabled(false);
        appendLog(tr("Couldn't start the training"));
    }
}

void AiMlView::showResult(const QJsonObject &r)
{
    if (!m_training)
        return;   // the environment check's own result
    const Palette &pal = Theme::current();
    if (r.contains("error")) {
        m_result->setText(tr("Failed: %1").arg(r.value("error").toString()));
        m_result->setStyleSheet(QString("color: %1;").arg(pal.Down.name()));
        return;
    }
    m_progress->setValue(1000);
    const QJsonObject m = r.value("metrics").toObject();
    m_result->setText(tr("%1 · %2 rows, %3 features · %4 s · %5")
                          .arg(metricsText(m)).arg(r.value("rows").toInt()).arg(r.value("features").toInt())
                          .arg(QLocale().toString(r.value("seconds").toDouble(), 'f', 1), r.value("model_file").toString()));
    m_result->setStyleSheet(QString("color: %1;").arg(pal.Up.name()));
    // Importances from run.json.
    QFile f(m_runDir + "/run.json");
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray imp = QJsonDocument::fromJson(f.readAll()).object().value("importances").toArray();
        const int n = std::min(15, int(imp.size()));
        m_importances->setRowCount(n);
        for (int i = 0; i < n; ++i) {
            const QJsonObject o = imp[i].toObject();
            m_importances->setItem(i, 0, cell(o.value("feature").toString()));
            m_importances->setItem(i, 1, cell(QLocale().toString(100 * o.value("importance").toDouble(), 'f', 1) + " %", Qt::AlignRight));
        }
    }
}

void AiMlView::refreshRuns()
{
    const QDir dir(MlEnvironment::runsDir());
    const QFileInfoList runs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    const QLocale loc;
    m_runs->setRowCount(0);
    for (const QFileInfo &fi : runs) {
        QFile f(fi.filePath() + "/run.json");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject r = QJsonDocument::fromJson(f.readAll()).object();
        const int row = m_runs->rowCount();
        m_runs->insertRow(row);
        m_runs->setItem(row, 0, cell(fi.fileName()));
        m_runs->setItem(row, 1, cell(r.value("framework").toString() + (r.value("model").toString().isEmpty() ? QString() : " · " + r.value("model").toString())));
        m_runs->setItem(row, 2, cell(r.value("data").toString()));
        m_runs->setItem(row, 3, cell(r.value("target").toString()));
        m_runs->setItem(row, 4, cell(metricsText(r.value("metrics").toObject())));
        m_runs->setItem(row, 5, cell(loc.toString(r.value("rows").toInt()), Qt::AlignRight));
        m_runs->setItem(row, 6, cell(loc.toString(r.value("seconds").toDouble(), 'f', 1) + " s", Qt::AlignRight));
        m_runs->item(row, 0)->setToolTip(QJsonDocument(r.value("params").toObject()).toJson(QJsonDocument::Compact));
    }
}
