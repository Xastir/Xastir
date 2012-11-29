#include "ui_mainwindow.h"
#include "xastir.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->disconnectServerButton->setEnabled(false);

    connect(ui->connectServerButton, SIGNAL(clicked()), this, SLOT(connectToServer()));
    connect(ui->disconnectServerButton, SIGNAL(clicked()), this, SLOT(closeConnection()));

    connect(&tcpSocket, SIGNAL(connected()), this, SLOT(nowConnected()));
    connect(&tcpSocket, SIGNAL(disconnected()), this, SLOT(connectionClosedByServer()));
    connect(&tcpSocket, SIGNAL(readyRead()), this, SLOT(updateList()));
    connect(&tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(error()));

    total_lines = 0;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::nowConnected()
{
    ui->disconnectServerButton->setEnabled(true);
    ui->connectServerButton->setEnabled(false);
}

void MainWindow::closeConnection()
{
    tcpSocket.close();
    ui->connectServerButton->setEnabled(true);
    ui->disconnectServerButton->setEnabled(false);
}

void MainWindow::connectToServer ()
{
    QString host = ui->InternetHost->text();
    QString portStr = ui->InternetPort->text();
    int port = portStr.toInt();
    tcpSocket.connectToHost(host, port);

    // Send authentication and filter info to server.
    // Note that the callsign is case-sensitive in javaAprsServer
    QString callsign = ui->Callsign->text().toUpper();
    QString passcode = ui->Passcode->text();
    QString filter = ui->Filter->text();
    QString loginStr;
    if (filter.size() > 0) {
        loginStr = "user " + callsign + " pass " + passcode + " vers XASTIR-QT 0.1 filter " + filter + "\r\n";
    }
    else {
        loginStr = "user " + callsign + " pass " + passcode + " vers XASTIR-QT 0.1 \r\n";
    }
    tcpSocket.write( loginStr.toAscii().data() );

    // Send a posit to the server
    if (callsign.startsWith("WE7U")) {
        tcpSocket.write( "WE7U>APX201:=/6<A5/VVOx   SCVSAR\r\n" );
    }
}

void MainWindow::updateList ()
{
    QTextStream in(&tcpSocket);
    int max_lines = 20;
    QString tmp;

    forever {
        if (!tcpSocket.canReadLine()) {
            break;
        }
        packetDisplay.append(in.readLine() + "\n");

        if (total_lines >= max_lines) {
            int ii = packetDisplay.indexOf("\n");
            // Chop first line
            tmp = packetDisplay.right(packetDisplay.size() - (ii + 1));
            packetDisplay = tmp;
        }
        else {
            total_lines++;
        }
        ui->incomingPackets->setText(packetDisplay);
    }
}

void MainWindow::connectionClosedByServer ()
{
    closeConnection();
}

void MainWindow::error ()
{
    closeConnection();
}

