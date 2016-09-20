#include "dfileview.h"
#include "dfilesystemmodel.h"
#include "fileitem.h"
#include "filemenumanager.h"
#include "dfileitemdelegate.h"
#include "fileinfo.h"
#include "dfilemenu.h"
#include "windowmanager.h"
#include "dfilemanagerwindow.h"
#include "dfileselectionmodel.h"
#include "xdndworkaround.h"
#include "dstatusbar.h"
#include "../app/global.h"
#include "../app/fmevent.h"
#include "../app/filemanagerapp.h"
#include "../app/filesignalmanager.h"

#include "../controllers/appcontroller.h"
#include "../controllers/filecontroller.h"
#include "../controllers/fileservices.h"
#include "../controllers/filejob.h"
#include "../controllers/fmstatemanager.h"
#include "../controllers/pathmanager.h"

#include "../models/dfilesystemmodel.h"

#include "../shutil/fileutils.h"
#include "../shutil/iconprovider.h"
#include "../shutil/mimesappsmanager.h"

#include "widgets/singleton.h"

#include <dthememanager.h>
#include <dscrollbar.h>

#include <QWheelEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimer>
#include <QX11Info>
#include <QUrlQuery>
#include <QProcess>

DWIDGET_USE_NAMESPACE

#define ICON_VIEW_SPACING 5
#define LIST_VIEW_SPACING 1

#define DEFAULT_HEADER_SECTION_WIDTH 140

#define LIST_VIEW_ICON_SIZE 28

QSet<DUrl> DFileView::m_cutUrlSet;

DFileView::DFileView(QWidget *parent) : DListView(parent)
{
    D_THEME_INIT_WIDGET(DFileView);
    m_displayAsActionGroup = new QActionGroup(this);
    m_sortByActionGroup = new QActionGroup(this);
    m_openWithActionGroup = new QActionGroup(this);
    initUI();
    initDelegate();
    initModel();
    initActions();
    initKeyboardSearchTimer();
    initConnects();
}

DFileView::~DFileView()
{
    disconnect(this, &DFileView::rowCountChanged, this, &DFileView::updateStatusBar);
    disconnect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &DFileView::updateStatusBar);
}

void DFileView::initUI()
{
    m_iconSizes << 48 << 64 << 96 << 128 << 256;

    setSpacing(ICON_VIEW_SPACING);
    setResizeMode(QListView::Adjust);
    setOrientation(QListView::LeftToRight, true);
    setIconSize(currentIconSize());
    setTextElideMode(Qt::ElideMiddle);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDropIndicatorShown(false);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QListView::EditKeyPressed | QListView::SelectedClicked);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBar(new DScrollBar);

    DListView::setSelectionRectVisible(false);

    m_selectionRectWidget = new QWidget(this);
    m_selectionRectWidget->hide();
    m_selectionRectWidget->resize(0, 0);
    m_selectionRectWidget->setObjectName("SelectionRect");
    m_selectionRectWidget->raise();
    m_selectionRectWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    linkIcon = QIcon(":/images/images/link_large.png");
    lockIcon = QIcon(":/images/images/lock_large.png");
    unreadableIcon = QIcon(":/images/images/unreadable.svg");

    m_statusBar = new DStatusBar(this);
    addFooterWidget(m_statusBar);
}

void DFileView::initDelegate()
{
    setItemDelegate(new DFileItemDelegate(this));
}

void DFileView::initModel()
{
    setModel(new DFileSystemModel(this));
#ifndef CLASSICAL_SECTION
    setSelectionModel(new DFileSelectionModel(model(), this));
#endif
}

void DFileView::initConnects()
{
    connect(this, &DFileView::doubleClicked,
            this, [this] (const QModelIndex &index) {
        if (!Global::keyCtrlIsPressed() && !Global::keyShiftIsPressed())
            openIndex(index);
    }, Qt::QueuedConnection);
//    connect(fileSignalManager, &FileSignalManager::fetchNetworksSuccessed,
//            this, &DFileView::cd);
//    connect(fileSignalManager, &FileSignalManager::requestChangeCurrentUrl,
//            this, &DFileView::preHandleCd);

    connect(fileSignalManager, &FileSignalManager::requestRename,
            this, static_cast<void (DFileView::*)(const FMEvent&)>(&DFileView::edit));
    connect(fileSignalManager, &FileSignalManager::requestViewSelectAll,
            this, &DFileView::selectAll);
    connect(fileSignalManager, &FileSignalManager::requestSelectFile,
            this, &DFileView::select);
    connect(fileSignalManager, &FileSignalManager::requestSelectRenameFile,
            this, &DFileView::selectAndRename);
    connect(this, &DFileView::rowCountChanged, this, &DFileView::updateStatusBar);

    connect(m_displayAsActionGroup, &QActionGroup::triggered, this, &DFileView::dislpayAsActionTriggered);
    connect(m_sortByActionGroup, &QActionGroup::triggered, this, &DFileView::sortByActionTriggered);
    connect(m_openWithActionGroup, &QActionGroup::triggered, this, &DFileView::openWithActionTriggered);
    connect(m_keyboardSearchTimer, &QTimer::timeout, this, &DFileView::clearKeyBoardSearchKeys);

    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &DFileView::updateStatusBar);
    connect(fileSignalManager, &FileSignalManager::requestFoucsOnFileView, this, &DFileView::setFoucsOnFileView);
    connect(fileSignalManager, &FileSignalManager::requestFreshFileView, this, &DFileView::refreshFileView);

    connect(itemDelegate(), &DFileItemDelegate::commitData, this, &DFileView::handleCommitData);
    connect(model(), &DFileSystemModel::dataChanged, this, &DFileView::handleDataChanged);
    connect(model(), &DFileSystemModel::rootUrlDeleted, this, &DFileView::updateContentLabel, Qt::QueuedConnection);
    connect(model(), &DFileSystemModel::stateChanged, this, &DFileView::onModelStateChanged);

    connect(fileIconProvider, &IconProvider::themeChanged, model(), &DFileSystemModel::update);
    connect(fileIconProvider, &IconProvider::iconChanged, this, [this] (const QString &filePath) {
        update(model()->index(DUrl::fromLocalFile(filePath)));
    });

    if (!m_cutUrlSet.capacity()) {
        m_cutUrlSet.reserve(1);

        connect(qApp->clipboard(), &QClipboard::dataChanged, [] {
            DFileView::m_cutUrlSet.clear();

            const QByteArray &data = qApp->clipboard()->mimeData()->data("x-special/gnome-copied-files");

            if (!data.startsWith("cut"))
                return;

            for (const QUrl &url : qApp->clipboard()->mimeData()->urls()) {
                DFileView::m_cutUrlSet << url;
            }
        });
    }

    connect(qApp->clipboard(), &QClipboard::dataChanged, this, [this] {
        FileIconItem *item = qobject_cast<FileIconItem*>(itemDelegate()->editingIndexWidget());

        if (item)
            item->setOpacity(isCutIndex(itemDelegate()->editingIndex()) ? 0.3 : 1);

        if (itemDelegate()->expandedIndex().isValid()) {
            item = itemDelegate()->expandedIndexWidget();

            item->setOpacity(isCutIndex(itemDelegate()->expandedIndex()) ? 0.3 : 1);
        }

        update();
    });
}

void DFileView::initActions()
{
    QAction *copy_action = new QAction(this);

    copy_action->setAutoRepeat(false);
    copy_action->setShortcut(QKeySequence::Copy);

    connect(copy_action, &QAction::triggered,
            this, [this] {
        fileService->copyFiles(selectedUrls());
    });

    QAction *cut_action = new QAction(this);

    cut_action->setAutoRepeat(false);
    cut_action->setShortcut(QKeySequence::Cut);

    connect(cut_action, &QAction::triggered,
            this, [this] {
        fileService->cutFiles(selectedUrls());
    });

    QAction *paste_action = new QAction(this);

    paste_action->setShortcut(QKeySequence::Paste);

    connect(paste_action, &QAction::triggered,
            this, [this] {
        FMEvent event;

        event = currentUrl();
        event = windowId();
        event = FMEvent::FileView;
        fileService->pasteFile(event);
    });

    addAction(copy_action);
    addAction(cut_action);
    addAction(paste_action);
}

void DFileView::initKeyboardSearchTimer()
{
    m_keyboardSearchTimer = new QTimer(this);
    m_keyboardSearchTimer->setInterval(500);
}

DFileSystemModel *DFileView::model() const
{
    return qobject_cast<DFileSystemModel*>(DListView::model());
}

