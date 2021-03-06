#include "windows/androidexplorer.h"
#include "windows/dialogs.h"
#include "widgets/deselectablelistview.h"
#include "widgets/loadingwidget.h"
#include "widgets/logview.h"
#include "widgets/toolbar.h"
#include "tools/adb.h"
#include "apk/logmodel.h"
#include "base/androidfilesystemmodel.h"
#include "base/application.h"
#include "base/settings.h"
#include "base/utils.h"
#include <QBoxLayout>
#include <QDockWidget>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QToolButton>

#ifdef QT_DEBUG
    #include <QDebug>
#endif

AndroidExplorer::AndroidExplorer(const QString &serial, QWidget *parent)
    : QMainWindow(parent)
    , serial(serial)
    , fileSystemModel(new AndroidFileSystemModel(serial, this))
{
    setCentralWidget(new QWidget(this));
    setWindowIcon(QIcon::fromTheme("tool-androidexplorer"));
    resize(Utils::scale(600, 540));

    actionDownload = new QAction(QIcon::fromTheme("download"), {}, this);
    actionDownload->setShortcut(QKeySequence::Save);
    connect(actionDownload, &QAction::triggered, this, [this]() {
        const auto index = fileList->currentIndex();
        if (index.isValid()) {
            const auto path = fileSystemModel->getItemPath(index);
            download(path);
        }
    });

    actionUpload = new QAction(QIcon::fromTheme("upload"), {}, this);
    actionUpload->setShortcut(QKeySequence("Ctrl+U"));
    connect(actionUpload, &QAction::triggered, this, [this]() {
        QString path = fileSystemModel->getCurrentPath();
        const auto index = fileList->currentIndex();
        if (index.isValid()) {
            if (fileSystemModel->getItemType(index) == AndroidFileSystemItem::AndroidFSDirectory) {
                path = fileSystemModel->getItemPath(index);
            }
        }
        upload(path);
    });

    actionCopy = new QAction(QIcon::fromTheme("edit-copy"), {}, this);
    actionCopy->setShortcut(QKeySequence::Copy);
    connect(actionCopy, &QAction::triggered, this, [this]() {
        const auto index = fileList->currentIndex();
        setClipboard(index, false);
    });

    actionCut = new QAction(QIcon::fromTheme("edit-cut"), {}, this);
    actionCut->setShortcut(QKeySequence::Cut);
    connect(actionCut, &QAction::triggered, this, [this]() {
        const auto index = fileList->currentIndex();
        setClipboard(index, true);
    });

    actionPaste = new QAction(QIcon::fromTheme("edit-paste"), {}, this);
    actionPaste->setEnabled(false);
    actionPaste->setShortcut(QKeySequence::Paste);
    connect(actionPaste, &QAction::triggered, this, [this]() {
        const QString src = clipboard.data;
        QString dst = fileSystemModel->getCurrentPath();
        const auto index = fileList->currentIndex();
        if (index.isValid()) {
            if (fileSystemModel->getItemType(index) == AndroidFileSystemItem::AndroidFSDirectory) {
                dst = fileSystemModel->getItemPath(index);
            }
        }
        if (clipboard.move) {
            move(src, dst);
            setClipboard({});
        } else {
            copy(src, dst);
        }
    });

    actionRename = new QAction(QIcon::fromTheme("edit-rename"), {}, this);
    actionRename->setShortcut(QKeySequence("F2"));
    connect(actionRename, &QAction::triggered, this, [this]() {
        const auto index = fileList->currentIndex();
        if (index.isValid()) {
            fileList->edit(index);
        }
    });

    actionDelete = new QAction(QIcon::fromTheme("edit-delete"), {}, this);
    actionDelete->setShortcut(QKeySequence::Delete);
    connect(actionDelete, &QAction::triggered, this, [this]() {
        remove(fileList->currentIndex());
    });

    auto actionInstall = app->actions.getInstallApk(this);
    connect(actionInstall, &QAction::triggered, this, &AndroidExplorer::install);

    auto actionScreenshot = app->actions.getTakeScreenshot(serial, this);

    menuFile = new QMenu(this);
    menuFile->addAction(actionDownload);
    menuFile->addAction(actionUpload);
    menuFile->addSeparator();
    menuFile->addAction(actionInstall);
    menuBar()->addMenu(menuFile);

    menuEdit = new QMenu(this);
    menuEdit->addAction(actionCopy);
    menuEdit->addAction(actionCut);
    menuEdit->addAction(actionPaste);
    menuEdit->addAction(actionRename);
    menuEdit->addAction(actionDelete);
    menuBar()->addMenu(menuEdit);

    menuTools = new QMenu(this);
    menuTools->addAction(actionScreenshot);
    menuBar()->addMenu(menuTools);

    menuSettings = new QMenu(this);
    menuSettings->addMenu(app->actions.getLanguages(this));
    menuBar()->addMenu(menuSettings);

    menuWindow = new QMenu(this);
    menuBar()->addMenu(menuWindow);

    toolbar = new Toolbar(this);
    toolbar->setObjectName("Toolbar");
    toolbar->addActionToPool("download", actionDownload);
    toolbar->addActionToPool("upload", actionUpload);
    toolbar->addActionToPool("copy", actionCopy);
    toolbar->addActionToPool("cut", actionCut);
    toolbar->addActionToPool("paste", actionPaste);
    toolbar->addActionToPool("rename", actionRename);
    toolbar->addActionToPool("delete", actionDelete);
    toolbar->addActionToPool("install", actionInstall);
    toolbar->addActionToPool("screenshot", actionScreenshot);
    toolbar->initialize(app->settings->getAndroidExplorerToolbar());
    addToolBar(toolbar);
    connect(toolbar, &Toolbar::updated, app->settings, &Settings::setAndroidExplorerToolbar);

    auto fileSelectionActions = new QActionGroup(this);
    fileSelectionActions->setEnabled(false);
    fileSelectionActions->setExclusive(false);
    fileSelectionActions->addAction(actionDownload);
    fileSelectionActions->addAction(actionCopy);
    fileSelectionActions->addAction(actionCut);
    fileSelectionActions->addAction(actionRename);
    fileSelectionActions->addAction(actionDelete);

    pathUpButton = new QToolButton(this);
    pathUpButton->setIcon(QIcon::fromTheme("go-up"));
    connect(pathUpButton, &QToolButton::clicked, this, &AndroidExplorer::goUp);

    auto pathUpShortcut = new QShortcut(this);
    pathUpShortcut->setKey(QKeySequence::Back);
    connect(pathUpShortcut, &QShortcut::activated, this, &AndroidExplorer::goUp);

    pathGoButton = new QToolButton(this);
    pathGoButton->setIcon(QIcon::fromTheme(layoutDirection() == Qt::LeftToRight ? "go-next" : "go-previous"));
    connect(pathGoButton, &QToolButton::clicked, this, [this]() {
        go(pathInput->text());
    });

    pathInput = new QLineEdit("/", this);
    connect(pathInput, &QLineEdit::returnPressed, pathGoButton, &QToolButton::click);

    auto pathBar = new QHBoxLayout;
    pathBar->setSpacing(2);
    pathBar->addWidget(pathUpButton);
    pathBar->addWidget(pathInput);
    pathBar->addWidget(pathGoButton);

    fileList = new DeselectableListView(this);
    fileList->setModel(fileSystemModel);
    fileList->setContextMenuPolicy(Qt::CustomContextMenu);
    fileList->setEditTriggers(QListView::SelectedClicked | QListView::EditKeyPressed);
    connect(fileList, &QListView::activated, this, [this](const QModelIndex &index) {
        const auto type = fileSystemModel->getItemType(index);
        const auto path = fileSystemModel->getItemPath(index);
        switch (type) {
        case AndroidFileSystemItem::AndroidFSDirectory:
            go(path);
            break;
        case AndroidFileSystemItem::AndroidFSFile:
            download(path);
            break;
        }
    });
    connect(fileList, &QListView::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu context(this);
        context.addSeparator();
        context.addAction(actionDownload);
        context.addAction(actionUpload);
        context.addSeparator();
        context.addAction(actionCopy);
        context.addAction(actionCut);
        context.addAction(actionPaste);
        context.addAction(actionRename);
        context.addAction(actionDelete);
        context.exec(fileList->viewport()->mapToGlobal(point));
    });
    connect(fileList->selectionModel(), &QItemSelectionModel::currentChanged,
            fileSelectionActions, [fileSelectionActions](const QModelIndex &index) {
        fileSelectionActions->setEnabled(index.isValid());
    });

    auto loading = new LoadingWidget(fileList);

    connect(fileSystemModel, &AndroidFileSystemModel::pathChanged, this, [=](const QString &path) {
        pathInput->setText(path);
        fileSelectionActions->setEnabled(false);
    });
    connect(fileSystemModel, &AndroidFileSystemModel::modelAboutToBeReset, this, [=]() {
        loading->show();
        fileSelectionActions->setEnabled(false);
    });
    connect(fileSystemModel, &AndroidFileSystemModel::modelReset, this, [=]() {
        loading->hide();
        fileList->scrollToTop();
    });
    connect(fileSystemModel, &AndroidFileSystemModel::error, this, [this](const QString &error) {
        QMessageBox::warning(this, QString(), error);
    });

    auto logView = new LogView(this);
    logView->setModel(logModel = new LogModel(this));
    logDock = new QDockWidget(this);
    logDock->setWidget(logView);
    logDock->setObjectName("DockLog");
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    auto layout = new QVBoxLayout(centralWidget());
    layout->addLayout(pathBar);
    layout->addWidget(fileList);

    restoreGeometry(app->settings->getAndroidExplorerGeometry());
    restoreState(app->settings->getAndroidExplorerState());

    retranslate();
}

