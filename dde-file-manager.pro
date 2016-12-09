#-------------------------------------------------
#
# Project created by QtCreator 2015-06-24T09:14:17
#
#-------------------------------------------------
system($$PWD/vendor/prebuild)
include($$PWD/vendor/vendor.pri)

QT       += core gui svg dbus x11extras network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

isEmpty(TARGET) {
    TARGET = dde-file-manager
}

TEMPLATE = app

isEmpty(VERSION) {
    VERSION = 1.2
}

DEFINES += QMAKE_TARGET=\\\"$$TARGET\\\" QMAKE_VERSION=\\\"$$VERSION\\\"

isEmpty(QMAKE_ORGANIZATION_NAME) {
    DEFINES += QMAKE_ORGANIZATION_NAME=\\\"deepin\\\"
}

ARCH = $$QMAKE_HOST.arch
isEqual(ARCH, sw_64) | isEqual(ARCH, mips64) | isEqual(ARCH, mips32) {
    DEFINES += ARCH_MIPSEL CLASSICAL_SECTION LOAD_FILE_INTERVAL=150
    DEFINES += AUTO_RESTART_DEAMON
}

isEqual(ARCH, mips64) | isEqual(ARCH, mips32) {
    DEFINES += SPLICE_CP
    DEFINES += MENU_DIALOG_PLUGIN
}

isEmpty(PREFIX){
    PREFIX = /usr
}

include(./widgets/widgets.pri)
include(./dialogs/dialogs.pri)
include(./utils/utils.pri)
include(./filemonitor/filemonitor.pri)
include(./deviceinfo/deviceinfo.pri)
include(./dbusinterface/dbusinterface.pri)
include(./simpleini/simpleini.pri)
include($$PWD/chinese2pinyin/chinese2pinyin.pri)
include($$PWD/xdnd/xdnd.pri)

