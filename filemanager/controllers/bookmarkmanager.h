#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include "basemanager.h"
#include "bookmark.h"
#include "abstractfilecontroller.h"

#include <QDir>

class AbstractFileInfo;

class BookMarkManager : public AbstractFileController, public BaseManager
{
    Q_OBJECT
public:
    explicit BookMarkManager(QObject *parent = 0);
    ~BookMarkManager();
    void load();
    void save();
    QList<BookMark *> getBookmarks();
    static QString cachePath();
private:
    void loadJson(const QJsonObject &json);
    void writeJson(QJsonObject &json);
    QList<BookMark *> m_bookmarks;

public slots:
    BookMark *writeIntoBookmark(int index, const QString &name, const DUrl &url);
    void removeBookmark(BookMark* bookmark);
    void renameBookmark(BookMark* bookmark, const QString &newname);
    void moveBookmark(int from, int to);
    // AbstractFileController interface
public:
    const QList<AbstractFileInfoPointer> getChildren(const DUrl &fileUrl, QDir::Filters filter, bool &accepted) const Q_DECL_OVERRIDE;
    const AbstractFileInfoPointer createFileInfo(const DUrl &fileUrl, bool &accepted) const Q_DECL_OVERRIDE;
};

#endif // BOOKMARKMANAGER_H
