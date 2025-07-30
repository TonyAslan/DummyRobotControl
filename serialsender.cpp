#include "serialsender.h"
#include <QDebug>

SerialDataPort::SerialDataPort(QObject *parent) : QObject(parent)
{

}

void SerialDataPort::onError(QSerialPort::SerialPortError value)
{
    if(value != 0)
    {
        QString ErrInfo;
        ErrInfo = "SerialPortError  ErrorCode:" + QString::number(value);
        emit signalError(ErrInfo);
    }
}

void SerialDataPort::onInit()
{
    m_serialPort = new QSerialPort;
    connect(m_serialPort, SIGNAL(readyRead()), this, SLOT(onRead()));
    connect(m_serialPort, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onError(QSerialPort::SerialPortError)));

}

void SerialDataPort::onOpen(const QString &portName, const int &baudRate)
{
    m_serialPort->setPortName(portName);
    if(m_serialPort->open(QIODevice::ReadWrite))
    {
        m_serialPort->setBaudRate(baudRate);
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setStopBits(QSerialPort::OneStop);
        emit signalConnected();
        //qDebug() << "port opened" << endl;
    }else{
        qDebug() << " serialport open fail!";
    }
}

void SerialDataPort::onRead()
{
    if (m_serialPort)
    {
       QByteArray data = m_serialPort->readAll();
       emit signalReceived(data);
       qDebug() << "serialport received : " << data.size() << data << endl;
    }
}

void SerialDataPort::onWrite(const QByteArray &data)
{
    m_serialPort->write(data);
}

void SerialDataPort::onClose()
{
    m_serialPort->close();
    emit signalDisconnected();
}

SerialSender::SerialSender(QObject *parent) : QObject(parent)
{
    m_thread = new QThread;
    m_serialDataPort = new SerialDataPort();
    //向串口操作
    //打开
    connect(this, SIGNAL(signalOpen(QString, int)), m_serialDataPort, SLOT(onOpen(QString, int)));
    //写入
    connect(this, SIGNAL(signalWrite(const QByteArray&)), m_serialDataPort, SLOT(onWrite(const QByteArray&)));
    //关闭
    connect(this, SIGNAL(signalClose()), m_serialDataPort, SLOT(onClose()));
    //接收串口信号
    //接收
    connect(m_serialDataPort, SIGNAL(signalReceived(const QByteArray&)), this, SLOT(onReceiveDatas(const QByteArray&)));//发送接收数据
    //错误
    connect(m_serialDataPort, SIGNAL(signalError(QString)), this, SIGNAL(signalError(QString)));
    //连接
    connect(m_serialDataPort, SIGNAL(signalConnected()), this, SIGNAL(signalOpened()));
    //关闭
    connect(m_serialDataPort, SIGNAL(signalDisconnected()), this, SIGNAL(signalClosed()));
    //响应退出信号
    //相应信号 串口删除
    connect(this, SIGNAL(signalQuiting()), m_serialDataPort, SLOT(deleteLater()));
    //串口删除 线程退出
    connect(m_serialDataPort, SIGNAL(destroyed(QObject*)), m_thread, SLOT(quit()));
    //线程退出 然后删除
    connect(m_thread, SIGNAL(finished()), m_thread, SLOT(deleteLater()));
    //响应子线程启动信号
    //线程开始 串口初始化
    connect(m_thread, SIGNAL(started()), m_serialDataPort, SLOT(onInit()));
    //移入线程执行
    m_serialDataPort->moveToThread(m_thread);
    m_thread->start();
}

SerialSender::~SerialSender()
{
    emit signalQuiting();
}

void SerialSender::sendDatas(const QByteArray &data)
{
    write(data);
}

void SerialSender::open(const QString &strAddress, const int &number)
{
    emit signalOpen(strAddress,number);
}

void SerialSender::close()
{
    emit signalClose();
}

void SerialSender::write(const QByteArray &data)
{
    emit signalWrite(data);
}

void SerialSender::onReceiveDatas(const QByteArray &rawData)
{
    emit signalReceived(rawData);
}
