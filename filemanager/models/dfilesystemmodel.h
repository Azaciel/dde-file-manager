#ifndef DFILESYSTEMMODEL_H
#define DFILESYSTEMMODEL_H

#include <QAbstractItemModel>
#include <QPointer>
#include <QDir>
#include <QFuture>

#include "durl.h"
#include "abstractfileinfo.h"

QT_BEGIN_NAMESPACE
class QEventLoop;
QT_END_NAMESPACE

class FileSystemNode;
class AbstractFileInfo;
class DFileView;
class FMEvent;
class JobController;

typedef QExplicitlySharedDataPointer<FileSystemNode> FileSystemNodePointer;

class DFileSystemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        FileIconRole = Qt::DecorationRole,
        FilePathRole = Qt::UserRole + 1,
        FileNameRole = Qt::UserRole + 2,
        FileSizeRole = Qt::UserRole + 3,
        FileMimeTypeRole = Qt::UserRole + 4,
        FileOwnerRole = Qt::UserRole + 5,
        FileLastModifiedRole = Qt::UserRole + 6,
        FileLastReadRole = Qt::UserRole + 7,
        FileCreatedRole = Qt::UserRole + 8,
        FileDisplayNameRole = Qt::UserRole + 9,
        FilePinyinName = Qt::UserRole + 10,
        FileUserRole = Qt::UserRole + 11,
        UnknowRole = Qt::UserRole + 999
    };

    enum State {
        Idle,
        Busy,
        Unknow
    };

    explicit DFileSystemModel(DFileView *parent = 0);
    ~DFileSystemModel();

    DFileView *parent() const;

    inline QModelIndex createIndex(int arow, int acolumn, quintptr aid) const
    { return QAbstractItemModel::createIndex(arow, acolumn, aid);}
    QModelIndex index(const DUrl &fileUrl, int column = 0);
    QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex parent(const QModelIndex &child) const Q_DECL_OVERRIDE;

    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    inline int columnWidth(int column) const
    { return columnWidthByRole(columnToRole(column));}
    QVariant columnNameByRole(int role, const QModelIndex &index = QModelIndex()) const;

    int columnWidthByRole(int role) const;

    bool columnDefaultVisibleForRole(int role, const QModelIndex &index = QModelIndex()) const;

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QVariant headerData(int column, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;

    static QString roleName(int role);

    int columnToRole(int column) const;
    int roleToColumn(int role) const;

    bool canFetchMore(const QModelIndex & parent) const Q_DECL_OVERRIDE;
    void fetchMore(const QModelIndex & parent) Q_DECL_OVERRIDE;

    Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;

    Qt::DropActions supportedDragActions() const Q_DECL_OVERRIDE;
    Qt::DropActions supportedDropActions() const Q_DECL_OVERRIDE;

    QStringList mimeTypes() const Q_DECL_OVERRIDE;
    bool dropMimeData(const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent) Q_DECL_OVERRIDE;
    QMimeData *mimeData(const QModelIndexList & indexes) const Q_DECL_OVERRIDE;

    QModelIndex setRootUrl(const DUrl &fileUrl);
    DUrl rootUrl() const;

    DUrl getUrlByIndex(const QModelIndex &index) const;

    void setSortColumn(int column, Qt::SortOrder order = Qt::AscendingOrder);
    void setSortRole(int role, Qt::SortOrder order = Qt::AscendingOrder);
//    void setActiveIndex(const QModelIndex &index);

    Qt::SortOrder sortOrder() const;
    int sortColumn() const;
    int sortRole() const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) Q_DECL_OVERRIDE;
    void sort();

    const AbstractFileInfoPointer fileInfo(const QModelIndex &index) const;
    const AbstractFileInfoPointer fileInfo(const DUrl &fileUrl) const;
    const AbstractFileInfoPointer parentFileInfo(const QModelIndex &index) const;
    const AbstractFileInfoPointer parentFileInfo(const DUrl &fileUrl) const;

    State state() const;

public slots:
    void updateChildren(QList<AbstractFileInfoPointer> list);
    /// warning: only refresh current url
    void refresh(const DUrl &fileUrl = DUrl());
    void update();
    void toggleHiddenFiles(const DUrl &fileUrl);

signals:
    void rootUrlDeleted();
    void stateChanged(State state);

private slots:
    void onFileCreated(const DUrl &fileUrl);
    void onFileDeleted(const DUrl &fileUrl);
    void onFileUpdated(const DUrl &fileUrl);

private:
    FileSystemNodePointer m_rootNode;

//    QHash<DUrl, FileSystemNodePointer> m_urlToNode;

    int m_sortRole = FileDisplayNameRole;
    QDir::Filters m_filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    Qt::SortOrder m_srotOrder = Qt::AscendingOrder;
//    QModelIndex m_activeIndex;

    QPointer<JobController> jobController;
    QEventLoop *eventLoop = Q_NULLPTR;
    QFuture<void> updateChildrenFuture;

    State m_state = Idle;

    bool childrenUpdated = false;

    inline const FileSystemNodePointer getNodeByIndex(const QModelIndex &index) const;
    QModelIndex createIndex(const FileSystemNodePointer &node, int column) const;
    using QAbstractItemModel::createIndex;

    bool isDir(const FileSystemNodePointer &node) const;

    void sort(const AbstractFileInfoPointer &parentInfo, QList<AbstractFileInfoPointer> &list) const;

    const FileSystemNodePointer createNode(FileSystemNode *parent, const AbstractFileInfoPointer &info);

    void deleteNode(const FileSystemNodePointer &node);
    void deleteNodeByUrl(const DUrl &url);
    void clear();

    void setState(State state);
    void onJobFinished();
    void addFile(const AbstractFileInfoPointer &fileInfo);

    friend class FileSystemNode;
};

#endif // DFILESYSTEMMODEL_H
