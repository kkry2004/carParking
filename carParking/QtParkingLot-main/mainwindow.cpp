#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "queue.h"
#include "table.h"
#include "queuefunction.cpp"
#include "tablefunction.cpp"
#include <QTime>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QTextStream>

// 2025.6.16版本
// 为什么不用多线程：锁粒度大，哈希分片要负载均衡，cas操作麻烦，日志需要双缓冲区

// 用智能指针管理table和queue
// t：停车场表，q：等待队列
// message：用于显示信息，waits：等待队列车牌号数组
// position：停车位占用情况，queueposition：队列占用情况，p：临时变量
// carTimers：每辆车的自动离场定时器

table t;
queue q = createQueue();
QString message, waits[Max];
int position[COL*ROW] = {0}, queueposition[Max] = {0}, p;
QMap<QString, QTimer*> carTimers;

// 日志记录函数，将信息写入 debuglog.txt
void myLog(const QString &msg) {
    QFile file("D:/QtParkingLot-main/debuglog.txt");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << msg << "\n";
        file.close();
    }
}

// 主窗口构造函数，初始化界面、定时器、TCP服务端等
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qDebug() << "MainWindow 构造开始";
    ui->setupUi(this);
    qDebug() << "setupUi 完成";
    queueTimer = new QTimer(this);
    // connect(发送者, &发送者::信号, 接收者, &接收者::槽函数);
    // connect(queueTimer, &QTimer::timeout, this, &MainWindow::checkQueueTimeout);
    connect(queueTimer, &QTimer::timeout, this, [=](){
        checkQueueTimeout();
    });
    queueTimer->start(1000); // 每秒检查一次队列超时

    // 初始化服务端监听
    server = new QTcpServer(this);
    qDebug() << "QTcpServer 创建完成";
    // 第一遍connect：监听服务器是否有新客户端连接。
    // 第二遍connect：为每个新连接的客户端socket设置数据和断开事件的处理。
    connect(server, &QTcpServer::newConnection, this, [=](){
        while (server->hasPendingConnections()) {
            QTcpSocket* client = server->nextPendingConnection();
            clients.append(client);
            // 处理客户端消息
            connect(client, &QTcpSocket::readyRead, this, [=](){
                // 当客户端有数据可读时，循环读取每一行数据
                while (client->bytesAvailable()) {
                    // 读取客户端发来的一行数据
                    QByteArray data = client->readLine();
                    // 将读取到的数据转换为QString，并去除首尾空白字符
                    QString msg = QString::fromUtf8(data).trimmed();
                    // 判断消息类型
                    if (msg.startsWith("IN:")) {
                        // 如果消息以IN:开头，表示有车辆要进入停车场
                        // 提取车牌号
                        QString plate = msg.section(':', 1, 1);
                        // 设置车牌号到输入框
                        ui->inputnum->setText(plate);
                        // 自动调用进车按钮的处理函数，实现车辆入场
                        on_enterbutton_clicked();
                    } else if (msg.startsWith("OUT:")) {
                        // 如果消息以OUT:开头，表示有车辆要离开停车场
                        // 提取车牌号
                        QString plate = msg.section(':', 1, 1);
                        // 设置车牌号到输入框
                        ui->inputnum->setText(plate);
                        // 自动调用出车按钮的处理函数，实现车辆出场
                        on_outbutton_clicked();
                    }
                }
            });
            // 客户端断开连接处理
            connect(client, &QTcpSocket::disconnected, this, [=](){
                clients.removeAll(client);
                client->deleteLater();
            });
        }
        int clientCountAfter = clients.size();
        myLog(QString("新客户端连接建立，当前客户端数量：%1").arg(clientCountAfter));
        qDebug() << QString("新客户端连接建立，当前客户端数量：%1").arg(clientCountAfter);
    });
    server->listen(QHostAddress::Any, 12345); // 监听端口
    qDebug() << "server listen:" << server->isListening();
}

// 析构函数，释放UI资源
MainWindow::~MainWindow()
{
    delete ui;
}

