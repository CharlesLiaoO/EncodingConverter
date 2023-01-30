#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QTextCodec>
#include <QFileDialog>
#include <QThread>
#include <QDebug>
#include <NotPrjRel.h>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QTime>

int iBigFileSize = 1024*1024*1024;  // 1GiB
uint iMaxFileSize = (uint)2*1024*1024*1024;  // 2GiB
int iBigFileSizeMB;
int iMaxFileSizeGB;

/// Ref: QtCreator's source code - qt-creator/src/plugins/texteditor/codecchooser.cpp
QStringList AvailableEncodingsWithAliases()
{
    QStringList ret;

    QList<int> mibs = QTextCodec::availableMibs();
    std::sort(mibs.begin(), mibs.end());
    QList<int>::iterator firstNonNegative =
        std::find_if(mibs.begin(), mibs.end(), [](int n) { return n >=0; });
    if (firstNonNegative != mibs.end())
        std::rotate(mibs.begin(), firstNonNegative, mibs.end());
    for (int &mib : mibs) {
//        if (filter == Filter::SingleByte && !isSingleByte(mib))
//            continue;
        if (QTextCodec *codec = QTextCodec::codecForMib(mib)) {
            QString compoundName = QLatin1String(codec->name());
            const QList<QByteArray> aliases = codec->aliases();
            for (const QByteArray &alias : aliases) {
                compoundName += QLatin1String(" / ");
                compoundName += QString::fromLatin1(alias);
            }
            ret<< compoundName;
        }
    }
    return ret;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->toolButton_About->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));

    iBigFileSizeMB = (double)iBigFileSize / (1024*1024);
    iMaxFileSizeGB = (double)iMaxFileSize / (1024*1024*1024);

    QStringList encodingList;
    encodingList<< "UTF-8";
//    encodingList<< "UTF-16";
    encodingList<< "UTF-16BE";
    encodingList<< "UTF-16LE";
//    encodingList<< "UTF-32";
    encodingList<< "UTF-32BE";
    encodingList<< "UTF-32LE";
    encodingList<< tr("System(ANSI in Windows)");
//    encodingList<< "GB18030";

//    qDebug()<< QTextCodec::availableCodecs();
    QStringList qtAvaEcs = AvailableEncodingsWithAliases();  // with aliases
    qtAvaEcs.removeOne("System");
    qtAvaEcs.removeOne("UTF-16");
    qtAvaEcs.removeOne("UTF-32");
    for (auto &top: encodingList) {
        qtAvaEcs.removeOne(top); }
    encodingList += qtAvaEcs;

    ui->comboBox_EncodingSrc->blockSignals(true);
    ui->comboBox_EncodingDest->blockSignals(true);
    QFontMetrics fm = fontMetrics();
    int cbboxMaxWidth = 0;
    for (auto &ec: encodingList) {
        QStringList aliases = ec.split(" / ");
        ui->comboBox_EncodingSrc->addItem(ec, aliases.first());
        ui->comboBox_EncodingDest->addItem(ec, aliases.first());
        int tmp = fm.width(ec + "   ");  // 3 space to distinguish with " / "
        if (tmp > cbboxMaxWidth)
            cbboxMaxWidth = tmp;
    }
    ui->comboBox_EncodingSrc->blockSignals(false);
    ui->comboBox_EncodingDest->blockSignals(false);
    ui->comboBox_EncodingSrc->view()->setFixedWidth(cbboxMaxWidth + 20);  // 20: scroll bar width
    ui->comboBox_EncodingDest->view()->setFixedWidth(cbboxMaxWidth + 20);

    sIniPath = "config.ini";
    IniSetting cfg(sIniPath);
    QString pathInDef = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    ui->lineEdit_PathIn->setText(cfg.value("pathIn", pathInDef).toString());

    sBakSuffixName = ".ecbak";
    sTmpSuffixName = ".tmp";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_PathInBrowse_clicked()
{
    QString sPathInLast = ui->lineEdit_PathIn->text();
    QString sDir = QFileDialog::getExistingDirectory(this, tr("Choose a directory"), sPathInLast);
//    QString sDir = QFileDialog::getSaveFileName(this, tr("Choose a directory or file"), sPathSrc);  //not support directory
    if (!sDir.isEmpty())
        ui->lineEdit_PathIn->setText(sDir);
}