DFileItemDelegate *DFileView::itemDelegate() const
{
    return qobject_cast<DFileItemDelegate*>(DListView::itemDelegate());
}

DUrl DFileView::currentUrl() const
{
    return model()->getUrlByIndex(rootIndex());
}

DUrlList DFileView::selectedUrls() const
{
    DUrlList list;

    for(const QModelIndex &index : selectedIndexes()) {
        list << model()->getUrlByIndex(index);
    }

    return list;
}

bool DFileView::isIconViewMode() const
{
    return orientation() == Qt::Vertical && isWrapping();
}

int DFileView::columnWidth(int column) const
{
    return m_headerView ? m_headerView->sectionSize(column) : 100;
}

void DFileView::setColumnWidth(int column, int width)
{
    if(!m_headerView)
        return;

    m_headerView->resizeSection(column, width);
}

int DFileView::columnCount() const
{
    return m_headerView ? m_headerView->count() : 1;
}

int DFileView::rowCount() const
{
    int count = this->count();
    int itemCountForRow = this->itemCountForRow();

    return count / itemCountForRow + int(count % itemCountForRow > 0);
}

int DFileView::itemCountForRow() const
{
    if (!isIconViewMode())
        return 1;

    int itemWidth = itemSizeHint().width() + ICON_VIEW_SPACING * 2;

    return (width() - ICON_VIEW_SPACING * 2.9) / itemWidth;
}

QList<int> DFileView::columnRoleList() const
{
    return m_columnRoles;
}

int DFileView::windowId() const
{
    return window()->winId();
}

void DFileView::setIconSize(const QSize &size)
{
    DListView::setIconSize(size);

    updateItemSizeHint();
    updateHorizontalOffset();
    updateGeometries();
}

DFileView::ViewMode DFileView::getDefaultViewMode()
{
    return m_defaultViewMode;
}

int DFileView::getSortRoles()
{
    return model()->sortRole();
}

bool DFileView::testViewMode(ViewModes modes, DFileView::ViewMode mode)
{
    return (modes | mode) == modes;
}

int DFileView::horizontalOffset() const
{
    return m_horizontalOffset;
}

bool DFileView::isSelected(const QModelIndex &index) const
{
#ifndef CLASSICAL_SECTION
    return static_cast<DFileSelectionModel*>(selectionModel())->isSelected(index);
#else
    return selectionModel()->isSelected(index);
#endif
}

int DFileView::selectedIndexCount() const
{
#ifndef CLASSICAL_SECTION
    return static_cast<const DFileSelectionModel*>(selectionModel())->selectedCount();
#else
    return selectionModel()->selectedIndexes().count();
#endif
}

QModelIndexList DFileView::selectedIndexes() const
{
#ifndef CLASSICAL_SECTION
    return static_cast<DFileSelectionModel*>(selectionModel())->selectedIndexes();
#else
    return selectionModel()->selectedIndexes();
#endif
}

QModelIndex DFileView::indexAt(const QPoint &point) const
{
    if (isIconViewMode()) {
        QWidget *widget = itemDelegate()->expandedIndexWidget();

        if (widget->isVisible() && widget->geometry().contains(point)) {
            return itemDelegate()->expandedIndex();
        }
    }

    FileIconItem *item = qobject_cast<FileIconItem*>(itemDelegate()->editingIndexWidget());

    if (item) {
        QRect geometry = item->icon->geometry();

        geometry.moveTopLeft(geometry.topLeft() + item->pos());

        if (geometry.contains(point))
            return itemDelegate()->editingIndex();

        geometry = item->edit->geometry();
        geometry.moveTopLeft(geometry.topLeft() + item->pos());
        geometry.setTop(item->icon->y() + item->icon->height() + item->y());

        if (geometry.contains(point))
            return itemDelegate()->editingIndex();
    }

    QPoint pos = QPoint(point.x() + horizontalOffset(), point.y() + verticalOffset());
    QSize item_size = itemSizeHint();

    if (pos.y() % (item_size.height() + spacing() * 2) < spacing())
        return QModelIndex();

    int index = -1;

    if (item_size.width() == -1) {
        int item_height = item_size.height() + LIST_VIEW_SPACING * 2;

        index = pos.y() / item_height;
    } else {
        int item_width = item_size.width() + ICON_VIEW_SPACING * 2;

        if (pos.x() % item_width <= ICON_VIEW_SPACING)
            return QModelIndex();

        int row_index = pos.y() / (item_size.height() + ICON_VIEW_SPACING * 2);
        int column_count = (width() - ICON_VIEW_SPACING * 2.9) / item_width;
        int column_index = pos.x() / item_width;

        if (column_index >= column_count)
            return QModelIndex();

        index = row_index * column_count + column_index;

        const QModelIndex &tmp_index = rootIndex().child(index, 0);
        QStyleOptionViewItem option = viewOptions();

        option.rect = QRect(QPoint(column_index * item_width + ICON_VIEW_SPACING,
                                    row_index * (item_size.height()  + ICON_VIEW_SPACING * 2) + ICON_VIEW_SPACING),
                            item_size);

        const QList<QRect> &list = itemDelegate()->paintGeomertys(option, tmp_index);

        for (const QRect &rect : list)
            if (rect.contains(pos))
                return tmp_index;

        return QModelIndex();
    }

    return rootIndex().child(index, 0);
}

QRect DFileView::visualRect(const QModelIndex &index) const
{
    QRect rect;

    if (index.column() != 0)
        return rect;

    QSize item_size = itemSizeHint();

    if (item_size.width() == -1) {
        rect.setLeft(LIST_VIEW_SPACING);
        rect.setRight(viewport()->width() - LIST_VIEW_SPACING - 1);
        rect.setTop(index.row() * (item_size.height() + LIST_VIEW_SPACING * 2) + LIST_VIEW_SPACING);
        rect.setHeight(item_size.height());
    } else {
        int item_width = item_size.width() + ICON_VIEW_SPACING * 2;
        int column_count = (width() - ICON_VIEW_SPACING * 2.9) / item_width;
        int column_index = index.row() % column_count;
        int row_index = index.row() / column_count;

        rect.setTop(row_index * (item_size.height()  + ICON_VIEW_SPACING * 2) + ICON_VIEW_SPACING);
        rect.setLeft(column_index * item_width + ICON_VIEW_SPACING);
        rect.setSize(item_size);
    }

    rect.moveLeft(rect.left() - horizontalOffset());
    rect.moveTop(rect.top() - verticalOffset());

    return rect;
}

DFileView::RandeIndexList DFileView::visibleIndexes(QRect rect) const
{
    RandeIndexList list;

    QSize item_size = itemSizeHint();
    QSize icon_size = iconSize();

    int count = this->count();
    int spacing  = this->spacing();
    int item_width = item_size.width() + spacing *  2;
    int item_height = item_size.height() + spacing * 2;

    if (item_size.width() == -1) {
        list << RandeIndex(qMax((rect.top() + spacing) / item_height, 0),
                           qMin((rect.bottom() - spacing) / item_height, count - 1));
    } else {
        rect -= QMargins(spacing, spacing, spacing, spacing);

        int column_count = (width() - spacing * 2.9) / item_width;
        int begin_row_index = rect.top() / item_height;
        int end_row_index = rect.bottom() / item_height;
        int begin_column_index = rect.left() / item_width;
        int end_column_index = rect.right() / item_width;

        if (rect.top() % item_height > icon_size.height())
            ++begin_row_index;

        int icon_margin = (item_width - icon_size.width()) / 2;

        if (rect.left() % item_width > item_width - icon_margin)
            ++begin_column_index;

        if (rect.right() % item_width < icon_margin)
            --end_column_index;

        begin_row_index = qMax(begin_row_index, 0);
        begin_column_index = qMax(begin_column_index, 0);
        end_row_index = qMin(end_row_index, count / column_count);
        end_column_index = qMin(end_column_index, column_count - 1);

        if (begin_row_index > end_row_index || begin_column_index > end_column_index)
            return list;

        int begin_index = begin_row_index * column_count;

        for (int i = begin_row_index; i <= end_row_index; ++i) {
            if (begin_index + begin_column_index >= count)
                break;

            list << RandeIndex(qMax(begin_index + begin_column_index, 0),
                               qMin(begin_index + end_column_index, count - 1));

            begin_index += column_count;
        }
    }

    return list;
}

bool DFileView::isCutIndex(const QModelIndex &index) const
{
    const AbstractFileInfoPointer &fileInfo = model()->fileInfo(index);

    if (!fileInfo || !fileInfo->canRedirectionFileUrl())
        return m_cutUrlSet.contains(model()->getUrlByIndex(index));

    return m_cutUrlSet.contains(fileInfo->redirectedFileUrl());
}

