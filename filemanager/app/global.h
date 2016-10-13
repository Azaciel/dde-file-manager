#ifndef GLOBAL_H
#define GLOBAL_H

//! Warning: Don't include file here
#include <QFontMetrics>
#include <QTextOption>
#include <QTextLayout>
// Warning: Don't include file here

#define fileManagerApp Singleton<FileManagerApp>::instance()
#define searchHistoryManager  Singleton<SearchHistroyManager>::instance()
#define bookmarkManager  Singleton<BookMarkManager>::instance()
#define trashManager  Singleton<TrashManager>::instance()
#define fileMenuManger  Singleton<FileMenuManager>::instance()
#define fileSignalManager Singleton<FileSignalManager>::instance()
#define dialogManager Singleton<DialogManager>::instance()
#define appController fileManagerApp->getAppController()
#define fileIconProvider Singleton<IconProvider>::instance()
#define fileService FileServices::instance()
#define deviceListener Singleton<UDiskListener>::instance()
#define mimeAppsManager Singleton<MimesAppsManager>::instance()
#define systemPathManager Singleton<PathManager>::instance()
#define mimeTypeDisplayManager Singleton<MimeTypeDisplayManager>::instance()
#define thumbnailManager Singleton<ThumbnailManager>::instance()
#define networkManager Singleton<NetworkManager>::instance()
#define gvfsMountClient Singleton<GvfsMountClient>::instance()
#define secrectManager Singleton<SecrectManager>::instance()

#ifdef MENU_DIALOG_PLUGIN
#define pluginManagerApp Singleton<PluginManagerApp>::instance()  // by txx
#endif

#define defaut_icon ":/images/images/default.png"
#define defaut_computerIcon ":/images/images/computer.png"
#define defaut_trashIcon ":/images/images/user-trash-full.png"

#define ComputerUrl "computer://"

#define TRASH_SCHEME "trash"
#define RECENT_SCHEME "recent"
#define BOOKMARK_SCHEME "bookmark"
#define FILE_SCHEME "file"
#define COMPUTER_SCHEME "computer"
#define SEARCH_SCHEME "search"
#define NETWORK_SCHEME "network"
#define SMB_SCHEME "smb"
#define AFC_SCHEME "afc"
#define MTP_SCHEME "mtp"
#define GPHOTO2_SCHEME "gphoto2"

#define TRASH_ROOT "trash:///"
#define RECENT_ROOT "recent:///"
#define BOOKMARK_ROOT "bookmark:///"
#define FILE_ROOT  "file:///"
#define COMPUTER_ROOT "computer:///"
#define NETWORK_ROOT "network:///"
#define SMB_ROOT "smb:///"

#define TRASHPATH QDir::homePath() + "/.local/share/Trash"
#define TRASHFILEPATH TRASHPATH + "/files"
#define TRASHINFOPATH TRASHPATH + "/info"

// begin file item global define
#define TEXT_LINE_HEIGHT 18
#define TEXT_PADDING 5
#define ICON_MODE_ICON_SPACING 5
#define COLUMU_PADDING 10
#define LEFT_PADDING 10
#define RIGHT_PADDING 10
// end

// begin file view global define
#define LIST_MODE_LEFT_MARGIN 20
#define LIST_MODE_RIGHT_MARGIN 20
// end

#define MAX_THREAD_COUNT 1000

#define MAX_FILE_NAME_CHAR_COUNT 255

#define ASYN_CALL(Fun, Code, captured...) { \
    QDBusPendingCallWatcher * watcher = new QDBusPendingCallWatcher(Fun); \
    auto onFinished = [watcher, captured]{ \
        const QVariantList & args = watcher->reply().arguments(); \
        Q_UNUSED(args);\
        Code \
        watcher->deleteLater(); \
    };\
    if(watcher->isFinished()) onFinished();\
    else QObject::connect(watcher, &QDBusPendingCallWatcher::finished, onFinished);}

#if QT_VERSION >= 0x050500
#define TIMER_SINGLESHOT(Time, Code, captured...){ \
    QTimer::singleShot(Time, [captured] {Code});\
}
#else
#define TIMER_SINGLESHOT(Time, Code, captured...){ \
    QTimer *timer = new QTimer;\
    timer->setSingleShot(true);\
    QObject::connect(timer, &QTimer::timeout, [timer, captured] {\
        timer->deleteLater();\
        Code\
    });\
    timer->start(Time);\
}
#endif

#define TIMER_SINGLESHOT_CONNECT_TYPE(Obj, Time, Code, ConnectType, captured...){ \
    QTimer *timer = new QTimer;\
    timer->setSingleShot(true);\
    QObject::connect(timer, &QTimer::timeout, Obj, [timer, captured] {\
        timer->deleteLater();\
        Code\
    }, ConnectType);\
    timer->start(Time);\
}

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
#define TIMER_SINGLESHOT_OBJECT(Obj, Time, Code, captured...)\
    TIMER_SINGLESHOT(Obj, Time, Code, Qt::AutoConnection, captured)
#else
#define TIMER_SINGLESHOT_OBJECT(Obj, Time, Code, captured...)\
    QTimer::singleShot(Time, Obj, [captured]{Code});
#endif

#define ASYN_CALL_SLOT(obj, fun, args...) \
    TIMER_SINGLESHOT_CONNECT_TYPE(obj, 0, {obj->fun(args);}, Qt::QueuedConnection, obj, args)

class Global
{
public:
    static QString wordWrapText(const QString &text, int width,
                         QTextOption::WrapMode wrapMode,
                         int *height = 0);

    static QString elideText(const QString &text, const QSize &size,
                      const QFontMetrics &fontMetrics,
                      QTextOption::WrapMode wordWrap,
                      Qt::TextElideMode mode,
                      int flags = 0);

    static QString toPinyin(const QString &text);
    static bool startWithHanzi(const QString &text);
    template<typename T>
    static bool startWithHanzi(T)
    { return false;}

    static bool keyShiftIsPressed();
    static bool keyCtrlIsPressed();
    static bool fileNameCorrection(const QString &filePath);
    static bool fileNameCorrection(const QByteArray &filePath);
};

#endif // GLOBAL_H
