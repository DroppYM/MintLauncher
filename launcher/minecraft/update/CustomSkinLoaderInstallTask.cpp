// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Mint Launcher - Minecraft Launcher
 *  Copyright (c) 2026 Mint Launcher Contributors
 */

#include "CustomSkinLoaderInstallTask.h"

#include <QDirIterator>

#include "Application.h"
#include "FileSystem.h"
#include "minecraft/PackProfile.h"
#include "modplatform/ModIndex.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"

namespace {

constexpr auto kModrinthProjectId = "idMHQ4n2";
constexpr auto kModrinthProjectName = "CustomSkinLoader";

bool modsFolderHasCustomSkinLoader(const QString& modsDir)
{
    QDirIterator it(modsDir, { "*.jar" }, QDir::Files);
    while (it.hasNext()) {
        if (it.nextFileInfo().fileName().contains("CustomSkinLoader", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool versionMatchesMinecraft(const ModPlatform::IndexedVersion& version, const QString& mcVersion)
{
    return version.mcVersion.contains(mcVersion);
}

bool versionMatchesLoaders(const ModPlatform::IndexedVersion& version, ModPlatform::ModLoaderTypes loaders)
{
    return (version.loaders & loaders) != 0;
}

ModPlatform::ModLoaderTypes pickLoaderFilter(std::optional<ModPlatform::ModLoaderTypes> instanceLoaders)
{
    if (!instanceLoaders.has_value()) {
        return {};
    }
    auto loaders = instanceLoaders.value();
    if (loaders & ModPlatform::NeoForge) {
        return ModPlatform::NeoForge;
    }
    if (loaders & ModPlatform::Forge) {
        return ModPlatform::Forge;
    }
    if (loaders & ModPlatform::Quilt) {
        return ModPlatform::Quilt;
    }
    if (loaders & ModPlatform::Fabric) {
        return ModPlatform::Fabric;
    }
    return {};
}

}  // namespace

CustomSkinLoaderInstallTask::CustomSkinLoaderInstallTask(MinecraftInstance* inst, Net::Mode mode)
    : m_inst(inst), m_netMode(mode)
{
}

bool CustomSkinLoaderInstallTask::abort()
{
    if (m_currentTask) {
        return m_currentTask->abort();
    }
    return true;
}

void CustomSkinLoaderInstallTask::finishSkipped(const QString& reason)
{
    if (!reason.isEmpty()) {
        qDebug() << "CustomSkinLoader:" << reason;
    }
    emitSucceeded();
}

void CustomSkinLoaderInstallTask::executeTask()
{
    if (!APPLICATION->settings()->get("MintInstallCustomSkinLoader").toBool()) {
        finishSkipped({});
        return;
    }

    if (m_netMode == Net::Mode::Offline) {
        finishSkipped(tr("Skipped (offline mode)."));
        return;
    }

    const QString modsDir = m_inst->modsRoot();
    if (!QDir(modsDir).exists() && !FS::ensureFolderPathExists(modsDir)) {
        finishSkipped(tr("Could not create mods folder."));
        return;
    }

    if (modsFolderHasCustomSkinLoader(modsDir)) {
        finishSkipped(tr("Already installed."));
        return;
    }

    const QString mcVersion = m_inst->getPackProfile()->getComponentVersion("net.minecraft");
    if (mcVersion.isEmpty()) {
        finishSkipped(tr("Minecraft version unknown."));
        return;
    }

    const auto loaderFilter = pickLoaderFilter(m_inst->getPackProfile()->getSupportedModLoaders());
    if (loaderFilter == ModPlatform::ModLoaderTypes{}) {
        finishSkipped(tr("Skipped: instance has no mod loader (Fabric, Forge, NeoForge, or Quilt)."));
        return;
    }

    setStatus(tr("Installing CustomSkinLoader..."));

    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->addonId = kModrinthProjectId;
    pack->name = kModrinthProjectName;
    pack->provider = ModPlatform::ResourceProvider::MODRINTH;

    static const ModrinthAPI api;
    ResourceAPI::VersionSearchArgs args;
    args.pack = pack;
    args.mcVersions = { Version(mcVersion) };
    args.loaders = loaderFilter;
    args.resourceType = ModPlatform::ResourceType::Mod;
    args.includeChangelog = false;

    m_currentTask = api.getProjectVersions(
        std::move(args),
        ResourceAPI::Callback<QVector<ModPlatform::IndexedVersion>>{
            [this, mcVersion, loaderFilter](QVector<ModPlatform::IndexedVersion>& versions) {
                for (const auto& version : versions) {
                    if (!versionMatchesMinecraft(version, mcVersion)) {
                        continue;
                    }
                    if (!versionMatchesLoaders(version, loaderFilter)) {
                        continue;
                    }
                    if (version.downloadUrl.isEmpty() || version.fileName.isEmpty()) {
                        continue;
                    }
                    downloadVersion(version);
                    return;
                }
                finishSkipped(tr("No compatible CustomSkinLoader version on Modrinth for %1.").arg(mcVersion));
            },
            [this](const QString& reason, int) {
                qWarning() << "CustomSkinLoader version lookup failed:" << reason;
                finishSkipped(tr("Could not fetch CustomSkinLoader from Modrinth."));
            },
            [this]() { finishSkipped(tr("Cancelled.")); }});

    if (!m_currentTask) {
        finishSkipped(tr("Could not start Modrinth request."));
        return;
    }

    connect(m_currentTask.get(), &Task::progress, this, &CustomSkinLoaderInstallTask::setProgress);
    connect(m_currentTask.get(), &Task::stepProgress, this, &CustomSkinLoaderInstallTask::propagateStepProgress);
    m_currentTask->start();
}

void CustomSkinLoaderInstallTask::downloadVersion(const ModPlatform::IndexedVersion& version)
{
    setDetails(tr("Downloading %1").arg(version.fileName));

    auto job = makeShared<NetJob>(tr("CustomSkinLoader download"), APPLICATION->network());
    job->addNetAction(Net::ApiDownload::makeFile(version.downloadUrl, FS::PathCombine(m_inst->modsRoot(), version.fileName)));

    m_currentTask = job;
    connect(job.get(), &NetJob::succeeded, this, [this, version] {
        setStatus(tr("Installed CustomSkinLoader: %1").arg(version.fileName));
        emitSucceeded();
    });
    connect(job.get(), &NetJob::failed, this, [this](QString reason) {
        qWarning() << "CustomSkinLoader download failed:" << reason;
        finishSkipped(tr("Download failed."));
    });
    connect(job.get(), &NetJob::progress, this, &CustomSkinLoaderInstallTask::setProgress);
    connect(job.get(), &NetJob::stepProgress, this, &CustomSkinLoaderInstallTask::propagateStepProgress);
    job->start();
}
