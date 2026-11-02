// Port of panels.js INDICATOR EDITOR (lines 1506-1606).

#include "PanelsPrivate.h"

#include "Api.h"
#include "Theme.h"
#include "../core/CustomIndicators.h"
#include "../server/Server.h"

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>

namespace qtm {

using namespace qtm::kit;
using theme::esc;

namespace {

const char *const TEMPLATE = R"JS(export default {
  name: 'EMA ribbon squeeze',
  overlay: true, // true = drawn on the price chart, false = its own pane
  inputs: [
    { key: 'fast', label: 'Fast EMA', type: 'int', default: 8 },
    { key: 'slow', label: 'Slow EMA', type: 'int', default: 34 },
  ],
  outputs: [
    { key: 'fast', label: 'Fast', kind: 'line', color: '#26a69a' },
    { key: 'slow', label: 'Slow', kind: 'line', color: '#ef5350' },
  ],
  // bars: [{ time, open, high, low, close, volume }], p: your inputs,
  // ta: sma, ema, rma, wma, hma, rsi, atr, stdev, highest, lowest, crossover, crossunder, source, compute(...)
  calc(bars, p, ta) {
    const close = ta.source(bars, 'close');
    const fast = ta.ema(close, p.fast), slow = ta.ema(close, p.slow);
    const up = ta.crossover(fast, slow);
    return {
      fast,
      slow,
      markers: bars.filter((b, i) => up[i]).map((b) => ({ time: b.time, position: 'belowBar', shape: 'arrowUp', color: '#26a69a', text: 'Cross' })),
    };
  },
};
)JS";

// <textarea class="in" id="edCode" spellcheck="false">: Tab inserts two spaces.
class CodeEdit : public CodeArea
{
public:
    CodeEdit() : CodeArea(true) { setMinimumHeight(220); }
};

// .card .sub with white-space: pre-line (the .card .sub colour wins over .down).
Text *preLineSub(const QString &msg)
{
    TextStyle st;
    st.font = uiFont(11);
    st.color = theme::color("--muted");
    auto *t = new Text(esc(msg).replace("\n", "<br>"), st);
    t->setContentsMargins(0, 2, 0, 0);
    return t;
}

} // namespace

QWidget *Panels::Private::edListWidget(const QJsonArray &list)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    for (const auto &cv : list) {
        const QJsonObject c = cv.toObject();
        const QString file = c.value("file").toString();
        Box *cardBox = card();
        auto *top = new QWidget;
        auto *tl = new QHBoxLayout(top);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->setSpacing(6);
        tl->addWidget(text(esc(file)), 1, Qt::AlignTop);
        auto *acts = new QWidget;
        auto *al = new QHBoxLayout(acts);
        al->setContentsMargins(0, 0, 0, 0);
        al->setSpacing(8);
        auto *bOpen = btn("Edit", "btn small");
        auto *bAdd = btn("Add to chart", "btn small");
        auto *bDel = btn("Delete", "btn small danger");
        al->addWidget(bOpen);
        al->addWidget(bAdd);
        al->addWidget(bDel);
        tl->addWidget(acts, 0, Qt::AlignTop);
        cardBox->layout()->addWidget(top);
        const QString err = c.value("error").toString();
        if (!err.isEmpty()) cardBox->layout()->addWidget(preLineSub(err));
        auto *wrap = new QWidget;
        auto *wl = new QHBoxLayout(wrap);
        wl->setContentsMargins(8, 0, 8, 0);
        wl->addWidget(cardBox);
        l->addSpacing(8);
        l->addWidget(wrap);

        QObject::connect(bOpen, &QPushButton::clicked, w, [this, file] {
            Backend *b = Backend::instance();
            QString code;
            if (b) {
                QFile f(QDir(b->customDir()).filePath(file));
                if (f.open(QIODevice::ReadOnly)) code = QString::fromUtf8(f.readAll());
            }
            if (edCode) static_cast<CodeEdit *>(edCode.data())->setCode(code);
            QString name = customIndicatorBaseName(file);
            if (edName) edName->setText(name.replace('-', ' '));
            edFile = file;
        });
        QObject::connect(bAdd, &QPushButton::clicked, w, [this, file] {
            const IndicatorDefPtr def = app->importCustom(file);
            if (def && app->active) app->active->addIndicator(def->id);
        });
        QObject::connect(bDel, &QPushButton::clicked, w, [this, file] {
            if (!kit::confirm(app, QString("Delete %1?").arg(file))) return;
            api::del(q, "/api/custom/" + QString::fromLatin1(QUrl::toPercentEncoding(file)),
                     [this](const QJsonValue &) { renderEditor(); }, [this](const QString &) { renderEditor(); });
        });
    }
    if (list.isEmpty()) {
        Text *none = hint("None yet.");
        none->setContentsMargins(0, 12, 0, 0);
        l->addWidget(none);
    }
    return w;
}

