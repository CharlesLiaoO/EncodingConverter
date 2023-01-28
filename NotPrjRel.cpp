#include "NotPrjRel.h"

IniSetting::IniSetting(const QString &iniFilePath, const QString &groupName, QObject *parent, bool sysFileSync) :
    QSettings(iniFilePath, QSettings::IniFormat, parent)
{
    this->iniFilePath = iniFilePath;
    this->sysFileSync = sysFileSync;
    setIniCodec("UTF-8");
    if (!groupName.isEmpty())
        beginGroup(groupName);
}

IniSetting::~IniSetting()
{
    sync();  //QSetting析构再次调用不会影响性能，因为sync内部有“是否有新修改”的标识
#ifdef Q_OS_UNIX
    if (sysFileSync)
        system(QString("sync -d %1").arg(iniFilePath).toStdString().c_str());
#endif
}

QString GetFileNameNS(const QString &path)
{
    QString unixPath = path;
    unixPath.replace('\\', '/');
    QString fileName = unixPath.mid(unixPath.lastIndexOf('/') + 1);
    return GetFilePathNS(fileName);
}

QString GetFilePathNS(const QString &path)
{
    return path.mid(0, path.lastIndexOf('.'));
}

QString GetFileSuffix(const QString &path)
{
    return path.mid(path.lastIndexOf('.') + 1);
}
