#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QSerialPort>
#include "serialsender.h"
#include <QMap>
#include <QFile>
#include <QTimer>
#include <QQueue>
#include <QButtonGroup>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWidget; }
QT_END_NAMESPACE

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private slots:
    void onSerialError(const QString& );
    void onDataReceived(const QByteArray&);
    void onSerialOpened();
    void onSerialClosed();
    //发送获取位姿的请求 用于拖动示教
    void onSendGetLPosRequest();

    void onTeaching();

    void onPlayRecord();
    void onJointAddBtnPressed(int);
    void onJointRecudeBtnPressed(int);
    void onPosAddBtnPressed(int);
    void onPosReduceBtnPressed(int);
    void onTeachBtnReleased();
private slots:

    void on_connect_Btn_clicked();

    void on_start_Btn_clicked();

    void on_stop_Btn_clicked();

    void on_home_Btn_clicked();

    void on_dragTeach_Btn_clicked();

    void on_stopDragTeach_Btn_clicked();

    void on_disable_Btn_clicked();

    void on_reapper_Btn_clicked();

    void on_stopReappear_Btn_clicked();

    void on_getJPos_Btn_clicked();

    void on_rest_Btn_clicked();

    void on_getLPos_Btn_clicked();

    //void on_speedSlider_sliderMoved(int position);

    void on_continueReappear_Btn_clicked();

    void on_deleteRecord_Btn_clicked();

    void on_startCreateTrajectory_Btn_clicked();

    void on_lineStart_Btn_clicked();

    void on_lineEnd_Btn_clicked();

    void on_addLine_Btn_clicked();

    void on_newTrajectory_Btn_clicked();

    void on_circleStart_Btn_clicked();

    void on_circleCenter_Btn_clicked();

    void on_circleEnd_Btn_clicked();

    void on_createCircleTrajectory_Btn_clicked();

    void on_speedSlider_valueChanged(int value);

    void on_selectMode_cbBox_currentIndexChanged(int index);

    void on_setKp_Btn_clicked();

    void on_setKi_Btn_clicked();

    void on_setKd_Btn_clicked();

private:
    QByteArray constructCmd(CMD_TYPE cmd, const QStringList &paraList = QStringList());
    void initMap();

    void scanSerialPort();

    void writeRecordFile(const QByteArray& data);

    void readRecordFile(const QString& fileName);

    void updateFileList();

    //示教功能需要先获取当前位置或者关节角
    void sendTeachGetRequest();

    void keyPressEvent(QKeyEvent *event);
    void keyReleaseEvent(QKeyEvent *event);

    void setPointPos(const QStringList&);

    QList<QVector3D> generateCirclePoints(
        const QVector3D &center,
        const QVector3D &point1,
        const QVector3D &point2,
        int numPoints = 100);

private:
    Ui::MainWidget *ui;
    SerialSender *m_serialSender;
    //指令映射
    QMap<CMD_TYPE,QString> m_CmdMap;

    QTimer* m_timer;
    QTimer* m_runTimer;
    QTimer* m_teachTimer;

//    float m_currentJoint[6] = {0.00, -75.00, 180.00, 0.00, 0.00, 0.00};
//    float m_currentPos[6] = {93.37, 0.00, 165, -180.00, 75.00, -180.00};
    QStringList m_CurAngleList;
    QStringList m_CurPosList;

    typedef enum MoveType{
        MOVE_JOINT,
        MOVE_LINE
    }MOVETYPE;

    typedef enum OperateType
    {
        ADD_VALUE,
        REDUCE_VALUE
    }OPERATETYPE;

    bool m_bIsRecording = false;   //记录中
    bool m_bIsReappearing = false; //回放中
    bool m_bIsTeaching = false;    //示教中
    bool m_bIsCreatePoint = false;  //创建点
    MoveType m_curTeachType;       //当前示教类型
    OperateType m_curOperateType;  //当前操作类型
    int m_nCurOpJoint = 0;
    int m_nCurOpPos = 0;
    int m_nReadLines = 0;
    float m_fSpeed = 100;
    QFile m_recordFile;

    QQueue<QByteArray> m_cmdQueue;

    QButtonGroup* m_jointAddBtnGroup;
    QButtonGroup* m_jointReduceBtnGroup;
    QButtonGroup* m_posAddBtnGroup;
    QButtonGroup* m_posReduceBtnGroup;


private:
    QVector<float> m_lineStart;
    QVector<float> m_lineEnd;
    QVector<float> m_circleStart;
    QVector<float> m_circleCenter;
    QVector<float> m_circleEnd;

    typedef enum CreatePoint{
        LINE_START,
        LINE_END,
        CIRCLE_START,
        CIRCLE_CENTER,
        CIRCLE_END
    }CREATEPOINT;

    CreatePoint m_CurCreatePoint;

};
#endif // MAINWIDGET_H
