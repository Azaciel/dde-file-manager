#include "filejob.h"
#include "../app/global.h"
#include "../app/filesignalmanager.h"
#include "../shutil/fileutils.h"

#include "widgets/singleton.h"

#include <QFile>
#include <QThread>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDirIterator>
#include <QProcess>
#include <QCryptographicHash>

QMap<DUrl, int> FileJob::SelectedFiles;

int FileJob::FileJobCount = 0;

void FileJob::setStatus(FileJob::Status status)
{
    m_status = status;
}

FileJob::FileJob(const QString &title, QObject *parent) : QObject(parent)
{
    FileJobCount += 1;
    m_status = FileJob::Started;
    QString user = getenv("USER");
    m_trashLoc = "/home/" + user + "/.local/share/Trash";
    m_id = QString::number(FileJobCount);
    m_title = title;
    connect(this, &FileJob::finished, this, &FileJob::handleJobFinished);
}

void FileJob::setJobId(const QString &id)
{
    m_id = id;
}

QString FileJob::getJobId()
{
    return m_id;
}

QString FileJob::checkDuplicateName(const QString &name)
{
    QString destUrl = name;
    QFile file(destUrl);
    QFileInfo startInfo(destUrl);
    int num = 1;
    while (file.exists())
    {
        if(num == 1)
        {
            if(startInfo.isDir())
                destUrl = QString("%1/%2(copy)").arg(startInfo.absolutePath()).
                        arg(startInfo.fileName());
            else
                destUrl = QString("%1/%2(copy).%3").arg(startInfo.absolutePath()).
                        arg(startInfo.baseName()).arg(startInfo.completeSuffix());
        }
        else
        {
            if(startInfo.isDir())
                destUrl = QString("%1/%2(copy %3)").arg(startInfo.absolutePath()).
                        arg(startInfo.fileName()).arg(num);
            else
                destUrl = QString("%1/%2(copy %3).%4").arg(startInfo.absolutePath()).
                        arg(startInfo.baseName()).arg(num).arg(startInfo.completeSuffix());
        }
        num++;
        file.setFileName(destUrl);
    }
    return destUrl;
}

void FileJob::setApplyToAll(bool v)
{
    m_applyToAll = v;
}

void FileJob::setReplace(bool v)
{
    m_isReplaced = v;
    qDebug() << m_srcPath << m_tarPath << m_id;
}

int FileJob::getWindowId()
{
    return m_windowId;
}

QString FileJob::getTargetDir()
{
    return m_tarPath;
}

DUrlList FileJob::doCopy(const DUrlList &files, const QString &destination)
{
    DUrlList list;

    qDebug() << "Do copy is started";
    //pre-calculate total size
    m_totalSize = FileUtils::totalSize(files);
    jobPrepared();
    for(int i = 0; i < files.size(); i++)
    {
        QUrl url = files.at(i);
        QDir srcDir(url.toLocalFile());
        QString targetPath;

        if(srcDir.exists())
        {
            copyDir(url.toLocalFile(), QUrl(destination).toLocalFile(), false,  &targetPath);
        }
        else
            copyFile(url.toLocalFile(), QUrl(destination).toLocalFile(), false,  &targetPath);

        if (!targetPath.isEmpty())
            list << DUrl::fromLocalFile(targetPath);
    }
    if(m_isJobAdded)
        jobRemoved();
    emit finished();
    qDebug() << "Do copy is done!";

    return list;
}

void FileJob::doDelete(const DUrlList &files)
{
    qDebug() << "Do delete is started";
    for(int i = 0; i < files.size(); i++)
    {
        QUrl url = files.at(i);
        QFileInfo info(url.path());
        if (info.isFile() || info.isSymLink()){
            deleteFile(url.path());
        }else{
            if (!deleteDir(url.path())) {
                QProcess::execute("rm -r \"" + url.path().toUtf8() + "\"");
            }
        }
    }
    if(m_isJobAdded)
        jobRemoved();
    emit finished();
    qDebug() << "Do delete is done!";
}

