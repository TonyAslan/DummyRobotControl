#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QSerialPortInfo>
#include <QScreen>
#include <QKeyEvent>
#include <QVector3D>
#include <cmath>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainWidget)
    , m_serialSender(new SerialSender(this))
{
    ui->setupUi(this);

    QList <QScreen *> list_screen = QGuiApplication::screens();

    /* 如果是 ARM 平台，直接设置大小为屏幕的大小 */
#if __arm__
    /* 重设大小 */
    this->resize(list_screen.at(0)->geometry().width(),
                 list_screen.at(0)->geometry().height());
#else
    /* 否则则设置主窗体大小为 800x480 */
    this->resize(800, 480);
#endif

    initMap();
    scanSerialPort();
    connect(m_serialSender, &SerialSender::signalReceived, this, &MainWidget::onDataReceived);
    connect(m_serialSender, &SerialSender::signalOpened, this, &MainWidget::onSerialOpened);
    connect(m_serialSender, &SerialSender::signalClosed, this, &MainWidget::onSerialClosed);
    connect(m_serialSender, &SerialSender::signalError, this, &MainWidget::onSerialError);

    m_timer = new QTimer(this);
    m_timer->setInterval(300);
    connect(m_timer,&QTimer::timeout,this,&MainWidget::onSendGetLPosRequest);

    m_runTimer = new QTimer(this);
    m_runTimer->setInterval(20*(100/m_fSpeed));//应该和速度负相关
    connect(m_runTimer,&QTimer::timeout,this,&MainWidget::onPlayRecord);

    updateFileList();

    //初始化示教按钮组
    m_jointAddBtnGroup = new QButtonGroup(this);
    m_jointAddBtnGroup->addButton(ui->J1add_Btn,0);
    m_jointAddBtnGroup->addButton(ui->J2add_Btn,1);
    m_jointAddBtnGroup->addButton(ui->J3add_Btn,2);
    m_jointAddBtnGroup->addButton(ui->J4add_Btn,3);
    m_jointAddBtnGroup->addButton(ui->J5add_Btn,4);
    m_jointAddBtnGroup->addButton(ui->J6add_Btn,5);
    connect(m_jointAddBtnGroup,SIGNAL(buttonPressed(int)),this,SLOT(onJointAddBtnPressed(int)));
    connect(m_jointAddBtnGroup,SIGNAL(buttonReleased(int)),this,SLOT(onTeachBtnReleased()));

    m_jointReduceBtnGroup = new QButtonGroup(this);
    m_jointReduceBtnGroup->addButton(ui->J1reduce_Btn,0);
    m_jointReduceBtnGroup->addButton(ui->J2reduce_Btn,1);
    m_jointReduceBtnGroup->addButton(ui->J3reduce_Btn,2);
    m_jointReduceBtnGroup->addButton(ui->J4reduce_Btn,3);
    m_jointReduceBtnGroup->addButton(ui->J5reduce_Btn,4);
    m_jointReduceBtnGroup->addButton(ui->J6reduce_Btn,5);
    connect(m_jointReduceBtnGroup,SIGNAL(buttonPressed(int)),this,SLOT(onJointRecudeBtnPressed(int)));
    connect(m_jointReduceBtnGroup,SIGNAL(buttonReleased(int)),this,SLOT(onTeachBtnReleased()));

    m_posAddBtnGroup = new QButtonGroup(this);
    m_posAddBtnGroup->addButton(ui->Xadd_Btn,0);
    m_posAddBtnGroup->addButton(ui->Yadd_Btn,1);
    m_posAddBtnGroup->addButton(ui->Zadd_Btn,2);
    m_posAddBtnGroup->addButton(ui->Aadd_Btn,3);
    m_posAddBtnGroup->addButton(ui->Badd_Btn,4);
    m_posAddBtnGroup->addButton(ui->Cadd_Btn,5);
    connect(m_posAddBtnGroup,SIGNAL(buttonPressed(int)),this,SLOT(onPosAddBtnPressed(int)));
    connect(m_posAddBtnGroup,SIGNAL(buttonReleased(int)),this,SLOT(onTeachBtnReleased()));

    m_posReduceBtnGroup = new QButtonGroup(this);
    m_posReduceBtnGroup->addButton(ui->Xreduce_Btn,0);
    m_posReduceBtnGroup->addButton(ui->Yreduce_Btn,1);
    m_posReduceBtnGroup->addButton(ui->Zreduce_Btn,2);
    m_posReduceBtnGroup->addButton(ui->Areduce_Btn,3);
    m_posReduceBtnGroup->addButton(ui->Breduce_Btn,4);
    m_posReduceBtnGroup->addButton(ui->Creduce_Btn,5);
    connect(m_posReduceBtnGroup,SIGNAL(buttonPressed(int)),this,SLOT(onPosReduceBtnPressed(int)));
    connect(m_posReduceBtnGroup,SIGNAL(buttonReleased(int)),this,SLOT(onTeachBtnReleased()));

    m_teachTimer = new QTimer(this);
    connect(m_teachTimer,&QTimer::timeout,this,&MainWidget::onTeaching);

    ui->addLine_groupBox->setDisabled(true);
    ui->addCircle_groupBox->setDisabled(true);

}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::onSerialError(const QString &strError)
{
    qDebug() << "serial error" << strError << endl;
}

