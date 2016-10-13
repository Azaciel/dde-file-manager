#include "abstractfileinfo.h"

#include "../views/dfileview.h"

#include "../models/dfilesystemmodel.h"

#include "../shutil/fileutils.h"
#include "../shutil/mimetypedisplaymanager.h"

#include "../controllers/pathmanager.h"
#include "../controllers/fileservices.h"

#include "../app/global.h"

#include "widgets/singleton.h"


#include <QDateTime>
#include <QDebug>
#include <QApplication>

#ifdef SW_LABEL
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "sw_label/filemanager.h"
#include "../views/filemenumanager.h"
#endif

#ifdef MENU_DIALOG_PLUGIN
#include "mips/plugin/pluginmanagerapp.h"  // by txx
#endif

namespace FileSortFunction {
Qt::SortOrder sortOrderGlobal;
AbstractFileInfo::sortFunction sortFun;

bool sortByString(const QString &str1, const QString &str2, Qt::SortOrder order)
{
    if (Global::startWithHanzi(str1)) {
        if (!Global::startWithHanzi(str2))
            return order != Qt::AscendingOrder;
    } else if (Global::startWithHanzi(str2)) {
        return order == Qt::AscendingOrder;
    }

    return ((order == Qt::DescendingOrder) ^ (Global::toPinyin(str1).toLower() < Global::toPinyin(str2).toLower())) == 0x01;
}

SORT_FUN_DEFINE(displayName, DisplayName, AbstractFileInfo)
SORT_FUN_DEFINE(size, Size, AbstractFileInfo)
SORT_FUN_DEFINE(lastModified, Modified, AbstractFileInfo)
SORT_FUN_DEFINE(mimeTypeDisplayNameOrder, Mime, AbstractFileInfo)
SORT_FUN_DEFINE(created, Created, AbstractFileInfo)

bool sort(const AbstractFileInfoPointer &info1, const AbstractFileInfoPointer &info2)
{
    return sortFun(info1, info2, sortOrderGlobal);
}
} /// end namespace FileSortFunction

QMap<DUrl, AbstractFileInfo::FileMetaData> AbstractFileInfo::metaDataCacheMap;

AbstractFileInfo::AbstractFileInfo()
    : data(new FileInfoData)
{
    init();
}

AbstractFileInfo::AbstractFileInfo(const DUrl &url)
    : data(new FileInfoData)
{
    data->url = url;
    data->fileInfo.setFile(url.path());

    init();
    updateFileMetaData();
//    updateFileInfo();
}

AbstractFileInfo::AbstractFileInfo(const QString &url)
    : data(new FileInfoData)
{
    data->url = DUrl::fromUserInput(url);
    data->fileInfo.setFile(data->url.path());

    init();
    updateFileMetaData();
//    updateFileInfo();
}

AbstractFileInfo::~AbstractFileInfo()
{
    delete data;
}

void AbstractFileInfo::setUrl(const DUrl &url)
{
    data->url = url;
    data->fileInfo.setFile(url.path());

    updateFileMetaData();
//    updateFileInfo();
}

bool AbstractFileInfo::exists() const
{
    return data->fileInfo.exists();
}

QString AbstractFileInfo::filePath() const
{
    if (data->filePath.isEmpty()){
        data->filePath = data->fileInfo.filePath();
    }
    return data->filePath;
}

QString AbstractFileInfo::absoluteFilePath() const
{
    if (data->absoluteFilePath.isEmpty()){
        data->absoluteFilePath = data->fileInfo.absoluteFilePath();
    }
    return data->absoluteFilePath;
}

QString AbstractFileInfo::baseName() const
{
    return data->fileInfo.baseName();
}

QString AbstractFileInfo::fileName() const
{
    if (data->fileName.isEmpty()){
        data->fileName = data->fileInfo.fileName();
    }
    return data->fileName;
}

QString AbstractFileInfo::displayName() const
{
    return fileName();
}

QString AbstractFileInfo::pinyinName() const
{
    const QString &diaplayName = this->displayName();

    if (data->pinyinName.isEmpty())
        data->pinyinName = Global::toPinyin(diaplayName);

    return data->pinyinName;
}

