#include "widgets/projecttabswidget.h"
#include "windows/devicemanager.h"
#include "windows/dialogs.h"
#include "editors/codeeditor.h"
#include "editors/imageeditor.h"
#include "base/application.h"
#include <QDebug>

ProjectTabsWidget::ProjectTabsWidget(Project *project, QWidget *parent) : QTabWidget(parent), project(project)
{
    setMovable(true);
    setTabsClosable(true);

    connect(this, &ProjectTabsWidget::tabCloseRequested, [=](int index) {
        BaseEditor *tab = static_cast<BaseEditor *>(widget(index));
        closeTab(tab);
    });

    openProjectTab();
}

ProjectManager *ProjectTabsWidget::openProjectTab()
{
    const QString identifier = "project";
    BaseEditor *existing = getTabByIdentifier(identifier);
    if (existing) {
        setCurrentIndex(indexOf(existing));
        return static_cast<ProjectManager *>(existing);
    }

    ProjectManager *tab = new ProjectManager(project, this);
    tab->setProperty("identifier", identifier);
    connect(tab, &ProjectManager::titleEditorRequested, this, &ProjectTabsWidget::openTitlesTab);
    connect(tab, &ProjectManager::apkSaveRequested, this, &ProjectTabsWidget::saveProject);
    connect(tab, &ProjectManager::apkInstallRequested, this, &ProjectTabsWidget::installProject);
    addTab(tab);
    return tab;
}

TitleEditor *ProjectTabsWidget::openTitlesTab()
{
    const QString identifier = "titles";
    BaseEditor *existing = getTabByIdentifier(identifier);
    if (existing) {
        setCurrentIndex(indexOf(existing));
        return static_cast<TitleEditor *>(existing);
    }

    TitleEditor *editor = new TitleEditor(project, this);
    editor->setProperty("identifier", identifier);
    addTab(editor);
    return editor;
}

BaseEditor *ProjectTabsWidget::openResourceTab(const ResourceModelIndex &index)
{
    const QString path = index.path();
    const QString identifier = path;
    BaseEditor *existing = getTabByIdentifier(identifier);
    if (existing) {
        setCurrentIndex(indexOf(existing));
        return existing;
    }

    BaseEditor *editor = nullptr;
    const QString extension = QFileInfo(path).suffix();
    if (CodeEditor::supportedFormats().contains(extension)) {
        editor = new CodeEditor(index, this);
    } else if (ImageEditor::supportedFormats().contains(extension)) {
        editor = new ImageEditor(index, this);
    } else {
        qDebug() << "No suitable editor found for" << extension;
        return nullptr;
    }
    editor->setProperty("identifier", identifier);
    addTab(editor);
    return editor;
}

bool ProjectTabsWidget::saveTabs()
{
    bool result = true;
    for (int index = 0; index < count(); ++index) {
        SaveableEditor *tab = static_cast<SaveableEditor *>(widget(index));
        if (!tab->save()) {
            result = false;
        }
    }
    return result;
}

bool ProjectTabsWidget::isUnsaved() const
{
    return project->getModifiedState() || hasUnsavedTabs();
}

bool ProjectTabsWidget::saveProject()
{
    if (hasUnsavedTabs()) {
        const int answer = QMessageBox::question(this, QString(), tr("Do you want to save changes before packing?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (answer) {
        case QMessageBox::Yes:
        case QMessageBox::Save:
            saveTabs();
            break;
        case QMessageBox::No:
        case QMessageBox::Discard:
            break;
        default:
            return false;
        }
    }
    const QString target = Dialogs::getSaveApkFilename(project, this);
    if (target.isEmpty()) {
        return false;
    }
    project->pack(target);
    return true;
}

bool ProjectTabsWidget::installProject()
{
    DeviceManager devices(this);
    const Device *device = devices.getDevice();
    if (!device) {
        return false;
    }

    if (isUnsaved()) {
        const QString question = tr("Do you want to save changes and pack the APK before installing?");
        const int answer = QMessageBox::question(this, QString(), question, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        switch (answer) {
        case QMessageBox::Yes:
        case QMessageBox::Save: {
            saveTabs();
            const QString target = Dialogs::getSaveApkFilename(project, this);
            if (target.isEmpty()) {
                return false;
            }
            project->saveAndInstall(target, device->getSerial());
            return true;
        }
        case QMessageBox::No:
        case QMessageBox::Discard:
            break;
        default:
            return false;
        }
    }

    project->install(device->getSerial());
    return true;
}

bool ProjectTabsWidget::exploreProject()
{
    app->explore(project->getContentsPath());
    return true;
}

bool ProjectTabsWidget::closeProject()
{
    if (project->getModifiedState()) {
        const int answer = QMessageBox::question(this, QString(), tr("Are you sure you want to close this APK?\nAny unsaved changes will be lost."));
        if (answer != QMessageBox::Yes) {
            return false;
        }
    }
    return app->projects.close(project);
}

int ProjectTabsWidget::addTab(BaseEditor *tab)
{
    const int tabIndex = QTabWidget::addTab(tab, tab->getIcon(), tab->getTitle());
    setCurrentIndex(tabIndex);
    auto saveableTab = qobject_cast<SaveableEditor *>(tab);
    if (saveableTab) {
        connect(saveableTab, &SaveableEditor::savedStateChanged, [=](bool tabSaved) {
            // Project save indicator:
            if (!tabSaved) {
                project->setModified(true);
            }
            // Tab save indicator:
            const QString indicator = QString(" %1").arg(QChar(0x2022));
            const int tabIndex = indexOf(saveableTab);
            QString tabTitle = tabText(tabIndex);
            const bool titleModified = tabTitle.endsWith(indicator);
            if (!tabSaved && !titleModified) {
                tabTitle.append(indicator);
                setTabText(tabIndex, tabTitle);
            } else if (tabSaved && titleModified) {
                tabTitle.chop(indicator.length());
                setTabText(tabIndex, tabTitle);
            }
        });
    }
    connect(tab, &BaseEditor::titleChanged, [=](const QString &title) {
        setTabText(indexOf(tab), title);
    });
    connect(tab, &BaseEditor::iconChanged, [=](const QIcon &icon) {
        setTabIcon(indexOf(tab), icon);
    });
    return tabIndex;
}

bool ProjectTabsWidget::closeTab(BaseEditor *editor)
{
    auto saveableTab = qobject_cast<SaveableEditor *>(editor);
    if (saveableTab && !saveableTab->commit()) {
        return false;
    }
    delete editor;
    return true;
}

bool ProjectTabsWidget::hasUnsavedTabs() const
{
    for (int i = 0; i < count(); ++i) {
        auto saveableTab = qobject_cast<SaveableEditor *>(widget(i));
        if (saveableTab && saveableTab->isModified()) {
            return true;
        }
    }
    return false;
}

BaseEditor *ProjectTabsWidget::getTabByIdentifier(const QString &identifier) const
{
    for (int index = 0; index < count(); ++index) {
        BaseEditor *tab = static_cast<BaseEditor *>(widget(index));
        if (tab->property("identifier") == identifier) {
            return tab;
        }
    }
    return nullptr;
}
