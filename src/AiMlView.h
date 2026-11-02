#pragma once

#include <QJsonObject>
#include <QWidget>

#include "MlData.h"
#include "MlEnvironment.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;

// The Machine learning section of the AI tab: the app's Python environment (scikit-learn, LightGBM, XGBoost,
// PyTorch), the export of Deepwater's data to CSV, a training form (framework, task, data set, target, derived
// target and lags for time series, hyper-parameters), the live log, the metrics and feature importances of the
// run, and the past runs with their models.
class AiMlView : public QWidget
{
    Q_OBJECT
public:
    explicit AiMlView(const MlDataExporter::Sources &sources, QWidget *parent = nullptr);

    // Dev flags: --ml-export, --ml-train <csv> <target>.
    void exportNow();
    void trainNow(const QString &file, const QString &target);

protected:
    void showEvent(QShowEvent *) override;

private:
    void updateEnvironment();
    void refreshFiles();
    void refreshColumns();
    void refreshRuns();
    void updateFrameworkForm();
    void train();
    void showResult(const QJsonObject &result);
    void appendLog(const QString &line);
    QString runDir() const { return m_runDir; }

    MlEnvironment *m_env;
    MlDataExporter *m_data;
    // Environment
    QLabel *m_envState;
    QComboBox *m_interpreter;
    QPushButton *m_create;
    QPushButton *m_check;
    QPushButton *m_cancel;
    QLabel *m_versions;
    QPlainTextEdit *m_log;
    // Data
    QLineEdit *m_symbols;
    QPushButton *m_export;
    QTableWidget *m_files;
    QLabel *m_filesNote;
    // Training
    QComboBox *m_framework;
    QComboBox *m_model;
    QComboBox *m_task;
    QComboBox *m_dataset;
    QComboBox *m_target;
    QComboBox *m_derived;
    QSpinBox *m_lags;
    QLineEdit *m_drop;
    QDoubleSpinBox *m_testSize;
    QLineEdit *m_params;
    QPushButton *m_train;
    QPushButton *m_stop;
    QProgressBar *m_progress;
    QLabel *m_result;
    QTableWidget *m_importances;
    QTableWidget *m_runs;
    QPushButton *m_openRun;
    QString m_runDir;
    bool m_training = false;
};