QString AbstractFileInfo::path() const
{
    if (data->path.isEmpty()){
        data->path = data->fileInfo.path();
    }
    return data->path;
}

QString AbstractFileInfo::absolutePath() const
{
    if (data->absolutePath.isEmpty()){
        data->absolutePath = data->fileInfo.absolutePath();
    }
    return data->absolutePath;
}

bool AbstractFileInfo::isReadable() const
{
    return metaData().isReadable;
}

bool AbstractFileInfo::isWritable() const
{
    return metaData().isWritable;
}

bool AbstractFileInfo::isExecutable() const
{
    return metaData().isExecutable;
}

bool AbstractFileInfo::isHidden() const
{
    return data->fileInfo.isHidden();
}

bool AbstractFileInfo::isRelative() const
{
    return data->fileInfo.isRelative();
}

bool AbstractFileInfo::isAbsolute() const
{
    return data->fileInfo.isAbsolute();
}

bool AbstractFileInfo::makeAbsolute()
{
    bool ok = data->fileInfo.makeAbsolute();

    data->url.setPath(data->fileInfo.filePath());

    return ok;
}

bool AbstractFileInfo::isFile() const
{
    return data->fileInfo.isFile();
}

bool AbstractFileInfo::isDir() const
{
    return data->fileInfo.isDir();
}

bool AbstractFileInfo::isSymLink() const
{
    return data->fileInfo.isSymLink();
}

bool AbstractFileInfo::isDesktopFile() const
{
    return mimeTypeName() == "application/x-desktop";
}

QString AbstractFileInfo::readLink() const
{
    return data->fileInfo.readLink();
}

QString AbstractFileInfo::owner() const
{
    return data->fileInfo.owner();
}

uint AbstractFileInfo::ownerId() const
{
    return data->fileInfo.ownerId();
}

QString AbstractFileInfo::group() const
{
    return data->fileInfo.group();
}

uint AbstractFileInfo::groupId() const
{
    return data->fileInfo.groupId();
}

QFileDevice::Permissions AbstractFileInfo::permissions() const
{
    return metaData().permissions;
}

qint64 AbstractFileInfo::size() const
{
    if (isFile()){
        if (data->size == -1){
            data->size = data->fileInfo.size();
        }
        return data->size;
    }else{
        return filesCount();
    }
}

qint64 AbstractFileInfo::filesCount() const
{
    return FileUtils::filesCount(data->fileInfo.absoluteFilePath());
}

QDateTime AbstractFileInfo::created() const
{
    if (data->created.isNull()){
        data->created = data->fileInfo.created();
    }
    return data->created;
}

QDateTime AbstractFileInfo::lastModified() const
{
    if (data->lastModified.isNull()){
        data->lastModified = data->fileInfo.lastModified();
    }
    return data->lastModified;
}

QDateTime AbstractFileInfo::lastRead() const
{
    return data->fileInfo.lastRead();
}

QString AbstractFileInfo::lastReadDisplayName() const
{
    return lastRead().toString(timeFormat());
}

QString AbstractFileInfo::lastModifiedDisplayName() const
{
    return lastModified().toString(timeFormat());
}

QString AbstractFileInfo::createdDisplayName() const
{
    return created().toString(timeFormat());
}

QString AbstractFileInfo::sizeDisplayName() const
{
    if (isFile()){
        return FileUtils::formatSize(size());
    }else{
        if (size() <= 1){
            return QObject::tr("%1 item").arg(size());
        }else{
            return QObject::tr("%1 items").arg(size());
        }
    }
}

QString AbstractFileInfo::mimeTypeDisplayName() const
{
    return mimeTypeDisplayManager->displayName(mimeTypeName());
}

int AbstractFileInfo::mimeTypeDisplayNameOrder() const
{
    return static_cast<int>(mimeTypeDisplayManager->displayNameOrder(mimeTypeName()));
}

DUrl AbstractFileInfo::parentUrl() const
{
    return DUrl::parentUrl(data->url);
}

