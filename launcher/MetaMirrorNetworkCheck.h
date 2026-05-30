// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher — fork of Prism Launcher / PineconeMC
 *  Copyright (C) 2026 Mint Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */

#pragma once

#include <QNetworkAccessManager>

class MetaMirrorNetworkCheck : public QObject {
    Q_OBJECT

   public:
    enum class Result : std::uint8_t {
        UsePrimary,
        UseNewFallback,
        UseOldFallback,
        Offline
    };
    Q_ENUM(Result)

    explicit MetaMirrorNetworkCheck(QNetworkAccessManager* network);
    ~MetaMirrorNetworkCheck() override = default;

   signals:
    void shouldReloadNews(QString newUrl);

   private:
    void launchRequest(const QUrl& url, Result ifSuccess);

    bool handleUrlOverride(const QString& overrideName, const QMap<Result, QString>& urlMap) const;
    void finished();

   private:
    QNetworkAccessManager* m_network = nullptr;
    Result m_result = Result::Offline;
    int m_pendingRequests = 0;
    bool m_finished = false;
};