DUrlList FileJob::doMoveToTrash(const DUrlList &files)
{
    QDir trashDir;
    DUrlList list;

    if(!trashDir.mkpath(TRASHFILEPATH)) {
        qDebug() << "mk" << TRASHINFOPATH << "failed!";
        /// TODO

        return list;
    }

    if(!trashDir.mkpath(TRASHINFOPATH)) {
        qDebug() << "mk" << TRASHINFOPATH << "failed!";
        /// TODO

        return list;
    }

    bool ok = true;

    for(int i = 0; i < files.size(); i++)
    {
        QUrl url = files.at(i);
        QDir dir(url.path());
        QString targetPath;

        if(dir.exists())
            ok = ok && moveDirToTrash(url.path(), &targetPath);
        else
            ok = ok && moveFileToTrash(url.path(), &targetPath);

        if (!targetPath.isEmpty())
            list << DUrl::fromLocalFile(targetPath);
    }
    if(m_isJobAdded)
        jobRemoved();

    emit finished();

    if (ok)
        qDebug() << "Move to Trash is done!";

    return list;
}

DUrlList FileJob::doMove(const DUrlList &files, const QString &destination)
{
    qDebug() << "Do move is started" << files << destination;
    m_totalSize = FileUtils::totalSize(files);
    jobPrepared();

    DUrlList list;
    QString tarLocal = QUrl(destination).toLocalFile();
    QStorageInfo tarInfo(tarLocal);
    QDir tarDir(tarLocal);

    if(!tarDir.exists())
    {
        qDebug() << "Destination must be directory";
        return list;
    }

    for(int i = 0; i < files.size(); i++)
    {
        QUrl url = files.at(i);
        QDir srcDir(url.toLocalFile());
        QStorageInfo srcInfo(srcDir);
        QString targetPath;

        if(srcDir.exists())
        {
            if(srcInfo.rootPath() == tarInfo.rootPath())
            {
                if (!moveDir(url.toLocalFile(), tarDir.path(), &targetPath)) {
                    if(copyDir(url.toLocalFile(), tarLocal, true, &targetPath))
                        deleteDir(url.toLocalFile());
                }
            }
            else
            {
                if(copyDir(url.toLocalFile(), tarLocal, true, &targetPath))
                    deleteDir(url.toLocalFile());
            }
        }
        else
        {
            if(srcInfo.rootPath() == tarInfo.rootPath())
            {
                if (!moveFile(url.toLocalFile(), tarDir.path(), &targetPath)) {
                    if(copyFile(url.toLocalFile(), QUrl(destination).toLocalFile(), true, &targetPath))
                        deleteFile(url.toLocalFile());
                }
            }
            else
            {
                if(copyFile(url.toLocalFile(), QUrl(destination).toLocalFile(), true, &targetPath))
                    deleteFile(url.toLocalFile());
            }
        }

        if (!targetPath.isEmpty())
            list << DUrl::fromLocalFile(targetPath);
    }
    if(m_isJobAdded)
        jobRemoved();
    emit finished();
    qDebug() << "Do move is done!";

    return list;
}

void FileJob::doTrashRestore(const QString &srcFile, const QString &tarFile)
{
//    qDebug() << srcFile << tarFile;
    qDebug() << "Do restore trash file is started";
    qDebug() << m_srcPath << m_tarPath << m_id;
    DUrlList files;
    files << QUrl::fromLocalFile(srcFile);
    m_totalSize = FileUtils::totalSize(files);
    jobPrepared();

    restoreTrashFile(srcFile, tarFile);

    if(m_isJobAdded)
        jobRemoved();
    emit finished();
    qDebug() << "Do restore trash file is done!";
}

void FileJob::paused()
{
    m_status = FileJob::Paused;
}

void FileJob::started()
{
    m_status = FileJob::Started;
}

void FileJob::cancelled()
{
    m_status = FileJob::Cancelled;
}

void FileJob::handleJobFinished()
{
    qDebug() << m_status;
    m_bytesCopied = m_totalSize;
    jobUpdated();
}

