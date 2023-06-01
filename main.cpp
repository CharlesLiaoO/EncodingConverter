#include "MainWindow.h"

#include <QApplication>
#include <QTranslator>
#include <NotPrjRel.h>
#include <QLocale>

#include <QDebug>
#include <QDir>

QTranslator trQt, trApp;
void InstTranslator(QLocale locale);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    IniSetting cfg("config.ini");
    QString sLanguage = cfg.value("language", "System").toString();
    if (sLanguage == "System")
        InstTranslator(QLocale());
    else if (sLanguage == "SimplifiedChinese")
        InstTranslator(QLocale(QLocale::Chinese, QLocale::SimplifiedChineseScript, QLocale::China));

    MainWindow w;
    w.show();
    return a.exec();
}

void InstTranslator(QLocale locale)
{
    switch (locale.script()) {
        // modified from qt office release
    case QLocale::SimplifiedChineseScript:
        qDebug()<< "ts qtMod"<< trQt.load(locale, "qtMod", "_", ":/translations");
        break;
    default:
        qDebug()<< "ts qt"<< trQt.load(locale, "qt", "_", ":/translations");
        break;
    }

    qDebug()<< "ts app"<< trApp.load(locale, "app", "_", ":/translations");  //fileName cannot be empty

    qApp->installTranslator(&trQt);
    qApp->installTranslator(&trApp);
}
