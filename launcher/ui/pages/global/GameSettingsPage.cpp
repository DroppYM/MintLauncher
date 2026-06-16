// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2024 Mint Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "GameSettingsPage.h"
#include "ui_GameSettingsPage.h"

#include "Application.h"
#include "BuildConfig.h"
#include "settings/SettingsObject.h"

GameSettingsPage::GameSettingsPage(QWidget* parent) : QWidget(parent), ui(new Ui::GameSettingsPage)
{
    ui->setupUi(this);
    loadSettings();
}

GameSettingsPage::~GameSettingsPage()
{
    delete ui;
}

bool GameSettingsPage::apply()
{
    applySettings();
    return true;
}

void GameSettingsPage::retranslate()
{
    ui->retranslateUi(this);
}

void GameSettingsPage::loadSettings()
{
    auto s = APPLICATION->settings();
    ui->fovSlider->setValue(s->get("GameFov").toInt());
    ui->mouseSensitivitySlider->setValue(s->get("GameMouseSensitivity").toInt());
    ui->gammaSlider->setValue(s->get("GameGamma").toInt());
    ui->renderDistanceSpinBox->setValue(s->get("GameRenderDistance").toInt());
    ui->guiScaleComboBox->setCurrentIndex(s->get("GameGuiScale").toInt());
    ui->autoJumpCheckBox->setChecked(s->get("GameAutoJump").toBool());
    ui->toggleSneakCheckBox->setChecked(s->get("GameToggleSneak").toBool());
    ui->toggleSprintCheckBox->setChecked(s->get("GameToggleSprint").toBool());
    ui->languageEdit->setText(s->get("GameLanguage").toString());
}

void GameSettingsPage::applySettings()
{
    auto s = APPLICATION->settings();
    s->set("GameFov", ui->fovSlider->value());
    s->set("GameMouseSensitivity", ui->mouseSensitivitySlider->value());
    s->set("GameGamma", ui->gammaSlider->value());
    s->set("GameRenderDistance", ui->renderDistanceSpinBox->value());
    s->set("GameGuiScale", ui->guiScaleComboBox->currentIndex());
    s->set("GameAutoJump", ui->autoJumpCheckBox->isChecked());
    s->set("GameToggleSneak", ui->toggleSneakCheckBox->isChecked());
    s->set("GameToggleSprint", ui->toggleSprintCheckBox->isChecked());
    s->set("GameLanguage", ui->languageEdit->text());
}