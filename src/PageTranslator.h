#pragma once

#include <QObject>
#include <QStringList>

#include <functional>

// Translates texts on this Mac through the deepwater-translate helper (src/translate-helper.swift, Apple's Translation
// framework). Not there elsewhere than on macOS: the reader then falls back on the local model for the headline.
namespace PageTranslator {

enum class Failure { None, SameLanguage, NotInstalled, Unsupported, SystemTooOld, Failed };
struct Result {
    Failure failure = Failure::None;
    QString source;       // the language found, "de"
    QStringList texts;    // in the order asked
};
bool available();
void translate(const QStringList &texts, const QString &target, QObject *ctx, std::function<void(const Result &)> done);

} // namespace PageTranslator