bool DFileView::isActiveIndex(const QModelIndex &index) const
{
    return dragMoveHoverIndex == index;
}

QList<QIcon> DFileView::fileAdditionalIcon(const QModelIndex &index) const
{
    QList<QIcon> icons;
    const AbstractFileInfoPointer &fileInfo = model()->fileInfo(index);

    if (!fileInfo)
        return icons;

    if (fileInfo->isSymLink()) {
        icons << linkIcon;
    }

    if (!fileInfo->isWritable())
        icons << lockIcon;

    if (!fileInfo->isReadable())
        icons << unreadableIcon;

    return icons;
}

void DFileView::preHandleCd(const FMEvent &event)
{
    qDebug() << event;
    if (event.fileUrl().isNetWorkFile()){
        emit fileSignalManager->requestFetchNetworks(event);
        return;
    }else if (event.fileUrl().isSMBFile()){
        emit fileSignalManager->requestFetchNetworks(event);
        return;
    }
    cd(event);
}

void DFileView::cd(const FMEvent &event)
{
    if (event.windowId() != windowId())
        return;

    const DUrl &fileUrl = event.fileUrl();

    if (fileUrl.isEmpty())
        return;

    itemDelegate()->hideAllIIndexWidget();

    clearSelection();

    if (!event.fileUrl().isSearchFile()){
        setFocus();
    }

    if (setCurrentUrl(fileUrl)) {
        FMEvent e = event;
        e = currentUrl();
        qDebug() << e;
        emit fileSignalManager->currentUrlChanged(e);

    }
}

void DFileView::cdUp(const FMEvent &event)
{
    AbstractFileInfoPointer fileInfo = model()->fileInfo(rootIndex());

    const DUrl &oldCurrentUrl = this->currentUrl();
    const DUrl& parentUrl = fileInfo ? fileInfo->parentUrl() : DUrl::parentUrl(oldCurrentUrl);
    const_cast<FMEvent&>(event) = parentUrl;

    cd(event);
}

void DFileView::edit(const FMEvent &event)
{
    if(event.windowId() != windowId())
        return;

    DUrl fileUrl = event.fileUrl();

    if(fileUrl.isEmpty())
        return;

    const QModelIndex &index = model()->index(fileUrl);

    edit(index, QAbstractItemView::EditKeyPressed, 0);
}

bool DFileView::edit(const QModelIndex &index, QAbstractItemView::EditTrigger trigger, QEvent *event)
{
    DUrl fileUrl = model()->getUrlByIndex(index);

    if (fileUrl.isEmpty() || selectedIndexCount() > 1 || (trigger == SelectedClicked && Global::keyShiftIsPressed()))
        return false;

    if (trigger == SelectedClicked) {
        QStyleOptionViewItem option = viewOptions();

        option.rect = visualRect(index);

        const QRect &file_name_rect = itemDelegate()->fileNameRect(option, index);

        if (!file_name_rect.contains(static_cast<QMouseEvent*>(event)->pos()))
            return false;
    }

    return DListView::edit(index, trigger, event);
}

bool DFileView::select(const FMEvent &event)
{
    if (event.windowId() != windowId()) {
        return false;
    }

    QModelIndex firstIndex;
    QModelIndex lastIndex;

    clearSelection();

    for (const DUrl &url : event.fileUrlList()) {
        const QModelIndex &index = model()->index(url);

        if (index.isValid()) {
            selectionModel()->select(index, QItemSelectionModel::Select);
        }

        if (!firstIndex.isValid())
            firstIndex = index;

        lastIndex = index;
    }

    if (lastIndex.isValid())
        selectionModel()->setCurrentIndex(lastIndex, QItemSelectionModel::Select);

    if (firstIndex.isValid())
        scrollTo(firstIndex, PositionAtTop);

    return true;
}

void DFileView::selectAndRename(const FMEvent &event)
{
    bool isSelected = select(event);

    if (isSelected) {
        FMEvent e = event;

        if (!e.fileUrlList().isEmpty())
            e = e.fileUrlList().first();

        appController->actionRename(e);
    }
}

void DFileView::setViewMode(DFileView::ViewMode mode)
{
    if(mode != m_defaultViewMode)
        m_defaultViewMode = mode;

    switchViewMode(mode);
}

void DFileView::sortByRole(int role)
{
    Qt::SortOrder order = (model()->sortRole() == role && model()->sortOrder() == Qt::AscendingOrder)
                            ? Qt::DescendingOrder
                            : Qt::AscendingOrder;

    if (m_headerView) {
        m_headerView->setSortIndicator(model()->roleToColumn(role), order);
    } else {
        model()->setSortRole(role, order);
        model()->sort();
    }

    FMStateManager::cacheSortState(currentUrl(), role, order);
}

void DFileView::sortByColumn(int column)
{
    sortByRole(model()->columnToRole(column));
}

void DFileView::selectAll(int windowId)
{
    if(windowId != WindowManager::getWindowId(window()))
        return;

    DListView::selectAll();
}

void DFileView::dislpayAsActionTriggered(QAction *action)
{
    DAction* dAction = static_cast<DAction*>(action);
    dAction->setChecked(true);
    MenuAction type = (MenuAction)dAction->data().toInt();
    switch(type){
        case MenuAction::IconView:
            setViewMode(IconMode);
            break;
        case MenuAction::ListView:
            setViewMode(ListMode);
            break;
        default:
            break;
    }
}

void DFileView::sortByActionTriggered(QAction *action)
{
    DAction* dAction = static_cast<DAction*>(action);

    sortByColumn(m_sortByActionGroup->actions().indexOf(dAction));
}

void DFileView::openWithActionTriggered(QAction *action)
{
    DAction* dAction = static_cast<DAction*>(action);
    QString app = dAction->property("app").toString();
    QString url = dAction->property("url").toString();
    FileUtils::openFileByApp(url, app);
}

void DFileView::wheelEvent(QWheelEvent *event)
{
    if(isIconViewMode() && Global::keyCtrlIsPressed()) {
        if(event->angleDelta().y() > 0) {
            enlargeIcon();
        } else {
            shrinkIcon();
        }

        event->accept();
    } else {
        verticalScrollBar()->setSliderPosition(verticalScrollBar()->sliderPosition() - event->angleDelta().y());
    }
}

void DFileView::keyPressEvent(QKeyEvent *event)
{
    const DUrlList& urls = selectedUrls();

    FMEvent fmevent;

    fmevent = urls;
    fmevent = FMEvent::FileView;
    fmevent = windowId();
    fmevent = currentUrl();

    switch (event->modifiers()) {
    case Qt::NoModifier:
    case Qt::KeypadModifier:
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (!itemDelegate()->editingIndex().isValid()) {
                appController->actionOpen(fmevent);

                return;
            }

            break;
        case Qt::Key_Backspace:
            cdUp(fmevent);

            return;
        case Qt::Key_F1:
            appController->actionHelp(fmevent);

            return;
        case Qt::Key_F5:
            model()->refresh();

            return;
        case Qt::Key_Delete:
            if (fmevent.fileUrlList().isEmpty()) {
                break;
            }

            if (fmevent.fileUrl().isTrashFile()) {
                fileService->deleteFiles(fmevent.fileUrlList(), fmevent);
            } else {
                fileService->moveToTrash(fmevent.fileUrlList());
            }

            break;
        default: break;
        }

        break;
    case Qt::ControlModifier:
        switch (event->key()) {
        case Qt::Key_F:
            appController->actionctrlF(fmevent);

            return;
        case Qt::Key_L:
            appController->actionctrlL(fmevent);

            return;
        case Qt::Key_N:
            appController->actionNewWindow(fmevent);

            return;
        case Qt::Key_H:
            preSelectionUrls = urls;

            itemDelegate()->hideAllIIndexWidget();
            clearSelection();

            model()->toggleHiddenFiles(fmevent.fileUrl());

            return;
        case Qt::Key_I:
            if (fmevent.fileUrlList().isEmpty())
                fmevent = DUrlList() << fmevent.fileUrl();

            appController->actionProperty(fmevent);

            return;
        case Qt::Key_Up:
            cdUp(fmevent);

            return;
        case Qt::Key_Down:
            appController->actionOpen(fmevent);

            return;
        case Qt::Key_Left:
            appController->actionBack(fmevent);

            return;
        case Qt::Key_Right:
            appController->actionForward(fmevent);

            return;
        default: break;
        }

        break;
    case Qt::ShiftModifier:
        if (event->key() == Qt::Key_Delete) {
            if (fmevent.fileUrlList().isEmpty())
                return;

            fileService->deleteFiles(fmevent.fileUrlList(), fmevent);

            return;
        } else if (event->key() == Qt::Key_T) {
            appController->actionOpenInTerminal(fmevent);

            return;
        }

        break;
    case Qt::ControlModifier | Qt::ShiftModifier:
        if (event->key() == Qt::Key_N) {
            if (itemDelegate()->editingIndex().isValid())
                return;

            clearSelection();
            appController->actionNewFolder(fmevent);

            return;
        } if (event->key() == Qt::Key_Question) {
            appController->actionShowHotkeyHelp(fmevent);

            return;
        }

        break;
    case Qt::AltModifier:
        switch (event->key()) {
        case Qt::Key_Up:
            cdUp(fmevent);

            return;
        case Qt::Key_Down:
            appController->actionOpen(fmevent);

            return;
        case Qt::Key_Left:
            appController->actionBack(fmevent);

            return;
        case Qt::Key_Right:
            appController->actionForward(fmevent);

            return;
        case Qt::Key_Home:
            fmevent = DUrl::fromLocalFile(QDir::homePath());

            cd(fmevent);

            return;
        default: break;
        }

        break;
    default: break;
    }

    DListView::keyPressEvent(event);
}

