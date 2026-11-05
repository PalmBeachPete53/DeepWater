#pragma once

#include <QWidget>

#include <functional>

class QCheckBox;
class QLabel;
class QPushButton;
class QProcess;
class McpServer;

// AI › Models › Integration › opencode: whether opencode (opencode.ai) is on this computer, Deepwater's tools served
// to it over MCP (McpServer: on/off, address; its skills are those of the slot it is dragged into), and its registration
// in opencode's global configuration, written by opencode itself (`opencode mcp add deepwater --global --url … --header …`).
class AiOpencodeView : public QWidget
{
    Q_OBJECT
public:
    explicit AiOpencodeView(class ModelSlotsView *slotsView, QWidget *parent = nullptr);

    // Where opencode is: PATH, then the installers' usual places (an app opened from the Finder has a short PATH).
    static QString executable();
    // opencode's global configuration file ($XDG_CONFIG_HOME or ~/.config, opencode/opencode.json[c]).
    static QString configFile();
    enum class Registration { None, Current, Stale, Unreadable };
    // Whether that file has Deepwater, at this address with this token.
    static Registration registration(const QString &url, const QString &token);

protected:
    void showEvent(QShowEvent *) override;

private:
    void detect();
    void updateState();
    void addToOpencode();
    void copyConfiguration();
    void run(const QStringList &args, const std::function<void(int code, const QString &output)> &done);

    McpServer *m_server;
    QString m_exe;
    QString m_version;
    QLabel *m_found;
    QCheckBox *m_serve;
    QLabel *m_serverState;
    QLabel *m_registered;
    QPushButton *m_add;
    QPushButton *m_copy;
    QPushButton *m_newToken;
    QLabel *m_calls;
    QProcess *m_process = nullptr;
};