void FileJob::jobUpdated()
{
    QMap<QString, QString> jobDataDetail;
    if(m_bytesPerSec > 0)
    {
        int remainTime = (m_totalSize - m_bytesCopied) / m_bytesPerSec;

        if (remainTime < 60){
            jobDataDetail.insert("remainTime", tr("%1 s").arg(QString::number(remainTime)));
        }else if (remainTime >=60  && remainTime < 3600){
            int min = remainTime / 60;
            int second = remainTime % 60;
            jobDataDetail.insert("remainTime", tr("%1 m %2 s").arg(QString::number(min),
                                                                        QString::number(second)));
        }else if (remainTime >=3600  && remainTime < 86400){
            int hour = remainTime / 3600;
            int min = (remainTime % 3600) / 60;
            int second = (remainTime % 3600) % 60;
            jobDataDetail.insert("remainTime", tr("%1 h %2 m %3 s").arg(QString::number(hour),
                                                                             QString::number(min),
                                                                             QString::number(second)));
        }else{
            int day = remainTime / 86400;
            int left = remainTime % 86400;
            int hour = left / 3600;
            int min = (left % 3600) / 60;
            int second = (left % 3600) % 60;
            jobDataDetail.insert("remainTime", tr("%1 d %2 h %3 m %4 s").arg(QString::number(day),
                                                                                  QString::number(hour),
                                                                                  QString::number(min),
                                                                                  QString::number(second)));
        }
    }
    QString speed;

    if (m_bytesCopied == m_totalSize){
        speed = QString("0 MB/s");
    }else{
        if(m_bytesPerSec > ONE_MB_SIZE)
        {
            m_bytesPerSec = m_bytesPerSec / ONE_MB_SIZE;
            speed = QString("%1 MB/s").arg(QString::number(m_bytesPerSec));
        }
        else
        {
            m_bytesPerSec = m_bytesPerSec / ONE_KB_SIZE;
            speed = QString("%1 KB/s").arg(QString::number(m_bytesPerSec));
        }
    }
    jobDataDetail.insert("speed", speed);
    jobDataDetail.insert("file", m_srcFileName);
    jobDataDetail.insert("progress", QString::number(m_bytesCopied * 100/ m_totalSize));
    jobDataDetail.insert("destination", m_tarFileName);
    emit fileSignalManager->jobDataUpdated(m_jobDetail, jobDataDetail);
}

void FileJob::jobAdded()
{
    if(m_isJobAdded)
        return;
    m_jobDetail.insert("jobId", m_id);
    m_jobDetail.insert("type", m_title);
    emit fileSignalManager->jobAdded(m_jobDetail);
    m_isJobAdded = true;
}

void FileJob::jobRemoved()
{
    emit fileSignalManager->jobRemoved(m_jobDetail);
}

void FileJob::jobAborted()
{
    emit fileSignalManager->abortTask(m_jobDetail);
}

void FileJob::jobPrepared()
{
    m_bytesCopied = 0;
    m_bytesPerSec = 0;
    m_timer.start();
    lastMsec = m_timer.elapsed();
    currentMsec = m_timer.elapsed();
}

void FileJob::jobConflicted()
{
    jobAdded();
    QMap<QString, QString> jobDataDetail;
    jobDataDetail.insert("remainTime", "");
    jobDataDetail.insert("speed", "");
    jobDataDetail.insert("file", m_srcFileName);
    jobDataDetail.insert("progress", "");
    jobDataDetail.insert("destination", m_tarFileName);
    emit fileSignalManager->jobDataUpdated(m_jobDetail, jobDataDetail);
    emit fileSignalManager->conflictDialogShowed(m_jobDetail);
    m_status = Paused;
}

