// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Mint Launcher Contributors
 */

#pragma once

#include <minecraft/launch/LaunchStep.h>
#include <minecraft/SkinProxyServer.h>

class AuthSession;
class MinecraftInstance;

class SkinProxyLaunchStep : public LaunchStep {
    Q_OBJECT

   public:
    explicit SkinProxyLaunchStep(LaunchTask* parent);
    ~SkinProxyLaunchStep() override = default;

    void executeTask() override;
    bool canAbort() const override { return true; }
    bool abort() override;

   private:
    void startProxy();
    void fetchTLauncherSkins();
    void detectAndFetchSkins(const QByteArray& profileJson);

    std::unique_ptr<SkinProxyServer> m_proxy;
    bool m_aborted = false;
};