void DFileView::keyReleaseEvent(QKeyEvent *event)
{
    if(event->modifiers()==Qt::NoModifier)
        QProcess::startDetached("deepin-shortcut-viewer");
    DListView::keyReleaseEvent(event);
}

void DFileView::showEvent(QShowEvent *event)
{
    DListView::showEvent(event);

    setFocus();
}

void DFileView::mousePressEvent(QMouseEvent *event)
{
    switch (event->button()) {
    case Qt::BackButton: {
        FMEvent event;

        event = this->windowId();
        event = FMEvent::FileView;

        fileSignalManager->requestBack(event);

        break;
    }
    case Qt::ForwardButton: {
        FMEvent event;

        event = this->windowId();
        event = FMEvent::FileView;

        fileSignalManager->requestForward(event);

        break;
    }
    case Qt::LeftButton: {
        bool isEmptyArea = this->isEmptyArea(event->pos());

        setDragEnabled(!isEmptyArea);

        if (isEmptyArea) {
            if (!Global::keyCtrlIsPressed()) {
                itemDelegate()->hideExpandedIndex();
                clearSelection();
            }

            if (canShowSelectionRect())
                m_selectionRectWidget->show();

            m_selectedGeometry.setTop(event->pos().y() + verticalOffset());
            m_selectedGeometry.setLeft(event->pos().x() + horizontalOffset());

            connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DFileView::updateSelectionRect);
        } else if (Global::keyCtrlIsPressed()) {
            const QModelIndex &index = indexAt(event->pos());

            if (selectionModel()->isSelected(index)) {
                m_mouseLastPressedIndex = index;

                DListView::mousePressEvent(event);

                selectionModel()->select(index, QItemSelectionModel::Select);

                return;
            }
        }

        m_mouseLastPressedIndex = QModelIndex();

        DListView::mousePressEvent(event);
        break;
    }
    default: break;
    }
}

void DFileView::mouseMoveEvent(QMouseEvent *event)
{
    if (dragEnabled())
        return DListView::mouseMoveEvent(event);

    updateSelectionRect();
    doAutoScroll();
}

void DFileView::mouseReleaseEvent(QMouseEvent *event)
{
    dragMoveHoverIndex = QModelIndex();

    disconnect(verticalScrollBar(), &QScrollBar::valueChanged, this, &DFileView::updateSelectionRect);

    if (m_mouseLastPressedIndex.isValid() && Global::keyCtrlIsPressed()) {
        if (m_mouseLastPressedIndex == indexAt(event->pos()))
            selectionModel()->select(m_mouseLastPressedIndex, QItemSelectionModel::Deselect);
    }

    if (dragEnabled())
        return DListView::mouseReleaseEvent(event);

    m_selectionRectWidget->resize(0, 0);
    m_selectionRectWidget->hide();
}

void DFileView::handleCommitData(QWidget *editor)
{
    if(!editor)
        return;

    const AbstractFileInfoPointer &fileInfo = model()->fileInfo(itemDelegate()->editingIndex());

    if(!fileInfo)
        return;

    QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor);
    FileIconItem *item = qobject_cast<FileIconItem*>(editor);

    FMEvent event;
    event = fileInfo->fileUrl();
    event = windowId();
    event = FMEvent::FileView;

    QString new_file_name = lineEdit ? lineEdit->text() : item ? item->edit->toPlainText() : "";

    new_file_name.remove('/');
    new_file_name.remove(QChar(0));

    if (fileInfo->fileName() == new_file_name || new_file_name.isEmpty()) {
        return;
    }

    DUrl old_url = fileInfo->fileUrl();
    DUrl new_url = fileInfo->getUrlByNewFileName(new_file_name);

    if (lineEdit) {
        /// later rename file.
        TIMER_SINGLESHOT(0, {
                             fileService->renameFile(old_url, new_url, event);
                         }, old_url, new_url, event)
    } else {
        fileService->renameFile(old_url, new_url, event);
    }
}

void DFileView::focusInEvent(QFocusEvent *event)
{
    DListView::focusInEvent(event);
    itemDelegate()->commitDataAndCloseActiveEditor();

    const DUrl &current_url = currentUrl();

    if (current_url.isLocalFile())
            QDir::setCurrent(current_url.toLocalFile());
}

void DFileView::resizeEvent(QResizeEvent *event)
{
    DListView::resizeEvent(event);

    updateHorizontalOffset();

    if (itemDelegate()->editingIndex().isValid())
        doItemsLayout();
}

void DFileView::contextMenuEvent(QContextMenuEvent *event)
{
    bool isEmptyArea = this->isEmptyArea(event->pos());

    const QModelIndex &index = indexAt(event->pos());

    if (isEmptyArea  && !selectionModel()->isSelected(index)) {
        itemDelegate()->hideExpandedIndex();
        clearSelection();
        showEmptyAreaMenu();
    } else {
        const QModelIndexList &list = selectedIndexes();

        if (!list.contains(index)) {
            setCurrentIndex(index);
        }

        showNormalMenu(index);
    }
}

//bool DFileView::event(QEvent *event)
//{
//    switch (event->type()) {
//    case QEvent::MouseButtonPress: {
//        QMouseEvent *e = static_cast<QMouseEvent*>(event);

//        const QPoint &pos = viewport()->mapFromParent(e->pos());
//        bool isEmptyArea = this->isEmptyArea(pos);

//        if (e->button() == Qt::LeftButton) {
//            if (isEmptyArea && !Global::keyCtrlIsPressed()) {
//                clearSelection();
//                itemDelegate()->hideAllIIndexWidget();
//            }

//            if (isEmptyArea) {
//                if (canShowSElectionRect())
//                    m_selectionRectWidget->show();

//                m_pressedPos = static_cast<QMouseEvent*>(event)->pos();
//            }
//        } else if (e->button() == Qt::RightButton) {
//            if (isEmptyArea  && !selectionModel()->isSelected(indexAt(pos))) {
//                clearSelection();
//                showEmptyAreaMenu();
//            } else {
//                if (!hasFocus() && this->childAt(pos)->hasFocus())
//                    return DListView::event(event);

//                const QModelIndex &index = indexAt(pos);
//                const QModelIndexList &list = selectedIndexes();

//                if (!list.contains(index)) {
//                    setCurrentIndex(index);
//                }

//                showNormalMenu(index);
//            }
//        }
//        break;
//    }
//    case QEvent::MouseMove: {
//        if (m_selectionRectWidget->isHidden())
//            break;

//        const QPoint &pos = static_cast<QMouseEvent*>(event)->pos();
//        QRect rect;

//        rect.adjust(qMin(m_pressedPos.x(), pos.x()), qMin(m_pressedPos.y(), pos.y()),
//                    qMax(pos.x(), m_pressedPos.x()), qMax(pos.y(), m_pressedPos.y()));

//        m_selectionRectWidget->setGeometry(rect);

////        rect.moveTopLeft(viewport()->mapFromParent(rect.topLeft()));

////        setSelection(rect, QItemSelectionModel::Current|QItemSelectionModel::Rows|QItemSelectionModel::ClearAndSelect);

//        break;
//    }
//    case QEvent::MouseButtonRelease: {
//        m_selectionRectWidget->resize(0, 0);
//        m_selectionRectWidget->hide();

//        break;
//    }
//    default:
//        break;
//    }