// 生成随机车牌号
QString MainWindow::generateLicensePlate() {
    // 省份代码
    QStringList provinces = { "京", "沪", "川", "渝", "鄂", "粤", "鲁", "晋", "蒙", "桂", "甘", "贵", "黑", "吉", "辽", "闽", "赣", "闽", "青", "琼", "藏", "新", "苏" };
    QString province = provinces[QRandomGenerator::global()->bounded(provinces.size())]; // 随机选择一个省份
    QString letter = QString(QChar('A' + QRandomGenerator::global()->bounded(10))); // 随机选择一个大写字母
    QString numberPart;
    numberPart.append(QString::number(QRandomGenerator::global()->bounded(10)));
    // 可扩展为五位字符组合
    return province + letter + numberPart; // 返回完整车牌号
}

// 生成车牌号按钮点击事件
void MainWindow::on_generateLicenseButton_clicked() {
    QString licensePlate = generateLicensePlate(); // 调用生成车牌号函数
    ui->inputnum->setText(licensePlate); // 将车牌号填充到 inputnum 输入框
}

// 动态生成停车位
void MainWindow::generateParkingSlots(int numSlots)
{
    // 1. 清空当前布局，防止重复添加
    QLayout *currentLayout = ui->parkingArea->layout();
    if (currentLayout != nullptr) {
        QLayoutItem *item;
        while ((item = currentLayout->takeAt(0)) != nullptr) {
            delete item->widget(); // 删除布局中的所有控件
            delete item; // 删除布局项
        }
        delete currentLayout; // 删除旧布局
    }
    // 2. 创建一个新的 QGridLayout 布局
    QGridLayout *parkingLayout = new QGridLayout();
    // 3. 动态生成车位
    int rows = 5;  // 每行5个车位
    int cols = (numSlots + rows - 1) / rows;  // 计算列数，确保合理分布车位
    t = createTable(numSlots);
    for (int i = 0; i < numSlots; ++i) {
        // 创建一个车位标签 (QLabel)
        QLabel *slotLabel = new QLabel("parkinglot"+ QString::number(i + 1), this);
        // 为车位设置图片 (可以是空车位图)
        slotLabel->setPixmap(QPixmap("D:/QtParkingLot-main/parkinglot.png"));
        slotLabel->setAlignment(Qt::AlignCenter); // 设置图片居中
        // 设置车位标签的大小
        slotLabel->setFixedSize(50, 50); // 设置每个车位的尺寸
        // 4. 将车位标签添加到 QGridLayout 中
        parkingLayout->addWidget(slotLabel, i / cols, i % cols); // 按行列布局
    }
    // 5. 将 QGridLayout 布局应用到停车区域
    ui->parkingArea->setLayout(parkingLayout);
}

// 生成停车位按钮点击事件
void MainWindow::on_generateButton_clicked()
{
    // 获取用户输入的车位数量
    int numSlots = ui->inputParkingSlots->text().toInt();
    // 确保输入为正整数
    if (numSlots > 0) {
        // 调用函数生成车位
        generateParkingSlots(numSlots);
    }
}

// 清空布局，释放控件
void MainWindow::clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        delete item->widget(); // 删除控件
        delete item; // 删除布局项
    }
}

