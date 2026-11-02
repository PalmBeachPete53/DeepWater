#pragma once

// The JS code throws `new Error(message)`, sometimes with `status` (HTTP code) or `upstream`
// (a data provider failed). C++ code throws qtm::Error with the same message text.

#include <QString>

#include <stdexcept>

namespace qtm {

class Error : public std::runtime_error
{
public:
    explicit Error(const QString &message, int status = 0, bool upstream = false)
        : std::runtime_error(message.toStdString())
        , m_message(message)
        , m_status(status)
        , m_upstream(upstream)
    {
    }
    QString message() const { return m_message; }
    int status() const { return m_status; }          // 0 = none
    bool upstream() const { return m_upstream; }

private:
    QString m_message;
    int m_status;
    bool m_upstream;
};

} // namespace qtm