void MainWidget::onDataReceived(const QByteArray &data)
{
    ui->textBrowser->append(data);
    QString strData = QString::fromLocal8Bit(data);

    if(m_bIsCreatePoint)
    {
        if(strData.contains("ok") && (strData.size() > 30))
        {
            strData.remove("ok");
            qDebug() << "strData = " << strData << endl;
            strData.replace("\r\n","");
            ui->currentPos_label->setText(strData);
            QStringList posList = strData.split(" ");
            posList.removeAt(0);
            setPointPos(posList);
            qDebug() << posList;
        }

        m_bIsCreatePoint = false;
    }

    if(m_bIsRecording)
    {
        //写示教记录
        if(strData.contains("ok"))
        {
            strData.remove("ok");
        }
        writeRecordFile(strData.toUtf8());
    }

    if(m_bIsTeaching)
    {
        if(m_curTeachType == MOVE_JOINT)
        {
            if(strData.contains("ok") && (strData.size() > 30))
            {
                strData.remove("ok");
                qDebug() << "strData = " << strData << endl;
                strData.replace("\r\n","");
                ui->currentAngle_label->setText(strData);
                QStringList angleList = strData.split(" ");
                angleList.removeAt(0);

                angleList.append(QString::number(m_fSpeed));
                qDebug() << "angleList = " << angleList << endl;
                m_CurAngleList = angleList;

                m_teachTimer->start();

            }
        }else if(m_curTeachType == MOVE_LINE)
        {
            if(strData.contains("ok") && (strData.size() > 30))
            {
                strData.remove("ok");
                qDebug() << "strData = " << strData << endl;
                strData.replace("\r\n","");
                ui->currentPos_label->setText(strData);
                QStringList posList = strData.split(" ");
                posList.removeAt(0);

                posList.append(QString::number(m_fSpeed));

                m_CurPosList = posList;

                m_teachTimer->start();

            }
        }
    }
}

void MainWidget::onSerialOpened()
{
    qDebug() << "serial opened" << endl;
    QMessageBox::information(this,QStringLiteral("tips"),QStringLiteral("connect success!"));
    ui->connect_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
}

void MainWidget::onSerialClosed()
{
    ui->connect_Btn->setStyleSheet("");
    qDebug() << "serial closed" << endl;
}

void MainWidget::onSendGetLPosRequest()
{
    m_serialSender->sendDatas(constructCmd(GETLPOS));
}

