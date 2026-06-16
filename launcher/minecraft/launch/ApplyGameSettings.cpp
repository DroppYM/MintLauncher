#include "ApplyGameSettings.h"
#include "Application.h"
#include "FileSystem.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

ApplyGameSettings::ApplyGameSettings(LaunchTask* parent) : LaunchStep(parent) {}

struct GameSetting {
    QString optionsKey;   // Key in options.txt
    QString globalSetting;  // Key in APPLICATION->settings()
    QString defaultValue;   // Default value in options.txt format
    bool isBool;            // true = "true"/"false", false = raw value
    int intDivisor;         // For sensitivity: value/100, for gamma: value/100
};

static const QList<GameSetting> s_gameSettings = {
    { "fov",               "GameFov",             "70",      false, 1   },
    { "mouseSensitivity",  "GameMouseSensitivity", "0.5",     false, 200 },
    { "gamma",             "GameGamma",            "0.5",     false, 100 },
    { "renderDistance",    "GameRenderDistance",   "12",      false, 1   },
    { "guiScale",          "GameGuiScale",         "0",       false, 1   },
    { "autoJump",          "GameAutoJump",         "true",    true,  1   },
    { "toggleSneak",       "GameToggleSneak",      "false",   true,  1   },
    { "toggleSprint",      "GameToggleSprint",     "false",   true,  1   },
    { "lang",              "GameLanguage",         "en_US",   false, 1   },
};

void ApplyGameSettings::executeTask()
{
    auto instance = m_parent->instance();
    if (!instance) {
        emitFailed(tr("No instance to apply game settings to."));
        return;
    }

    auto mcInstance = dynamic_cast<MinecraftInstance*>(instance);
    if (!mcInstance) {
        emitSucceeded();
        return;
    }

    // Check if this instance uses global game settings
    if (!mcInstance->settings()->get("UseGlobalGameSettings").toBool()) {
        qDebug() << "Instance does not use global game settings, skipping.";
        emitSucceeded();
        return;
    }

    QString optionsPath = FS::PathCombine(mcInstance->gameRoot(), "options.txt");
    qDebug() << "Applying global game settings to" << optionsPath;

    // Read existing options.txt, or create empty map
    QMap<QString, QString> options;
    QFile file(optionsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            int colonIdx = line.indexOf(':');
            if (colonIdx > 0) {
                QString key = line.left(colonIdx).trimmed();
                QString value = line.mid(colonIdx + 1).trimmed();
                options[key] = value;
            }
        }
        file.close();
    }

    // Override with global settings
    auto settings = APPLICATION->settings();
    for (const auto& setting : s_gameSettings) {
        QVariant val = settings->get(setting.globalSetting);
        QString newValue;

        if (setting.isBool) {
            newValue = val.toBool() ? "true" : "false";
        } else if (setting.intDivisor > 1) {
            // Convert launcher scale (0-200) to Minecraft scale (0.0-1.0)
            double scaled = static_cast<double>(val.toInt()) / setting.intDivisor;
            newValue = QString::number(scaled, 'f', 2);
        } else {
            newValue = val.toString();
        }

        // Only set non-empty values
        if (!setting.globalSetting.startsWith("GameLanguage") || !val.toString().isEmpty()) {
            options[setting.optionsKey] = newValue;
        }
    }

    // Write back options.txt
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (auto it = options.begin(); it != options.end(); ++it) {
            out << it.key() << ":" << it.value() << "\n";
        }
        file.close();
        qDebug() << "Global game settings applied successfully.";
    } else {
        qWarning() << "Failed to write" << optionsPath;
    }

    emitSucceeded();
}