bool FileJob::copyFile(const QString &srcFile, const QString &tarDir, bool isMoved, QString *targetPath)
{
    if(m_applyToAll && m_status == FileJob::Cancelled){
        return false;
    }else if(!m_applyToAll && m_status == FileJob::Cancelled){
        m_status = Started;
    }

    QFile from(srcFile);   
    QFileInfo sf(srcFile);
    QFileInfo tf(tarDir);
    m_srcFileName = sf.fileName();
    m_tarFileName = tf.fileName();
    m_srcPath = srcFile;
    m_tarPath = tarDir + "/" + m_srcFileName;
    QFile to(tarDir + "/" + m_srcFileName);
    m_status = Started;

    //We only check the conflict of the files when
    //they are not in the same folder
    if(sf.absolutePath() != tf.absoluteFilePath())
        if(to.exists() && !m_applyToAll)
        {
            if (!isMoved){
                jobConflicted();
            }else{
                m_isReplaced = true;
            }
        }

    bool isGreater = false;
    char block[DATA_BLOCK_SIZE];
    qint64 thres = 0;
    bool startToDisplay = false;
    while(true)
    {
        switch(m_status)
        {
            case FileJob::Started:
            {
                if(!m_isReplaced)
                {
                    m_tarPath = checkDuplicateName(m_tarPath);
                }
                else
                {
                    if(!m_applyToAll)
                        m_isReplaced = false;
                }
                if(!from.open(QIODevice::ReadOnly))
                {
                    //Operation failed
                    qDebug() << srcFile << "isn't read only";
                    return false;
                }
                to.setFileName(m_tarPath);
                if(!to.open(QIODevice::WriteOnly))
                {
                    //Operation failed
                    qDebug() << to.fileName() << to.error() << to.errorString() << "isn't write only";
                    QString toFileName =  tarDir + "/" + DUrl::toPercentEncoding(m_srcFileName);
                    qDebug() << "toPercentEncoding" <<  toFileName;
                    to.setFileName(toFileName);
                    if (!to.open(QIODevice::WriteOnly)){
                        qDebug() << to.fileName() << to.error() <<to.errorString() << "isn't write only";
                        return false;
                    }
                }
                m_status = Run;
                break;
            }
            case FileJob::Run:
            {
                if(from.atEnd())
                {
                    if ((m_totalSize - m_bytesCopied) <= 1){
                        m_bytesCopied = m_totalSize;
                    }
                    jobUpdated();
                    to.flush();
                    from.close();
                    to.close();

                    if (targetPath)
                        *targetPath = m_tarPath;

                    return true;
                }
                qint64 inBytes = from.read(block, DATA_BLOCK_SIZE);
                to.write(block, inBytes);
                m_bytesCopied += inBytes;

                if (m_bytesCopied % (1024 * 1024 * 16) == 0){
                    to.flush();
                    to.close();
                    if(!to.open(QIODevice::WriteOnly | QIODevice::Append))
                    {
                        //Operation failed
                        qDebug() << tarDir << "isn't write only";
                        return false;
                    }
                }
                m_bytesPerSec += inBytes;
                currentMsec = m_timer.elapsed();

                if(startToDisplay)
                {
                    if(isGreater)
                    {
                        if(currentMsec - lastMsec > MSEC_FOR_DISPLAY)
                        {
                            m_factor = (currentMsec - lastMsec);
                            m_factor /= 1000;
                            m_bytesPerSec /= m_factor;
                            jobUpdated();
                            lastMsec = m_timer.elapsed();
                            m_bytesPerSec = 0;
                        }
                    }
                    else
                    {
                        if(m_bytesPerSec > thres)
                        {
                            m_factor = (currentMsec - lastMsec);
                            m_factor /= 1000;

                            if(m_factor)
                                m_bytesPerSec /= m_factor;
                            else
                                m_bytesPerSec = 0;
                            jobUpdated();
                            lastMsec = m_timer.elapsed();
                            m_bytesPerSec = 0;
                        }
                    }
                    break;
                }

                if(currentMsec - lastMsec > MSEC_FOR_DISPLAY && !startToDisplay)
                {
                    startToDisplay = true;
                    if(!m_isJobAdded)
                        jobAdded();

                    if(m_totalSize / m_bytesCopied > TRANSFER_RATE)
                        isGreater = true;
                    else
                    {
                        thres = (m_totalSize - m_bytesCopied) / TRANSFER_RATE;
                        m_bytesPerSec = 0;
                    }
                }
                break;
            }
            case FileJob::Paused:
                QThread::msleep(100);
                lastMsec = m_timer.elapsed();
                break;
            case FileJob::Cancelled:
                from.close();
                to.close();
                return false;
            default:
                from.close();
                to.close();
                return false;
         }

    }
    return false;
}

