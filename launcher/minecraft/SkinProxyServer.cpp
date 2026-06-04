#include "SkinProxyServer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUrl>

SkinProxyServer::SkinProxyServer(QObject* parent) : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &SkinProxyServer::onNewConnection);
}

SkinProxyServer::~SkinProxyServer()
{
    stop();
}

bool SkinProxyServer::start()
{
    if (m_server->isListening())
        return true;

    // Listen on a random available port on localhost
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        emit error(tr("Failed to start skin proxy server: %1").arg(m_server->errorString()));
        return false;
    }

    qDebug() << "SkinProxyServer: listening on port" << m_server->serverPort();
    return true;
}

void SkinProxyServer::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        qDebug() << "SkinProxyServer: stopped";
    }
}

bool SkinProxyServer::isRunning() const
{
    return m_server->isListening();
}

int SkinProxyServer::port() const
{
    return m_server->serverPort();
}

void SkinProxyServer::setProfileData(const QByteArray& profileJson)
{
    m_profileJson = profileJson;
}

void SkinProxyServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        auto socket = m_server->nextPendingConnection();
        handleRequest(socket);
    }
}

void SkinProxyServer::handleRequest(QTcpSocket* socket)
{
    // Read the HTTP request
    QByteArray request;
    while (socket->canReadLine()) {
        QByteArray line = socket->readLine();
        request.append(line);
        if (line.trimmed().isEmpty())
            break;  // End of headers
    }

    // Parse the request line: "GET /path HTTP/1.1"
    QByteArray method;
    QByteArray path;
    auto lines = request.split('\n');
    if (!lines.isEmpty()) {
        auto requestLine = lines[0].trimmed().split(' ');
        if (requestLine.size() >= 2) {
            method = requestLine[0];
            path = requestLine[1];
        }
    }

    qDebug() << "SkinProxyServer:" << method << path;

    // We only handle GET requests to /session/minecraft/profile/<uuid>
    if (method == "GET" && path.startsWith("/session/minecraft/profile/")) {
        if (m_profileJson.isEmpty()) {
            // No profile data available, return 404
            QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 2\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{}";
            socket->write(response);
        } else {
            // Return the cached profile data
            QByteArray body = m_profileJson;
            QByteArray response = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                 "Connection: close\r\n"
                                 "\r\n" +
                                 body;
            socket->write(response);
        }
    } else {
        // For any other request, return 404
        QByteArray response = "HTTP/1.1 404 Not Found\r\n"
                             "Content-Type: text/plain\r\n"
                             "Content-Length: 9\r\n"
                             "Connection: close\r\n"
                             "\r\n"
                             "Not Found";
        socket->write(response);
    }

    socket->flush();
    socket->disconnectFromHost();
}