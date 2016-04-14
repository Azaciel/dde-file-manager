#ifndef MIMESAPPSMANAGER_H
#define MIMESAPPSMANAGER_H


#include <QObject>
#include <QSet>
#include <QMimeType>
#include <QMap>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include "desktopfile.h"


class MimeAppsWorker: public QObject
{
   Q_OBJECT

public:
    MimeAppsWorker(QObject *parent = 0);
    ~MimeAppsWorker();

    void initConnect();
public slots:
    void startWatch();
    void handleDirectoryChanged();
    void handleFileChanged();
    void updateCache();

private:
    QFileSystemWatcher* m_fileSystemWatcher = NULL;
};


class MimesAppsManager: public QObject
{
    Q_OBJECT

public:
    MimesAppsManager(QObject *parent = 0);
    ~MimesAppsManager();

    static QStringList DesktopFiles;
    static QMap<QString, QStringList> MimeApps;
    static QMap<QString, DesktopFile> DesktopObjs;

    static QString getMimeTypeByFileName(const QString& fileName);
    static QString getDefaultAppByFileName(const QString& fileName);
    static QString getDefaultAppByMimeType(const QMimeType& mimeType);
    static QString getDefaultAppByMimeType(const QString& mimeType);

    static QStringList getApplicationsFolders();
    static QStringList getDesktopFiles();
    static QMap<QString, DesktopFile> getDesktopObjs();
    static QMap<QString, QStringList> getMimeTypeApps();
    static bool lessByDateTime(const QFileInfo& f1,  const QFileInfo& f2);

signals:
    void requestUpdateCache();

private:
    MimeAppsWorker* m_mimeAppsWorker=NULL;

};

#endif // MIMESAPPSMANAGER_H