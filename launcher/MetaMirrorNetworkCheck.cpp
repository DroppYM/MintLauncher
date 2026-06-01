// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher — fork of Prism Launcher / PineconeMC
 *  Copyright (C) 2026 Mint Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#include "MetaMirrorNetworkCheck.h"

#include <array>

#include <QNetworkReply>

#include "Application.h"
#include "net/HttpMetaCache.h"
#include "settings/SettingsObject.h"

MetaMirrorNetworkCheck::MetaMirrorNetworkCheck(QNetworkAccessManager* network)
{
    m_network = network;

    // Prefer Ely meta first (Mint Launcher needs by.ely.authlib / authlib-injector).
    static auto s_urlToResult = std::array{
        std::pair { QUrl("https://elyprismlauncher.github.io"), Result::UsePrimary },
        std::pair { QUrl("https://pineconemc.github.io"), Result::UseNewFallback },
        std::pair { QUrl("https://meta.prismlauncher.org"), Result::UseOldFallback },
    };
    for (auto& [url, result] : s_urlToResult) {
        launchRequest(url, result);
    }
}

void MetaMirrorNetworkCheck::launchRequest(const QUrl& url, Result ifSuccess)
{
    QNetworkRequest request(url);
    request.setTransferTimeout(std::chrono::seconds(3));
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    auto* reply = m_network->head(request);
    m_pendingRequests++;

    qInfo() << "[MetaMirrorNetworkCheck] Checking" << url;
    connect(reply, &QNetworkReply::finished, this, [this, reply, ifSuccess] {
        m_pendingRequests--;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qInfo() << "[MetaMirrorNetworkCheck]" << reply->url() << "result:" << reply->error() << status;
        if (reply->error() == QNetworkReply::NoError && status < 400 && ifSuccess < m_result) {
            m_result = ifSuccess;
        }
        reply->deleteLater();

        if (!m_finished && (m_pendingRequests == 0 || m_result == Result::UsePrimary)) {
            qInfo() << "[MetaMirrorNetworkCheck] Final result:" << m_result;
            finished();
            m_finished = true;
        }
    });
}

bool MetaMirrorNetworkCheck::handleUrlOverride(const QString& overrideName, const QMap<Result, QString>& urlMap) const
{
    if (!urlMap.contains(m_result)) {
        return false;
    }
    const QString newOverride = urlMap.value(m_result);

    auto* settings = APPLICATION->settings();
    const auto currentOverride = settings->get(overrideName).toString();
    if (currentOverride == newOverride) {
        return false;
    }
    if (!currentOverride.isEmpty() && !urlMap.values().contains(currentOverride)) {
        return false;
    }

    settings->set(overrideName, newOverride);
    qInfo() << "[MetaMirrorNetworkCheck] Updated setting" << overrideName << "to" << newOverride;
    return true;
}

void MetaMirrorNetworkCheck::finished()
{
    const QMap<Result, QString> metaUrls = {
        { Result::UsePrimary, "https://elyprismlauncher.github.io/meta/v1/" },
        { Result::UseNewFallback, "https://pineconemc.github.io/meta/v1/" },
        { Result::UseOldFallback, "https://meta.prismlauncher.org/v1/" },
    };
    if (handleUrlOverride("MetaURLOverride", metaUrls)) {
        if (!APPLICATION->metacache()->evictAll()) {
            qWarning() << "Could not evict metacache during automatic meta switch";
        }
        APPLICATION->metacache()->SaveNow();
    }

    const QMap<Result, QString> fmlLibsUrls = {
        { Result::UsePrimary, "" },
        { Result::UseNewFallback, "https://pineconemc.github.io/files/fmllibs/" },
        { Result::UseOldFallback, "https://elyprismlauncher.github.io/files/fmllibs/" },
    };
    std::ignore = handleUrlOverride("LegacyFMLLibsURLOverride", fmlLibsUrls);

    // News feed URL is fixed at build time (BuildConfig.NEWS_RSS_URL); do not override from mirror checks.
}