QVector<MenuAction> AbstractFileInfo::menuActionList(AbstractFileInfo::MenuType type) const
{

    QVector<MenuAction> actionKeys;

    if (type == SpaceArea) {
        actionKeys.reserve(9);

        actionKeys << MenuAction::NewFolder
                   << MenuAction::NewDocument
                   << MenuAction::Separator
                   << MenuAction::DisplayAs
                   << MenuAction::SortBy
                   << MenuAction::OpenInTerminal
                   << MenuAction::Separator
                   << MenuAction::Paste
                   << MenuAction::SelectAll
                   << MenuAction::Separator
                   << MenuAction::Property;
    } else if (type == SingleFile) {

        if (isDir() && systemPathManager->isSystemPath(filePath())) {
            actionKeys << MenuAction::Open
                       << MenuAction::OpenInNewWindow
                       << MenuAction::Separator
                       << MenuAction::Copy
                       << MenuAction::CreateSymlink
                       << MenuAction::SendToDesktop
                       << MenuAction::OpenInTerminal
                       << MenuAction::Separator
                       << MenuAction::Compress
                       << MenuAction::Separator
                       << MenuAction::Property;
        } else {
            actionKeys << MenuAction::Open;

            if (isDir()){
                actionKeys << MenuAction::OpenInNewWindow;
//                actionKeys << MenuAction::OpenInNewTab;
            }else{
                if (!isDesktopFile())
                    actionKeys << MenuAction::OpenWith;
            }
            actionKeys << MenuAction::Separator
                       << MenuAction::Cut
                       << MenuAction::Copy
                       << MenuAction::CreateSymlink
                       << MenuAction::SendToDesktop;
            if (isDir()){
                actionKeys << MenuAction::AddToBookMark;
            }


            actionKeys << MenuAction::Rename;

            if (isDir()) {
                actionKeys << MenuAction::Compress << MenuAction::OpenInTerminal;
            } else if(isFile()) {
                if (mimeTypeName().startsWith("image")) {
                    actionKeys << MenuAction::SetAsWallpaper;
                }

                actionKeys << MenuAction::Separator;

                if (FileUtils::isArchive(absoluteFilePath())){
                    actionKeys << MenuAction::Decompress << MenuAction::DecompressHere;
                }else{
                    actionKeys << MenuAction::Compress;
                }
            }
#ifdef MENU_DIALOG_PLUGIN
            pluginManagerApp->additionalMenu( absoluteFilePath(), actionKeys );  // by txx, 增加取回显示的菜单
#endif
            actionKeys << MenuAction::Separator
                       << MenuAction::Delete
                       << MenuAction::Separator;
#ifdef SW_LABEL
            qDebug() << m_labelMenuItemIds;
            if (m_labelMenuItemIds.length() == 1){
                foreach (QString id, m_labelMenuItemIds) {
                    if(id == "020100"){
                        fileMenuManger->setActionString(MenuAction::SetLabel, m_labelMenuItemData[id].label);
                    }
                }
                actionKeys << MenuAction::SetLabel;
            }else{
                foreach (QString id, m_labelMenuItemIds) {
                    if(id == "010101"){
                        actionKeys << MenuAction::ViewLabel;
                        fileMenuManger->setActionString(MenuAction::ViewLabel, m_labelMenuItemData[id].label);
                    }else if(id == "010201"){
                        actionKeys << MenuAction::EditLabel;
                        fileMenuManger->setActionString(MenuAction::EditLabel, m_labelMenuItemData[id].label);
                    }else if(id == "010501"){
                        actionKeys << MenuAction::PrivateFileToPublic;
                        fileMenuManger->setActionString(MenuAction::PrivateFileToPublic, m_labelMenuItemData[id].label);
                    }
                }
            }
            actionKeys << MenuAction::Separator;
#endif

            actionKeys << MenuAction::Property;
        }
    } else if (type == MultiFiles) {
        actionKeys << MenuAction::Open
                   << MenuAction::Separator
                   << MenuAction::Cut
                   << MenuAction::Copy
                   << MenuAction::SendToDesktop
                   << MenuAction::Separator
                   << MenuAction::Compress
                   << MenuAction::Separator
                   << MenuAction::Delete
                   << MenuAction::Separator
                   << MenuAction::Property;
    } else if (type == MultiFilesSystemPathIncluded) {
        actionKeys << MenuAction::Open
                   << MenuAction::Separator
                   << MenuAction::Copy
                   << MenuAction::SendToDesktop
                   << MenuAction::Separator
                   << MenuAction::Compress
                   << MenuAction::Separator
                   << MenuAction::Property;
    }

    return actionKeys;
}


