/*
 *  Copyright (c) 2014 Victor Lafon metabolic.ewilan@hotmail.fr
 *  Copyright (c) 2017 Aniketh Girish <anikethgireesh@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "dlg_bundle_manager.h"
#include "ui_wdgdlgbundlemanager.h"

#include "resourcemanager.h"
#include "dlg_create_bundle.h"

#include <QListWidget>
#include <QTreeWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QMessageBox>
#include <QMenu>
#include <QFileInfo>

#include <kis_icon.h>
#include "kis_action.h"
#include <kis_resource_server_provider.h>
#include <KoResourcePaths.h>
#include "KoResourceModel.h"
#include "KoResourceServer.h"

#ifdef HAVE_NEWSTUFF
#include "content_dowloader_dialog.h"
#endif

#define ICON_SIZE 48

DlgBundleManager::DlgBundleManager(ResourceManager *resourceManager, KisActionManager* actionMgr, QWidget *parent)
    : KoDialog(parent)
    , m_page(new QWidget())
    , m_ui(new Ui::WdgDlgBundleManager)
    , m_currentBundle(0)
    , m_resourceManager(resourceManager)
{
    setCaption(i18n("Manage Resource Bundles"));
    m_ui->setupUi(m_page);
    setMainWidget(m_page);
    resize(m_page->sizeHint());
    setButtons(Ok | Cancel);
    setDefaultButton(Ok);
    m_ui->lblDescription->setReadOnly(true);

    QString approot = KoResourcePaths::getApplicationRoot();

    m_knsrcFile = approot + "/etc/xdg/" + "kritaresourcebundles.knsrc";

    m_ui->searchLineEdit->setClearButtonEnabled(true);
    m_ui->searchLineEdit->addAction(QIcon::fromTheme(QStringLiteral("system-search")), QLineEdit::LeadingPosition);
    m_ui->searchLineEdit->setPlaceholderText("Search for the bundle name..");

    m_ui->listActive->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    connect(m_ui->listActive, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), SLOT(itemSelected(QListWidgetItem*,QListWidgetItem*)));
    connect(m_ui->listActive, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(itemSelected(QListWidgetItem*)));

    m_ui->listBundleContents->setHeaderLabel(i18n("Resource"));
    m_ui->listBundleContents->setSelectionMode(QAbstractItemView::NoSelection);

    m_actionManager = actionMgr;

    QMenu *importAll = new QMenu();

    KisAction *importBundle = m_actionManager->actionByName("import_bundles");
    importBundle->setText(i18n("Import Bundles"));
    importAll->addAction(importBundle);

    KisAction *importBrush = m_actionManager->actionByName("import_brushes");
    importBrush->setText(i18n("Import Brushes"));
    importAll->addAction(importBrush);

    KisAction *importGradient = m_actionManager->actionByName("import_gradients");
    importGradient->setText(i18n("Import Gradients"));
    importAll->addAction(importGradient);

    KisAction *importPalette = m_actionManager->actionByName("import_palettes");
    importPalette->setText(i18n("Import Palettes"));
    importAll->addAction(importPalette);

    KisAction *importPattern = m_actionManager->actionByName("import_patterns");
    importPattern->setText(i18n("Import Patterns"));
    importAll->addAction(importPattern);

    KisAction *importPreset = m_actionManager->actionByName("import_presets");
    importPreset->setText(i18n("Import Presets"));
    importAll->addAction(importPreset);

    KisAction *importWorkSpace = m_actionManager->actionByName("import_workspaces");
    importWorkSpace->setText(i18n("Import Workspace"));
    importAll->addAction(importWorkSpace);

    m_ui->m_importResources->setMenu(importAll);

    refreshListData();

    connect(m_ui->bnEditBundle, SIGNAL(clicked()), SLOT(editBundle()));
#ifndef HAVE_NEWSTUFF
    m_ui->bnShareResources->setVisible(false);
#endif
    connect(m_ui->bnShareResources, SIGNAL(clicked()), SLOT(slotShareResources()));

    connect(m_ui->createBundleButton, SIGNAL(clicked()), SLOT(slotCreateBundle()));
    connect(m_ui->deleteBackupFilesButton, SIGNAL(clicked()), SLOT(slotDeleteBackupFiles()));
    connect(m_ui->openResourceFolderButton, SIGNAL(clicked()), SLOT(slotOpenResourceFolder()));

    connect(m_ui->searchLineEdit, SIGNAL(textChanged(QString)), SLOT(refreshListData()));
    connect(m_ui->searchLineEdit, SIGNAL(textEdited(QString)), SLOT(refreshListData()));
}

DlgBundleManager::~DlgBundleManager()
{
    delete(m_ui->m_importResources->menu());
    qDeleteAll(m_blacklistedBundles);
}

void DlgBundleManager::refreshListData()
{
    KoResourceServer<KisResourceBundle> *bundleServer = KisResourceServerProvider::instance()->resourceBundleServer();

    m_ui->listActive->clear();

    qDeleteAll(m_blacklistedBundles);

    Q_FOREACH (const QString &f, bundleServer->blackListedFiles()) {
        KisResourceBundle *bundle = new KisResourceBundle(f);
        bundle->load();
        if (bundle->valid()) {
            bundle->setInstalled(false);
            m_blacklistedBundles.append(bundle);
        }
    }
    Q_FOREACH (KisResourceBundle *bundle, bundleServer->resources()) {

        m_activeBundles.removeAll(bundle);

        if (bundle->valid() && bundle->name().toLower().contains(m_ui->searchLineEdit->text())) {
            m_activeBundles.append(bundle);
        }

        else if(bundle->valid() && bundle->getMeta("author").contains(m_ui->searchLineEdit->text())) {
            m_activeBundles.append(bundle);
        }

        else if(bundle->valid() && bundle->getMeta("description").contains(m_ui->searchLineEdit->text().trimmed())) {
            m_activeBundles.append(bundle);
        }

    }
    fillListWidget(m_activeBundles + m_blacklistedBundles, m_ui->listActive);

}

void DlgBundleManager::accept()
{
    KoResourceServer<KisResourceBundle> *bundleServer = KisResourceServerProvider::instance()->resourceBundleServer();

    for (int i = 0; i < m_ui->listActive->count(); ++i) {
        QListWidgetItem *item = m_ui->listActive->item(i);
        QByteArray ba = item->data(Qt::UserRole).toByteArray();
        KisResourceBundle *bundle = bundleServer->resourceByMD5(ba);
        QMessageBox bundleFeedback;
        bundleFeedback.setIcon(QMessageBox::Warning);
        QString feedback = "bundlefeedback";

        if (!bundle) {
            // Get it from the blacklisted bundles
            Q_FOREACH (KisResourceBundle *b2, m_blacklistedBundles) {
                if (b2->md5() == ba) {
                    bundle = b2;
                    break;
                }
            }
        }

        if (bundle) {
            if(!bundle->isInstalled()){
                bundle->install();
                //this removes the bundle from the blacklist and add it to the server without saving or putting it in front//
                if(!bundleServer->addResource(bundle, false, false)){

                    feedback = i18n("Couldn't add bundle to resource server");
                    bundleFeedback.setText(feedback);
                    bundleFeedback.exec();
                }
                if(!bundleServer->removeFromBlacklist(bundle)){
                    feedback = i18n("Couldn't remove bundle from blacklist");
                    bundleFeedback.setText(feedback);
                    bundleFeedback.exec();
                }
            }
            else {
            bundleServer->removeFromBlacklist(bundle);
            //let's asume that bundles who exist and are installed have to be removed from the blacklist, and if they were already this returns false, so that's not a problem.
            }
        }
        else{
        QString feedback = i18n("Bundle doesn't exist!");
        bundleFeedback.setText(feedback);
        bundleFeedback.exec();

        }
    }

    KoDialog::accept();
}

void DlgBundleManager::itemSelected(QListWidgetItem *current, QListWidgetItem *)
{
    if (!current) {
        m_ui->lblName->clear();
        m_ui->lblAuthor->clear();
        m_ui->lblEmail->clear();
        m_ui->lblLicense->clear();
        m_ui->lblWebsite->clear();
        m_ui->lblDescription->clear();
        m_ui->lblCreated->clear();
        m_ui->lblUpdated->clear();
        m_ui->lblPreview->setPixmap(QPixmap::fromImage(QImage()));
        m_ui->listBundleContents->clear();
        m_ui->bnEditBundle->setEnabled(false);
        m_currentBundle = 0;
    }
    else {

        QByteArray ba = current->data(Qt::UserRole).toByteArray();
        KoResourceServer<KisResourceBundle> *bundleServer = KisResourceServerProvider::instance()->resourceBundleServer();
        KisResourceBundle *bundle = bundleServer->resourceByMD5(ba);

        if (!bundle) {
            // Get it from the blacklisted bundles
            Q_FOREACH (KisResourceBundle *b2, m_blacklistedBundles) {
                if (b2->md5() == ba) {
                    bundle = b2;
                    break;
                }
            }
        }

        if (bundle) {

            m_currentBundle = bundle;
            m_ui->bnEditBundle->setEnabled(true);

            m_ui->lblName->setText(bundle->name());
            m_ui->lblAuthor->setText(bundle->getMeta("author"));
            m_ui->lblEmail->setText(bundle->getMeta("email"));
            m_ui->lblLicense->setText(bundle->getMeta("license"));
            m_ui->lblWebsite->setText(bundle->getMeta("website"));
            m_ui->lblDescription->setPlainText(bundle->getMeta("description"));
            m_ui->lblCreated->setText(bundle->getMeta("created"));
            m_ui->lblUpdated->setText(bundle->getMeta("updated"));
            m_ui->lblPreview->setPixmap(QPixmap::fromImage(bundle->image().scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            m_ui->listBundleContents->clear();

            Q_FOREACH (const QString & resType, bundle->resourceTypes()) {

                QTreeWidgetItem *toplevel = new QTreeWidgetItem();
                if (resType == "gradients") {
                    toplevel->setText(0, i18n("Gradients"));
                }
                else if (resType  == "patterns") {
                    toplevel->setText(0, i18n("Patterns"));
                }
                else if (resType  == "brushes") {
                    toplevel->setText(0, i18n("Brushes"));
                }
                else if (resType  == "palettes") {
                    toplevel->setText(0, i18n("Palettes"));
                }
                else if (resType  == "workspaces") {
                    toplevel->setText(0, i18n("Workspaces"));
                }
                else if (resType  == "paintoppresets") {
                    toplevel->setText(0, i18n("Brush Presets"));
                }

                m_ui->listBundleContents->addTopLevelItem(toplevel);

                Q_FOREACH (const KoResource *res, bundle->resources(resType)) {
                    if (res) {
                        QTreeWidgetItem *i = new QTreeWidgetItem();
                        i->setIcon(0, QIcon(QPixmap::fromImage(res->image())));
                        i->setText(0, res->name());
                        toplevel->addChild(i);
                    }
                }
            }
        }
        else {
            m_currentBundle = 0;
        }
    }
}

void DlgBundleManager::itemSelected(QListWidgetItem *current)
{
    m_ui->listActive->setSelectionMode(QAbstractItemView::SingleSelection);
    itemSelected(current, 0);
}

void DlgBundleManager::editBundle()
{
    if (m_currentBundle) {
        DlgCreateBundle dlg(m_currentBundle);
        if (m_activeBundles.contains(m_currentBundle)) {
            m_activeBundles.remove(m_activeBundles.indexOf(m_currentBundle));
            m_currentBundle = 0;
        }
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        m_currentBundle = m_resourceManager->saveBundle(dlg);
        refreshListData();
    }
}

void DlgBundleManager::fillListWidget(QVector<KisResourceBundle *> bundles, QListWidget *w)
{
    w->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    w->setSelectionMode(QAbstractItemView::SingleSelection);
    w->clear();

    Q_FOREACH (KisResourceBundle *bundle, bundles) {
        QPixmap pixmap(ICON_SIZE, ICON_SIZE);
        pixmap.fill(Qt::gray);
        if (!bundle->image().isNull()) {
            QImage scaled = bundle->image().scaled(ICON_SIZE, ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            int x = (ICON_SIZE - scaled.width()) / 2;
            int y = (ICON_SIZE - scaled.height()) / 2;
            QPainter gc(&pixmap);
            gc.drawImage(x, y, scaled);
            gc.end();
        }

        QListWidgetItem *item = new QListWidgetItem(pixmap, bundle->name());
        item->setData(Qt::UserRole, bundle->md5());
        w->addItem(item);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_activeBundles.contains(bundle) ? Qt::Checked : Qt::Unchecked);
    }
}

void DlgBundleManager::slotCreateBundle() {

    if (m_actionManager) {
        KisAction *action = m_actionManager->actionByName("create_bundle");
        action->trigger();
        refreshListData();
    }
}

void DlgBundleManager::slotDeleteBackupFiles() {

    if (m_actionManager) {
        KisAction *action = m_actionManager->actionByName("edit_blacklist_cleanup");
        action->trigger();
    }
}

void DlgBundleManager::slotOpenResourceFolder() {

    if (m_actionManager) {
        KisAction *action = m_actionManager->actionByName("open_resources_directory");
        action->trigger();
    }
}

void DlgBundleManager::slotShareResources()
{
#ifdef HAVE_NEWSTUFF
    ContentDownloaderDialog dialog(m_knsrcFile, this);
    dialog.exec();

    foreach (const KNSCore::EntryInternal& e, dialog.changedEntries()) {

        qDebug() << e.name() << e.category() << e.installedFiles();

        foreach(const QString &file, e.installedFiles()) {
            QFileInfo fi(file);
            if (fi.exists()) {
                //d->model->importResourceFile(fi.absoluteFilePath() , false );
            }
            else {
                qWarning() << "Failed to install resource file" << fi.absolutePath()+'/'+fi.fileName() << "as the file info does not exist";
            }
        }

        foreach(const QString &file, e.uninstalledFiles()) {
            QFileInfo fi(file);
            if (fi.exists()) {
                //d->model->removeResourceFile(fi.absoluteFilePath());
            }

            else {
                qWarning() << "Failed to Remove/Uninstall resource file" << fi.absolutePath()+'/'+fi.fileName() << "as the file does not exist";
            }
        }
    }
#endif
}