//    return DListView::event(event);
//}

void DFileView::dragEnterEvent(QDragEnterEvent *event)
{
    for (const DUrl &url : event->mimeData()->urls()) {
        const AbstractFileInfoPointer &fileInfo = FileServices::instance()->createFileInfo(url);

        if (!fileInfo->isWritable()) {
            event->ignore();

            return;
        }
    }

    preproccessDropEvent(event);

    if (event->mimeData()->hasFormat("XdndDirectSave0")) {
        event->setDropAction(Qt::CopyAction);
        event->acceptProposedAction();

        return;
    }

    DListView::dragEnterEvent(event);
}

void DFileView::dragMoveEvent(QDragMoveEvent *event)
{
    dragMoveHoverIndex = indexAt(event->pos());

    if (dragMoveHoverIndex.isValid()) {
        const AbstractFileInfoPointer &fileInfo = model()->fileInfo(dragMoveHoverIndex);

        if (fileInfo) {
            if (!fileInfo->isDir()) {
                dragMoveHoverIndex = QModelIndex();
            } else if(!fileInfo->supportedDropActions().testFlag(event->dropAction())) {
                dragMoveHoverIndex = QModelIndex();

                update();
                return event->ignore();
            }
        }
    }

    update();

    if (dragDropMode() == InternalMove
        && (event->source() != this || !(event->possibleActions() & Qt::MoveAction)))
        QAbstractItemView::dragMoveEvent(event);
}

void DFileView::dragLeaveEvent(QDragLeaveEvent *event)
{
    dragMoveHoverIndex = QModelIndex();

    DListView::dragLeaveEvent(event);
}

void DFileView::dropEvent(QDropEvent *event)
{
    dragMoveHoverIndex = QModelIndex();

    preproccessDropEvent(event);

    // Try to support XDS
    // NOTE: in theory, it's not possible to implement XDS with pure Qt.
    // We achieved this with some dirty XCB/XDND workarounds.
    // Please refer to XdndWorkaround::clientMessage() in xdndworkaround.cpp for details.
    if (QX11Info::isPlatformX11() && event->mimeData()->hasFormat("XdndDirectSave0")) {
        event->setDropAction(Qt::CopyAction);
        //    const QWidget* targetWidget = childView()->viewport();
        // these are dynamic QObject property set by our XDND workarounds in xworkaround.cpp.
        //    xcb_window_t dndSource = xcb_window_t(targetWidget->property("xdnd::lastDragSource").toUInt());
        //    xcb_timestamp_t dropTimestamp = (xcb_timestamp_t)targetWidget->property("xdnd::lastDropTime").toUInt();

        xcb_window_t dndSource = xcb_window_t(XdndWorkaround::lastDragSource);
//        xcb_timestamp_t dropTimestamp = (xcb_timestamp_t)XdndWorkaround::lastDropTime;
        // qDebug() << "XDS: source window" << dndSource << dropTimestamp;
        if (dndSource != 0) {
            xcb_connection_t* conn = QX11Info::connection();
            xcb_atom_t XdndDirectSaveAtom = XdndWorkaround::internAtom("XdndDirectSave0", 15);
            xcb_atom_t textAtom = XdndWorkaround::internAtom("text/plain", 10);

            // 1. get the filename from XdndDirectSave property of the source window


            QByteArray basename = XdndWorkaround::windowProperty(dndSource, XdndDirectSaveAtom, textAtom, 1024);

            // 2. construct the fill URI for the file, and update the source window property.

            QByteArray fileUri;

            const QModelIndex &index = indexAt(event->pos());
            const AbstractFileInfoPointer &fileInfo = model()->fileInfo(index.isValid() ? index : rootIndex());

            if (fileInfo && fileInfo->fileUrl().isLocalFile() && fileInfo->isDir()) {
                fileUri.append(fileInfo->fileUrl().toString() + "/" + basename);
            }

            XdndWorkaround::setWindowProperty(dndSource,  XdndDirectSaveAtom, textAtom, (void*)fileUri.constData(), fileUri.length());

            // 3. send to XDS selection data request with type "XdndDirectSave" to the source window and
            //    receive result from the source window. (S: success, E: error, or F: failure)
            QByteArray result = event->mimeData()->data("XdndDirectSave0");
            Q_UNUSED(result)
//            qDebug() <<"result" << result;
            // NOTE: there seems to be some bugs in file-roller so it always replies with "E" even if the
            //       file extraction is finished successfully. Anyways, we ignore any error at the moment.
        }

        event->accept(); // yeah! we've done with XDS so stop Qt from further event propagation.
    } else {
        QModelIndex index = indexAt(event->pos());

        if (!index.isValid())
            index = rootIndex();

        if (!index.isValid())
            return;

        if (isSelected(index))
            return;

        if (model()->supportedDropActions() & event->dropAction() && model()->flags(index) & Qt::ItemIsDropEnabled) {
            const Qt::DropAction action = dragDropMode() == InternalMove ? Qt::MoveAction : event->dropAction();
            if (model()->dropMimeData(event->mimeData(), action, index.row(), index.column(), index)) {
                if (action != event->dropAction()) {
                    event->setDropAction(action);
                    event->accept();
                } else {
                    event->acceptProposedAction();
                }
            }
        }

        stopAutoScroll();
        setState(NoState);
        viewport()->update();
    }
}

void DFileView::setSelection(const QRect &rect, QItemSelectionModel::SelectionFlags flags)
{
    if (Global::keyShiftIsPressed()) {
        const QModelIndex &index = indexAt(rect.bottomRight());

        if (!index.isValid())
            return;

        const QModelIndex &lastSelectedIndex = indexAt(rect.topLeft());

        if (!lastSelectedIndex.isValid())
            return;

        selectionModel()->select(QItemSelection(lastSelectedIndex, index), QItemSelectionModel::ClearAndSelect);

        return;
    }

    if (flags == (QItemSelectionModel::Current|QItemSelectionModel::Rows|QItemSelectionModel::ClearAndSelect)) {
        QRect tmp_rect;

        tmp_rect.setCoords(qMin(rect.left(), rect.right()), qMin(rect.top(), rect.bottom()),
                           qMax(rect.left(), rect.right()), qMax(rect.top(), rect.bottom()));

        const RandeIndexList &list = visibleIndexes(tmp_rect);

        if (list.isEmpty()) {
            clearSelection();

            return;
        }

#ifndef CLASSICAL_SECTION
        return selectionModel()->select(QItemSelection(rootIndex().child(list.first().first, 0),
                                                       rootIndex().child(list.last().second, 0)), flags);
#else
        QItemSelection selection;

        for (const RandeIndex &index : list) {
            selection.append(QItemSelectionRange(rootIndex().child(index.first, 0), rootIndex().child(index.second, 0)));
        }

        return selectionModel()->select(selection, flags);
#endif
    }

    DListView::setSelection(rect, flags);
}

QModelIndex DFileView::moveCursor(QAbstractItemView::CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    QModelIndex current = currentIndex();

    if (!current.isValid()) {
        m_lastCursorIndex = DListView::moveCursor(cursorAction, modifiers);

        return m_lastCursorIndex;
    }

    if (rectForIndex(current).isEmpty()) {
        m_lastCursorIndex = model()->index(0, 0, rootIndex());

        return m_lastCursorIndex;
    }

    QModelIndex index;

    switch (cursorAction) {
    case MoveLeft:
        if (Global::keyShiftIsPressed()) {
            index = DListView::moveCursor(cursorAction, modifiers);

            if (index == m_lastCursorIndex) {
                index = index.sibling(index.row() - 1, index.column());
            }
        } else {
            index = current.sibling(current.row() - 1, current.column());
        }

        break;
    case MoveRight:
        if (Global::keyShiftIsPressed()) {
            index = DListView::moveCursor(cursorAction, modifiers);

            if (index == m_lastCursorIndex) {
                index = index.sibling(index.row() + 1, index.column());
            }
        } else {
            index = current.sibling(current.row() + 1, current.column());
        }

        break;
    default:
        index = DListView::moveCursor(cursorAction, modifiers);
        break;
    }

    if (index.isValid()) {
        m_lastCursorIndex = index;

        return index;
    }

    m_lastCursorIndex = current;

    return current;
}

void DFileView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    for (const QModelIndex &index : selectedIndexes()) {
        if (index.parent() != parent)
            continue;

        if (index.row() >= start && index.row() <= end) {
            selectionModel()->select(index, QItemSelectionModel::Clear);
        }
    }

    DListView::rowsAboutToBeRemoved(parent, start, end);
}