void AndroidExplorer::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QMainWindow::changeEvent(event);
}

void AndroidExplorer::closeEvent(QCloseEvent *event)
{
    app->settings->setAndroidExplorerGeometry(saveGeometry());
    app->settings->setAndroidExplorerState(saveState());
}

void AndroidExplorer::go(const QString &directory)
{
    fileSystemModel->cd(directory);
}

void AndroidExplorer::goUp()
{
    go("..");
}

void AndroidExplorer::download(const QString &path)
{
    const auto dst = Dialogs::getSaveFilename(path, this);
    if (dst.isEmpty()) {
        return;
    }

    fileSystemModel->download(path, dst);
}

void AndroidExplorer::upload(const QString &path)
{
    const auto src = Dialogs::getOpenFilename(this);
    if (src.isEmpty()) {
        return;
    }

    fileSystemModel->upload(src, path);
}

void AndroidExplorer::copy(const QString &src, const QString &dst)
{
    fileSystemModel->copy(src, dst);
}

void AndroidExplorer::move(const QString &src, const QString &dst)
{
    fileSystemModel->move(src, dst);
}

void AndroidExplorer::remove(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    QString question;
    switch (fileSystemModel->getItemType(index)) {
    case AndroidFileSystemItem::AndroidFSFile:
        question = tr("Are you sure you want to delete this file?");
        break;
    case AndroidFileSystemItem::AndroidFSDirectory:
        question = tr("Are you sure you want to delete this directory?");
        break;
    }
    if (QMessageBox::question(this, {}, question) == QMessageBox::Yes) {
        const auto path = fileSystemModel->getItemPath(index);
        fileSystemModel->remove(path);
    }
}