void MainWidget::onTeaching()
{
    if(m_curTeachType == MOVE_JOINT)
    {
        //qDebug() << " m_CurAngleList = " << m_CurAngleList << endl;
        if(m_curOperateType == ADD_VALUE)
        {
            m_CurAngleList.replace(m_nCurOpJoint,QString::number(m_CurAngleList[m_nCurOpJoint].toFloat() + 1));
        }else{
            m_CurAngleList.replace(m_nCurOpJoint,QString::number(m_CurAngleList[m_nCurOpJoint].toFloat() - 1));
        }
        //qDebug() << " m_CurAngleList = " << m_CurAngleList << endl;

        m_CurAngleList.replace(6,QString::number(m_fSpeed));
        qDebug() << " m_CurAngleList = " << m_CurAngleList << endl;
        ui->currentAngle_label->setText(m_CurAngleList.join(","));
        m_serialSender->sendDatas(constructCmd(MOVEJ,m_CurAngleList));
    }else
    {
        if(m_curOperateType == ADD_VALUE)
        {
            m_CurPosList.replace(m_nCurOpPos,QString::number(m_CurPosList[m_nCurOpPos].toFloat() + 1));
        }else{
            m_CurPosList.replace(m_nCurOpPos,QString::number(m_CurPosList[m_nCurOpPos].toFloat() - 1));
        }
        m_CurPosList.replace(6,QString::number(m_fSpeed));
        //qDebug() << " m_CurPosList = " << m_CurPosList << endl;
        ui->currentPos_label->setText(m_CurPosList.join(","));
        m_serialSender->sendDatas(constructCmd(MOVEL,m_CurPosList));
    }
}

void MainWidget::sendTeachGetRequest()
{
    if(m_curTeachType == MOVE_JOINT)
    {
        m_serialSender->sendDatas(constructCmd(GETJPOS));
    }else if(m_curTeachType == MOVE_LINE)
    {
        m_serialSender->sendDatas(constructCmd(GETLPOS));
    }
}

void MainWidget::keyPressEvent(QKeyEvent *event)
{
#if __arm__
    //判断按下的按键，也就是板子 KEY0 按键
    if(event->key() == Qt::Key_VolumeDown) {
        m_serialSender->sendDatas(constructCmd(STOP));
        if(m_runTimer->isActive())
        {
            m_runTimer->stop();
        }
    }
#endif

    QWidget::keyPressEvent(event);

}

void MainWidget::keyReleaseEvent(QKeyEvent *event)
{
    QWidget::keyReleaseEvent(event);
}

void MainWidget::setPointPos(const QStringList & posList)
{
    if(m_CurCreatePoint == LINE_START)
    {
        m_lineStart.clear();
        for(int i = 0; i < 6; ++i)
        {
            m_lineStart << posList.at(i).toFloat();
        }
        ui->lineStart_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    }else if(m_CurCreatePoint == LINE_END)
    {
        m_lineEnd.clear();
        for(int i = 0; i < 6; ++i)
        {
            m_lineEnd << posList.at(i).toFloat();
        }
        ui->lineEnd_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    }else if(m_CurCreatePoint == CIRCLE_START)
    {

        m_circleStart.clear();
        for(int i = 0; i < 6; ++i)
        {
            m_circleStart << posList.at(i).toFloat();
        }
        ui->circleStart_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    }else if(m_CurCreatePoint == CIRCLE_CENTER)
    {
        m_circleCenter.clear();
        for(int i = 0; i < 6; ++i)
        {
            m_circleCenter << posList.at(i).toFloat();
        }
        ui->circleCenter_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    }else if(m_CurCreatePoint == CIRCLE_END)
    {
        m_circleEnd.clear();
        for(int i = 0; i < 6; ++i)
        {
            m_circleEnd << posList.at(i).toFloat();
        }
        ui->circleEnd_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    }
}

