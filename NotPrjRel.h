#ifndef NOTPRJREL_H
#define NOTPRJREL_H

#include <QLocale>
#include <QProcess>
#include <QSettings>

// Not Project Related APIs
// 与项目无关的一些API


/// 将数值转成字串（保留3位小数）
inline QString StrF3(double val) {
    return QString::number(val, 'f', 3);
}

/// 将数值转成字串（保留最少位小数）
inline QString StrF(double val) {
#if QT_VERSION >= 0x050700
    return QString::number(val, 'f', QLocale::FloatingPointShortest);
#else
    //QLocale::FloatingPointShortest的qt实现复杂，不参考
    //始终填充prec个小数位，>6可能包含不精确的，虽然有办法去除，但应用不应该使用多于6个小数位的数据，人难读
    QString tmp = QString::number(val, 'f'/*, 16*/);
    if (tmp.contains('.')) {
        while (tmp.endsWith('0'))
            tmp.chop(1);
        if (tmp.endsWith('.'))
            tmp.chop(1);
    }
    return tmp;
#endif
}

/// 从QString返回系统默认编码的QByteArray_存储它到上下文变量中，如果要使用它的char*
QByteArray ToSysDefCodecBa_StIfUseCa(const QString &qStr);

/// 获取文件无后缀（No Suffix）的名字
QString GetFileNameNS(const QString &path);
/// 获取文件无后缀（No Suffix）的路径
QString GetFilePathNS(const QString &path);
/// 获取路径的后缀
QString GetFileSuffix(const QString &path);


/// 便利的Ini格式QSettings。在构造函数中支持组名，使用utf8编码
class IniSetting : public QSettings
{
public:
    /// 参数 sysFileSync: unix是否需要sync命令同步此文件
    IniSetting(const QString &iniFilePath, const QString &groupName="", QObject *parent=nullptr, bool sysFileSync=true);
    ~IniSetting();
private:
    QString iniFilePath;
    bool sysFileSync;
};

#endif // NOTPRJREL_H
