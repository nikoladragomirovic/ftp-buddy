// mainwindow.h

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork>
#include <QtCore>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QFileDialog>
#include <QTextEdit>
#include <QDesktopServices>


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void on_button_list_clicked();

private slots:
    void on_button_upload_clicked();
    void on_button_download_clicked();
    void on_button_remove_clicked();
    bool ftp_login();
    bool waitForFtpResponse(QTcpSocket &socket);
    void sendFtpCommand(QTcpSocket &socket, const QString &command);

private:
    Ui::MainWindow *ui;
    QStringListModel *fileListModel;
    QTcpSocket socket;
    QTcpSocket dataSocket;
};

#endif // MAINWINDOW_H
