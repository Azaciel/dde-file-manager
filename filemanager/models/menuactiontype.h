#ifndef MENUACTIONTYPE_H
#define MENUACTIONTYPE_H

#include <QObject>

class MenuActionType : public QObject
{
    Q_OBJECT

//    Q_ENUMS(MenuAction)

public:

    enum MenuAction {
        Open,
        OpenDisk,
        OpenInNewWindow,
        OpenInNewTab,
        OpenDiskInNewWindow,
        OpenWith,
        OpenWithCustom,
        OpenFileLocation,
        Compress,
        Decompress,
        DecompressHere,
        Cut,
        Copy,
        Paste,
        Rename,
        Remove,
        CreateSymlink,
        SendToDesktop,
        AddToBookMark,
        Delete,
        Property,
        NewFolder,
        NewWindow,
        SelectAll,
        Separator,
        ClearRecent,
        ClearTrash,
        DisplayAs, /// sub menu
        SortBy, /// sub menu
        NewDocument, /// sub menu
        NewWord, /// sub menu
        NewExcel, /// sub menu
        NewPowerpoint, /// sub menu
        NewText, /// sub menu
        OpenInTerminal,
        Restore,
        RestoreAll,
        CompleteDeletion,
        Mount,
        Unmount,
        Eject,
        Name,
        Size,
        Type,
        CreatedDate,
        LastModifiedDate,
        DeletionDate,
        SourcePath,
        AbsolutePath,
        Settings,
        Help,
        About,
        Exit,
        IconView,
        ListView,
        ExtendView,
        SetAsWallpaper,
        ForgetPassword,
#ifdef SW_LABEL
        SetLabel,
        ViewLabel,
        EditLabel,
        PrivateFileToPublic,
#endif
        Unknow
    };

    Q_ENUM(MenuAction)

    explicit MenuActionType(QObject *parent = 0);
    ~MenuActionType();

signals:

public slots:
};

#endif // MENUACTIONTYPE_H
