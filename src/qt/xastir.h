#ifndef XASTIR_H
#define XASTIR_H

#include <QMainWindow>
#include <QtNetwork>


namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void connectToServer();
    void updateList();
    void connectionClosedByServer();
    void error();
    void nowConnected();
    void closeConnection();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::MainWindow *ui;

    QTcpSocket tcpSocket;
    QString packetDisplay;
    int total_lines;
};

#endif // XASTIR_H
