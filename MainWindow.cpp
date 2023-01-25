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

int iBigFileSize = 1024*1024*1024;  // 1GiB
uint iMaxFileSize = (uint)2*1024*1024*1024;  // 2GiB
int iBigFileSizeMB;
int iMaxFileSizeGB;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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

    encodingList<< "GB18030";

    ui->comboBox_EncodingDest->addItems(encodingList);
    ui->comboBox_EncodingSrc->addItems(encodingList);

    sIniPath = "config.ini";
    IniSetting cfg(sIniPath);
    QString pathSrcDef = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    ui->lineEdit_PathSrc->setText(cfg.value("pathSrc", pathSrcDef).toString());

    sBakSuffixName = ".ecbak";
    sTmpSuffixName = ".tmp";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_PathSrcBrowse_clicked()
{
    QString sPathLast = ui->lineEdit_PathSrc->text();
    QString sDir = QFileDialog::getExistingDirectory(this, tr("选择输入文件夹"), sPathLast);
//    QString sDir = QFileDialog::getSaveFileName(this, tr("选择输入文件或文件夹"), sPathSrc);  //不支持文件夹
    if (!sDir.isEmpty())
        ui->lineEdit_PathSrc->setText(sDir);
}

void MainWindow::on_pushButton_PathDestBrowse_clicked()
{
    QString sPathLast = ui->lineEdit_PathDest->text();
    QString sDir = QFileDialog::getExistingDirectory(this, tr("选择输入文件夹"), sPathLast);
    if (!sDir.isEmpty())
        ui->lineEdit_PathDest->setText(sDir);
}

