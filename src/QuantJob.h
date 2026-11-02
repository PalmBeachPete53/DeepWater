#pragma once

// Long estimations of the Quant page off the UI thread. JobRunner starts one job at a time on the
// global thread pool and hands its typed result back on the UI thread, with the engine's progress
// reports (markov::Progress) relayed as a signal; starting a new job cancels the one running, whose
// result is then dropped. JobBar is the strip the views show while a job runs: progress, text and a
// Cancel button.

#include "markov/Job.h"

#include <QCoreApplication>
#include <QFrame>
#include <QFutureWatcher>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

#include <atomic>
#include <functional>
#include <memory>

class QLabel;
class QProgressBar;
class QPushButton;

namespace quant {

class JobRunner : public QObject
{
    Q_OBJECT
public:
    explicit JobRunner(QObject *parent = nullptr) : QObject(parent) {}
    ~JobRunner() override { cancel(); }

    template <typename T>
    void start(std::function<T(const markov::Progress &)> work, std::function<void(const T &)> done)
    {
        cancel();
        auto flag = std::make_shared<std::atomic<bool>>(false);
        m_cancel = flag;
        const int gen = ++m_generation;
        QPointer<JobRunner> self(this);
        markov::Progress progress;
        progress.cancel = flag.get();
        progress.report = [self, gen](int percent, const QString &text) {
            // From the worker thread to the UI thread, dropped if the runner is gone or has moved on.
            QMetaObject::invokeMethod(QCoreApplication::instance(), [self, gen, percent, text] {
                if (self && self->m_generation == gen)
                    emit self->progress(percent, text);
            }, Qt::QueuedConnection);
        };
        auto *watcher = new QFutureWatcher<T>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, gen, done, flag] {
            watcher->deleteLater();
            if (gen != m_generation)
                return;
            m_running = false;
            emit finished();
            if (!flag->load())
                done(watcher->result());
        });
        m_running = true;
        emit started();
        watcher->setFuture(QtConcurrent::run([work, flag, progress] { return work(progress); }));
    }

    void cancel()
    {
        if (m_cancel)
            m_cancel->store(true);
        if (m_running) {
            m_running = false;
            ++m_generation;   // the running job's result is dropped
            emit finished();
        }
    }
    bool isRunning() const { return m_running; }

signals:
    void started();
    void progress(int percent, const QString &text);
    void finished();

private:
    std::shared_ptr<std::atomic<bool>> m_cancel;
    int m_generation = 0;
    bool m_running = false;
};

class JobBar : public QFrame
{
    Q_OBJECT
public:
    explicit JobBar(JobRunner *runner, QWidget *parent = nullptr);

private:
    QProgressBar *m_bar;
    QLabel *m_text;
    QPushButton *m_cancel;
};

} // namespace quant