// 进车按钮点击事件，处理车辆进入停车场或等待队列的逻辑
void MainWindow::on_enterbutton_clicked()
{
    QString num = ui->inputnum->text(),etime;
    QTime current_time = QTime::currentTime();
    etime = current_time.toString("hh:mm:ss");//获取当前时间

    //如果输入为空且队列为空
    if(num == "" && q->size == 0){
        message.append("请输入车牌号！\n");
    }
    //第一次输入车牌号，进停车场
    else if(t->length == 0){
        carEnter(t,num,etime,createPos(position,t->size));
        int index = search(t, num);
        int position = t->cars[index].pos;
        message.append(etime + ": " + "车牌号为"+num + "进入停车场\n"+"停在了车位："+QString::number(position)+"\n");
        // 启动自动离开定时器
        int randomSeconds = QRandomGenerator::global()->bounded(10, 31); // 10~30秒
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [=]() {
            ui->inputnum->setText(num);
            on_outbutton_clicked();
            carTimers.remove(num);
            timer->deleteLater();
        });
        carTimers[num] = timer;
        timer->start(randomSeconds * 1000);
    }
    //当停车场为满时
    else if(t->length == t->size){
        int index = search(t,num);//输入车牌号，查找停车场中是否有这辆车
        if(index == -1){
            if(IsFull(q))//若队列为满，不进行入队并提示
                message.append("等待队列已满！\n");
            else {
                if(q->size == 0){//第一次往队列中进车
                    message.append("停车场已满！\n");
                    enter(q,num,etime);
                    message.append(etime + ": " + "车牌号为"+ num + "进入等待队列\n");
                }
                else{
                    int index = search(q,num);//查找队列中是否有这辆车
                    if(index == -1){//若没有，则入队列
                        enter(q,num,etime);
                        message.append(etime + ": " + "车牌号为"+ num + "进入等待队列\n");
                    }
                    else{//若有，则重新输入
                        message.append("该车辆已存在等候区！\n");
                    }
                }
            }
        }
        else
            message.append("该车辆已存在停车场！请重新输入。\n");
    }
    else if(t->length >= 0 && q->size > 0){
        //停车场有空位且队列不为空（防御性编程，健壮性）
        //车牌不为空时，队列首位车入停车场，输入的车入队列
        if(t->length < t->size){
            if(num == ""){
                message.append("请输入车牌号！\n");
            }
            else{
                int index = search(t,num);//输入车牌号，查找停车场中是否有该车
                if(index == -1){
                    if(IsFull(q))
                        message.append("等待队列已满！\n");
                    else {
                        if(q->size == 0){
                            message.append("停车场已满！\n");
                            enter(q,num,etime);
                            message.append(etime + ": " + "车牌号为"+ num + "进入等待队列\n");
                        }
                        else{
                            int index = search(q,num);
                            if(index == -1){
                                enter(q,num,etime);
                                message.append(etime + ": " + "车牌号为"+ num + "进入等待队列\n");
                            }
                            else{
                                message.append("该车辆已存在等待区！请重新输入。\n");
                            }
                        }
                    }
                }
                else{
                    message.append("该车辆已存在停车场！请重新输入。\n");
                }
            }
        }

        int index = search(t,num);
        if(index >= 0){
            message.append("该车辆已存在！请重新输入。\n");
        }
        else{
            QString tempnum = q->wait[q->front].num;
            //出车后，复原队列车位图标
            carEnter(t,tempnum,etime,createPos(position,t->size));
            switch (q->size) {
            case 1:
                ui->queue1->setText("1");
                break;
            case 2:
                ui->queue2->setText("2");
                break;
            case 3:
                ui->queue3->setText("3");
                break;
            case 4:
                ui->queue4->setText("4");
                break;
            case 5:
                ui->queue5->setText("5");
                break;
            }
            DeQueue(q);//队首入停车场，出队列
            message.append(etime + ": " + tempnum + "进入停车场\n");
            // 启动自动离开定时器
            int randomSeconds = QRandomGenerator::global()->bounded(10, 31); // 10~30秒
            QTimer* timer = new QTimer(this);
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, [=]() {
                ui->inputnum->setText(tempnum);
                on_outbutton_clicked();
                carTimers.remove(tempnum);
                timer->deleteLater();
            });
            carTimers[tempnum] = timer;
            timer->start(randomSeconds * 1000);
        }
    }
    //若停车场不满，获取输入车牌号并进行判断，若无该车，则入停车场
    else if(t->length > 0){
        int index = search(t,num);
        if(index >= 0){
            message.append("该车辆已存在！请重新输入。\n");
        }
        else{
            carEnter(t,num,etime,createPos(position,t->size));
            int index = search(t, num);
            int position = t->cars[index].pos;
            message.append(etime + ": " + "车牌号为"+ num + "进入停车场\n"+"停在了车位："+QString::number(position)+"\n");
            // 启动自动离开定时器
            int randomSeconds = QRandomGenerator::global()->bounded(10, 31); // 10~30秒
            QTimer* timer = new QTimer(this);
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, [=]() {
                ui->inputnum->setText(num);
                on_outbutton_clicked();
                carTimers.remove(num);
                timer->deleteLater();
            });
            carTimers[num] = timer;
            timer->start(randomSeconds * 1000);
        }
    }
    // 更新停车场车位图标
    for (int i = 0; i < t->length; i++) {
        int pos = t->cars[i].pos - 1; // 车位位置，从0开始
        if (pos >= 0 && pos < t->size) { // 确保位置在有效范围内
            QString imagePath = "D:/QtParkingLot-main/car.png";
            // 从 parkingArea 中查找对应的车位标签
            // 把停车区里所有的车位标签（QLabel）都找出来，放到一个列表里
            QList<QLabel*> slotLabels = ui->parkingArea->findChildren<QLabel*>();
            // 防止越界
            if (slotLabels.size() > pos) {
                // 取出第pos个车位标签
                QLabel* slotLabel = slotLabels.at(pos);
                slotLabel->setPixmap(QPixmap(imagePath)); // 设置车位为占用
            }
        }
    }
    // 更新等待队列的图标
    for (int i = 0; i < q->size; i++) {
        int queueIndex = i + 1;
        QString imagePath = "D:/QtParkingLot-main/car.png";
        QLabel* queueLabel = findChild<QLabel*>(QString("queue%1").arg(queueIndex));
        if (queueLabel) {
            queueLabel->setPixmap(QPixmap(imagePath)); // 设置队列的车辆图标
        }
    }
    //在控制台输出信息
    ui->showmessage->setText(message);
}

