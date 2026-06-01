// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ExternalResourcesPage.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_ExternalResourcesPage.h"

#include "DesktopServices.h"
#include "Version.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "ui/GuiUtil.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <algorithm>
#include <utility>

ExternalResourcesPage::ExternalResourcesPage(BaseInstance* instance, ResourceFolderModel* model, QWidget* parent)
    : QMainWindow(parent), m_instance(instance), ui(new Ui::ExternalResourcesPage), m_model(model)
{
    ui->setupUi(this);

    ui->actionsToolbar->insertSpacer(ui->actionViewFolder);

    m_filterModel = model->createFilterProxyModel(this);
    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterKeyColumn(-1);
    ui->treeView->setModel(m_filterModel);
    ui->treeView->setResizeModes(m_model->columnResizeModes());

    ui->treeView->installEventFilter(this);
    ui->treeView->sortByColumn(1, Qt::AscendingOrder);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // The default function names by Qt are pretty ugly, so let's just connect the actions manually,
    // to make it easier to read :)
    connect(ui->actionAddItem, &QAction::triggered, this, &ExternalResourcesPage::addItem);
    connect(ui->actionRemoveItem, &QAction::triggered, this, &ExternalResourcesPage::removeItem);
    connect(ui->actionEnableItem, &QAction::triggered, this, &ExternalResourcesPage::enableItem);
    connect(ui->actionDisableItem, &QAction::triggered, this, &ExternalResourcesPage::disableItem);
    connect(ui->actionViewHomepage, &QAction::triggered, this, &ExternalResourcesPage::viewHomepage);
    connect(ui->actionViewConfigs, &QAction::triggered, this, &ExternalResourcesPage::viewConfigs);
    connect(ui->actionViewFolder, &QAction::triggered, this, &ExternalResourcesPage::viewFolder);

    connect(ui->treeView, &ModListView::customContextMenuRequested, this, &ExternalResourcesPage::ShowContextMenu);
    connect(ui->treeView, &ModListView::activated, this, &ExternalResourcesPage::itemActivated);

    if (auto* selection_model = ui->treeView->selectionModel()) {
        connect(selection_model, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
            if (!canShowResourcePreview() || m_suspendFrameUpdates || !ui || !ui->frame) {
                return;
            }
            if (!current.isValid()) {
                ui->frame->clear();
                return;
            }
            QMetaObject::invokeMethod(this, &ExternalResourcesPage::refreshFrameFromSelection, Qt::QueuedConnection);
        });
        connect(selection_model, &QItemSelectionModel::selectionChanged, this, [this]() {
            if (updateExtraInfo) {
                updateExtraInfo(id(), extraHeaderInfoString());
            }
            updateActions();
        });
    }

    auto suspendFrameUpdates = [this]() {
        m_suspendFrameUpdates++;
        if (ui && ui->frame) {
            ui->frame->clear();
        }
    };
    auto resumeFrameUpdates = [this]() {
        if (m_suspendFrameUpdates > 0) {
            m_suspendFrameUpdates--;
        }
        if (m_suspendFrameUpdates == 0 && canShowResourcePreview()) {
            QMetaObject::invokeMethod(this, &ExternalResourcesPage::refreshFrameFromSelection, Qt::QueuedConnection);
        }
    };

    connect(m_model, &QAbstractItemModel::rowsAboutToBeRemoved, this, suspendFrameUpdates);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, resumeFrameUpdates);
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this, suspendFrameUpdates);
    connect(m_model, &QAbstractItemModel::modelReset, this, resumeFrameUpdates);
    connect(m_model, &QAbstractItemModel::layoutAboutToBeChanged, this, suspendFrameUpdates);
    connect(m_model, &QAbstractItemModel::layoutChanged, this, resumeFrameUpdates);
    connect(m_model, &ResourceFolderModel::updateFinished, this, [this]() {
        updateActions();
        if (canShowResourcePreview()) {
            QMetaObject::invokeMethod(this, &ExternalResourcesPage::refreshFrameFromSelection, Qt::QueuedConnection);
        }
    });

    if (m_instance) {
        connect(m_instance, &BaseInstance::runningStatusChanged, this, [this](bool running) {
            if (!ui || !ui->frame) {
                return;
            }
            if (running) {
                ui->frame->clear();
                ui->frame->setEnabled(false);
            } else {
                ui->frame->setEnabled(true);
                if (isOpened) {
                    m_model->update();
                }
            }
        });
    }

    connect(model, &ResourceFolderModel::parseFinished, this, [this]() {
        if (updateExtraInfo) {
            updateExtraInfo(id(), extraHeaderInfoString());
        }
    });

    connect(m_model, &ResourceFolderModel::rowsInserted, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::rowsRemoved, this, [this] { updateActions(); });

    auto viewHeader = ui->treeView->header();
    viewHeader->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(viewHeader, &QHeaderView::customContextMenuRequested, this, &ExternalResourcesPage::ShowHeaderContextMenu);

    m_model->loadColumns(ui->treeView);
    connect(ui->treeView->header(), &QHeaderView::sectionResized, this, [this] { m_model->saveColumns(ui->treeView); });
    connect(ui->filterEdit, &QLineEdit::textChanged, this, &ExternalResourcesPage::filterTextChanged);
    updateActions();
}

