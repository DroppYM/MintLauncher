#pragma once

#include <QObject>
#include <QTcpServer>
#include <QByteArray>
#include <QString>

class MinecraftInstance;

/**
 * @brief A lightweight local HTTP server that acts as a session server proxy.
 *
 * When enabled, this server intercepts Minecraft's session server requests
 * and returns the player's ely.by profile data (including custom skins)
 * instead of contacting Mojang's session server.
 *
 * This allows vanilla Minecraft to display ely.by custom skins without mods.
 */
class SkinProxyServer : public QObject {
    Q_OBJECT

   public:
    explicit SkinProxyServer(QObject* parent = nullptr);
    ~SkinProxyServer() override;

    /**
     * @brief Start the proxy server on a random available port.
     * @return true if the server started successfully.
     */
    bool start();

    /**
     * @brief Stop the proxy server.
     */
    void stop();

    /**
     * @brief Check if the server is currently running.
     */
    bool isRunning() const;

    /**
     * @brief Get the port the server is listening on.
     */
    int port() const;

    /**
     * @brief Set the profile JSON data to serve.
     * This should be the ely.by profile response (same format as Mojang's session server).
     */
    void setProfileData(const QByteArray& profileJson);

   signals:
    void error(const QString& message);

   private slots:
    void onNewConnection();

   private:
    void handleRequest(QTcpSocket* socket);

    QTcpServer* m_server = nullptr;
    QByteArray m_profileJson;
};