// 出车按钮点击事件，处理车辆离开停车场的逻辑
void MainWindow::on_outbutton_clicked()
{
    QTime current_time = QTime::currentTime();
    QString outtime = current_time.toString("hh:mm:ss"), num = ui->inputnum->text(); // 获取出停车场时间
    int index = search(t, num);

    if (t->length == 0) {
        message.append("停车场为空！\n");
    }
    else if (index > -1) {
        // 释放车位
        position[t->cars[index].pos - 1] = 0;
        // 查找并重置停车位图标
        int pos = t->cars[index].pos - 1; // 获取车位的索引
        if (pos >= 0 && pos < t->size) {
            QString imagePath ="D:/QtParkingLot-main/parkinglot.png"; // 空车位的图片路径
            // 从 parkingArea 中查找对应的车位标签
            QList<QLabel*> slotLabels = ui->parkingArea->findChildren<QLabel*>();
            if (slotLabels.size() > pos) {
                QLabel* slotLabel = slotLabels.at(pos);
                slotLabel->setPixmap(QPixmap(imagePath)); // 设置车位为空
            }
        }
        // 车辆出停车场
        carOut(t, num);
        // 移除并销毁定时器
        if (carTimers.contains(num)) {
            carTimers[num]->stop();
            carTimers[num]->deleteLater();
            carTimers.remove(num);
        }
        // 新增：通知所有客户端帮助离场
        for (QTcpSocket* client : clients) {
            if (client && client->state() == QAbstractSocket::ConnectedState) {
                client->write(QString("HELP_OUT:%1\n").arg(num).toUtf8());
            }
        }
        // 计算停车费用
        float cost = calculate(t->cars[index].entime, outtime);
        QString qcost = QString::asprintf("%.2f", cost);
        message.append(outtime + ": " + num + "退出停车场\n");
        message.append("收费: " + qcost + "元\n");
        // 如果队列中有车，首辆车进入停车场
        if (!IsEmpty(q)) {
            QString tempnum = q->wait[q->front].num;
            carEnter(t, tempnum, outtime, createPos(position, t->size));
            message.append(outtime + ": " + tempnum + "从等待队列进入停车场\n");
            // 找到该车停放的位置
            int newPos = t->cars[search(t, tempnum)].pos - 1; // 获取新停放位置索引
            if (newPos >= 0 && newPos < t->size) {
                QString imagePath = "D:/QtParkingLot-main/car.png";
                QList<QLabel*> slotLabels = ui->parkingArea->findChildren<QLabel*>();
                if (slotLabels.size() > newPos) {
                    QLabel* slotLabel = slotLabels.at(newPos);
                    slotLabel->setPixmap(QPixmap(imagePath)); // 更新车位图标为已占用
                }
            }
            // 出队列并更新队列图标
            DeQueue(q);
            updateQueueIcons();
            // 启动自动离开定时器
            int randomSeconds = QRandomGenerator::global()->bounded(10, 31); // 10~30秒
            QTimer* timer = new QTimer(this);
            timer->setSingleShot(true);
            connect(timer, &QTimer::timeout, this, [=]() {
                ui->inputnum->setText(tempnum);
                on_outbutton_clicked();
                carTimers.remove(tempnum);
                timer->deleteLater();
            });
            carTimers[tempnum] = timer;
            timer->start(randomSeconds * 1000);
    }
    }
    ui->showmessage->setText(message);
}

