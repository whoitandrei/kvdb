#pragma once

#include <QObject>
#include <QString>
#include <QTcpSocket>

struct Reply {
    bool ok = false;
    QString error = "";
    QString header = "";
    QStringList items{};
    qint64 rtt_micros = 0;

    bool isServerError() const;
};

class KvdbClient : public QObject {
    Q_OBJECT
  public:
    explicit KvdbClient(QObject* parent = nullptr);

    bool connectToServer(const QString& host, quint16 port, QString* error = nullptr);
    void disconnectFromServer();
    bool isConnected() const;

    Reply exec(const QString& command);

  signals:
    void connectionLost(const QString& reason);

  private:
    bool readLine(QString& out, QString* error);

    QTcpSocket socket_;
    int timeout_ms_ = 3000;
};