void Panels::Private::renderEditor()
{
    auto build = [this](const QJsonArray &list) {
        if (bottomTab != "editor" || collapsed) return;
        auto *split = new Split;
        QWidget *left = split->left->body();
        auto *row = rowFlex();
        auto *edNew = btn("New indicator", "btn primary");
        row->layout()->addWidget(edNew);
        add(left, row);
        add(left, h4("Your indicators"), 10, 0);
        auto *listBox = new QWidget;
        auto *ll = new QVBoxLayout(listBox);
        ll->setContentsMargins(0, 0, 0, 0);
        ll->addWidget(edListWidget(list));
        add(left, listBox);
        add(left,
            hint("Each indicator is a small JavaScript file in the app's <code>custom</code> folder, in the format in "
                 "<code>docs/custom-indicators.md</code> (start from <code>docs/indicator-template.js</code>), or a "
                 "Pine Script (<code>//@version=6</code>, <code>indicator(...)</code>, <code>plot(...)</code>) run by "
                 "PineTS with Node.js. Each one is checked before it's saved. You can also ask your AI agent: \"add an "
                 "indicator to Quant Terminal that…\" and it will write the file for you."),
            12, 12);

        // right: name row, code, error (flex column, gap 10)
        auto *rl = split->right->layout();
        rl->setSpacing(10);
        split->right->setFill(true);
        auto *nameRow = rowFlex();
        auto *name = new Input(Input::In);
        name->setPlaceholderText("Indicator name");
        name->setProperty("flexGrow", 1);
        name->setMinimumWidth(0);
        auto *edSave = btn("Save", "btn");
        auto *edSaveAdd = btn("Save and add to chart", "btn primary");
        nameRow->layout()->addWidget(name);
        nameRow->layout()->addWidget(edSave);
        nameRow->layout()->addWidget(edSaveAdd);
        rl->addWidget(nameRow);
        auto *code = new CodeEdit;
        rl->addWidget(code, 1);
        TextStyle st;
        st.color = theme::color("--down");
        auto *err = new Text(QString(), st);
        err->setMaximumHeight(0);
        rl->addWidget(err);

        edList = listBox;
        edCode = code;
        edName = name;
        bottom->setContent(split, true);
        code->setCode(QString::fromUtf8(TEMPLATE));
        name->setText("EMA ribbon squeeze");

        QPointer<Text> errP(err);
        auto setErr = [errP](const QString &msg) {
            if (!errP) return;
            errP->setHtml(esc(msg).replace("\n", "<br>"));
            errP->setMaximumHeight(msg.isEmpty() ? 0 : QWIDGETSIZE_MAX);
        };
        QObject::connect(edNew, &QPushButton::clicked, split, [this] {
            edFile.clear();
            if (edCode)
                static_cast<CodeEdit *>(edCode.data())
                    ->setCode(QString::fromUtf8(TEMPLATE).replace("'EMA ribbon squeeze'", "'My indicator'"));
            if (edName) edName->setText("My indicator");
        });
        auto save = [this, setErr, split](bool addIt) {
            setErr(QString());
            QString nm = edName ? edName->text().trimmed() : QString();
            if (nm.isEmpty()) nm = "indicator";
            const QString src = edCode ? edCode->toPlainText() : QString();
            api::post(split, "/api/custom", QJsonObject {{"name", nm}, {"code", src}},
                      [this, setErr, addIt](const QJsonValue &v) {
                          const QString file = v.toObject().value("file").toString();
                          const IndicatorDefPtr def = app->importCustom(file);
                          if (!def) return setErr("Saved, but the browser could not load it. Check the code.");
                          app->toast("Saved " + esc(file));
                          if (addIt) {
                              if (!app->active)
                                  return setErr("Cannot read properties of null (reading 'addIndicator')");
                              app->active->addIndicator(def->id);
                          } else {
                              for (ChartPane *p : app->panes) p->render(false);
                          }
                          edFile = file;
                          renderEditorList();
                      },
                      [setErr](const QString &m) { setErr(m); });
        };
        QObject::connect(edSave, &QPushButton::clicked, split, [save] { save(false); });
        QObject::connect(edSaveAdd, &QPushButton::clicked, split, [save] { save(true); });
    };
    api::get(q, "/api/custom", [build](const QJsonValue &v) { build(v.toArray()); },
             [build](const QString &) { build(QJsonArray()); });
}

void Panels::Private::renderEditorList()
{
    QPointer<QWidget> box = edList;
    if (!box) return;
    auto put = [this, box](const QJsonArray &list) {
        if (!box) return;
        clear(box->layout());
        box->layout()->addWidget(edListWidget(list));
    };
    api::get(box, "/api/custom", [put](const QJsonValue &v) { put(v.toArray()); },
             [put](const QString &) { put(QJsonArray()); });
}

} // namespace qtm
