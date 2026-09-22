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
    Reply reply;

    if (socket_.state() != QTcpSocket::ConnectedState) {
        reply.error = QStringLiteral("no connection");
        return reply;
    }
    if (command.contains(u'\n') || command.contains(u'\r')) {
        reply.error = QStringLiteral("command must not contain line breaks");
        return reply;
    }

    auto fail = [this](Reply r) {
        socket_.abort();
        return r;
    };

    QElapsedTimer timer;
    timer.start();

    socket_.write(command.toUtf8() + '\n');
    if (!socket_.waitForBytesWritten(timeout_ms_)) {
        reply.error = socket_.state() == QTcpSocket::ConnectedState
                          ? QStringLiteral("timeout while sending")
                          : socket_.errorString();
        return fail(reply);
    }

    if (!readLine(reply.header, &reply.error)) {
        return fail(reply);
    }

    const QStringList words = reply.header.split(u' ', Qt::SkipEmptyParts);
    const bool multiline = words.size() >= 2 && (words[0] == QStringLiteral("ITEMS") ||
                                                 words[0] == QStringLiteral("FIELDS"));

    if (multiline) {
        bool parsed = false;
        const int n = words[1].toInt(&parsed);
        if (!parsed || n < 0) {
            reply.error = QStringLiteral("malformed header: ") + reply.header;
            return fail(reply);
        }

        reply.items.reserve(n);
        for (int i = 0; i < n; ++i) {
            QString line;
            if (!readLine(line, &reply.error)) {
                return fail(reply);
            }
            reply.items.append(line);
        }
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
