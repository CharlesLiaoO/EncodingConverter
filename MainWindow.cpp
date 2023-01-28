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

    encodingList<< tr("系统（Windows为ANSI）");

    encodingList<< "GB18030";

    ui->comboBox_EncodingSrc->addItems(encodingList);
    ui->comboBox_EncodingDest->addItems(encodingList);

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
    QString sDir = QFileDialog::getExistingDirectory(this, tr("选择输入文件夹"), sPathInLast);
//    QString sDir = QFileDialog::getSaveFileName(this, tr("选择输入文件或文件夹"), sPathSrc);  //不支持文件夹
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

/// 判断文本文件的编码，如果使用 *bUtf8Bom ，则UTF-8 BOM编编只返回"UTF-8"，并设置 *bUtf8Bom 为true
/// 参考：CSDN博主「hellokandy」文章：https://blog.csdn.net/hellokandy/article/details/126147776
/// QTextStream的自动检测不能区分utf-8（无bom）与ANSI，因为它_应该_没有读取整个文件读取判断，也没有动态检测编码【Windows测试】
QString DetectEncoding(QFile &file, bool *bUtf8Bom=nullptr)
{
    QString sEncoding;

    //读取4字节用于判断bom
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
        buf = file.readAll();  Q_UNUSED(iMaxFileSize)  //读整个文件判断编码，QByteArray最大容量2GB-1
        tc->toUnicode(buf.constData(), buf.size(), &cs);
        if (cs.invalidChars > 0)  //尝试用utf8转换，如果无效字符数大于0，则使用系统编码
            sEncoding = "";  //空为System，适应系统和地区本地编码
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
    ui->plainTextEdit_MsgOutput->appendPlainText(tr("---- 开始于 %1 ----").arg(QTime::currentTime().toString("hh:mm:ss")));
    QCoreApplication::processEvents();  //显示开始时间，避免转换结果（消息输出）与上一次相同时，用户无法感知响应

    QString pathIn = ui->lineEdit_PathIn->text();
    QString sEncodingSrc = ui->comboBox_EncodingSrc->currentText();
    QString sEncodingDest = ui->comboBox_EncodingDest->currentText();

    QString sNewLine = ui->comboBox_NewLineDest->currentText();
    if (sNewLine == "CRLF")
        sNewLine = "\r\n";
    else if (sNewLine == "CR")
        sNewLine = "\r";
    else if (sNewLine == "LF")
        sNewLine = "\n";

    if (!QFileInfo::exists(pathIn)) {
        QMessageBox::information(this, "", tr("输入路径不存在或非法"));
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
            ui->plainTextEdit_MsgOutput->appendPlainText(tr("正在递归查找所有文件..."));
        fiFileSrcList = GetFileInfoList(fiPathIn, nameFilters, Recursively, bUserStop);
        if (Recursively)
            ui->plainTextEdit_MsgOutput->appendPlainText(tr("开始转换文件..."));
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
        if (!fileSrc.open(QFile::ReadOnly/* | QFile::Text*/)) {  //读模式的文本模式没有影响
            QMessageBox::critical(this, "", tr("打开输入文件错误"));
            return;
        }
        if (!fileDest.open(QFile::WriteOnly)) {  //因为要指定换行符，不用文本模式
            QMessageBox::critical(this, "", tr("打开输出文件错误"));
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
                QString sSameCodec = tr("%1\n    的输入输出编码相同，跳过").arg(fiFileSrc.absoluteFilePath());
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
//            QThread::msleep(100);

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
            return;  //终止不保存文件

        fileSrc.close();
        fileDest.close();

        if (ui->checkBox_Bak->isChecked()) {
            QFile::remove(fileSrc.fileName() + sBakSuffixName);
            QFile::rename(fileSrc.fileName(), fileSrc.fileName() + sBakSuffixName);
        } else
            QFile::remove(fileSrc.fileName());
        QFile::rename(fileSrc.fileName() + sTmpSuffixName, fileSrc.fileName());
    }  // for (int fileIdx = 0;

    ui->plainTextEdit_MsgOutput->appendPlainText(tr("---- 完成于 %1 ----").arg(QTime::currentTime().toString("hh:mm:ss")));
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
        QString sOverSize = tr("%1\n    的文件大小大于等于%2GB，跳过").arg(fiFileSrc.absoluteFilePath()).arg(iMaxFileSizeGB);
        ui->plainTextEdit_MsgOutput->appendPlainText(sOverSize);
        return false;
    }

    if (fiFileSrc.size() > iBigFileSize) {
        int ret = QMessageBox::question(this, tr("超大文件确认"), tr("%1\n的文件大小大于%2MB，仍要进行转换吗？").arg(fiFileSrc.absoluteFilePath()).arg(iBigFileSizeMB));
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
    QString sOutputLine = tr("%0 （%1行，%2%）").arg(fiFileSrc.absoluteFilePath()).arg(lineNum).arg(sPercentage);
    ui->plainTextEdit_MsgOutput->appendPlainText(sOutputLine);
}

#include <AboutDlg.h>
void MainWindow::on_toolButton_About_clicked()
{
    AboutDlg aboutDlg(this);
    aboutDlg.exec();
}

