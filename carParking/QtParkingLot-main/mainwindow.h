#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 主窗口类，负责停车场管理系统的主要逻辑和界面
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow(); 

private slots:
    void updateQueueIcons(); // 更新等待队列图标
    void on_generateLicenseButton_clicked(); // 生成随机车牌号按钮点击事件
    QString generateLicensePlate(); // 生成随机车牌号
    void generateParkingSlots(int); // 动态生成停车位
    void on_generateButton_clicked(); // 生成停车位按钮点击事件
    void clearLayout(QLayout*); // 清空布局

    void on_enterbutton_clicked();// 进车按钮点击事件
    void on_outbutton_clicked();// 出车按钮点击事件
    void on_checkbutton_clicked();// 查询信息按钮点击事件
    void checkQueueTimeout(); // 检查队列超时槽函数

protected:
    Ui::MainWindow *ui; // 主界面指针
    QTimer* queueTimer; // 队列超时检测定时器
    QTcpServer* server; // TCP服务端监听对象
    QList<QTcpSocket*> clients; // 已连接的客户端列表
    QMap<QString, QTimer*> carTimers; // 车牌号到定时器的映射，用于自动离场
};

#endif // MAINWINDOW_H
