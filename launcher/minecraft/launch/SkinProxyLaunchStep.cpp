// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Mint Launcher Contributors
 */

#include "SkinProxyLaunchStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "net/NetJob.h"

SkinProxyLaunchStep::SkinProxyLaunchStep(LaunchTask* parent)
    : LaunchStep(parent)
{
}

void SkinProxyLaunchStep::executeTask()
{
    auto instance = m_parent->instance();
    if (!instance) {
        emitSucceeded();
        return;
    }

    auto mcInstance = dynamic_cast<MinecraftInstance*>(instance);
    if (!mcInstance) {
        emitSucceeded();
        return;
    }

    // Only run if OnlineFixes is enabled
    if (!mcInstance->shouldApplyOnlineFixes()) {
        qDebug() << "SkinProxyLaunchStep: OnlineFixes not enabled, skipping skin proxy";
        emitSucceeded();
        return;
    }

    startProxy();
}

void SkinProxyLaunchStep::startProxy()
{
    m_proxy = std::make_unique<SkinProxyServer>(this);
    
    if (!m_proxy->start()) {
        emit logLine(tr("Failed to start skin proxy server"), MessageLevel::Warning);
        emitSucceeded();
        return;
    }

    qDebug() << "SkinProxyLaunchStep: Proxy started on port" << m_proxy->port();

    // Get the session to fetch profile data
    auto session = m_parent->authSession();
    if (!session || session->access_token.isEmpty()) {
        qDebug() << "SkinProxyLaunchStep: No session available, proxy will serve cached data only";
        emitSucceeded();
        return;
    }

    // Fetch the Minecraft profile to get skin data
    auto instance = m_parent->instance();
    auto mcInstance = dynamic_cast<MinecraftInstance*>(instance);
    if (!mcInstance) {
        emitSucceeded();
        return;
    }

    // Fetch profile from Ely or Mojang
    QUrl profileUrl;
    if (session->wantsElyPatch) {
        profileUrl = QUrl("https://account.ely.by/api/mojang/services/minecraft/profile");
    } else {
        profileUrl = QUrl("https://api.minecraftservices.com/minecraft/profile");
    }

    auto [request, response] = Net::Download::makeByteArray(profileUrl);
    QList<Net::HeaderPair> headers;
    headers << qMakePair(QString("Content-Type"), QString("application/json"));
    headers << qMakePair(QString("Accept"), QString("application/json"));
    headers << qMakePair(QString("Authorization"), QString("Bearer %1").arg(session->access_token));
    request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    request->enableAutoRetry(true);

    auto task = makeShared<NetJob>("SkinProxyFetchProfile", APPLICATION->network());
    task->setAskRetry(false);
    task->addNetAction(request);

    connect(task.get(), &Task::finished, this, [this, response, request, task]() {
        if (m_aborted) {
            emitSucceeded();
            return;
        }

        if (request->error() == QNetworkReply::NoError) {
            QByteArray profileData = *response;
            m_proxy->setProfileData(profileData);
            
            // Detect TLauncher skins and fetch them
            detectAndFetchSkins(profileData);
            
            emit logLine(tr("Skin proxy server running on port %1").arg(m_proxy->port()), MessageLevel::Launcher);
        } else {
            qWarning() << "SkinProxyLaunchStep: Failed to fetch profile:" << request->errorString();
        }
        
        emitSucceeded();
    });

    connect(task.get(), &Task::progress, this, &Task::setProgress);
    connect(task.get(), &Task::stepProgress, this, &Task::propagateStepProgress);
    task->start();
}

void SkinProxyLaunchStep::detectAndFetchSkins(const QByteArray& profileJson)
{
    QJsonDocument doc = QJsonDocument::fromJson(profileJson);
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();
    if (!root.contains("properties") || !root["properties"].isArray())
        return;

    QJsonArray properties = root["properties"].toArray();
    for (const auto& propVal : properties) {
        QJsonObject prop = propVal.toObject();
        if (prop["name"].toString() != "textures" || !prop.contains("value"))
            continue;

        QByteArray texturesData = QByteArray::fromBase64(prop["value"].toString().toUtf8());
        QJsonDocument texturesDoc = QJsonDocument::fromJson(texturesData);
        if (!texturesDoc.isObject())
            continue;

        QJsonObject texturesObj = texturesDoc.object();
        if (!texturesObj.contains("textures") || !texturesObj["textures"].isObject())
            continue;

        QJsonObject textures = texturesObj["textures"].toObject();
        QString username = root["name"].toString();

        // Check for TLauncher skin URLs
        if (textures.contains("SKIN")) {
            QJsonObject skinObj = textures["SKIN"].toObject();
            QString skinUrl = skinObj["url"].toString();
            
            if (skinUrl.contains("tlauncher.org")) {
                qDebug() << "SkinProxyLaunchStep: Detected TLauncher skin for" << username << "at" << skinUrl;
                fetchTLauncherSkin(username, skinUrl);
            }
        }

        // Check for TLauncher cape URLs
        if (textures.contains("CAPE")) {
            QJsonObject capeObj = textures["CAPE"].toObject();
            QString capeUrl = capeObj["url"].toString();
            
            if (capeUrl.contains("tlauncher.org")) {
                qDebug() << "SkinProxyLaunchStep: Detected TLauncher cape for" << username << "at" << capeUrl;
                fetchTLauncherSkin(username + "_cape", capeUrl);
            }
        }
    }
}

void SkinProxyLaunchStep::fetchTLauncherSkins()
{
    // This method is kept for potential future use
    // Currently skin fetching is done in detectAndFetchSkins
}

void SkinProxyLaunchStep::fetchTLauncherSkin(const QString& username, const QString& url)
{
    if (m_proxy->hasSkin(username)) {
        qDebug() << "SkinProxyLaunchStep: Skin already cached for" << username;
        return;
    }

    qDebug() << "SkinProxyLaunchStep: Fetching TLauncher skin for" << username << "from" << url;

    auto [request, response] = Net::Download::makeByteArray(QUrl(url));
    request->enableAutoRetry(true);

    auto task = makeShared<NetJob>("SkinProxyFetchSkin", APPLICATION->network());
    task->setAskRetry(false);
    task->addNetAction(request);

    connect(task.get(), &Task::finished, this, [this, username, response, request]() {
        if (m_aborted)
            return;

        if (request->error() == QNetworkReply::NoError) {
            QByteArray skinData = *response;
            m_proxy->cacheSkin(username, skinData);
            qDebug() << "SkinProxyLaunchStep: Successfully cached skin for" << username;
        } else {
            qWarning() << "SkinProxyLaunchStep: Failed to fetch skin for" << username << ":" << request->errorString();
        }
    });

    connect(task.get(), &Task::progress, this, &Task::setProgress);
    connect(task.get(), &Task::stepProgress, this, &Task::propagateStepProgress);
    task->start();
}

bool SkinProxyLaunchStep::abort()
{
    m_aborted = true;
    if (m_proxy) {
        m_proxy->stop();
    }
    return true;
}

#include "SkinProxyLaunchStep.moc"