isEqual(ARCH, sw_64){
    isEqual(ENABLE_SW_LABLE, YES){
        DEFINES += SW_LABEL
        include(./sw_label/sw_label.pri)
        LIBS += -L$$PWD/sw_label -lfilemanager -lllsdeeplabel

        sw_label.path = /usr/lib/sw_64-linux-gnu/
        sw_label.files = $$PWD/sw_label/*.so
        INSTALLS += sw_label
    }
}

PKGCONFIG += x11 gtk+-2.0 xcb xcb-ewmh gsettings-qt libudev x11 xext libsecret-1 gio-unix-2.0 libstartup-notification-1.0 xcb-aux
CONFIG += c++11 link_pkgconfig
#DEFINES += QT_NO_DEBUG_OUTPUT
DEFINES += QT_MESSAGELOGCONTEXT

LIBS += -lmagic
# Automating generation .qm files from .ts files
# system($$PWD/desktop/translate_generation.sh)

CONFIG(debug, debug|release) {
#    LIBS += -lprofiler
#    DEFINES += ENABLE_PPROF
}

RESOURCES += \
    skin/skin.qrc \
    skin/dialogs.qrc \
    skin/filemanager.qrc \
    filemanager/themes/themes.qrc

HEADERS += \
    filemanager/app/define.h \
    filemanager/app/global.h \
    filemanager/controllers/appcontroller.h \
    filemanager/app/filemanagerapp.h \
    filemanager/views/dmovablemainwindow.h \
    filemanager/views/dleftsidebar.h \
    filemanager/views/dtoolbar.h \
    filemanager/views/dfileview.h \
    filemanager/views/ddetailview.h \
    filemanager/views/dicontextbutton.h \
    filemanager/views/dstatebutton.h \
    filemanager/views/dcheckablebutton.h \
    filemanager/models/dfilesystemmodel.h \
    filemanager/controllers/filecontroller.h \
    filemanager/app/filesignalmanager.h \
    filemanager/views/fileitem.h \
    filemanager/views/filemenumanager.h \
    filemanager/views/dsearchbar.h \
    filemanager/views/dfileitemdelegate.h \
    filemanager/models/fileinfo.h \
    filemanager/models/desktopfileinfo.h \
    filemanager/shutil/iconprovider.h \
    filemanager/models/bookmark.h \
    filemanager/models/imagefileinfo.h \
    filemanager/models/searchhistory.h \
    filemanager/models/fmsetting.h \
    filemanager/models/fmstate.h \
    filemanager/controllers/bookmarkmanager.h \
    filemanager/controllers/recenthistorymanager.h \
    filemanager/controllers/fmstatemanager.h \
    filemanager/controllers/basemanager.h \
    filemanager/dialogs/dialogmanager.h \
    filemanager/controllers/searchhistroymanager.h \
    filemanager/views/windowmanager.h \
    filemanager/shutil/desktopfile.h \
    filemanager/shutil/fileutils.h \
    filemanager/shutil/properties.h \
    filemanager/views/dfilemanagerwindow.h \
    filemanager/views/dcrumbwidget.h \
    filemanager/views/dcrumbbutton.h \
    filemanager/views/dhorizseparator.h \
    filemanager/app/fmevent.h \
    filemanager/views/historystack.h\
    filemanager/dialogs/propertydialog.h \
    filemanager/controllers/filejob.h \
    filemanager/views/dfilemenu.h \
    filemanager/views/dhoverbutton.h \
    filemanager/views/dbookmarkscene.h \
    filemanager/views/dbookmarkitem.h \
    filemanager/views/dbookmarkitemgroup.h \
    filemanager/views/dbookmarkrootitem.h \
    filemanager/views/dbookmarkview.h \
    filemanager/controllers/trashmanager.h \
    filemanager/views/dsplitter.h \
    filemanager/models/abstractfileinfo.h \
    filemanager/controllers/fileservices.h \
    filemanager/controllers/abstractfilecontroller.h \
    filemanager/models/recentfileinfo.h \
    filemanager/app/singleapplication.h \
    filemanager/app/logutil.h \
    filemanager/models/trashfileinfo.h \
    filemanager/shutil/mimesappsmanager.h \
    filemanager/views/dbookmarkline.h \
    filemanager/views/dsplitterhandle.h \
    filemanager/dialogs/openwithdialog.h \
    filemanager/models/durl.h \
    filemanager/controllers/searchcontroller.h \
    filemanager/models/searchfileinfo.h\
    filemanager/shutil/standardpath.h \
    filemanager/dialogs/basedialog.h \
    filemanager/models/ddiriterator.h \
    filemanager/views/extendview.h \
    filemanager/controllers/pathmanager.h \
    filemanager/views/ddragwidget.h \
    filemanager/shutil/mimetypedisplaymanager.h \
    filemanager/views/dstatusbar.h \
    filemanager/controllers/subscriber.h \
    filemanager/shutil/thumbnailmanager.h \
    filemanager/models/menuactiontype.h \
    filemanager/models/dfileselectionmodel.h \
    filemanager/dialogs/closealldialogindicator.h \
    filemanager/gvfs/gvfsmountclient.h \
    filemanager/gvfs/mountaskpassworddialog.h \
    filemanager/gvfs/networkmanager.h \
    filemanager/gvfs/secrectmanager.h \
    filemanager/models/networkfileinfo.h \
    filemanager/controllers/networkcontroller.h \
    filemanager/dialogs/openwithotherdialog.h \
    filemanager/dialogs/trashpropertydialog.h \
    filemanager/views/dbookmarkmountedindicatoritem.h \
    filemanager/views/deditorwidgetmenu.h \
    filemanager/controllers/jobcontroller.h \
    filemanager/shutil/filessizeworker.h \
    filemanager/views/computerview.h \
    filemanager/views/flowlayout.h \
    filemanager/shutil/shortcut.h \
    mips/plugin/ddefileinterface.h \
    mips/plugin/pluginmanagerapp.h



SOURCES += \
    filemanager/controllers/appcontroller.cpp \
    filemanager/main.cpp \
    filemanager/app/filemanagerapp.cpp \
    filemanager/views/dmovablemainwindow.cpp \
    filemanager/views/dleftsidebar.cpp \
    filemanager/views/dtoolbar.cpp \
    filemanager/views/dfileview.cpp \
    filemanager/views/ddetailview.cpp \
    filemanager/views/dicontextbutton.cpp \
    filemanager/views/dstatebutton.cpp \
    filemanager/views/dcheckablebutton.cpp \
    filemanager/models/dfilesystemmodel.cpp \
    filemanager/controllers/filecontroller.cpp \
    filemanager/views/fileitem.cpp \
    filemanager/views/filemenumanager.cpp \
    filemanager/views/dsearchbar.cpp \
    filemanager/views/dfileitemdelegate.cpp \
    filemanager/models/fileinfo.cpp \
    filemanager/models/desktopfileinfo.cpp \
    filemanager/shutil/iconprovider.cpp \
    filemanager/models/bookmark.cpp \
    filemanager/models/imagefileinfo.cpp \
    filemanager/models/searchhistory.cpp \
    filemanager/models/fmsetting.cpp \
    filemanager/models/fmstate.cpp \
    filemanager/controllers/bookmarkmanager.cpp \
    filemanager/controllers/recenthistorymanager.cpp \
    filemanager/controllers/fmstatemanager.cpp \
    filemanager/controllers/basemanager.cpp \
    filemanager/dialogs/dialogmanager.cpp \
    filemanager/controllers/searchhistroymanager.cpp \
    filemanager/views/windowmanager.cpp \
    filemanager/shutil/desktopfile.cpp \
    filemanager/shutil/fileutils.cpp \
    filemanager/shutil/properties.cpp \
    filemanager/views/dfilemanagerwindow.cpp \
    filemanager/views/dcrumbwidget.cpp \
    filemanager/views/dcrumbbutton.cpp \
    filemanager/views/dhorizseparator.cpp \
    filemanager/app/fmevent.cpp \
    filemanager/views/historystack.cpp\
    filemanager/dialogs/propertydialog.cpp \
    filemanager/controllers/filejob.cpp \
    filemanager/views/dfilemenu.cpp \
    filemanager/views/dhoverbutton.cpp \
    filemanager/views/dbookmarkscene.cpp \
    filemanager/views/dbookmarkitem.cpp \
    filemanager/views/dbookmarkitemgroup.cpp \
    filemanager/views/dbookmarkrootitem.cpp \
    filemanager/views/dbookmarkview.cpp \
    filemanager/controllers/trashmanager.cpp \
    filemanager/views/dsplitter.cpp \
    filemanager/models/abstractfileinfo.cpp \
    filemanager/controllers/fileservices.cpp \
    filemanager/controllers/abstractfilecontroller.cpp \
    filemanager/models/recentfileinfo.cpp \
    filemanager/app/singleapplication.cpp \
    filemanager/app/logutil.cpp \
    filemanager/models/trashfileinfo.cpp \
    filemanager/shutil/mimesappsmanager.cpp \
    filemanager/views/dbookmarkline.cpp \
    filemanager/views/dsplitterhandle.cpp \
    filemanager/dialogs/openwithdialog.cpp \
    filemanager/models/durl.cpp \
    filemanager/controllers/searchcontroller.cpp \
    filemanager/models/searchfileinfo.cpp\
    filemanager/shutil/standardpath.cpp \
    filemanager/dialogs/basedialog.cpp \
    filemanager/views/extendview.cpp \
    filemanager/controllers/pathmanager.cpp \
    filemanager/views/ddragwidget.cpp \
    filemanager/shutil/mimetypedisplaymanager.cpp \
    filemanager/views/dstatusbar.cpp \
    filemanager/controllers/subscriber.cpp \
    filemanager/shutil/thumbnailmanager.cpp \
    filemanager/models/menuactiontype.cpp \
    filemanager/models/dfileselectionmodel.cpp \
    filemanager/dialogs/closealldialogindicator.cpp \
    filemanager/app/global.cpp \
    filemanager/gvfs/gvfsmountclient.cpp \
    filemanager/gvfs/mountaskpassworddialog.cpp \
    filemanager/gvfs/networkmanager.cpp \
    filemanager/gvfs/secrectmanager.cpp \
    filemanager/models/networkfileinfo.cpp \
    filemanager/controllers/networkcontroller.cpp \
    filemanager/dialogs/openwithotherdialog.cpp \
    filemanager/dialogs/trashpropertydialog.cpp \
    filemanager/views/dbookmarkmountedindicatoritem.cpp \
    filemanager/views/deditorwidgetmenu.cpp \
    filemanager/controllers/jobcontroller.cpp \
    filemanager/shutil/filessizeworker.cpp \
    filemanager/views/computerview.cpp \
    filemanager/views/flowlayout.cpp \
    filemanager/shutil/shortcut.cpp \
    mips/plugin/pluginmanagerapp.cpp



INCLUDEPATH += filemanager/models

BINDIR = $$PREFIX/bin
APPSHAREDIR = $$PREFIX/share/$$TARGET
HELPSHAREDIR = $$PREFIX/share/dman/$$TARGET
ICONDIR = $$PREFIX/share/icons/hicolor/scalable/apps
DEFINES += APPSHAREDIR=\\\"$$APPSHAREDIR\\\"

target.path = $$BINDIR

desktop.path = $${PREFIX}/share/applications/

isEqual(ARCH, sw_64) | isEqual(ARCH, mips64) | isEqual(ARCH, mips32) {
    desktop.files = $$PWD/mips/$${TARGET}.desktop
}else{
    desktop.files = $${TARGET}.desktop
}

templateFiles.path = $$APPSHAREDIR/templates
templateFiles.files = skin/templates/newDoc.doc \
    skin/templates/newExcel.xls \
    skin/templates/newPowerPoint.ppt \
    skin/templates/newTxt.txt


mimetypeFiles.path = $$APPSHAREDIR/mimetypes
mimetypeFiles.files += \
    mimetypes/archive.mimetype \
    mimetypes/text.mimetype \
    mimetypes/video.mimetype \
    mimetypes/audio.mimetype \
    mimetypes/image.mimetype \
    mimetypes/executable.mimetype

TRANSLATIONS += $$PWD/translations/$${TARGET}.ts \
    $$PWD/translations/$${TARGET}_zh_CN.ts

# Automating generation .qm files from .ts files
CONFIG(release, debug|release) {
    system($$PWD/generate_translations.sh)
}

translations.path = $$APPSHAREDIR/translations
translations.files = translations/*.qm


help.path = $$HELPSHAREDIR
help.files = help/*

icon.path = $$ICONDIR
icon.files = skin/images/$${TARGET}.svg

dde-xdg-user-dirs-update.path = $$BINDIR
dde-xdg-user-dirs-update.files = $$PWD/dde-xdg-user-dirs-update.sh

INSTALLS += target desktop templateFiles translations mimetypeFiles help icon dde-xdg-user-dirs-update

isEqual(ARCH, sw_64) | isEqual(ARCH, mips64) | isEqual(ARCH, mips32) {
    dde-mips-shs.path = $$BINDIR
    dde-mips-shs.files = $$PWD/mips/dde-computer.sh \
                         $$PWD/mips/dde-trash.sh \
                         $$PWD/mips/file-manager.sh

    dde-file-manager-autostart.path = /etc/xdg/autostart
    dde-file-manager-autostart.files = $$PWD/mips/dde-file-manager-autostart.desktop

    INSTALLS += dde-mips-shs dde-file-manager-autostart
}else{
    xdg_autostart.path = /etc/xdg/autostart
    xdg_autostart.files = dde-file-manager-xdg-autostart.desktop
    INSTALLS += xdg_autostart
}
