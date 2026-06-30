#pragma once

#include <QObject>
#include <QTcpServer>
#include <QByteArray>
#include <QString>
#include <QMap>

class MinecraftInstance;

/**
 * @brief A lightweight local HTTP server that acts as a session server and skin proxy.
 *
 * When enabled, this server intercepts Minecraft's session server requests
 * and returns the player's profile data (including custom skins) instead of
 * contacting Mojang's or ely.by's servers.
 *
 * This allows vanilla Minecraft to display custom skins from various providers
 * (ely.by, TLauncher, etc.) without mods.
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
     * This should be the profile response (same format as Mojang's session server).
     */
    void setProfileData(const QByteArray& profileJson);

    /**
     * @brief Cache a skin texture for a username.
     * @param username The player username.
     * @param skinData The PNG skin data.
     */
    void cacheSkin(const QString& username, const QByteArray& skinData);

    /**
     * @brief Check if a skin is cached for a username.
     */
    bool hasSkin(const QString& username) const;

    /**
     * @brief Clear all cached skins.
     */
    void clearSkinCache();

   signals:
    void error(const QString& message);

   private slots:
    void onNewConnection();

   private:
    void handleRequest(QTcpSocket* socket);
    QByteArray handleProfileRequest(const QString& path);
    QByteArray handleSkinRequest(const QString& path);

    QTcpServer* m_server = nullptr;
    QByteArray m_profileJson;
    QMap<QString, QByteArray> m_skinCache;
};
