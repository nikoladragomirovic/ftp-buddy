#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->server_address->setText("ftp.dlptest.com");
    ui->server_port->setText("21");
    ui->username->setText("dlpuser");
    ui->password->setText("rNrKYTX9g7z3RgJRmxWuGHbeu");
    ui->progress->setValue(0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_button_upload_clicked()
{
    ui->progress->setValue(0);
    ui->server_response->clear();
    QCoreApplication::processEvents();

    QString filePath = QFileDialog::getOpenFileName(this, "Select File to Upload", QCoreApplication::applicationDirPath());

    if (filePath.isEmpty()) {
        ui->server_response->append("No file selected.");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->server_response->append("Error opening file: " + file.errorString());
        return;
    }

    QFileInfo fileInfo(file);

    QTcpSocket socket;
    ui->progress->setValue(10);

    QString server_address = ui->server_address->text();
    int server_port = ui->server_port->text().toInt();
    QString username = ui->username->text();
    QString password = ui->password->text();

    socket.connectToHost(server_address, server_port);

    if (socket.waitForConnected()) {
        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to receive welcome message.");
            return;
        }
        ui->progress->setValue(20);

        sendFtpCommand(socket, "USER " + username);
        sendFtpCommand(socket, "PASS " + password);

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Login failed.");
            return;
        }
        ui->progress->setValue(30);

        sendFtpCommand(socket, "PASV");

        QRegularExpressionMatch match;

        if (socket.waitForReadyRead()) {
            QString response = socket.readAll().trimmed();
            QRegularExpression regex("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
            match = regex.match(response);
        } else {
            ui->server_response->append("Error waiting for FTP response: " + socket.errorString());
        }

        QString ipAddress = match.captured(1) + "." + match.captured(2) + "." + match.captured(3) + "." + match.captured(4);
        int port = match.captured(5).toInt() * 256 + match.captured(6).toInt();

        QTcpSocket dataSocket;

        dataSocket.connectToHost(ipAddress, port);
        ui->progress->setValue(50);

        sendFtpCommand(socket, "STOR " + fileInfo.fileName());

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to initiate upload.");
            return;
        }else{
            ui->server_response->append("Download completed successfully.");
            ui->progress->setValue(80);
        }

        QByteArray fileData = file.readAll();
        dataSocket.write(fileData);
        dataSocket.waitForBytesWritten();

        socket.disconnectFromHost();
        dataSocket.disconnectFromHost();
        ui->progress->setValue(100);

        QThread::msleep(1000);
        on_button_list_clicked();

        for (int i = 0; i < ui->file_list->count(); ++i) {
            QListWidgetItem *currentItem = ui->file_list->item(i);
            if (currentItem && currentItem->text() == fileInfo.fileName()) {
                ui->file_list->setCurrentItem(currentItem);
                ui->file_list->scrollToItem(currentItem);
                break;
            }
        }

    } else {
        ui->server_response->append("Failed to connect to the FTP server:" + socket.errorString());
    }
}