quint8 AbstractFileInfo::supportViewMode() const
{
    return DFileView::AllViewMode;
}

QAbstractItemView::SelectionMode AbstractFileInfo::supportSelectionMode() const
{
    return QAbstractItemView::ExtendedSelection;
}

QVariant AbstractFileInfo::userColumnDisplayName(int userColumnRole) const
{
    return DFileSystemModel::roleName(userColumnRole);
}

QVariant AbstractFileInfo::userColumnData(int userColumnRole) const
{
    switch (userColumnRole) {
    case DFileSystemModel::FileLastModifiedRole:
        return lastModifiedDisplayName();
    case DFileSystemModel::FileSizeRole:
        return sizeDisplayName();
    case DFileSystemModel::FileMimeTypeRole:
        return mimeTypeDisplayName();
    case DFileSystemModel::FileCreatedRole:
        return createdDisplayName();
    default:
        break;
    }

    return QVariant();
}

int AbstractFileInfo::userColumnWidth(int userColumnRole) const
{
    switch (userColumnRole) {
    case DFileSystemModel::FileSizeRole:
        return 80;
    case DFileSystemModel::FileMimeTypeRole:
        return 80;
    default:
        return qApp->fontMetrics().width("0000/00/00 00:00:00");
    }
}

bool AbstractFileInfo::columnDefaultVisibleForRole(int role) const
{
    return !(role == DFileSystemModel::FileCreatedRole
             || role == DFileSystemModel::FileMimeTypeRole);
}

AbstractFileInfo::sortFunction AbstractFileInfo::sortFunByColumn(int columnRole) const
{
    switch (columnRole) {
    case DFileSystemModel::FileDisplayNameRole:
        return FileSortFunction::sortFileListByDisplayName;
    case DFileSystemModel::FileLastModifiedRole:
        return FileSortFunction::sortFileListByModified;
    case DFileSystemModel::FileSizeRole:
        return FileSortFunction::sortFileListBySize;
    case DFileSystemModel::FileMimeTypeRole:
        return FileSortFunction::sortFileListByMime;
    case DFileSystemModel::FileCreatedRole:
        return FileSortFunction::sortFileListByCreated;
    default:
        return sortFunction();
    }
}

void AbstractFileInfo::sortByColumnRole(QList<AbstractFileInfoPointer> &fileList, int columnRole, Qt::SortOrder order) const
{
    FileSortFunction::sortOrderGlobal = order;
    FileSortFunction::sortFun = sortFunByColumn(columnRole);

    if (!FileSortFunction::sortFun)
        return;

    qSort(fileList.begin(), fileList.end(), FileSortFunction::sort);
}

int AbstractFileInfo::getIndexByFileInfo(getFileInfoFun fun, const AbstractFileInfoPointer &info, int columnType, Qt::SortOrder order) const
{
    FileSortFunction::sortOrderGlobal = order;
    FileSortFunction::sortFun = sortFunByColumn(columnType);

    if (!FileSortFunction::sortFun)
        return -1;

    int index = -1;

    forever {
        const AbstractFileInfoPointer &tmp_info = fun(++index);

        if(!tmp_info)
            break;

        if(FileSortFunction::sort(info, tmp_info)) {
            break;
        }
    }

    return index;
}

bool AbstractFileInfo::canRedirectionFileUrl() const
{
    return false;
}

DUrl AbstractFileInfo::redirectedFileUrl() const
{
    return fileUrl();
}

