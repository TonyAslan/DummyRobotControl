#ifndef SERIALSENDER_H
#define SERIALSENDER_H

#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>

typedef enum CmdType
{
    STOP,
    START,
    HOME,
    CALIBRATION,
    RESET,
    DISABLE,
    GETJPOS,
    GETLPOS,
    CMDMODE,
    MOVEJ,
    MOVEL,
    SETKP,
    SETKI,
    SETKD
}CMD_TYPE;

// 工作线程中执行串口操作的类
class SerialDataPort : public QObject
{
    Q_OBJECT
public:
    explicit SerialDataPort(QObject *parent = nullptr);

signals:
    void signalReceived(const QByteArray& data);
    void signalError(QString);
    void signalConnected();
    void signalDisconnected();
public slots:
    void onError(QSerialPort::SerialPortError);
    void onInit();
    void onOpen(const QString& portName, const int& baudRate);
    void onRead();
    void onWrite(const QByteArray& data);
    void onClose();
private:
    QSerialPort* m_serialPort;
    mutable QMutex m_mutex;
};

// 供主线程使用的串口发送器类
class SerialSender : public QObject
{
    Q_OBJECT
public:
    explicit SerialSender(QObject *parent = nullptr);
    ~SerialSender();

    void sendDatas(const QByteArray& data);

    //打开 串口：串口号、波特率 网络：地址、端口
    void open(const QString& strAddress, const int& number);

    void close();

private:
    void write(const QByteArray& data);

private slots:
    //接收到数据
    void onReceiveDatas(const QByteArray &rawData);

signals:
    //对外
    void signalReceived(const QByteArray& data);
    void signalError(QString);
    void signalOpened();
    void signalClosed();
    //对内
    void signalWrite(const QByteArray& data);
    void signalOpen(QString str, int number);
    void signalClose();
    void signalQuiting();

private:
    QThread* m_thread;
    SerialDataPort* m_serialDataPort;
    //接收缓冲区
    QByteArray m_receiveBuffer;
};

#endif // SERIALSENDER_H
