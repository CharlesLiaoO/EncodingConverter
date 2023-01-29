#include "MainWindow.h"

#include <QApplication>
#include <QTranslator>
#include <NotPrjRel.h>
#include <QLocale>

void LoadQtTranslation(QTranslator &tr_qt, QLocale::Language lang);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator tr_qt;
    IniSetting cfg("config.ini");
    QString sLanguage = cfg.value("language", "System").toString();
    if (sLanguage == "System")
        LoadQtTranslation(tr_qt, QLocale().language());
    else if (sLanguage == "Chinese")
        LoadQtTranslation(tr_qt, QLocale::Chinese);
    a.installTranslator(&tr_qt);

    MainWindow w;
    w.show();
    return a.exec();
}

void LoadQtTranslation(QTranslator &tr_qt, QLocale::Language lang)
{
    switch (lang) {
    case QLocale::Chinese:
        tr_qt.load(":/translations/qtMod_zh_CN.qm");  // modified from qt office release
        break;
    default:
        break;
    }
}