ExternalResourcesPage::~ExternalResourcesPage()
{
    delete ui;
}

QMenu* ExternalResourcesPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->actionsToolbar->toggleViewAction());
    return filteredMenu;
}

void ExternalResourcesPage::ShowContextMenu(const QPoint& pos)
{
    auto menu = ui->actionsToolbar->createContextMenu(this, tr("Context menu"));
    menu->exec(ui->treeView->mapToGlobal(pos));
    delete menu;
}

void ExternalResourcesPage::ShowHeaderContextMenu(const QPoint& pos)
{
    auto menu = m_model->createHeaderContextMenu(ui->treeView);
    menu->exec(ui->treeView->mapToGlobal(pos));
    menu->deleteLater();
}

void ExternalResourcesPage::openedImpl()
{
    if (ui && ui->treeView && ui->treeView->selectionModel()) {
        ui->treeView->selectionModel()->clearSelection();
    }
    if (ui && ui->frame) {
        ui->frame->clear();
        ui->frame->setEnabled(!m_instance || !m_instance->isRunning());
    }

    m_model->startWatching();

    auto const setting_name = QString("WideBarVisibility_%1").arg(id());
    m_wide_bar_setting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

    ui->actionsToolbar->setVisibilityState(QByteArray::fromBase64(m_wide_bar_setting->get().toString().toUtf8()));
}

void ExternalResourcesPage::closedImpl()
{
    m_model->stopWatching();

    m_wide_bar_setting->set(QString::fromUtf8(ui->actionsToolbar->getVisibilityState().toBase64()));
}

bool ExternalResourcesPage::canShowResourcePreview() const
{
    return m_instance && !m_instance->isRunning();
}

void ExternalResourcesPage::retranslate()
{
    ui->retranslateUi(this);
}

void ExternalResourcesPage::itemActivated(const QModelIndex&)
{
    if (!ui || !ui->treeView || !ui->treeView->selectionModel()) {
        return;
    }

    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::TOGGLE);
}

void ExternalResourcesPage::filterTextChanged(const QString& newContents)
{
    m_viewFilter = newContents;
    m_filterModel->setFilterRegularExpression(m_viewFilter);
}

bool ExternalResourcesPage::shouldDisplay() const
{
    return true;
}

bool ExternalResourcesPage::listFilter(QKeyEvent* keyEvent)
{
    switch (keyEvent->key()) {
        case Qt::Key_Delete:
            removeItem();
            return true;
        case Qt::Key_Plus:
            addItem();
            return true;
        default:
            break;
    }
    return QWidget::eventFilter(ui->treeView, keyEvent);
}

bool ExternalResourcesPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, ev);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
    if (obj == ui->treeView)
        return listFilter(keyEvent);

    return QWidget::eventFilter(obj, ev);
}

void ExternalResourcesPage::addItem()
{
    auto list = GuiUtil::BrowseForFiles(
        helpPage(), tr("Select %1", "Select whatever type of files the page contains. Example: 'Loader Mods'").arg(displayName()),
        m_fileSelectionFilter.arg(displayName()), APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        for (auto filename : list) {
            m_model->installResource(filename);
        }
    }
}