QFileInfoList GetFileInfoList(const QFileInfo &dirInfo, const QStringList &nameFilters, bool bRecursively, bool &bStop)
{
    if (bStop)
        return QFileInfoList();

    QCoreApplication::processEvents();

    QDir dirSrc(dirInfo.filePath());
    QFileInfoList fileList = dirSrc.entryInfoList(nameFilters, QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    if (bRecursively) {
        QFileInfoList dirList = dirSrc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (auto &fi: dirList)
            fileList << GetFileInfoList(fi, nameFilters, bRecursively, bStop);
    }

    return fileList;
}

/// Adjust the encoding of text file. If use *bUtf8Bom and the encoding is "UTF-8 BOM", return "UTF-8" and set *bUtf8Bom as true.
/// QTextStream's auto detection cannot distinguish UTF-8(without bom) and ANSI, because it _might_ not read hole file or dynamicly to detect encoding [tested on GB18030 and UTF-8 in Windows]
/// Ref: Blog of CSDN's Blogger [hellokandy]: https://blog.csdn.net/hellokandy/article/details/126147776
QString DetectEncoding(QFile &file, bool *bUtf8Bom=nullptr)
{
    QString sEncoding;

    // read 4 bytes to detect bom
    QByteArray buf = file.read(4);

    if (buf.size() >= 3 && (uchar)buf.at(0) == 0xEF && (uchar)buf.at(1) == 0xBB && (uchar)buf.at(2) == 0xBF) {
        if (bUtf8Bom) {
            sEncoding = "UTF-8";
            *bUtf8Bom = true;
        } else
            sEncoding = "UTF-8 BOM";
    } else if (buf.size() >= 2 && (uchar)buf.at(0) == 0xFF && (uchar)buf.at(1) == 0xFE)
        sEncoding = "UTF-16LE";
    else if (buf.size() >= 2 && (uchar)buf.at(0) == 0xFE && (uchar)buf.at(1) == 0xFF)
        sEncoding = "UTF-16BE";
    else if (buf.size() >= 4 && (uchar)buf.at(0) == 0x00 && (uchar)buf.at(1) == 0x00
             && (uchar)buf.at(2) == 0xFF && (uchar)buf.at(3) == 0xFE)
        sEncoding = "UTF-32LE";
    else if (buf.size() >= 4 && (uchar)buf.at(0) == 0x00 && (uchar)buf.at(1) == 0x00
             && (uchar)buf.at(2) == 0xFE && (uchar)buf.at(3) == 0xFF)
        sEncoding = "UTF-32BE";
    else {
        QTextCodec::ConverterState cs;
        QTextCodec* tc = QTextCodec::codecForName("UTF-8");
        file.seek(0);
        buf = file.readAll();  Q_UNUSED(iMaxFileSize)  // read hole file to detect, QByteArray's max size is 2GB-1
        tc->toUnicode(buf.constData(), buf.size(), &cs);
        if (cs.invalidChars > 0)  // try utf8, if number of invalid character is more than 0, assume the encoding is System
            sEncoding = "System";
        else {
            sEncoding = "UTF-8";
            if (bUtf8Bom)
                *bUtf8Bom = false;
        }
    }

    file.seek(0);

    return sEncoding;
}

void MainWindow::on_pushButton_Start_clicked()
{
    bUserStop = false;
    ui->label_FileProgress->setText("0/0");
    ui->plainTextEdit_MsgOutput->clear();

    // show Start Time to avoid user cannot sense the response when the msg output is same as last
    ui->plainTextEdit_MsgOutput->appendPlainText(tr("---- Started at %1 ----").arg(QTime::currentTime().toString("hh:mm:ss")));
    QCoreApplication::processEvents();

    QString pathIn = ui->lineEdit_PathIn->text();
    QString sEncodingSrc = ui->comboBox_EncodingSrc->currentData().toString();
    QString sEncodingDest = ui->comboBox_EncodingDest->currentData().toString();

    QString sNewLine = ui->comboBox_NewLineDest->currentText();
    if (sNewLine == "CRLF")
        sNewLine = "\r\n";
    else if (sNewLine == "CR")
        sNewLine = "\r";
    else if (sNewLine == "LF")
        sNewLine = "\n";

    if (!QFileInfo::exists(pathIn)) {
        QMessageBox::information(this, "", tr("Not existing or invalid path"));
        return;
    }

    IniSetting cfg(sIniPath);
    cfg.setValue("pathIn", pathIn);

    QFileInfo fiPathIn(pathIn);
    QFileInfoList fiFileSrcList;
    if (fiPathIn.isDir()) {
        QString sSuffixNames = ui->plainTextEdit_SuffixName->toPlainText();
        QStringList nameFilters = sSuffixNames.split(";");
        bool Recursively = ui->checkBox_Recursively->isChecked();
        if (Recursively)
            ui->plainTextEdit_MsgOutput->appendPlainText(tr("Finding files recursively..."));
        fiFileSrcList = GetFileInfoList(fiPathIn, nameFilters, Recursively, bUserStop);
        if (Recursively)
            ui->plainTextEdit_MsgOutput->appendPlainText(tr("Start to convert..."));
    } else if (fiPathIn.isFile())
        fiFileSrcList<< pathIn;

    // do!
    for (int fileIdx = 0/*, dealedFileNum = 0*/; fileIdx < fiFileSrcList.size() && !bUserStop; fileIdx++) {
        ui->label_FileProgress->setText(QString("%1/%2").arg(fileIdx + 1).arg(fiFileSrcList.size()));

        fiFileSrc = fiFileSrcList.at(fileIdx);
        if (!CheckFileSize())
            continue;

        QFile fileSrc(fiFileSrc.absoluteFilePath());
        QFile fileDest(fileSrc.fileName() + sTmpSuffixName);
        if (!fileSrc.open(QFile::ReadOnly/* | QFile::Text*/)) {  // Text mode has no effect in read mode
            QString sOpenErr = tr("Error when open input file\n%1").arg(fileSrc.fileName());
            ui->plainTextEdit_MsgOutput->appendPlainText(sOpenErr);
            QMessageBox::critical(this, "", sOpenErr);
            return;
        }
        if (!fileDest.open(QFile::WriteOnly)) {  // Not use Text mode, because we need to specify newline char
            QString sOpenErr = tr("Error when open temporary output file\n%1").arg(fileDest.fileName());
            ui->plainTextEdit_MsgOutput->appendPlainText(sOpenErr);
            QMessageBox::critical(this, "", sOpenErr);
            return;
        }

        QTextStream tsSrc(&fileSrc);
        QTextStream tsDest(&fileDest);

        bool bUtf8bomSrc = false, bUtf8bomDest;
        if (ui->comboBox_EncodingSrc->currentIndex() == 0)  //Auto Detect
            sEncodingSrc = DetectEncoding(fileSrc, &bUtf8bomSrc);
        bUtf8bomDest = ui->checkBox_BomDest->isChecked();
        if (ui->checkBox_SkipSameEncoding->isChecked() && sEncodingSrc == sEncodingDest) {
            bool bSameCodec = true;
            if (sEncodingSrc == "UTF-8" && bUtf8bomSrc != bUtf8bomDest)
                bSameCodec = false;
            if (bSameCodec) {
                QString sSameCodec = tr("%1\n    's input encoding is same as output, skip").arg(fiFileSrc.absoluteFilePath());
                ui->plainTextEdit_MsgOutput->appendPlainText(sSameCodec);
                continue;
            }
        }
        tsSrc.setCodec(sEncodingSrc.toLatin1().data());
        tsDest.setCodec(sEncodingDest.toLatin1().data());
        tsDest.setGenerateByteOrderMark(bUtf8bomDest);
        //qDebug()<< tsDest.codec()->name();

//        if (dealedFileNum != 0)
            ui->plainTextEdit_MsgOutput->appendPlainText("");
//        dealedFileNum++;

        int lineNum = 1;
        QElapsedTimer eltUpdateUi, eltInteractUi;
        eltUpdateUi.start();
        eltInteractUi.start();
        UpdateProgress(lineNum, 0);
        for (; !tsSrc.atEnd() && !bUserStop; lineNum++) {
            QString sLine = tsSrc.readLine();
            tsDest<< sLine + sNewLine;
//            QThread::msleep(100);  // to simulate slow deal

            if (eltUpdateUi.elapsed() >= 500) {
                eltUpdateUi.restart();
                float percentage = (double)tsSrc.pos() / fiFileSrc.size() * 100;
                UpdateProgress(lineNum, percentage);
            }
            if (eltInteractUi.elapsed() >= 50) {
                eltInteractUi.restart();
                QCoreApplication::processEvents();
            }
        }
        if (!bUserStop)
            UpdateProgress(lineNum, 100);
        else
            return;  // Not save files

        fileSrc.close();
        fileDest.close();

        if (ui->checkBox_Bak->isChecked()) {
            QFile::remove(fileSrc.fileName() + sBakSuffixName);
            QFile::rename(fileSrc.fileName(), fileSrc.fileName() + sBakSuffixName);
        } else
            QFile::remove(fileSrc.fileName());
        QFile::rename(fileSrc.fileName() + sTmpSuffixName, fileSrc.fileName());
    }  // for (int fileIdx = 0;

    ui->plainTextEdit_MsgOutput->appendPlainText(tr("---- Finished at %1 ----").arg(QTime::currentTime().toString("hh:mm:ss")));
}

void MainWindow::on_pushButton_Stop_clicked()
{
    bUserStop = true;
}

void MainWindow::on_comboBox_EncodingDest_currentTextChanged(const QString &arg1)
{
    if (arg1.startsWith("UTF-")) {
        if (arg1 == "UTF-8")
            ui->checkBox_BomDest->setEnabled(true);
        else {
            ui->checkBox_BomDest->setEnabled(false);
            ui->checkBox_BomDest->setChecked(true);
        }
    } else {
        ui->checkBox_BomDest->setEnabled(false);
        ui->checkBox_BomDest->setChecked(false);
    }
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    bUserStop = true;
    Q_UNUSED(e)
}

bool MainWindow::CheckFileSize()
{
    if (fiFileSrc.size() >= iMaxFileSize) {
        QString sOverSize = tr("%1\n    's size is >= %2GB, skip").arg(fiFileSrc.absoluteFilePath()).arg(iMaxFileSizeGB);
        ui->plainTextEdit_MsgOutput->appendPlainText(sOverSize);
        return false;
    }

    if (fiFileSrc.size() > iBigFileSize) {
        int ret = QMessageBox::question(this, tr("Big file comfirmation"), tr("%1\n's size > %2MB, still to convert?").arg(fiFileSrc.absoluteFilePath()).arg(iBigFileSizeMB));
        if (ret == QMessageBox::No)
            return false;
    }
    return true;
}

void MainWindow::UpdateProgress(int lineNum, float percentage)
{
    QString sPercentage;
    if (percentage < 100)
        sPercentage = QString::number(percentage, 'f', 2);
    else
        sPercentage = "100";

    QTextCursor tc = ui->plainTextEdit_MsgOutput->textCursor();
    tc.movePosition(QTextCursor::End);  // ensure select end of doc
    tc.select(QTextCursor::BlockUnderCursor);
//    QString str = tc.selectedText();
    tc.removeSelectedText();
    QString sOutputLine = tr("%0 (%1 lines, %2%)").arg(fiFileSrc.absoluteFilePath()).arg(lineNum).arg(sPercentage);
    ui->plainTextEdit_MsgOutput->appendPlainText(sOutputLine);
}

#include <AboutDlg.h>
void MainWindow::on_toolButton_About_clicked()
{
    AboutDlg aboutDlg(this);
    aboutDlg.exec();
}

#include <SettingDlg.h>
void MainWindow::on_toolButton_Setting_clicked()
{
    SettingDlg settingDlg(this);
    settingDlg.exec();
}
