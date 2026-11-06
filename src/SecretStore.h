#pragma once

#include <QString>

// A secret in the system's own store rather than in the settings: the Keychain on macOS, the Credential Manager on Windows.
// Used for what can place real orders (the MetaTrader page's key). No store: writing fails, and nothing is kept.
namespace SecretStore {

bool write(const QString &name, const QString &value, QString *error = nullptr);
QString read(const QString &name);   // empty when there is none
bool remove(const QString &name);

// --secret-check, for the CI: a throwaway secret written, read back and removed. 0 when all went well.
int check();

} // namespace SecretStore
