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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_PathSrcBrowse_clicked()
{
    QString sDir = QFileDialog::getExistingDirectory(this, tr("选择输入文件夹"), "");
//    QString sDir = QFileDialog::getSaveFileName(this, tr("选择输入文件或文件夹"), "");  //不支持文件夹
    if (!sDir.isEmpty()) {
        ui->lineEdit_PathSrc->setText(sDir);
    }
}

void MainWindow::on_pushButton_PathDestBrowse_clicked()
{

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

    QFileInfo fiInput(pathSrc);
    QFileInfoList fiList;
    if (fiInput.isDir()) {
        QString sSuffixNames = ui->plainTextEdit_SuffixName->toPlainText();
        QStringList nameFilters = sSuffixNames.split(";");
        fiList = GetAllFileRecursively(fiInput, nameFilters);
    } else if (fiInput.isFile())
        fiList<< pathSrc;

    for (int fileIdx = 0; fileIdx < fiList.size() && !bUserStop; fileIdx++) {
        ui->label_FileProgress->setText(QString("%1/%2").arg(fileIdx + 1).arg(fiList.size()));

        fiSrc = fiList.at(fileIdx);
        QFile fileSrc(fiSrc.absoluteFilePath());
        QFile fileDest(fileSrc.fileName() + ".tmp");
        if (!fileSrc.open(QFile::ReadOnly)) {
            QMessageBox::critical(this, "", tr("打开输入文件错误"));
            return;
        }
        if (!fileDest.open(QFile::WriteOnly)) {
            QMessageBox::critical(this, "", tr("打开输出文件错误"));
            return;
        }

        QTextStream tsSrc(&fileSrc);
        QTextStream tsDest(&fileDest);
        tsSrc.setCodec(sCodecSrc.toLatin1().data());  // if not set, can be error
        tsDest.setCodec(sCodecDest.toLatin1().data());
        tsDest.setGenerateByteOrderMark(ui->checkBox_BomDest->isChecked());
        //qDebug()<< tsDest.codec()->name();

        if (fileIdx != 0)
            ui->plainTextEdit_Output->insertPlainText("\n");

        int lineNum = 1;
        for (; !tsSrc.atEnd() && !bUserStop; lineNum++) {
            float percentage = tsSrc.pos() * 100.0 / fiSrc.size();
            UpdateProgress(lineNum, percentage);
            QString sLine = tsSrc.readLine();
            tsDest<< sLine + sNewLine;

            if (lineNum % 1 == 0) {
                QCoreApplication::processEvents();
                QThread::msleep(10);
            }
        }
        if (!bUserStop)
            UpdateProgress(lineNum, 100);

        fileSrc.close();
        fileDest.close();
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

