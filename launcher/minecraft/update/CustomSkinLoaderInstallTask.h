// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Mint Launcher Contributors
 */

#pragma once

#include <memory>

#include "minecraft/MinecraftInstance.h"
#include "net/Mode.h"
#include "tasks/Task.h"

class CustomSkinLoaderInstallTask : public Task {
    Q_OBJECT

   public:
    CustomSkinLoaderInstallTask(MinecraftInstance* inst, Net::Mode mode);
    ~CustomSkinLoaderInstallTask() override = default;

    bool canAbort() const override { return true; }
    bool abort() override;

    void executeTask() override;

   private:
    void downloadVersion(const ModPlatform::IndexedVersion& version);
    void finishSkipped(const QString& reason);

    MinecraftInstance* m_inst;
    Net::Mode m_netMode;
    Task::Ptr m_currentTask;
};
