#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //Set default values for elements
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

// Every action needs a 'login' first, so its a separate function to be re-used
bool MainWindow::ftp_login(){

    //Disconnect sockets first
    socket.disconnectFromHost();
    dataSocket.disconnectFromHost();

    //Reset progress bar
    ui->progress->setValue(0);

    //Clear server response text field
    ui->server_response->clear();

    //Process events to update UI instantly
    QCoreApplication::processEvents();

    ui->progress->setValue(10);

    //Get variables from input fields
    QString server_address = ui->server_address->text();
    int server_port = ui->server_port->text().toInt();
    QString username = ui->username->text();
    QString password = ui->password->text();

    socket.connectToHost(server_address, server_port);

    //Check if connection established
    if(!socket.waitForConnected()){
        ui->server_response->append("Failed to connect to the FTP server:" + socket.errorString());
        return false;
    }

    //Wait for a welcome message upon connection
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Failed to receive welcome message.");
        return false;
    }

    ui->progress->setValue(20);

    sendFtpCommand(socket, "USER " + username);

    //Wait for password prompt from server
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Login failed.");
        return false;
    }

    ui->progress->setValue(30);

    sendFtpCommand(socket, "PASS " + password);

    //Wait for login auth response from server
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Login failed.");
        return false;
    }

    ui->progress->setValue(40);

    //Send passive mode command
    sendFtpCommand(socket, "PASV");

    QRegularExpressionMatch match;

    //Parse server response (IP & PORT for data connection)
    if (socket.waitForReadyRead()) {
        QString response = socket.readAll().trimmed();
        QRegularExpression regex("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
        match = regex.match(response);
    } else {
        ui->server_response->append("Error waiting for FTP response: " + socket.errorString());
        return false;
    }

    QString ip_address = match.captured(1) + "." + match.captured(2) + "." + match.captured(3) + "." + match.captured(4);
    int port = match.captured(5).toInt() * 256 + match.captured(6).toInt();

    //Connect separate data socket to specified IP & PORT in PASV
    dataSocket.connectToHost(ip_address, port);

    //Update progress if dataSocket connected
    if(dataSocket.waitForConnected()){
        ui->server_response->append("Data socket connected");
        ui->progress->setValue(60);
    }else{
        return false;
    }

    return true;
}

// Upload a file to the FTP server
void MainWindow::on_button_upload_clicked()
{

    //Native file picker popup
    QString filePath = QFileDialog::getOpenFileName(this, "Select File to Upload", QCoreApplication::applicationDirPath());

    //Check if file picker exited
    if (filePath.isEmpty()) {
        ui->server_response->append("No file selected.");
        return;
    }

    //Check for corrupt files
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->server_response->append("Error opening file: " + file.errorString());
        return;
    }

    //Used to get file name
    QFileInfo fileInfo(file);

    //Log in to server
    if (!ftp_login()) return;

    //Command to store file to FTP server
    sendFtpCommand(socket, "STOR " + fileInfo.fileName());

    //Wait for server to respond if data can be sent
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Failed to initiate upload.");
        return;
    }else{
        ui->progress->setValue(80);
    }

    //Read file as bytes
    QByteArray fileData = file.readAll();

    //Write to dataSocket instead of default socket
    dataSocket.write(fileData);

    while(dataSocket.waitForBytesWritten());

    //Update file list after upload finishes
    on_button_list_clicked();

    //Select uploaded item when list updates
    for (int i = 0; i < ui->file_list->count(); ++i) {
        QListWidgetItem *currentItem = ui->file_list->item(i);
        if (currentItem && currentItem->text() == fileInfo.fileName()) {
            ui->file_list->setCurrentItem(currentItem);
            ui->file_list->scrollToItem(currentItem);
            break;
        }
    }

    ui->server_response->append("Upload completed successfully!");
    ui->progress->setValue(100);

    }

    //Download a file from FTP server
void MainWindow::on_button_download_clicked()
{
    //Download selected item from list
    QListWidgetItem *selectedItem = ui->file_list->currentItem();

    //If clicked without selecting item does nothing
    if(!selectedItem){
        return;
    }

    //Get selected file name
    QString selectedFileName = selectedItem->text();

    //Put it in downloads folder
    QString filePath = "/Users/nile/Downloads/" + selectedFileName ;

    //Log in to server
    if (!ftp_login()) return;

    //Send command to download file
    sendFtpCommand(socket, "RETR " + selectedFileName);

    //Check for FTP response
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Failed to initiate download.");
        return;
    }else{
        ui->progress->setValue(80);
    }

    QFile file(filePath);

    //Create file
    if (!file.open(QIODevice::WriteOnly)) {
        ui->server_response->append("Error opening file for writing:" + file.errorString());
        return;
    }

    //Get file data from dataSocket
    while (dataSocket.waitForReadyRead()) {
        QByteArray fileData = dataSocket.readAll();
        file.write(fileData);
    }


    file.close();

    ui->progress->setValue(100);
    ui->server_response->append("Download successful");

    //Highlight file in default viewer
    QStringList args;
    args << "-R" << filePath;
    QProcess::startDetached("open", args);

    }

    //List files in FTP server
void MainWindow::on_button_list_clicked()
{

    //Log in to server
    if (!ftp_login()) return;

    ui->progress->setValue(70);

    //Send list command to FTP server
    sendFtpCommand(socket, "NLST");

    //Wait for FTP response
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Failed to initiate file listing.");
        return;
    }

    ui->progress->setValue(80);

    dataSocket.waitForReadyRead();

    //Parse string of files as QStringList
    QByteArray fileData = dataSocket.readAll();
    QStringList fileList = QString(fileData).split("\r\n", Qt::SkipEmptyParts);

    //Clear file list
    ui->file_list->clear();

    //Update file list
    foreach (const QString &item, fileList) {
        ui->file_list->addItem(item);
    }

    //Update progress
    ui->progress->setValue(100);
    ui->server_response->append("File listing completed successfully.");

}

void MainWindow::on_button_remove_clicked()
{
    // Log in to the server
    if (!ftp_login()) return;

    // Get selected file name
    QListWidgetItem *selectedItem = ui->file_list->currentItem();

    // If no item is selected, do nothing
    if (!selectedItem) {
        ui->server_response->append("No file selected for removal.");
        return;
    }

    QString selectedFileName = selectedItem->text();

    // Send command to remove file
    sendFtpCommand(socket, "DELE " + selectedFileName);

    // Wait for FTP response
    if (!waitForFtpResponse(socket)) {
        ui->server_response->append("Failed to initiate file removal.");
        return;
    } else {
        ui->progress->setValue(80);
    }

    ui->server_response->append("File removal successful.");

    // Refresh the file list after removal
    on_button_list_clicked();
}

    //Wrapper function based on waitForReadyRead()
bool MainWindow::waitForFtpResponse(QTcpSocket &socket)
{
    //Log server response
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

    //Sends FTP commands via TCP sockets
void MainWindow::sendFtpCommand(QTcpSocket &socket, const QString &command)
{
    //Adds "\r\n" to indicate message end
    QByteArray commandData = (command + "\r\n").toUtf8();

    //Writes command to socket (socket connected to FTP server)
    socket.write(commandData);
    socket.waitForBytesWritten();

    //To give server time to respond (can be higher or lower based on server speed)
    QThread::msleep(100);
}
