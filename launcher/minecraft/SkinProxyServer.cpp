#include "SkinProxyServer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

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

void SkinProxyServer::cacheSkin(const QString& username, const QByteArray& skinData)
{
    m_skinCache[username] = skinData;
    qDebug() << "SkinProxyServer: cached skin for" << username << "(" << skinData.size() << "bytes)";
}

bool SkinProxyServer::hasSkin(const QString& username) const
{
    return m_skinCache.contains(username);
}

void SkinProxyServer::clearSkinCache()
{
    m_skinCache.clear();
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

    QByteArray response;
    if (method == "GET") {
        if (path.startsWith("/session/minecraft/profile/")) {
            response = handleProfileRequest(path);
        } else if (path.startsWith("/skin/")) {
            response = handleSkinRequest(path);
        } else {
            // For any other request, return 404
            response = "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 9\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "Not Found";
        }
    } else {
        // Only handle GET requests
        response = "HTTP/1.1 405 Method Not Allowed\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: 18\r\n"
                   "Connection: close\r\n"
                   "\r\n"
                   "Method Not Allowed";
    }

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

QByteArray SkinProxyServer::handleProfileRequest(const QString& path)
{
    if (m_profileJson.isEmpty()) {
        // No profile data available, return 404
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: 2\r\n"
               "Connection: close\r\n"
               "\r\n"
               "{}";
    }

    // Parse the profile JSON and rewrite skin URLs to point to our local proxy
    QJsonDocument doc = QJsonDocument::fromJson(m_profileJson);
    if (doc.isObject()) {
        QJsonObject root = doc.object();
        
        // Check if there's a "properties" array with textures
        if (root.contains("properties") && root["properties"].isArray()) {
            QJsonArray properties = root["properties"].toArray();
            bool modified = false;
            
            for (int i = 0; i < properties.size(); ++i) {
                QJsonObject prop = properties[i].toObject();
                if (prop["name"].toString() == "textures" && prop.contains("value")) {
                    // Parse the textures data (base64 encoded JSON)
                    QByteArray texturesData = QByteArray::fromBase64(prop["value"].toString().toUtf8());
                    QJsonDocument texturesDoc = QJsonDocument::fromJson(texturesData);
                    
                    if (texturesDoc.isObject()) {
                        QJsonObject texturesObj = texturesDoc.object();
                        if (texturesObj.contains("textures") && texturesObj["textures"].isObject()) {
                            QJsonObject textures = texturesObj["textures"].toObject();
                            
                            // Rewrite skin URL
                            if (textures.contains("SKIN")) {
                                QJsonObject skinObj = textures["SKIN"].toObject();
                                QString skinUrl = skinObj["url"].toString();
                                
                                // Check if it's a TLauncher URL
                                if (skinUrl.contains("tlauncher.org")) {
                                    // Extract username from URL or use a placeholder
                                    QUrl url(skinUrl);
                                    QString username = url.path().split('/').last();
                                    if (username.isEmpty()) {
                                        username = "player";
                                    }
                                    
                                    // Rewrite URL to point to our local proxy
                                    QString newUrl = QString("http://localhost:%1/skin/%2").arg(port()).arg(username);
                                    skinObj["url"] = newUrl;
                                    textures["SKIN"] = skinObj;
                                    texturesObj["textures"] = textures;
                                    texturesDoc.setObject(texturesObj);
                                    
                                    // Re-encode and update
                                    prop["value"] = QString::fromUtf8(texturesDoc.toJson(QJsonDocument::Compact).toBase64());
                                    properties[i] = prop;
                                    root["properties"] = properties;
                                    modified = true;
                                    
                                    qDebug() << "SkinProxyServer: Rewrote TLauncher skin URL to local proxy";
                                }
                            }
                            
                            // Also check for cape URL
                            if (textures.contains("CAPE")) {
                                QJsonObject capeObj = textures["CAPE"].toObject();
                                QString capeUrl = capeObj["url"].toString();
                                
                                if (capeUrl.contains("tlauncher.org")) {
                                    QUrl url(capeUrl);
                                    QString username = url.path().split('/').last();
                                    if (username.isEmpty()) {
                                        username = "player";
                                    }
                                    
                                    QString newUrl = QString("http://localhost:%1/skin/%2").arg(port()).arg(username);
                                    capeObj["url"] = newUrl;
                                    textures["CAPE"] = capeObj;
                                    texturesObj["textures"] = textures;
                                    texturesDoc.setObject(texturesObj);
                                    
                                    prop["value"] = QString::fromUtf8(texturesDoc.toJson(QJsonDocument::Compact).toBase64());
                                    properties[i] = prop;
                                    root["properties"] = properties;
                                    modified = true;
                                    
                                    qDebug() << "SkinProxyServer: Rewrote TLauncher cape URL to local proxy";
                                }
                            }
                        }
                    }
                }
            }
            
            if (modified) {
                // Return the modified profile
                QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
                return "HTTP/1.1 200 OK\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                       "Connection: close\r\n"
                       "\r\n" +
                       body;
            }
        }
    }

    // Return the original profile data unchanged
    QByteArray body = m_profileJson;
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n" +
           body;
}

QByteArray SkinProxyServer::handleSkinRequest(const QString& path)
{
    // Extract username from path: /skin/<username>
    QStringList parts = path.split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return "HTTP/1.1 400 Bad Request\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 11\r\n"
               "Connection: close\r\n"
               "\r\n"
               "Bad Request";
    }
    
    QString username = parts[1];
    
    // Check if we have this skin cached
    if (!m_skinCache.contains(username)) {
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Content-Length: 9\r\n"
               "Connection: close\r\n"
               "\r\n"
               "Not Found";
    }
    
    QByteArray skinData = m_skinCache[username];
    QByteArray response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: image/png\r\n"
                          "Content-Length: " + QByteArray::number(skinData.size()) + "\r\n"
                          "Connection: close\r\n"
                          "\r\n" +
                          skinData;
    return response;
}
