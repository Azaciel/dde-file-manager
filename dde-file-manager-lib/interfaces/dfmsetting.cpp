/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dfmsetting.h"

#include <QJsonDocument>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QProcess>

#include <DSettings>
#include <qsettingbackend.h>

#include "dfmstandardpaths.h"
#include "singleton.h"
#include "app/filesignalmanager.h"
#include "interfaces/dfileservices.h"
#include "interfaces/dabstractfilewatcher.h"
#include "app/define.h"
#include "shutil/fileutils.h"

DFMSetting *DFMSetting::instance()
{
    return globalSetting;
}

DFMSetting::DFMSetting(QObject *parent) : QObject(parent)
{
    m_newTabOptionPaths
            << "Current Path"
            << DFMStandardPaths::standardLocation(DFMStandardPaths::ComputerRootPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::HomePath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::DesktopPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::VideosPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::MusicPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::PicturesPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::DocumentsPath)
            << DFMStandardPaths::standardLocation(DFMStandardPaths::DownloadsPath);

    m_defaultWindowOptionPaths = m_newTabOptionPaths;
    m_defaultWindowOptionPaths.removeFirst();


#ifdef DISABLE_COMPRESS_PREIVEW
    //load temlate
    m_settings = Dtk::Core::DSettings::fromJsonFile(":/configure/global-setting-template-pro.json").data();
#else
#ifndef SUPPORT_FFMEPG
    m_settings = Dtk::Core::DSettings::fromJsonFile(":/configure/global-setting-template-fedora.json").data();
#else
    m_settings = Dtk::Core::DSettings::fromJsonFile(":/configure/global-setting-template.json").data();
#endif
#endif

    //load conf value
    auto backen = new DTK_CORE_NAMESPACE::QSettingBackend(getConfigFilePath());
    m_settings->setBackend(backen);

    m_fileSystemWathcer = fileService->createFileWatcher(this, DUrl::fromLocalFile(getConfigFilePath()).parentUrl(), this);
    m_fileSystemWathcer->startWatcher();

    initConnections();
}

void DFMSetting::initConnections()
{
    connect(m_settings, &Dtk::Core::DSettings::valueChanged, this, &DFMSetting::onValueChanged);
    connect(m_fileSystemWathcer, &DAbstractFileWatcher::fileMoved, this, &DFMSetting::onConfigFileChanged);
}

QVariant DFMSetting::getValueByKey(const QString &key)
{
    return m_settings->value(key);
}

bool DFMSetting::isAllwayOpenOnNewWindow()
{
    if (DFMGlobal::IsFileManagerDiloagProcess){
        return false;
    }
    return getValueByKey("base.open_action.allways_open_on_new_window").toBool();
}

int DFMSetting::iconSizeIndex()
{
    return getValueByKey("base.default_view.icon_size").toInt();
}

int DFMSetting::viewMode()
{
    return getValueByKey("base.default_view.view_mode").toInt() + 1;
}

int DFMSetting::openFileAction()
{
    if (DFMGlobal::IsFileManagerDiloagProcess){
        return 1;
    }
    const int& index = getValueByKey("base.open_action.open_file_action").toInt();
    return index;
}

QString DFMSetting::defaultWindowPath()
{
    const int& index = getValueByKey("base.new_tab_windows.default_window_path").toInt();
    if(index < m_defaultWindowOptionPaths.count() && index >= 0)
        return m_defaultWindowOptionPaths[index];
    return DFMStandardPaths::standardLocation(DFMStandardPaths::HomePath);
}

QString DFMSetting::newTabPath()
{
    const int& index = getValueByKey("base.new_tab_windows.new_tab_path").toInt();
    if(index < m_newTabOptionPaths.count() && index >= 0)
        return m_newTabOptionPaths[index];
    return "Current Path";
}

QString DFMSetting::getConfigFilePath()
{
    return QString("%1/%2").arg(DFMStandardPaths::getConfigPath(), "dde-file-manager.conf");
}

QPointer<Dtk::Core::DSettings> DFMSetting::settings()
{
    return m_settings;
}

void DFMSetting::onValueChanged(const QString &key, const QVariant &value)
{
    Q_UNUSED(value);
    QStringList previewKeys;
    previewKeys << "advance.preview.text_file_preview"
              << "advance.preview.document_file_preview"
              << "advance.preview.image_file_preview"
              << "advance.preview.video_file_preview";

    if(previewKeys.contains(key)){
        emit fileSignalManager->requestFreshAllFileView();
    } else if(key == "base.default_view.icon_size"){
        emit fileSignalManager->requestChangeIconSizeBySizeIndex(iconSizeIndex());
    } else if(key == "base.hidden_files.show_hidden"){
        emit fileSignalManager->showHiddenOnViewChanged();
    } else if (key == "advance.preview.compress_file_preview"){
        if (value.toBool()){
            FileUtils::mountAVFS();
        }else{
            FileUtils::umountAVFS();
        }
    }else if (key == "base.default_view.view_mode"){
        emit fileSignalManager->defaultViewModeChanged(viewMode());
    }
}

void DFMSetting::onConfigFileChanged(const DUrl &fromUrl, const DUrl &toUrl)
{
    Q_UNUSED(fromUrl)
    if (toUrl == DUrl::fromLocalFile(getConfigFilePath())){
        auto backen = new DTK_CORE_NAMESPACE::QSettingBackend(getConfigFilePath());
        m_settings->setBackend(backen);
        qDebug() << toUrl;
        emit showHiddenChanged(isShowedHiddenOnView());
    }
}

bool DFMSetting::isQuickSearch()
{
    return getValueByKey("advance.search.quick_search").toBool();
}

bool DFMSetting::isCompressFilePreview()
{
    return getValueByKey("advance.preview.compress_file_preview").toBool();
}

bool DFMSetting::isTextFilePreview()
{
    return getValueByKey("advance.preview.text_file_preview").toBool();
}

bool DFMSetting::isDocumentFilePreview()
{
    return getValueByKey("advance.preview.document_file_preview").toBool();
}

bool DFMSetting::isImageFilePreview()
{
    return getValueByKey("advance.preview.image_file_preview").toBool();
}

bool DFMSetting::isVideoFilePreview()
{
    return getValueByKey("advance.preview.video_file_preview").toBool();
}

bool DFMSetting::isAutoMount()
{
    return getValueByKey("advance.mount.auto_mount").toBool();
}

bool DFMSetting::isAutoMountAndOpen()
{
    return getValueByKey("advance.mount.auto_mount_and_open").toBool();
}

bool DFMSetting::isDefaultChooserDialog()
{
    return getValueByKey("advance.dialog.default_chooser_dialog").toBool();
}

bool DFMSetting::isShowedHiddenOnSearch()
{
    return getValueByKey("advance.search.show_hidden").toBool();
}

bool DFMSetting::isShowedHiddenOnView()
{
    return getValueByKey("base.hidden_files.show_hidden").toBool();
}