void DFileView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    DListView::rowsInserted(parent, start, end);
}

bool DFileView::isEmptyArea(const QModelIndex &index, const QPoint &pos) const
{
    if(index.isValid() && selectionModel()->selectedIndexes().contains(index)) {
        return false;
    } else {
        const QRect &rect = visualRect(index);

        if(!rect.contains(pos))
            return true;

        QStyleOptionViewItem option = viewOptions();

        option.rect = rect;

        const QList<QRect> &geometry_list = itemDelegate()->paintGeomertys(option, index);

        for(const QRect &rect : geometry_list) {
            if(rect.contains(pos)) {
                return false;
            }
        }
    }

    return true;
}

QSize DFileView::currentIconSize() const
{
    int size = m_iconSizes.value(m_currentIconSizeIndex);

    return QSize(size, size);
}

void DFileView::enlargeIcon()
{
    if(m_currentIconSizeIndex < m_iconSizes.count() - 1)
        ++m_currentIconSizeIndex;

    setIconSize(currentIconSize());
}

void DFileView::shrinkIcon()
{
    if(m_currentIconSizeIndex > 0)
        --m_currentIconSizeIndex;

    setIconSize(currentIconSize());
}

void DFileView::openIndex(const QModelIndex &index)
{
    qDebug() << index << model()->hasChildren(index);
    if(model()->hasChildren(index)){
        FMEvent event;

        event = model()->getUrlByIndex(index);
        event = FMEvent::FileView;
        event = windowId();
        qDebug() << event;
        emit fileSignalManager->requestChangeCurrentUrl(event);
    } else {
        emit fileService->openFile(model()->getUrlByIndex(index));
    }
}

void DFileView::keyboardSearch(const QString &search)
{
    m_keyboardSearchKeys.append(search);
    m_keyboardSearchTimer->start();
    QModelIndexList matchModelIndexListCaseSensitive = model()->match(rootIndex(), DFileSystemModel::FilePinyinName, m_keyboardSearchKeys, -1,
                                                                      Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap | Qt::MatchCaseSensitive | Qt::MatchRecursive));
    foreach (const QModelIndex& index, matchModelIndexListCaseSensitive) {
        QString absolutePath = FileInfo(model()->getUrlByIndex(index).path()).absolutePath();
        if (absolutePath == currentUrl().path()){
            setCurrentIndex(index);
            scrollTo(index, PositionAtTop);
            return;
        }
    }

    QModelIndexList matchModelIndexListNoCaseSensitive = model()->match(rootIndex(), DFileSystemModel::FilePinyinName, m_keyboardSearchKeys, -1,
                                                                        Qt::MatchFlags(Qt::MatchStartsWith|Qt::MatchWrap | Qt::MatchRecursive));
    foreach (const QModelIndex& index, matchModelIndexListNoCaseSensitive) {
        QString absolutePath = FileInfo(model()->getUrlByIndex(index).path()).absolutePath();
        if (absolutePath == currentUrl().path()){
            setCurrentIndex(index);
            scrollTo(index, PositionAtTop);
            return;
        }
    }

}

bool DFileView::setCurrentUrl(DUrl fileUrl)
{

    if (fileUrl.isTrashFile() && fileUrl.path().isEmpty()){
        fileUrl.setPath("/");
    };
    const AbstractFileInfoPointer &info = FileServices::instance()->createFileInfo(fileUrl);

    if (!info){
        qDebug() << "This scheme isn't support";
        return false;
    }

    if(info->canRedirectionFileUrl()) {
        const DUrl old_url = fileUrl;

        fileUrl = info->redirectedFileUrl();

        qDebug() << "url redirected, from:" << old_url << "to:" << fileUrl;
    }

    qDebug() << "cd: current url:" << currentUrl() << "to url:" << fileUrl;

    const DUrl &currentUrl = this->currentUrl();


    if(currentUrl == fileUrl)
        return false;

//    QModelIndex index = model()->index(fileUrl);

//    if(!index.isValid())
    QModelIndex index = model()->setRootUrl(fileUrl);

//    model()->setActiveIndex(index);
    setRootIndex(index);

    if (!model()->canFetchMore(index)) {
        updateContentLabel();
    }

    if (m_currentViewMode == ListMode) {
        updateListHeaderViewProperty();
    } else {
        const QPair<int, int> &sort_config = FMStateManager::SortStates.value(fileUrl, QPair<int, int>(DFileSystemModel::FileDisplayNameRole, Qt::AscendingOrder));

        model()->setSortRole(sort_config.first, (Qt::SortOrder)sort_config.second);
    }

    if(info) {
        ViewModes modes = (ViewModes)info->supportViewMode();

        if(!testViewMode(modes, m_defaultViewMode)) {
            if(testViewMode(modes, IconMode)) {
                switchViewMode(IconMode);
            } else if(testViewMode(modes, ListMode)) {
                switchViewMode(ListMode);
            } else if(testViewMode(modes, ExtendMode)) {
                switchViewMode(ExtendMode);
            }
        } else {
            switchViewMode(m_defaultViewMode);
        }
    } 
    //emit currentUrlChanged(fileUrl);

    if (focusWidget() && focusWidget()->window() == window() && fileUrl.isLocalFile())
        QDir::setCurrent(fileUrl.toLocalFile());

    setSelectionMode(info->supportSelectionMode());

    const DUrl &defaultSelectUrl = DUrl(QUrlQuery(fileUrl.query()).queryItemValue("selectUrl"));

    if (defaultSelectUrl.isValid()) {
        preSelectionUrls << defaultSelectUrl;
    } else {
        DUrl oldCurrentUrl = currentUrl;

        forever {
            const DUrl &tmp_url = oldCurrentUrl.parentUrl();

            if (tmp_url == fileUrl || !tmp_url.isValid())
                break;

            oldCurrentUrl = tmp_url;
        }

        preSelectionUrls << oldCurrentUrl;
    }

    return true;
}

void DFileView::clearHeardView()
{
    if(m_headerView) {
        removeHeaderWidget(0);

        m_headerView = Q_NULLPTR;
    }
}

void DFileView::clearKeyBoardSearchKeys()
{
    m_keyboardSearchKeys.clear();
    m_keyboardSearchTimer->stop();
}

void DFileView::setFoucsOnFileView(const FMEvent &event)
{
    if (event.windowId() == windowId())
        setFocus();
}

void DFileView::refreshFileView(const FMEvent &event)
{
    if (event.windowId() != windowId()){
        return;
    }
    model()->refresh();
}

void DFileView::clearSelection()
{
    QListView::clearSelection();
}

void DFileView::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    DListView::dataChanged(topLeft, bottomRight, roles);
}

void DFileView::updateStatusBar()
{
    if (model()->state() != DFileSystemModel::Idle)
        return;

    FMEvent event;
    event = windowId();
    event = selectedUrls();
    int count = selectedIndexCount();

    if (count == 0){
        emit fileSignalManager->statusBarItemsCounted(event, this->count());
    }else{
        emit fileSignalManager->statusBarItemsSelected(event, count);
    }
}

void DFileView::setSelectionRectVisible(bool visible)
{
    m_selectionRectVisible = visible;
}

bool DFileView::isSelectionRectVisible() const
{
    return m_selectionRectVisible;
}

bool DFileView::canShowSelectionRect() const
{
    return m_selectionRectVisible && selectionMode() != NoSelection && selectionMode() != SingleSelection;
}

