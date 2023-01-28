#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileInfo>
#include <QTextStream>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_PathInBrowse_clicked();
    void on_pushButton_Start_clicked();
    void on_pushButton_Stop_clicked();

    void on_comboBox_EncodingDest_currentTextChanged(const QString &arg1);

private:
    Ui::MainWindow *ui;

    bool bUserStop = false;
    QFileInfo fiFileSrc;
    QString sIniPath;
    bool CheckFileSize();
    void UpdateProgress(int lineNum, float percentage);

    QString sBakSuffixName;
    QString sTmpSuffixName;
};
#endif // MAINWINDOW_H