bool FileJob::copyDir(const QString &srcPath, const QString &tarPath, bool isMoved, QString *targetPath)
{
    if(m_applyToAll && m_status == FileJob::Cancelled){
        return false;
    }else if(!m_applyToAll && m_status == FileJob::Cancelled){
        m_status = Started;
    }
    QDir sourceDir(srcPath);
    QDir targetDir(tarPath + "/" + sourceDir.dirName());
    QFileInfo sf(srcPath);
    QFileInfo tf(tarPath + "/" + sourceDir.dirName());
    m_srcFileName = sf.fileName();
    m_tarFileName = tf.dir().dirName();
    m_srcPath = srcPath;
    m_tarPath = targetDir.absolutePath();
    m_status = Started;
    //We only check the conflict of the files when
    //they are not in the same folder
    if(sf.absolutePath() != tf.absolutePath())
        if(targetDir.exists() && !m_applyToAll)
        {
            if (!isMoved){
                jobConflicted();
            }else{
                m_isReplaced = true;
            }
        }
    while(true)
    {
        switch(m_status)
        {
        case Started:
        {
            if(!m_isReplaced)
            {
                m_tarPath = checkDuplicateName(m_tarPath);
            }
            else
            {
                if(!m_applyToAll)
                    m_isReplaced = false;
            }
            targetDir.setPath(m_tarPath);
            if(!targetDir.exists())
            {
                if(!targetDir.mkdir(targetDir.absolutePath()))
                    return false;
            }
            m_status = Run;
            break;
        }
        case Run:
        {
            QFileInfoList fileInfoList = sourceDir.entryInfoList(QDir::AllEntries | QDir::System
                                                                  | QDir::NoDotAndDotDot | QDir::NoSymLinks
                                                                  | QDir::Hidden);            foreach(QFileInfo fileInfo, fileInfoList)
            {
                if(fileInfo.isDir())
                {
                    if(!copyDir(fileInfo.filePath(), targetDir.absolutePath())){
                        qDebug() << "coye dir" << fileInfo.filePath() << "failed";
                    }
                }
                else
                {
                    if(!copyFile(fileInfo.filePath(), targetDir.absolutePath()))
                    {
                        qDebug() << "coye file" << fileInfo.filePath() << "failed";
                    }
                }
            }

            if (targetPath)
                *targetPath = m_tarPath;

            return true;
            break;
        }
        case Paused:
            QThread::msleep(100);
            break;
        case Cancelled:
            return false;
        default: break;
        }
    }

    if (targetPath)
        *targetPath = m_tarPath;

    return true;
}

bool FileJob::moveFile(const QString &srcFile, const QString &tarDir, QString *targetPath)
{
    if(m_applyToAll && m_status == FileJob::Cancelled){
        return false;
    }else if(!m_applyToAll && m_status == FileJob::Cancelled){
        m_status = Started;
    }
    QFile from(srcFile);
    QDir to(tarDir);
    QFileInfo fromInfo(srcFile);
    m_srcFileName = fromInfo.absoluteFilePath();
    m_tarFileName = to.dirName();
    m_srcPath = srcFile;
    m_tarPath = tarDir;
    m_status = Started;

    //We only check the conflict of the files when
    //they are not in the same folder
    if(fromInfo.absolutePath() == to.path() && to.exists(fromInfo.fileName()))
        return false;
    else
        if(to.exists(fromInfo.fileName()) && !m_applyToAll)
        {
            jobConflicted();
        }

    while(true)
    {
        switch(m_status)
        {
            case FileJob::Started:
            {
                if(!m_isReplaced)
                {
                    m_srcPath = checkDuplicateName(m_tarPath + "/" + fromInfo.fileName());
                }
                else
                {
                    if(!m_applyToAll)
                        m_isReplaced = false;
                    m_srcPath = m_tarPath + "/" + fromInfo.fileName();
                }
                m_status = Run;
                break;
            }
            case FileJob::Run:
            {
                if(m_isReplaced)
                {
                    QFile localFile(to.path() + "/" + fromInfo.fileName());
                    if(localFile.exists())
                        localFile.remove();

                }

                bool ok = from.rename(m_srcPath);

                if (ok)
                    *targetPath = m_srcPath;

                return ok;
            }
            case FileJob::Paused:
                QThread::msleep(100);
                break;
            case FileJob::Cancelled:
                return true;
            default:
                return false;
         }

    }
    return false;
}