void ExternalResourcesPage::removeItem()
{
    if (!ui || !ui->treeView || !ui->treeView->selectionModel()) {
        return;
    }

    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection());

    int count = 0;
    bool folder = false;
    for (auto& i : selection.indexes()) {
        if (i.column() == 0 && m_model->validateIndex(i)) {
            count++;

            // if a folder is selected, show the confirmation dialog
            if (m_model->at(i.row()).fileinfo().isDir())
                folder = true;
        }
    }

    QString text;
    bool multiple = count > 1;

    if (multiple) {
        text = tr("You are about to remove %1 items.\n"
                  "This may be permanent and they will be gone from the folder.\n\n"
                  "Are you sure?")
                   .arg(count);
    } else if (folder) {
        const QModelIndex first = selection.indexes().at(0);
        text = tr("You are about to remove the folder \"%1\".\n"
                  "This may be permanent and it will be gone from the parent folder.\n\n"
                  "Are you sure?")
                   .arg(m_model->validateIndex(first) ? m_model->at(first.row()).fileinfo().fileName() : QString());
    }

    if (!text.isEmpty()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"), text, QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }

    removeItems(selection);
}

void ExternalResourcesPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove this resource while the game is running it may crash your game.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    m_model->deleteResources(selection.indexes());
}

void ExternalResourcesPage::enableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::ENABLE);
}

void ExternalResourcesPage::disableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::DISABLE);
}

void ExternalResourcesPage::viewHomepage()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    for (auto resource : m_model->selectedResources(selection)) {
        auto url = resource->homepage();
        if (!url.isEmpty())
            DesktopServices::openUrl(url);
    }
}

void ExternalResourcesPage::viewConfigs()
{
    DesktopServices::openPath(m_instance->instanceConfigFolder(), true);
}

void ExternalResourcesPage::viewFolder()
{
    DesktopServices::openPath(m_model->dir().absolutePath(), true);
}

void ExternalResourcesPage::updateActions()
{
    if (!ui || !m_model) {
        return;
    }

    auto* selection_model = ui->treeView->selectionModel();
    if (!selection_model) {
        return;
    }

    const bool hasSelection = selection_model->hasSelection();
    QModelIndexList selection = m_filterModel->mapSelectionToSource(selection_model->selection()).indexes();
    selection.erase(std::remove_if(selection.begin(), selection.end(),
                                   [this](const QModelIndex& idx) { return !m_model->validateIndex(idx); }),
                    selection.end());
    const QList<Resource*> selectedResources = m_model->selectedResources(selection);

    ui->actionUpdateItem->setEnabled(!m_model->empty());
    ui->actionResetItemMetadata->setEnabled(hasSelection);

    ui->actionChangeVersion->setEnabled(selectedResources.size() == 1 && selectedResources[0]->metadata() != nullptr);

    ui->actionRemoveItem->setEnabled(hasSelection);
    ui->actionEnableItem->setEnabled(hasSelection);
    ui->actionDisableItem->setEnabled(hasSelection);

    ui->actionViewHomepage->setEnabled(hasSelection && std::any_of(selectedResources.begin(), selectedResources.end(),
                                                                   [](Resource* resource) { return !resource->homepage().isEmpty(); }));
    ui->actionExportMetadata->setEnabled(!m_model->empty());
}

int ExternalResourcesPage::validatedSourceRow(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid() || !m_filterModel || !m_model) {
        return -1;
    }

    const QModelIndex sourceIndex = m_filterModel->mapToSource(proxyIndex);
    if (!m_model->validateIndex(sourceIndex)) {
        return -1;
    }

    return sourceIndex.row();
}

void ExternalResourcesPage::refreshFrameFromSelection()
{
    if (!canShowResourcePreview() || m_suspendFrameUpdates || !ui || !ui->treeView || !ui->treeView->selectionModel()) {
        return;
    }

    updateFrame(ui->treeView->selectionModel()->currentIndex(), QModelIndex());
}

void ExternalResourcesPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!canShowResourcePreview() || m_suspendFrameUpdates || !ui || !ui->frame || !m_model || !m_filterModel) {
        if (ui && ui->frame) {
            ui->frame->clear();
        }
        return;
    }

    if (!current.isValid()) {
        ui->frame->clear();
        return;
    }

    const int row = validatedSourceRow(current);
    if (row < 0) {
        ui->frame->clear();
        return;
    }

    Resource const& resource = m_model->at(row);
    ui->frame->updateWithResource(resource);
}

QString ExternalResourcesPage::extraHeaderInfoString()
{
    if (ui && ui->treeView && ui->treeView->selectionModel()) {
        auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
        if (auto count = std::count_if(selection.cbegin(), selection.cend(), [](auto v) { return v.column() == 0; }); count != 0)
            return tr(" (%1 installed, %2 selected)").arg(m_model->size()).arg(count);
    }
    return tr(" (%1 installed)").arg(m_model->size());
}
