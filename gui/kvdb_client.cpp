#include "kvdb_client.hpp"

#include <QElapsedTimer>

KvdbClient::KvdbClient(QObject* parent) : QObject(parent), socket_(this) {
    connect(&socket_, &QAbstractSocket::disconnected, this,
            [this] { emit connectionLost(tr("connection closed")); });
}

bool KvdbClient::connectToServer(const QString& host, quint16 port, QString* error) {
    if (socket_.state() != QTcpSocket::UnconnectedState)
        socket_.disconnectFromHost();

    socket_.connectToHost(host, port);
    auto conection_state = socket_.waitForConnected(timeout_ms_);

    if (conection_state == false) {
        if (error != nullptr)
            *error = socket_.errorString();
        socket_.abort();
        return false;
    }

    socket_.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    return true;
}

void KvdbClient::disconnectFromServer() {
    QSignalBlocker blocker(socket_);
    socket_.abort();
}

bool KvdbClient::isConnected() const {
    return socket_.state() == QTcpSocket::ConnectedState;
}

Reply KvdbClient::exec(const QString& command) {
    if (socket_.state() != QTcpSocket::ConnectedState)
        return Reply{.error = "no connection"};
    if (command.contains("\n") || command.contains("\r"))
        return Reply{.error = "command contains terminal symbols. aborting"};

    QElapsedTimer timer;
    timer.start();

    socket_.write(command.toUtf8() + '\n');
    auto state = socket_.waitForBytesWritten(timeout_ms_);

    if (!state) {
        if (socket_.state() != QTcpSocket::ConnectedState)
            return Reply{.error = socket_.errorString()};
        else
            return Reply{.error = "timeout for execution"};
    }

    Reply reply;
    if (!readLine(reply.header, &reply.error))
        return Reply{.error = "readLine internal error"};

    const QStringList words = reply.header.split(u' ', Qt::SkipEmptyParts);
    bool parsed = false;
    const int n = words[1].toInt(&parsed);

    if (n < 0)
        return Reply{.error = "reply has < 0 lines. internal logic error"};

    for (int i = 0; i < n; ++i) {
        if (!readLine(reply.items[i], &reply.error)) return reply;
    }
    reply.rtt_micros = timer.nsecsElapsed() / 1000;
    reply.ok = true;

    return reply;
}

bool KvdbClient::readLine(QString& out, QString* error) {
    while (!socket_.canReadLine()) {
        if (!socket_.waitForReadyRead(timeout_ms_)) {
            if (error && socket_.state() == QTcpSocket::ConnectedState)
                *error = "timeout";
            else if (error)
                *error = socket_.errorString();
            return false;
        }
    }

    auto line = socket_.readLine();
    while (line.endsWith('\n') || line.endsWith("\r"))
        line.chop(1);

    out = QString::fromUtf8(line);
    return true;
}