void MainWidget::onPlayRecord()
{
    if(!m_cmdQueue.isEmpty())
    {
        QString strData = QString::fromUtf8(m_cmdQueue.dequeue());
       // qDebug() << "m_cmdQueue.data = " << strData << endl;
        strData.replace(' ',',');
        strData.remove(1,1);
        strData += ",";
        strData += QString::number(m_fSpeed);
        strData += "\r\n";
        QByteArray data = strData.toUtf8();
        qDebug() << " data = " << data;
        m_serialSender->sendDatas(data);
        m_nReadLines++;
    }else{
        m_runTimer->stop();
    }
}

void MainWidget::onJointAddBtnPressed(int nJoint)
{
    m_bIsTeaching = true;
    m_curTeachType = MOVE_JOINT;
    m_curOperateType = ADD_VALUE;
    m_teachTimer->setInterval(40*(100/m_fSpeed));
    m_nCurOpJoint = nJoint;
    sendTeachGetRequest();
}

void MainWidget::onJointRecudeBtnPressed(int nJoint)
{
    m_bIsTeaching = true;
    m_curTeachType = MOVE_JOINT;
    m_curOperateType = REDUCE_VALUE;
    m_teachTimer->setInterval(40*(100/m_fSpeed));
    m_nCurOpJoint = nJoint;
    sendTeachGetRequest();
}

void MainWidget::onPosAddBtnPressed(int nPos)
{
    m_bIsTeaching = true;
    m_curTeachType = MOVE_LINE;
    m_curOperateType = ADD_VALUE;
    m_teachTimer->setInterval(40*(100/m_fSpeed));
    m_nCurOpPos = nPos;
    sendTeachGetRequest();
}

void MainWidget::onPosReduceBtnPressed(int nPos)
{
    m_bIsTeaching = true;
    m_curTeachType = MOVE_LINE;
    m_curOperateType = REDUCE_VALUE;
    m_teachTimer->setInterval(40*(100/m_fSpeed));
    m_nCurOpPos = nPos;
    sendTeachGetRequest();
}

void MainWidget::onTeachBtnReleased()
{
    m_bIsTeaching = false;
    if(m_teachTimer->isActive())
    {
        m_teachTimer->stop();
    }
}

void MainWidget::on_connect_Btn_clicked()
{
    //ui->connect_Btn->setStyleSheet("background-color: rgb(0, 255, 0);");
    if(ui->connect_Btn->styleSheet().contains("background"))
    {
        m_serialSender->close();
        return;
    }
    m_serialSender->open(ui->comboBox->currentText(),115200);
}

void MainWidget::on_start_Btn_clicked()
{
    qDebug() << __FUNCTION__ << endl;
    m_serialSender->sendDatas(constructCmd(START));
}

void MainWidget::on_stop_Btn_clicked()
{
    qDebug() << __FUNCTION__ << endl;
    m_serialSender->sendDatas(constructCmd(STOP));
}

void MainWidget::on_home_Btn_clicked()
{
    qDebug() << __FUNCTION__ << endl;
    m_serialSender->sendDatas(constructCmd(HOME));
}

void MainWidget::scanSerialPort()
{
    /* 查找可用串口 */
    foreach (const QSerialPortInfo &info,
             QSerialPortInfo::availablePorts()) {
        ui->comboBox->addItem(info.portName());
    }
}

void MainWidget::initMap()
{
    m_CmdMap.insert(STOP,QStringLiteral("!STOP"));
    m_CmdMap.insert(START,QStringLiteral("!START"));
    m_CmdMap.insert(HOME,QStringLiteral("!HOME"));
    m_CmdMap.insert(CALIBRATION,QStringLiteral("!CALIBRATION"));
    m_CmdMap.insert(RESET,QStringLiteral("!RESET"));
    m_CmdMap.insert(DISABLE,QStringLiteral("!DISABLE"));
    m_CmdMap.insert(GETJPOS,QStringLiteral("#GETJPOS"));
    m_CmdMap.insert(GETLPOS,QStringLiteral("#GETLPOS"));
    m_CmdMap.insert(CMDMODE,QStringLiteral("#CMDMODE")); //有参数 lu
    m_CmdMap.insert(MOVEJ,QStringLiteral("&"));
    m_CmdMap.insert(MOVEL,QStringLiteral("@"));
    m_CmdMap.insert(SETKP,QStringLiteral("#SET_DCE_KP"));
    m_CmdMap.insert(SETKI,QStringLiteral("#SET_DCE_KI"));
    m_CmdMap.insert(SETKD,QStringLiteral("#SET_DCE_KD"));
}