void AndroidExplorer::install()
{
    const QStringList paths = Dialogs::getOpenApkFilenames(this);
    for (const QString &path : paths) {
        auto install = new Adb::Install(path, serial);
        //: "%1" will be replaced with a path to the APK.
        const QPersistentModelIndex entryIndex(logModel->add(tr("Installing %1...").arg(path)));
        connect(install, &Command::finished, this, [=](bool success) {
            if (success) {
                if (entryIndex.isValid()) {
                    //: "%1" will be replaced with a path to the APK.
                    logModel->update(entryIndex, tr("Successfully installed %1").arg(path), {}, LogEntry::Success);
                }
            } else {
                if (entryIndex.isValid()) {
                    //: "%1" will be replaced with a path to the APK.
                    logModel->update(entryIndex, tr("Could not install %1").arg(path),
                                      install->output(), LogEntry::Error);
                }
            }
            install->deleteLater();
        });
        install->run();
    }
}

void AndroidExplorer::setClipboard(const QModelIndex &index, bool move)
{
    const bool isValid = index.isValid();
    clipboard.data = isValid ? fileSystemModel->getItemPath(index) : QString();
    clipboard.move = move;
    actionPaste->setEnabled(isValid);
}

void AndroidExplorer::retranslate()
{
    setWindowTitle(tr("Android Explorer"));
    actionDownload->setText(tr("Download"));
    actionUpload->setText(tr("Upload"));
    actionCopy->setText(tr("Copy"));
    actionCut->setText(tr("Cut"));
    actionPaste->setText(tr("Paste"));
    actionRename->setText(tr("Rename"));
    actionDelete->setText(tr("Delete"));
    //: Navigate up one directory in a file manager hierarchy.
    pathUpButton->setText(tr("Up"));
    pathUpButton->setToolTip(tr("Up"));
    //: Navigate to a directory in a file manager.
    pathGoButton->setText(tr("Go"));
    pathGoButton->setToolTip(tr("Go"));
    logDock->setWindowTitle(tr("Tasks"));

    menuFile->setTitle(qApp->translate("MainWindow", "&File"));
    //: Refers to a menu bar (along with File, View, Window, Help, and similar items).
    menuEdit->setTitle(tr("&Edit"));
    menuTools->setTitle(qApp->translate("MainWindow", "&Tools"));
    menuSettings->setTitle(qApp->translate("MainWindow", "&Settings"));
    menuWindow->setTitle(qApp->translate("MainWindow", "&Window"));
    menuWindow->clear();
    menuWindow->addActions(createPopupMenu()->actions());
    toolbar->setWindowTitle(qApp->translate("MainWindow", "Tools"));
}