bool FileJob::restoreTrashFile(const QString &srcFile, const QString &tarFile)
{
//    if(m_applyToAll && m_status == FileJob::Cancelled){
//        return false;
//    }else if(!m_applyToAll && m_status == FileJob::Cancelled){
//        m_status = Started;
//    }
    qDebug() << srcFile << tarFile;
    QFile from(srcFile);
    QFile to(tarFile);

    QFileInfo toInfo(tarFile);
    m_srcFileName = toInfo.fileName();
    m_tarPath = toInfo.absoluteDir().path();
    m_tarFileName = toInfo.absoluteDir().dirName();
    m_status = Started;

    if(toInfo.exists())
    {
        jobConflicted();
    }else{
        bool result = from.rename(tarFile);

        if (!result) {
            result = (QProcess::execute("mv -T \"" + from.fileName().toUtf8() + "\" \"" + tarFile.toUtf8() + "\"") == 0);
        }

        return result;
    }

    while(true)
    {
        switch(m_status)
        {
            case FileJob::Started:
            {
                if(!m_isReplaced)
                {
                    m_srcPath = checkDuplicateName(m_tarPath + "/" + toInfo.fileName());
                }
                else
                {
                    m_srcPath = m_tarPath + "/" + toInfo.fileName();
                }
                m_status = Run;
                break;
            }
            case FileJob::Run:
            {
                if(m_isReplaced)
                {
                    if(to.exists()){
                        if (toInfo.isDir()){
                            bool result = QDir(tarFile).removeRecursively();

                            if (!result) {
                                result = QProcess::execute("rm -r \"" + tarFile.toUtf8() + "\"") == 0;
                            }
                        }else if (toInfo.isFile()){
                            to.remove();
//                            qDebug() << to.error() << to.errorString();
                        }
                    }
                }
                bool result = from.rename(m_srcPath);

                if (!result) {
                    result = (QProcess::execute("mv -T \"" + from.fileName().toUtf8() + "\" \"" + m_srcPath.toUtf8() + "\"") == 0);
                }
//                qDebug() << m_srcPath << from.error() << from.errorString();
                return result;
            }
            case FileJob::Paused:
//                qDebug() << srcFile << tarFile;
                QThread::msleep(100);
                break;
            case FileJob::Cancelled:
                return true;
            default:
                return false;
         }

    }
    return false;
}

bool FileJob::moveDir(const QString &srcFile, const QString &tarDir, QString *targetPath)
{
    if(m_applyToAll && m_status == FileJob::Cancelled){
        return false;
    }else if(!m_applyToAll && m_status == FileJob::Cancelled){
        m_status = Started;
    }
    QDir from(srcFile);
    QFileInfo fromInfo(srcFile);
    QDir to(tarDir);
    m_srcFileName = from.dirName();
    m_tarFileName = to.dirName();
    m_srcPath = srcFile;
    m_tarPath = tarDir;
    m_status = Started;

    //We only check the conflict of the files when
    //they are not in the same folder
    if(fromInfo.absolutePath()== to.path() && to.exists(fromInfo.fileName()))
        return false;
    else
        if(to.exists(from.dirName()) && !m_applyToAll)
        {
            jobConflicted();
        }

    while(true)
    {
        switch(m_status)
        {
            case FileJob::Started:
            {
                if(!m_isReplaced)
                {
                    m_srcPath = checkDuplicateName(m_tarPath + "/" + from.dirName());
                }
                else
                {
                    if(!m_applyToAll)
                        m_isReplaced = false;
                    m_srcPath = m_tarPath + "/" + from.dirName();
                }
                m_status = Run;
                break;
            }
            case FileJob::Run:
            {
                if(m_isReplaced)
                {
                    QDir localDir(to.path() + "/" + from.dirName());
                    if(localDir.exists())
                        localDir.removeRecursively();

                }

                bool ok = from.rename(from.absolutePath(), m_srcPath);

                if (ok && targetPath)
                    *targetPath = m_srcPath;
                qDebug() << m_srcPath << ok;
                return ok;
            }
            case FileJob::Paused:
                QThread::msleep(100);
                break;
            case FileJob::Cancelled:
                return true;
            default:
                return false;
         }

    }
    return false;
}

bool FileJob::deleteFile(const QString &file)
{
    if(QFile::remove(file)){
//        qDebug() << " delete file:" << file << "successfully";
        return true;
    }
    else
    {
        qDebug() << "unable to delete file:" << file;
        return false;
    }
}