void MainWidget::writeRecordFile(const QByteArray& data)
{
    if(m_recordFile.isOpen())
    {
        QTextStream stream(&m_recordFile);
        stream << data ;
        stream.flush();
        qDebug() << "write data to file" << endl;
    }
}

void MainWidget::readRecordFile(const QString& fileName)
{
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString directoryPath = documentsPath + "/TeachRecords";
    QString filePath = directoryPath + "/" + fileName;
    QFile file(filePath);
    qDebug() << "read filepath = " << filePath << endl;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open playback file:" << filePath;
        return;
    }

    // 清空队列
    m_cmdQueue.clear();

    // 解析文件内容
    QTextStream in(&file);
    while (!in.atEnd()) {
        //如果是继续运动，跳过之前已经运行过的
        if(m_nReadLines > 0)
        {
            for(int i = 0; i < m_nReadLines; ++i)
            {
                in.readLine();
            }
        }

        QString line = in.readLine();
        QByteArray cmdData = QString("@"+line).toUtf8();
        m_cmdQueue.enqueue(cmdData);
        //qDebug() << "enqueue data = " << cmdData;
    }
    file.close();
}

void MainWidget::updateFileList()
{
    ui->listWidget->clear();
    QStringList fileNameList;
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString directoryPath = documentsPath + "/TeachRecords";
    QDir dir(directoryPath);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    fileNameList = dir.entryList();

    ui->listWidget->addItems(fileNameList);

}