QFileInfoList GetAllFileRecursively(const QFileInfo &fi, const QStringList &nameFilters)
{
    QFileInfoList fileList;
    if (fi.isDir()) {
        QDir dirSrc(fi.filePath());
        fileList = dirSrc.entryInfoList(nameFilters, QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        QFileInfoList dirList = dirSrc.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (auto &fi: dirList) {
            fileList << GetAllFileRecursively(fi, nameFilters);
        }
    }
    return fileList;
}

// 参考：CSDN博主「hellokandy」文章：https://blog.csdn.net/hellokandy/article/details/126147776
QString DetectEncoding(QFile &fileSrc)
{
    QString sEncoding;
    //读取4字节用于判断bom
    QByteArray buffer = fileSrc.read(4);
    quint8 buf1st, buf2nd, buf3rd, buf4th;
    if (buffer.size() >= 2) {
        buf1st = buffer.at(0);
        buf2nd = buffer.at(1);
    } else {
        buf1st = 0;
        buf2nd = 0;
    }
    if (buffer.size() >= 3)
        buf3rd = buffer.at(2);
    else
        buf3rd = 0;
    if (buffer.size() >= 3)
        buf4th = buffer.at(3);
    else
        buf4th = 0;

    if (buf1st == 0xEF && buf2nd == 0xBB && buf3rd == 0xBF)
        sEncoding = "UTF-8 BOM";
    else if (buf1st == 0xFF && buf2nd == 0xFE)
        sEncoding = "UTF-16LE";
    else if (buf1st == 0xFE && buf2nd == 0xFF)
        sEncoding = "UTF-16BE";
    else if (buf1st == 0x00 && buf2nd == 0x00 && buf3rd == 0xFF && buf4th == 0xFE)
        sEncoding = "UTF-32LE";
    else if (buf1st == 0x00 && buf2nd == 0x00 && buf3rd == 0xFE && buf4th == 0xFF)
        sEncoding = "UTF-16BE";
    else {
        QTextCodec::ConverterState cs;
        QTextCodec* tc = QTextCodec::codecForName("UTF-8");
        fileSrc.seek(0);
        buffer = fileSrc.readAll();  Q_UNUSED(iMaxFileSize)  //读整个文件判断编码，QByteArray最大容量2GB-1
        tc->toUnicode(buffer.constData(), buffer.size(), &cs);
        if (cs.invalidChars > 0)  //尝试用utf8转换，如果无效字符数大于0，则使用系统编码
            sEncoding = "";  //空为System（Windows为ASNI），自适应系统和地区本地编码
        else
            sEncoding = "UTF-8";
    }

    fileSrc.seek(0);

    return sEncoding;
}

void MainWindow::on_pushButton_Start_clicked()
{
    bUserStop = false;
    ui->label_FileProgress->setText("0/0");
    ui->plainTextEdit_Output->clear();

    QString pathSrc = ui->lineEdit_PathSrc->text();
    QString sCodecSrc = ui->comboBox_EncodingSrc->currentText();
    QString sCodecDest = ui->comboBox_EncodingDest->currentText();

    QString sNewLine = ui->comboBox_NewLineDest->currentText();
    if (sNewLine == "CRLF")
        sNewLine = "\r\n";
    else if (sNewLine == "CR")
        sNewLine = "\r";
    else if (sNewLine == "LF")
        sNewLine = "\n";

    if (!QFileInfo::exists(pathSrc)) {
        QMessageBox::information(this, "", tr("输入路径不存在或非法"));
        return;
    }

    IniSetting cfg(sIniPath);
    cfg.setValue("pathSrc", pathSrc);

    QFileInfo fiInput(pathSrc);
    QFileInfoList fiList;
    if (fiInput.isDir()) {
        QString sSuffixNames = ui->plainTextEdit_SuffixName->toPlainText();
        QStringList nameFilters = sSuffixNames.split(";");
        fiList = GetAllFileRecursively(fiInput, nameFilters);
    } else if (fiInput.isFile())
        fiList<< pathSrc;

    // do!
    for (int fileIdx = 0; fileIdx < fiList.size() && !bUserStop; fileIdx++) {
        ui->label_FileProgress->setText(QString("%1/%2").arg(fileIdx + 1).arg(fiList.size()));

        fiSrc = fiList.at(fileIdx);
        if (!CheckFileSize(fiSrc))
            continue;

        QFile fileSrc(fiSrc.absoluteFilePath());
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
        if (ui->comboBox_EncodingSrc->currentIndex() == 0)
            tsSrc.setCodec(DetectEncoding(fileSrc).toLatin1().data());
        else
            tsSrc.setCodec(sCodecSrc.toLatin1().data());
        tsDest.setCodec(sCodecDest.toLatin1().data());
        tsDest.setGenerateByteOrderMark(ui->checkBox_BomDest->isChecked());
        //qDebug()<< tsDest.codec()->name();

        if (fileIdx != 0)
            ui->plainTextEdit_Output->insertPlainText("\n");

        int lineNum = 1;
        QElapsedTimer eltUpdateUi, eltInteractUi;
        eltUpdateUi.start();
        eltInteractUi.start();
        UpdateProgress(lineNum, 0);
        for (; !tsSrc.atEnd() && !bUserStop; lineNum++) {
            QString sLine = tsSrc.readLine();
            tsDest<< sLine + sNewLine;

            if (eltUpdateUi.elapsed() >= 500) {
                eltUpdateUi.restart();
                float percentage = (double)tsSrc.pos() / fiSrc.size() * 100;
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

        QFile::remove(fileSrc.fileName() + sBakSuffixName);
        QFile::rename(fileSrc.fileName(), fileSrc.fileName() + sBakSuffixName);
        QFile::rename(fileSrc.fileName() + sTmpSuffixName, fileSrc.fileName());
    }  // for (int fileIdx = 0;
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

bool MainWindow::CheckFileSize(const QFileInfo &fiSrc)
{
    if (fiSrc.size() >= iMaxFileSize) {
        QMessageBox::information(this, tr("跳过文件"), tr("%1\n的文件大小大于等于%2GB，将跳过").arg(fiSrc.absoluteFilePath()).arg(iMaxFileSizeGB));
        return false;
    }

    if (fiSrc.size() > iBigFileSize) {
        int ret = QMessageBox::question(this, tr("超大文件确认"), tr("%1\n的文件大小大于%2MB，仍要进行转换吗？").arg(fiSrc.absoluteFilePath()).arg(iBigFileSizeMB));
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

    QTextCursor tc = ui->plainTextEdit_Output->textCursor();
    tc.select(QTextCursor::BlockUnderCursor);
    tc.removeSelectedText();
    QString sOutputLine = tr("%0 （%1行，%2%）").arg(fiSrc.fileName()).arg(lineNum).arg(sPercentage);
    ui->plainTextEdit_Output->appendPlainText(sOutputLine);
//    ui->plainTextEdit_Output->insertPlainText(sOutputLine);
}