// 更新等待队列的图标显示
void MainWindow::updateQueueIcons()
{
    // 清空所有队列图标
    for (int i = 1; i <= Max; i++) {
        QLabel* queueLabel = findChild<QLabel*>(QString("queue%1").arg(i));
        if (queueLabel) {
            QString emptyPath = "D:/QtParkingLot-main/parkinglot.png";
            queueLabel->setPixmap(QPixmap(emptyPath));  // 队列图标清空
        }
    }
    // 重新设置队列图标
    for (int i = 0; i < q->size; i++) {
        int queueIndex = i + 1;
        QString imagePath = "D:/QtParkingLot-main/car.png";
        QLabel* queueLabel = findChild<QLabel*>(QString("queue%1").arg(queueIndex));
        if (queueLabel) {
            queueLabel->setPixmap(QPixmap(imagePath));  // 设置队列中的车辆图标
        }
    }
}

// 查询按钮点击事件，显示停车场和队列剩余情况及车辆信息
void MainWindow::on_checkbutton_clicked()
{
    QString qnum = QString::number(Max - q->size),tnum = QString::number(t->size - t->length);//计算停车场剩余位置和队列剩余位置
    message.append("停车场剩余" + tnum + "位," + "等候队列剩余" + qnum + "位。\n");
    QString num = ui->inputnum->text();
    if(num == ""){
        message.append("请输入车牌号！\n");
    }else if(t->length == 0){
        message.append("停车场为空！\n");
    }else if(t->length > 0){
        int index = search(t,num);
        if(index >= 0){
            QString pos = QString::number(t->cars[index].pos);
            message.append("车牌号：" + num + "停车位：" + pos + "进入时间：" + t->cars[index].entime + "\n");
        }else{
            message.append("车辆不存在！\n");
        }
    }
    ui->showmessage->setText(message);
}

// 检查等待队列超时，超时则自动离开
void MainWindow::checkQueueTimeout()
{
    if(q->size == 0) return;
    QTime now = QTime::currentTime();
    // 循环检查队首，只要超时就一直处理
    while (q->size > 0) {
        QString enterTime = q->wait[q->front].enterTime;
        QTime carTime = QTime::fromString(enterTime, "hh:mm:ss");
        if (carTime.isValid() && carTime.secsTo(now) >= 10) {
            QString carNum = q->wait[q->front].num;
            message.append(now.toString("hh:mm:ss") + ": " + "车牌号为" + carNum + "在队列中等待超过10秒，已自动离开\n");
            DeQueue(q);
            updateQueueIcons();
        } else {
            // 队首没超时，后面的车肯定也没超时，直接退出循环
            break;
        }
    }
    ui->showmessage->setText(message);
}
