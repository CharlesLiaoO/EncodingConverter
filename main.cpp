#include "MainWindow.h"

#include <QApplication>
#include <QTranslator>
#include <NotPrjRel.h>
#include <QLocale>

QTranslator trQt, trApp;
void LoadTranslator(QLocale::Language lang);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    IniSetting cfg("config.ini");
    QString sLanguage = cfg.value("language", "System").toString();
    if (sLanguage == "System")
        LoadTranslator(QLocale().language());
    else if (sLanguage == "Chinese")
        LoadTranslator(QLocale::Chinese);
    a.installTranslator(&trQt);
    a.installTranslator(&trApp);

    MainWindow w;
    w.show();
    return a.exec();
}

void LoadTranslator(QLocale::Language lang)
{
    switch (lang) {
    case QLocale::Chinese:
        trQt.load(":/translations/qtMod_zh_CN.qm");  // modified from qt office release
        trApp.load(":/translations/zh_CN.qm");
        break;
    default:
        break;
    }
}