void MainWindow::on_button_download_clicked()
{
    QListWidgetItem *selectedItem = ui->file_list->currentItem();

    if(!selectedItem){
        return;
    }

    ui->server_response->clear();
    QCoreApplication::processEvents();

    QString selectedFileName = selectedItem->text();

    QString filePath = "/Users/nile/Downloads/" + selectedFileName ;

    ui->progress->setValue(10);
    QTcpSocket socket;

    QString server_address = ui->server_address->text();
    int server_port = ui->server_port->text().toInt();
    QString username = ui->username->text();
    QString password = ui->password->text();

    socket.connectToHost(server_address, server_port);

    if (socket.waitForConnected()) {
        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to receive welcome message.");
            return;
        }
        ui->progress->setValue(20);
        sendFtpCommand(socket, "USER " + username);
        sendFtpCommand(socket, "PASS " + password);

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Login failed.");
            return;
        }
        ui->progress->setValue(30);

        sendFtpCommand(socket, "PASV");

        QRegularExpressionMatch match;

        if (socket.waitForReadyRead()) {
            QString response = socket.readAll().trimmed();
            QRegularExpression regex("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
            match = regex.match(response);
        } else {
            ui->server_response->append("Error waiting for FTP response:" + socket.errorString());
        }

        QString ipAddress = match.captured(1) + "." + match.captured(2) + "." + match.captured(3) + "." + match.captured(4);
        int port = match.captured(5).toInt() * 256 + match.captured(6).toInt();


        QTcpSocket dataSocket;

        dataSocket.connectToHost(ipAddress, port);

        ui->progress->setValue(50);

        sendFtpCommand(socket, "RETR " + selectedFileName);

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to initiate download.");
            return;
        }else{
            ui->server_response->append("Upload completed successfully.");
            ui->progress->setValue(80);
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            ui->server_response->append("Error opening file for writing:" + file.errorString());
            return;
        }

        while (dataSocket.waitForReadyRead()) {
            QByteArray fileData = dataSocket.readAll();
            file.write(fileData);
        }
        ui->progress->setValue(100);

        file.close();

        socket.disconnectFromHost();
        dataSocket.disconnectFromHost();

        QStringList args;
        args << "-R" << filePath;
        QProcess::startDetached("open", args);

    } else {
        ui->server_response->append("Failed to connect to the FTP server:" + socket.errorString());
    }
}

void MainWindow::on_button_list_clicked()
{
    ui->server_response->clear();
    QCoreApplication::processEvents();
    ui->progress->setValue(10);
    QTcpSocket socket;

    QString server_address = ui->server_address->text();
    int server_port = ui->server_port->text().toInt();
    QString username = ui->username->text();
    QString password = ui->password->text();

    socket.connectToHost(server_address, server_port);

    if (socket.waitForConnected()) {
        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to receive welcome message.");
            return;
        }
            ui->progress->setValue(20);
        sendFtpCommand(socket, "USER " + username);
        sendFtpCommand(socket, "PASS " + password);

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Login failed.");
            return;
        }
            ui->progress->setValue(50);
        sendFtpCommand(socket, "PASV");

        QRegularExpressionMatch match;

        if (socket.waitForReadyRead()) {
            QString response = socket.readAll().trimmed();
            QRegularExpression regex("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
            match = regex.match(response);
        } else {
            ui->server_response->append("Error waiting for FTP response:" + socket.errorString());
        }

        QString ipAddress = match.captured(1) + "." + match.captured(2) + "." + match.captured(3) + "." + match.captured(4);
        int port = match.captured(5).toInt() * 256 + match.captured(6).toInt();

        QTcpSocket dataSocket;

        dataSocket.connectToHost(ipAddress, port);

                    ui->progress->setValue(70);

        sendFtpCommand(socket, "NLST");

        if (!waitForFtpResponse(socket)) {
            ui->server_response->append("Failed to initiate file listing.");
            return;
        }

        if (dataSocket.waitForReadyRead()) {
            QByteArray fileData = dataSocket.readAll();
            QStringList fileList = QString(fileData).split("\r\n", Qt::SkipEmptyParts);

            ui->file_list->clear();

            foreach (const QString &item, fileList) {
                ui->file_list->addItem(item);
            }

            ui->progress->setValue(100);
            ui->server_response->append("File listing completed successfully.");

        } else {
            ui->server_response->append("Error waiting for FTP response:" + dataSocket.errorString());
        }

        socket.disconnectFromHost();
        dataSocket.disconnectFromHost();

    } else {
        ui->server_response->append("Failed to connect to the FTP server:" + socket.errorString());
    }
}


bool MainWindow::waitForFtpResponse(QTcpSocket &socket)
{
    if (socket.waitForReadyRead()) {
        QString response = socket.readAll();
        ui->server_response->append(response.trimmed());
        QCoreApplication::processEvents();
        return true;
    } else {
        ui->server_response->append("Error waiting for FTP response:" + socket.errorString());
        return false;
    }
}

void MainWindow::sendFtpCommand(QTcpSocket &socket, const QString &command)
{
    QByteArray commandData = (command + "\r\n").toUtf8();
    socket.write(commandData);
    socket.waitForBytesWritten();

    QThread::msleep(1000);
}