QByteArray MainWidget::constructCmd(CMD_TYPE cmd, const QStringList &paraList)
{
    QString strCmd;
    strCmd = m_CmdMap.value(cmd);
    switch (cmd) {
    case CMD_TYPE::STOP:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::START:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::HOME:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::RESET:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::DISABLE:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::GETJPOS:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::GETLPOS:
    {
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::CMDMODE:
    {
        strCmd += paraList.at(0);
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }
    case CMD_TYPE::MOVEJ:
    {
        for(int i = 0; i < paraList.size(); ++i)
        {
            strCmd += paraList.at(i);
            if(i == paraList.size() - 1) break;
            strCmd += ",";
        }
        strCmd += "\r\n";
        qDebug() << "strCmd = " << strCmd;
        return QByteArray(strCmd.toUtf8());
        break;
    }case CMD_TYPE::MOVEL:
    {
        for(int i = 0; i < paraList.size(); ++i)
        {
            strCmd += paraList.at(i);
            if(i == paraList.size() - 1) break;
            strCmd += ",";
        }
        strCmd += "\r\n";
        qDebug() << "strCmd = " << strCmd;
        return QByteArray(strCmd.toUtf8());
        break;
    }case CMD_TYPE::SETKP:
    {
        for(int i = 0; i < paraList.size(); ++i)
        {
            strCmd += " ";
            strCmd += paraList.at(i);
        }
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }case CMD_TYPE::SETKI:
    {
        for(int i = 0; i < paraList.size(); ++i)
        {
            strCmd += " ";
            strCmd += paraList.at(i);
        }
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }case CMD_TYPE::SETKD:
    {
        for(int i = 0; i < paraList.size(); ++i)
        {
            strCmd += " ";
            strCmd += paraList.at(i);
        }
        strCmd += "\r\n";
        return QByteArray(strCmd.toUtf8());
        break;
    }

    default:
        return QByteArray();
        break;
    }

}

void MainWidget::on_dragTeach_Btn_clicked()
{
    // 1. 创建专用的记录目录
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir recordsDir(documentsPath + "/TeachRecords");

    if (!recordsDir.exists()) {
        recordsDir.mkpath(".");
    }

    // 2. 生成安全的文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("teach_record_%1.txt").arg(timestamp);
    QString filePath = recordsDir.filePath(filename);

    m_recordFile.setFileName(filePath);
    if (!m_recordFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "Failed to open record file:" << filename;
        return;
    }

    m_bIsRecording = true;
    m_timer->start();
    ui->dragTeach_Btn->setDisabled(true);
    ui->stopDragTeach_Btn->setDisabled(false);
}

void MainWidget::on_stopDragTeach_Btn_clicked()
{
    m_recordFile.close();
    m_bIsRecording = false;
    m_timer->stop();
    ui->dragTeach_Btn->setDisabled(false);
    ui->stopDragTeach_Btn->setDisabled(true);
    updateFileList();
}

void MainWidget::on_disable_Btn_clicked()
{
    m_serialSender->sendDatas(constructCmd(DISABLE));
}

void MainWidget::on_reapper_Btn_clicked()
{
    if(ui->listWidget->currentRow() < 0)
        return;

    m_nReadLines = 0;
    readRecordFile(ui->listWidget->currentItem()->text());
    m_runTimer->start();
}

void MainWidget::on_continueReappear_Btn_clicked()
{
    if(ui->listWidget->currentRow() < 0)
        return;
    readRecordFile(ui->listWidget->currentItem()->text());
    m_runTimer->start();
}

void MainWidget::on_stopReappear_Btn_clicked()
{
    m_runTimer->stop();
}

void MainWidget::on_getJPos_Btn_clicked()
{
    m_serialSender->sendDatas(constructCmd(GETJPOS));
}

void MainWidget::on_rest_Btn_clicked()
{
    QStringList paraList = {"0","-75","180","0","0","0",QString::number(m_fSpeed)};
    m_serialSender->sendDatas(constructCmd(MOVEJ,paraList));
}

void MainWidget::on_getLPos_Btn_clicked()
{
    m_serialSender->sendDatas(constructCmd(GETLPOS));
}

//void MainWidget::on_speedSlider_sliderMoved(int position)
//{
//    m_fSpeed = position;
//    qDebug() << "m_fSpeed = " << m_fSpeed << endl;
//}

void MainWidget::on_deleteRecord_Btn_clicked()
{
    QString fileName = ui->listWidget->currentItem()->text();
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString directoryPath = documentsPath + "/TeachRecords";
    QString filePath = directoryPath + "/" + fileName;

    if(QFile::remove(filePath))
    {
        QMessageBox::information(this,QStringLiteral("tips"),QStringLiteral("delete file success!"));
    }

    updateFileList();
}
//开始创建轨迹 打开一个文件
void MainWidget::on_startCreateTrajectory_Btn_clicked()
{
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir recordsDir(documentsPath + "/TeachRecords");

    if (!recordsDir.exists()) {
        recordsDir.mkpath(".");
    }

    // 2. 生成安全的文件名
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("teach_record_%1.txt").arg(timestamp);
    QString filePath = recordsDir.filePath(filename);

    m_recordFile.setFileName(filePath);
    if (!m_recordFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "Failed to open record file:" << filename;
        return;
    }

    ui->addLine_groupBox->setDisabled(false);
    ui->addCircle_groupBox->setDisabled(false);

}

void MainWidget::on_lineStart_Btn_clicked()
{
    m_bIsCreatePoint = true;
    m_CurCreatePoint = LINE_START;
    //获取位置
    onSendGetLPosRequest();
}

void MainWidget::on_lineEnd_Btn_clicked()
{
    m_bIsCreatePoint = true;
    m_CurCreatePoint = LINE_END;
    //获取位置
    onSendGetLPosRequest();
}
//添加直线即添加两个点
void MainWidget::on_addLine_Btn_clicked()
{

    if(!ui->lineStart_Btn->styleSheet().contains("background") ||
            !ui->lineEnd_Btn->styleSheet().contains("background"))
    {
        return;
    }

    /*
    float segmentLength = 1;

    //计算空间中两点的距离
    QVector3D startPoint = {m_lineStart.at(0),m_lineStart.at(1),m_lineStart.at(2)};
    QVector3D endPoint = {m_lineEnd.at(0),m_lineEnd.at(1),m_lineEnd.at(2)};
    float fDistance = startPoint.distanceToPoint(endPoint);

    //将距离等分
    int segmentCount = static_cast<int>(std::ceil(fDistance / segmentLength));
    // 计算方向向量
    QVector3D direction = (endPoint - startPoint).normalized();

    QVector<QVector3D> result;

    // 生成分割点
    for (int i = 0; i <= segmentCount; ++i) {
        float currentDistance = i * segmentLength;
        if (currentDistance > fDistance) {
            currentDistance = fDistance; // 确保不超过总距离
        }
        QVector3D currentPoint = startPoint + direction * currentDistance;
        result.append(currentPoint);
    }

//    for(auto point : result)
//    {
//        qDebug() << point.x() << point.y() << point.z();
//    }

    //构建轨迹
    QString strPoint;
    for(auto point : result)
    {
        //qDebug() << point.x() << point.y() << point.z();
        strPoint.clear();
        strPoint += QString(" %1 %2 %3 %4 %5 %6").arg(point.x())
                .arg(point.y())
                .arg(point.z())
                .arg(m_lineStart.at(3))
                .arg(m_lineStart.at(4))
                .arg(m_lineStart.at(5));
        strPoint += "\r\n";
        writeRecordFile(strPoint.toUtf8());
    }
    */
    QString strPoint = QString(" %1 %2 %3 %4 %5 %6")
                               .arg(m_lineStart.at(0))
                               .arg(m_lineStart.at(1))
                                .arg(m_lineStart.at(2))
                                .arg(m_lineStart.at(3))
                                .arg(m_lineStart.at(4))
                                .arg(m_lineStart.at(5));
    writeRecordFile(strPoint.toUtf8());

    strPoint.clear();

    strPoint = QString(" %1 %2 %3 %4 %5 %6")
                               .arg(m_lineEnd.at(0))
                               .arg(m_lineEnd.at(1))
                                .arg(m_lineEnd.at(2))
                                .arg(m_lineEnd.at(3))
                                .arg(m_lineEnd.at(4))
                                .arg(m_lineEnd.at(5));
    writeRecordFile(strPoint.toUtf8());

    ui->lineEnd_Btn->setStyleSheet("");
    ui->lineStart_Btn->setStyleSheet("");

}

void MainWidget::on_newTrajectory_Btn_clicked()
{
    m_recordFile.close();
    updateFileList();
}

void MainWidget::on_circleStart_Btn_clicked()
{
    m_bIsCreatePoint = true;
    m_CurCreatePoint = CIRCLE_START;
    //获取位置
    onSendGetLPosRequest();
}

void MainWidget::on_circleCenter_Btn_clicked()
{
    m_bIsCreatePoint = true;
    m_CurCreatePoint = CIRCLE_CENTER;
    //获取位置
    onSendGetLPosRequest();
}

void MainWidget::on_circleEnd_Btn_clicked()
{
    m_bIsCreatePoint = true;
    m_CurCreatePoint = CIRCLE_END;
    //获取位置
    onSendGetLPosRequest();
}

/**
 * @brief 生成圆轨迹上的位置点
 * @param center 圆心坐标
 * @param point1 圆上第一个点
 * @param point2 圆上第二个点
 * @param numPoints 要生成的轨迹点数
 * @return QList<QVector3D> 轨迹点位置列表
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
QList<QVector3D> MainWidget::generateCirclePoints(
    const QVector3D &center,
    const QVector3D &point1,
    const QVector3D &point2,
    int numPoints)
{
    QList<QVector3D> points;

    // 计算圆平面法向量
    QVector3D v1 = point1 - center;
    QVector3D v2 = point2 - center;
    QVector3D normal = QVector3D::crossProduct(v1, v2);

    // 检查三点是否共线
    if (normal.length() < 1e-6) {
        qWarning() << "三点共线，无法确定圆平面";
        return points; // 返回空列表
    }
    normal.normalize();

    // 计算半径（使用 point1 到圆心的距离）
    float radius = v1.length();

    // 构建局部坐标系
    QVector3D u = v1.normalized(); // X轴方向
    QVector3D v = QVector3D::crossProduct(normal, u).normalized(); // Y轴方向

    // 生成轨迹点
    for (int i = 0; i < numPoints; ++i) {
        float theta = 2 * M_PI * i / numPoints;
        float cosTheta = cos(theta);
        float sinTheta = sin(theta);

        // 计算位置
        QVector3D position = center + radius * (cosTheta * u + sinTheta * v);
        points.append(position);
    }

    return points;
}


void MainWidget::on_createCircleTrajectory_Btn_clicked()
{
    if(!ui->circleCenter_Btn->styleSheet().contains("background") ||
        !ui->circleEnd_Btn->styleSheet().contains("background") ||
            !ui->circleStart_Btn->styleSheet().contains("background"))
    {
        return;
    }

    QVector3D circleCenter = {m_circleCenter.at(0),m_circleCenter.at(1),m_circleCenter.at(2)};
    QVector3D circleStart = {m_circleStart.at(0),m_circleStart.at(1),m_circleStart.at(2)};
    QVector3D circleEmd = {m_circleEnd.at(0),m_circleEnd.at(1),m_circleEnd.at(2)};

    QList<QVector3D> pointsList = generateCirclePoints(circleCenter,circleStart,circleEmd);

    QString strPoint;
    for(auto point : pointsList)
    {
        //qDebug() << point.x() << point.y() << point.z() << endl;
        strPoint.clear();
        strPoint += QString(" %1 %2 %3 %4 %5 %6").arg(point.x())
                .arg(point.y())
                .arg(point.z())
                .arg(m_circleStart.at(3))
                .arg(m_circleStart.at(4))
                .arg(m_circleStart.at(5));
        strPoint += "\r\n";
        writeRecordFile(strPoint.toUtf8());
    }

    ui->circleCenter_Btn->setStyleSheet("");
    ui->circleStart_Btn->setStyleSheet("");
    ui->circleEnd_Btn->setStyleSheet("");

}

void MainWidget::on_speedSlider_valueChanged(int value)
{
    m_fSpeed = value;
    qDebug() << "m_fSpeed = " << m_fSpeed << endl;
}

void MainWidget::on_selectMode_cbBox_currentIndexChanged(int index)
{
    QString strMode = QString::number(index + 1);
    QStringList paraList;
    paraList += strMode;
    m_serialSender->sendDatas(constructCmd(CMDMODE,paraList));
}

void MainWidget::on_setKp_Btn_clicked()
{
    QString strJoint = QString::number(ui->setPidJoint_cbBox->currentIndex() + 1);
    QString strKpValue = QString::number(ui->setKp_SpinBox->value());
    QStringList paraList = {strJoint,strKpValue};
    m_serialSender->sendDatas(constructCmd(SETKP,paraList));
    qDebug() << constructCmd(SETKP,paraList) ;
}

void MainWidget::on_setKi_Btn_clicked()
{
    QString strJoint = QString::number(ui->setPidJoint_cbBox->currentIndex() + 1);
    QString strKiValue = QString::number(ui->setKi_SpinBox->value());
    QStringList paraList = {strJoint,strKiValue};
    m_serialSender->sendDatas(constructCmd(SETKI,paraList));
}

void MainWidget::on_setKd_Btn_clicked()
{
    QString strJoint = QString::number(ui->setPidJoint_cbBox->currentIndex() + 1);
    QString strKdValue = QString::number(ui->setKd_SpinBox->value());
    QStringList paraList = {strJoint,strKdValue};
    m_serialSender->sendDatas(constructCmd(SETKD,paraList));
}