bool AbstractFileInfo::isEmptyFloder() const
{
    if (!isDir())
        return false;

    DDirIteratorPointer it = FileServices::instance()->createDirIterator(fileUrl(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System,
                                                                         QDirIterator::NoIteratorFlags);

    return it && !it->hasNext();
}

Qt::ItemFlags AbstractFileInfo::fileItemDisableFlags() const
{
    return Qt::ItemFlags();
}

bool AbstractFileInfo::canIteratorDir() const
{
    return false;
}

DUrl AbstractFileInfo::getUrlByNewFileName(const QString &fileName) const
{
    DUrl url = fileUrl();

    url.setPath(absolutePath() + "/" + fileName);

    return url;
}

DUrl AbstractFileInfo::mimeDataUrl() const
{
    if (canRedirectionFileUrl())
        return redirectedFileUrl();

    return fileUrl();
}

Qt::DropActions AbstractFileInfo::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions AbstractFileInfo::supportedDropActions() const
{
    if (isWritable())
        return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;

    return Qt::IgnoreAction;
}

QString AbstractFileInfo::loadingTip() const
{
    return QObject::tr("Loading...");
}

QString AbstractFileInfo::subtitleForEmptyFloder() const
{
    return QString();
}

QString AbstractFileInfo::suffix() const
{
    const QString &suffix = data->fileInfo.suffix();
    const QString &completeSuffix = data->fileInfo.completeSuffix();

    if (completeSuffix != suffix) {
        QStringList suffixes = completeSuffix.split(".");

        if (suffixes.length() >= 2 && suffixes.at(suffixes.length() - 2) == "tar") {
            return QString("%1.%2").arg("tar", suffix);
        }
    }

    return suffix;
}

QString AbstractFileInfo::completeSuffix() const
{
    return data->fileInfo.completeSuffix();
}

void AbstractFileInfo::updateFileInfo()
{
    metaDataCacheMap.remove(data->url);

    data->fileInfo.refresh();
    data->size = -1;
    data->created = QDateTime();
    data->lastModified = QDateTime();

    updateFileMetaData();
#ifdef SW_LABEL
    updateLabelMenuItems();
#endif
}

void AbstractFileInfo::updateFileMetaData()
{
    this->data->pinyinName = QString();

    if (metaDataCacheMap.contains(this->data->url))
        return;

    QFile::Permissions permissions = this->data->fileInfo.permissions();

    if (permissions == 0 && !exists()) {
        return;
    }

    FileMetaData data;

    data.isExecutable = this->data->fileInfo.isExecutable();
    data.isReadable = this->data->fileInfo.isReadable();
    data.isWritable = this->data->fileInfo.isWritable();
    data.permissions = permissions;

    metaDataCacheMap[this->data->url] = data;
}

void AbstractFileInfo::init()
{
    m_userColumnRoles << DFileSystemModel::FileLastModifiedRole << DFileSystemModel::FileSizeRole
                      << DFileSystemModel::FileMimeTypeRole;
}

#ifdef SW_LABEL
QString AbstractFileInfo::getLabelIcon()
{
    std::string path = this->data->url.toLocalFile().toStdString();
    char* icon = auto_add_emblem(const_cast<char*>(path.c_str()));
    if (QString::fromLocal8Bit(icon) == "No"){
        return "";
    }else{
        return QString::fromLocal8Bit(icon);
    }
}

void AbstractFileInfo::updateLabelMenuItems()
{
    m_labelMenuItemIds.clear();
    m_labelMenuItemData.clear();
//    QString menu = "{\"id\":[\"010101\",\"010201\",\"010501\"],\"label\":[\"查看标签\",\"编辑标签\",\"转换为公有\"],\"tip\":[\"\",\"\",\"\"],\"icon\":[\"viewlabel.svg\",\"editlabel.svg\",\"editlabel.svg\"],\"sub0\":\"\",\"sub1\":\"\",\"sub2\":\"\"}";
//    QString menu = "{\"id\":[\"020100\"],\"label\":[\"设置标签\"],\"tip\":[\"\"],\"icon\":[\"setlabel.svg\"],\"sub0\":\"\"}";
    std::string path = this->data->url.toLocalFile().toStdString();
    QString menu = auto_add_rightmenu(const_cast<char*>(path.c_str()));
    QJsonParseError error;
    QJsonDocument doc=QJsonDocument::fromJson(menu.toLocal8Bit(),&error);
    if (error.error == QJsonParseError::NoError){
        QJsonObject obj = doc.object();
        QJsonArray ids =  obj.value("id").toArray();
        QJsonArray labels =  obj.value("label").toArray();
        QJsonArray tips =  obj.value("tip").toArray();
        QJsonArray icons =  obj.value("icon").toArray();

        for(int i=0; i< ids.count(); i++){
            LabelMenuItemData item;
            item.id = ids.at(i).toString();
            item.label = labels.at(i).toString();
            item.tip = tips.at(i).toString();
            item.icon = icons.at(i).toString();
            m_labelMenuItemIds.append(item.id);
            m_labelMenuItemData.insert(item.id, item);
            qDebug() << item.id << item.icon << item.label;
        }
    }else{
        qDebug() << "load menu fail: " << error.errorString();
    }
}
#endif

QMap<MenuAction, QVector<MenuAction> > AbstractFileInfo::subMenuActionList() const
{
    QMap<MenuAction, QVector<MenuAction> > actions;

    QVector<MenuAction> openwithMenuActionKeys;
    actions.insert(MenuAction::OpenWith, openwithMenuActionKeys);


    QVector<MenuAction> docmentMenuActionKeys;
    docmentMenuActionKeys << MenuAction::NewWord
                          << MenuAction::NewExcel
                          << MenuAction::NewPowerpoint
                          << MenuAction::NewText;
    actions.insert(MenuAction::NewDocument, docmentMenuActionKeys);

    QVector<MenuAction> displayAsMenuActionKeys;

    int support_view_mode = supportViewMode();

    if ((support_view_mode & DListView::IconMode) == DListView::IconMode)
        displayAsMenuActionKeys << MenuAction::IconView;

    if ((support_view_mode & DListView::ListMode) == DListView::ListMode)
        displayAsMenuActionKeys << MenuAction::ListView;

    actions.insert(MenuAction::DisplayAs, displayAsMenuActionKeys);

    QVector<MenuAction> sortByMenuActionKeys;
    sortByMenuActionKeys << MenuAction::Name;

    for (int role : userColumnRoles()) {
        sortByMenuActionKeys << menuActionByColumnRole(role);
    }

    actions.insert(MenuAction::SortBy, sortByMenuActionKeys);

#ifdef MENU_DIALOG_PLUGIN
    pluginManagerApp->additionalSubMenu( absoluteFilePath(), actions );  // by txx, 增加取回显示的子菜单
#endif
    return actions;
}

QSet<MenuAction> AbstractFileInfo::disableMenuActionList() const
{
    QSet<MenuAction> list;

    if (!isWritable()) {
        list << MenuAction::NewFolder
             << MenuAction::NewDocument
             << MenuAction::Paste;
    }

    if (!isCanRename()) {
        list << MenuAction::Cut << MenuAction::Rename << MenuAction::Remove << MenuAction::Delete;
    }

    return list;
}

MenuAction AbstractFileInfo::menuActionByColumnRole(int role) const
{
    switch (role) {
    case DFileSystemModel::FileDisplayNameRole:
    case DFileSystemModel::FileNameRole:
        return MenuAction::Name;
    case DFileSystemModel::FileSizeRole:
        return MenuAction::Size;
    case DFileSystemModel::FileMimeTypeRole:
        return MenuAction::Type;
    case DFileSystemModel::FileCreatedRole:
        return MenuAction::CreatedDate;
    case DFileSystemModel::FileLastModifiedRole:
        return MenuAction::LastModifiedDate;
    default:
        return MenuAction::Unknow;
    }
}

QT_BEGIN_NAMESPACE
QDebug operator<<(QDebug deg, const AbstractFileInfo &info)
{
    deg << "file url:" << info.fileUrl()
        << "mime type:" << info.mimeTypeName();

    return deg;
}
QT_END_NAMESPACE

