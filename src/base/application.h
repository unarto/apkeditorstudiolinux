#ifndef APPLICATION_H
#define APPLICATION_H

#include "apk/packagelistmodel.h"
#include "base/actionprovider.h"
#include "base/language.h"
#include <SingleApplication>
#include <KSyntaxHighlighting/Repository>
#include <QTranslator>

class MainWindow;
class Settings;

class Application : public SingleApplication
{
    Q_OBJECT

public:
    Application(int &argc, char **argv);
    ~Application() override;

    int exec();

    static QList<Language> getLanguages();

    MainWindow *createNewInstance();
    void setLanguage(const QString &locale);
    void setTheme(const QString &theme);

    Settings *settings;
    ActionProvider actions;
    KSyntaxHighlighting::Repository highlightingRepository;

protected:
    bool event(QEvent *event) override;

private:
    void start(quint32 instanceId = 0, QByteArray message = {});
    void startStudio(const QStringList &args);
    void startStudioInstance(const QStringList &args);
    void startExplorer();

    QList<MainWindow *> instances;
    PackageListModel packages;
    QTranslator translator;
    QTranslator translatorQt;
};

#define app (static_cast<Application *>(qApp))

#endif // APPLICATION_H