void DFileView::setContentLabel(const QString &text)
{
    if (text.isEmpty()) {
        if (m_contentLabel) {
            m_contentLabel->deleteLater();
            m_contentLabel = Q_NULLPTR;
        }

        return;
    }

    if (!m_contentLabel) {
        m_contentLabel = new QLabel(this);

        m_contentLabel->show();
        m_contentLabel.setCenterIn(this);
        m_contentLabel->setObjectName("contentLabel");
        m_contentLabel->setStyleSheet(this->styleSheet());
        m_contentLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    m_contentLabel->setText(text);
    m_contentLabel->adjustSize();
}

void DFileView::updateHorizontalOffset()
{
    if (isIconViewMode()) {
        int contentWidth = width();
        int itemWidth = itemSizeHint().width() + ICON_VIEW_SPACING * 2;
        int itemColumn = (contentWidth - ICON_VIEW_SPACING * 2.9) / itemWidth;

        m_horizontalOffset = -(contentWidth - itemWidth * itemColumn) / 2;
    } else {
        m_horizontalOffset = 0;
    }
}

void DFileView::switchViewMode(DFileView::ViewMode mode)
{
    if (m_currentViewMode == mode) {
        return;
    }

    const AbstractFileInfoPointer &fileInfo = model()->fileInfo(currentUrl());

    if (fileInfo && (fileInfo->supportViewMode() & mode) == 0) {
        return;
    }

    m_currentViewMode = mode;

    itemDelegate()->hideAllIIndexWidget();

    switch (mode) {
    case IconMode: {
        clearHeardView();
        m_columnRoles.clear();
        setIconSize(currentIconSize());
        setOrientation(QListView::LeftToRight, true);
        setSpacing(ICON_VIEW_SPACING);

        break;
    }
    case ListMode: {
        itemDelegate()->hideAllIIndexWidget();

        if(!m_headerView) {
            m_headerView = new QHeaderView(Qt::Horizontal);

            updateListHeaderViewProperty();

            m_headerView->setHighlightSections(false);
            m_headerView->setSectionsClickable(true);
            m_headerView->setSortIndicatorShown(true);
            m_headerView->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_headerView->setContextMenuPolicy(Qt::CustomContextMenu);

            if(selectionModel()) {
                m_headerView->setSelectionModel(selectionModel());
            }

            connect(m_headerView, &QHeaderView::sectionResized,
                    this, static_cast<void (DFileView::*)()>(&DFileView::update));
            connect(m_headerView, &QHeaderView::sortIndicatorChanged,
                    model(), &QAbstractItemModel::sort);
            connect(m_headerView, &QHeaderView::customContextMenuRequested,
                    this, &DFileView::popupHeaderViewContextMenu);

            m_headerView->setAttribute(Qt::WA_TransparentForMouseEvents, model()->state() == DFileSystemModel::Busy);
        }

        addHeaderWidget(m_headerView);

        setIconSize(QSize(LIST_VIEW_ICON_SIZE, LIST_VIEW_ICON_SIZE));
        setOrientation(QListView::TopToBottom, false);
        setSpacing(LIST_VIEW_SPACING);

        break;
    }
    case ExtendMode: {
        itemDelegate()->hideAllIIndexWidget();
        if(!m_headerView) {
            m_headerView = new QHeaderView(Qt::Horizontal);

            updateExtendHeaderViewProperty();

            m_headerView->setHighlightSections(true);
            m_headerView->setSectionsClickable(true);
            m_headerView->setSortIndicatorShown(true);
            m_headerView->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            if(selectionModel()) {
                m_headerView->setSelectionModel(selectionModel());
            }

            connect(m_headerView, &QHeaderView::sectionResized,
                    this, static_cast<void (DFileView::*)()>(&DFileView::update));
            connect(m_headerView, &QHeaderView::sortIndicatorChanged,
                    model(), &QAbstractItemModel::sort);
        }
        setColumnWidth(0, 200);
        setIconSize(QSize(30, 30));
        setOrientation(QListView::TopToBottom, false);
        setSpacing(LIST_VIEW_SPACING);
        break;
    }
    default:
        break;
    }

    updateItemSizeHint();

    emit viewModeChanged(mode);
}

void DFileView::showEmptyAreaMenu()
{
    const QModelIndex &index = rootIndex();
    const AbstractFileInfoPointer &info = model()->fileInfo(index);
    info->updateFileInfo();

    const QVector<MenuAction> &actions = info->menuActionList(AbstractFileInfo::SpaceArea);

    if (actions.isEmpty())
        return;

    const QMap<MenuAction, QVector<MenuAction> > &subActions = info->subMenuActionList();

    QSet<MenuAction> disableList = FileMenuManager::getDisableActionList(model()->getUrlByIndex(index));

    if (!count())
        disableList << MenuAction::SelectAll;

    DFileMenu *menu = FileMenuManager::genereteMenuByKeys(actions, disableList, true, subActions);
    DAction *tmp_action = menu->actionAt(fileMenuManger->getActionString(MenuAction::DisplayAs));
    DFileMenu *displayAsSubMenu = static_cast<DFileMenu*>(tmp_action ? tmp_action->menu() : Q_NULLPTR);
    tmp_action = menu->actionAt(fileMenuManger->getActionString(MenuAction::SortBy));
    DFileMenu *sortBySubMenu = static_cast<DFileMenu*>(tmp_action ? tmp_action->menu() : Q_NULLPTR);

    for (QAction *action : m_displayAsActionGroup->actions()) {
        m_displayAsActionGroup->removeAction(action);
    }

    if (displayAsSubMenu){
        foreach (DAction* action, displayAsSubMenu->actionList()) {
            action->setActionGroup(m_displayAsActionGroup);
        }
        if (m_currentViewMode == IconMode){
            displayAsSubMenu->actionAt(fileMenuManger->getActionString(MenuAction::IconView))->setChecked(true);
        }else if (m_currentViewMode == ListMode){
            displayAsSubMenu->actionAt(fileMenuManger->getActionString(MenuAction::ListView))->setChecked(true);
        }else if (m_currentViewMode == ExtendMode){
            displayAsSubMenu->actionAt(fileMenuManger->getActionString(MenuAction::ExtendView))->setChecked(true);
        }
    }

    for (QAction *action : m_sortByActionGroup->actions()) {
        m_sortByActionGroup->removeAction(action);
    }

    if (sortBySubMenu){
        foreach (DAction* action, sortBySubMenu->actionList()) {
            action->setActionGroup(m_sortByActionGroup);
        }

        DAction *action = sortBySubMenu->actionAt(model()->sortColumn());

        if (action)
            action->setChecked(true);
    }

    DUrlList urls;
    urls.append(currentUrl());

    FMEvent event;
    event = currentUrl();
    event = urls;
    event = windowId();
    event = FMEvent::FileView;
    menu->setEvent(event);


    menu->exec();
    menu->deleteLater();
}


void DFileView::showNormalMenu(const QModelIndex &index)
{
    if(!index.isValid())
        return;

    DUrlList list = selectedUrls();

    DFileMenu* menu;

    const AbstractFileInfoPointer &info = model()->fileInfo(index);
    info->updateFileInfo();

    if (list.length() == 1) {
        const QVector<MenuAction> &actions = info->menuActionList(AbstractFileInfo::SingleFile);

        if (actions.isEmpty())
            return;

        const QMap<MenuAction, QVector<MenuAction> > &subActions = info->subMenuActionList();
        const QSet<MenuAction> &disableList = FileMenuManager::getDisableActionList(list);

        menu = FileMenuManager::genereteMenuByKeys(actions, disableList, true, subActions);

        DAction *openWithAction = menu->actionAt(FileMenuManager::getActionString(MenuActionType::OpenWith));
        DFileMenu* openWithMenu = openWithAction ? qobject_cast<DFileMenu*>(openWithAction->menu()) : Q_NULLPTR;

        if (openWithMenu) {
            QMimeType mimeType = info->mimeType();
            QStringList recommendApps = mimeAppsManager->MimeApps.value(mimeType.name());

            foreach (QString name, mimeType.aliases()) {
                QStringList apps = mimeAppsManager->MimeApps.value(name);
                foreach (QString app, apps) {
                    if (!recommendApps.contains(app)){
                        recommendApps.append(app);
                    }
                }
            }

            for (QAction *action : m_openWithActionGroup->actions()) {
                m_openWithActionGroup->removeAction(action);
            }

            foreach (QString app, recommendApps) {
                DAction* action = new DAction(mimeAppsManager->DesktopObjs.value(app).getLocalName(), 0);
                action->setProperty("app", app);
                action->setProperty("url", info->redirectedFileUrl().toLocalFile());
                openWithMenu->addAction(action);
                m_openWithActionGroup->addAction(action);
            }

            DAction* action = new DAction(fileMenuManger->getActionString(MenuAction::OpenWithCustom), 0);
            action->setData((int)MenuAction::OpenWithCustom);
            openWithMenu->addAction(action);
        }
    } else {
        bool isSystemPathIncluded = false;
        bool isAllCompressedFiles = true;

        foreach (DUrl url, list) {
            const AbstractFileInfoPointer &fileInfo = fileService->createFileInfo(url);

            if(!FileUtils::isArchive(url.path()))
                isAllCompressedFiles = false;

            if (systemPathManager->isSystemPath(fileInfo->redirectedFileUrl().toLocalFile())) {
                isSystemPathIncluded = true;
            }
        }

        QVector<MenuAction> actions;

        if (isSystemPathIncluded)
            actions = info->menuActionList(AbstractFileInfo::MultiFilesSystemPathIncluded);
        else
            actions = info->menuActionList(AbstractFileInfo::MultiFiles);

        if (actions.isEmpty())
            return;

        if(isAllCompressedFiles){
            actions.insert(actions.length() - 4, MenuAction::Decompress);
            actions.insert(actions.length() - 4, MenuAction::DecompressHere);
        }

        const QMap<MenuAction, QVector<MenuAction> > subActions;
        const QSet<MenuAction> &disableList = FileMenuManager::getDisableActionList(list);
        menu = FileMenuManager::genereteMenuByKeys(actions, disableList, true, subActions);
    }

    FMEvent event;

    event = info->fileUrl();
    event = list;
    event = windowId();
    event = FMEvent::FileView;

    menu->setEvent(event);
    menu->exec();
    menu->deleteLater();
}

void DFileView::updateListHeaderViewProperty()
{
    if (!m_headerView)
        return;

    m_headerView->setModel(Q_NULLPTR);
    m_headerView->setModel(model());
    m_headerView->setSectionResizeMode(QHeaderView::Fixed);
    m_headerView->setDefaultSectionSize(DEFAULT_HEADER_SECTION_WIDTH);
    m_headerView->setMinimumSectionSize(DEFAULT_HEADER_SECTION_WIDTH);

    const QPair<int, int> &sort_config = FMStateManager::SortStates.value(currentUrl(), QPair<int, int>(DFileSystemModel::FileDisplayNameRole, Qt::AscendingOrder));
    int sort_column = model()->roleToColumn(sort_config.first);

    m_headerView->setSortIndicator(sort_column, Qt::SortOrder(sort_config.second));

    m_columnRoles.clear();

    for (int i = 0; i < m_headerView->count(); ++i) {
        m_columnRoles << model()->columnToRole(i);

        int column_width = model()->columnWidth(i);

        if (column_width >= 0) {
            m_headerView->resizeSection(i, column_width + COLUMU_PADDING * 2);
        } else {
            m_headerView->setSectionResizeMode(i, QHeaderView::Stretch);
        }

        const QString &column_name = model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();

        if (!m_columnForRoleHiddenMap.contains(column_name)) {
            m_headerView->setSectionHidden(i, !model()->columnDefaultVisibleForRole(model()->columnToRole(i)));
        } else {
            m_headerView->setSectionHidden(i, m_columnForRoleHiddenMap.value(column_name));
        }
    }

    updateColumnWidth();
}

void DFileView::updateExtendHeaderViewProperty()
{
    if(!m_headerView)
        return;
    m_headerView->setModel(Q_NULLPTR);
    m_headerView->setModel(model());
    m_headerView->setSectionResizeMode(QHeaderView::Fixed);
    m_headerView->setSectionResizeMode(0, QHeaderView::Stretch);
    m_headerView->setDefaultSectionSize(DEFAULT_HEADER_SECTION_WIDTH);
    m_headerView->setMinimumSectionSize(DEFAULT_HEADER_SECTION_WIDTH);

    m_columnRoles.clear();
    m_columnRoles << model()->columnToRole(0);
}

void DFileView::updateItemSizeHint()
{
    if(isIconViewMode()) {
        int width = iconSize().width() * 1.8;

        m_itemSizeHint = QSize(width, iconSize().height() + 2 * TEXT_PADDING  + ICON_MODE_ICON_SPACING + LIST_VIEW_SPACING + 3 * TEXT_LINE_HEIGHT);
    } else {
        m_itemSizeHint = QSize(-1, iconSize().height() * 1.1);
    }
}

void DFileView::updateColumnWidth()
{
    int column_count = m_headerView->count();
    int i = 0;
    int j = column_count - 1;

    for (; i < column_count; ++i) {
        if (m_headerView->isSectionHidden(i))
            continue;

        m_headerView->resizeSection(i, model()->columnWidth(i) + LEFT_PADDING + LIST_MODE_LEFT_MARGIN + 2 * COLUMU_PADDING);
        break;
    }

    for (; j > 0; --j) {
        if (m_headerView->isSectionHidden(j))
            continue;

        m_headerView->resizeSection(j, model()->columnWidth(j) + RIGHT_PADDING + LIST_MODE_RIGHT_MARGIN + 2 * COLUMU_PADDING);
        break;
    }

    if (firstVisibleColumn != i) {
        if (firstVisibleColumn > 0)
            m_headerView->resizeSection(firstVisibleColumn, model()->columnWidth(firstVisibleColumn) + 2 * COLUMU_PADDING);

        firstVisibleColumn = i;
    }

    if (lastVisibleColumn != j) {
        if (lastVisibleColumn > 0)
            m_headerView->resizeSection(lastVisibleColumn, model()->columnWidth(lastVisibleColumn) + 2 * COLUMU_PADDING);

        lastVisibleColumn = j;
    }
}

void DFileView::popupHeaderViewContextMenu(const QPoint &/*pos*/)
{
    DMenu *menu = new DMenu();

    for (int i = 1; i < m_headerView->count(); ++i) {
        DAction *action = new DAction(menu);

        action->setText(model()->columnNameByRole(model()->columnToRole(i)).toString());
        action->setCheckable(true);
        action->setChecked(!m_headerView->isSectionHidden(i));

        connect(action, &DAction::triggered, this, [this, action, i] {
            m_columnForRoleHiddenMap[action->text()] = action->isChecked();

            m_headerView->setSectionHidden(i, action->isChecked());

            updateColumnWidth();
        });

        menu->addAction(action);
    }

    menu->exec();
}

void DFileView::onModelStateChanged(int state)
{
    FMEvent event;

    event = windowId();
    event = currentUrl();

    emit fileSignalManager->loadingIndicatorShowed(event, state == DFileSystemModel::Busy);

    if (state == DFileSystemModel::Busy) {
        setContentLabel(QString());

        disconnect(this, &DFileView::rowCountChanged, this, &DFileView::updateContentLabel);

        if (m_headerView) {
            m_headerView->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
    } else if (state == DFileSystemModel::Idle) {
        for (const DUrl &url : preSelectionUrls) {
            selectionModel()->select(model()->index(url), QItemSelectionModel::SelectCurrent);
        }

        if (!preSelectionUrls.isEmpty()) {
            scrollTo(model()->index(preSelectionUrls.first()), PositionAtTop);
        }

        preSelectionUrls.clear();

        updateStatusBar();
        updateContentLabel();

        connect(this, &DFileView::rowCountChanged, this, &DFileView::updateContentLabel);

        if (m_headerView) {
            m_headerView->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        }
    }
}

void DFileView::updateContentLabel()
{
    int count = this->count();
    const DUrl &currentUrl = this->currentUrl();

    if (count <= 0) {
        const AbstractFileInfoPointer &fileInfo = fileService->createFileInfo(currentUrl);

        setContentLabel(fileInfo->subtitleForEmptyFloder());
    } else {
        setContentLabel(QString());
    }
}

void DFileView::updateSelectionRect()
{
    if (dragEnabled())
        return;

    QPoint pos = mapFromGlobal(QCursor::pos());

    if (m_selectionRectWidget->isVisible()) {
        QRect rect;
        QPoint pressedPos = viewport()->mapToParent(m_selectedGeometry.topLeft());

        pressedPos.setX(pressedPos.x() - horizontalOffset());
        pressedPos.setY(pressedPos.y() - verticalOffset());

        rect.setCoords(qMin(pressedPos.x(), pos.x()), qMin(pressedPos.y(), pos.y()),
                       qMax(pos.x(), pressedPos.x()), qMax(pos.y(), pressedPos.y()));

        m_selectionRectWidget->setGeometry(rect);
    }

    pos = viewport()->mapFromParent(pos);

    m_selectedGeometry.setBottom(pos.y() + verticalOffset());
    m_selectedGeometry.setRight(pos.x() + horizontalOffset());

    setSelection(m_selectedGeometry, QItemSelectionModel::Current|QItemSelectionModel::Rows|QItemSelectionModel::ClearAndSelect);
}

void DFileView::preproccessDropEvent(QDropEvent *event) const
{
    if (event->source() == this && !Global::keyCtrlIsPressed()) {
        event->setDropAction(Qt::MoveAction);
    } else {
        AbstractFileInfoPointer info = model()->fileInfo(indexAt(event->pos()));

        if (!info)
            info = model()->fileInfo(rootIndex());

        if (info && !info->supportedDropActions().testFlag(event->dropAction())) {
            QList<Qt::DropAction> actions;

            actions.reserve(3);
            actions << Qt::CopyAction << Qt::MoveAction << Qt::LinkAction;

            for (Qt::DropAction action : actions) {
                if (event->possibleActions().testFlag(action) && info->supportedDropActions().testFlag(action)) {
                    event->setDropAction(action);

                    break;
                }
            }
        }
    }
}