bool FileJob::deleteDir(const QString &dir)
{
    if (m_status == FileJob::Cancelled) {
        emit result("cancelled");
        return false;
    }

    QDir sourceDir(dir);

    sourceDir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::AllDirs);

    QDirIterator iterator(sourceDir, QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QFileInfo &fileInfo = iterator.next();

        if (fileInfo.isFile() || fileInfo.isSymLink()) {
            if (!deleteFile(fileInfo.filePath())) {
                emit error("Unable to remove file");
                qDebug() << "Unable to remove file" << fileInfo.filePath();
            }
        } else {
            if (!deleteDir(fileInfo.filePath())){
                qDebug() << "Unable to remove dir" << fileInfo.filePath();
            }
        }
    }

    if (!sourceDir.rmdir(QDir::toNativeSeparators(sourceDir.path()))) {
        qDebug() << "Unable to remove dir:" << sourceDir.path();
        emit("Unable to remove dir: " + sourceDir.path());

        return false;
    }

    return true;
}

bool FileJob::moveDirToTrash(const QString &dir, QString *targetPath)
{
    if(m_status == FileJob::Cancelled)
    {
        emit result("cancelled");
        return false;
    }
    QDir sourceDir(dir);

    QString oldName = sourceDir.dirName();
    QString baseName = getNotExistsTrashFileName(sourceDir.dirName());
    QString newName = m_trashLoc + "/files/" + baseName;
    QString delTime = QDateTime::currentDateTime().toString(Qt::ISODate);

    QDir tempDir(newName);

    if (!writeTrashInfo(baseName, dir, delTime))
        return false;

    if (!sourceDir.rename(sourceDir.path(), newName)) {
        if (QProcess::execute("mv -T \"" + sourceDir.path().toUtf8() + "\" \"" + newName.toUtf8() + "\"") != 0) {
            qDebug() << "Unable to trash dir:" << sourceDir.path();
            return false;
        }
    }

    if (targetPath)
        *targetPath = newName;

    return true;
}

QString FileJob::getNotExistsTrashFileName(const QString &fileName)
{
    QByteArray name = fileName.toUtf8();

    int index = name.lastIndexOf('/');

    if (index >= 0)
        name = name.mid(index + 1);

    index = name.lastIndexOf('.');
    QByteArray suffix;

    if (index >= 0)
        suffix = name.mid(index);

    if (suffix.size() > 200)
        suffix = suffix.left(200);

    name.chop(suffix.size());
    name = name.left(200 - suffix.size());

    while (QFile::exists(TRASHFILEPATH + "/" + name + suffix)) {
        name = QCryptographicHash::hash(name, QCryptographicHash::Md5).toHex();
    }

    return QString::fromUtf8(name + suffix);
}

bool FileJob::moveFileToTrash(const QString &file, QString *targetPath)
{
    if(m_status == FileJob::Cancelled)
    {
        emit result("cancelled");
        return false;
    }

    QFile localFile(file);
    QString path = m_trashLoc + "/files/";
    QString baseName = getNotExistsTrashFileName(localFile.fileName());
    QString newName = path + baseName;
    QString delTime = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (!writeTrashInfo(baseName, file, delTime))
        return false;

    if (!localFile.rename(newName))
    {
        if (QProcess::execute("mv -T \"" + file.toUtf8() + "\" \"" + newName.toUtf8() + "\"") != 0) {
            //Todo: find reason
            qDebug() << "Unable to trash file:" << localFile.fileName();
            return false;
        }
    }

    if (targetPath)
        *targetPath = newName;

    return true;
}

bool FileJob::writeTrashInfo(const QString &fileBaseName, const QString &path, const QString &time)
{
    QFile metadata( m_trashLoc + "/info/" + fileBaseName + ".trashinfo" );

    if (!metadata.open( QIODevice::WriteOnly )) {
        qDebug() << metadata.fileName() << "file open error:" << metadata.errorString();

        return false;
    }

    QByteArray data;

    data.append("[Trash Info]\n");
    data.append("Path=").append(path.toUtf8().toPercentEncoding("/")).append("\n");
    data.append("DeletionDate=").append(time).append("\n");

    qint64 size = metadata.write(data);

    metadata.close();

    if (size < 0) {
        qDebug() << "write file " << metadata.fileName() << "error:" << metadata.errorString();
    }

    return size > 0;
}
