#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <memory>
#include "mainwindow.h"
#define Max 5 // 等待队列最大容量

// 队列中车辆信息结构体
struct qcar
{
    int pos;           // 停车位编号
    QString num;       // 车牌号
    QString enterTime; // 进入队列的时间
};

// 循环队列结构体
struct queueNode{
    int capacity;                // 队列容量
    int front;                   // 队首指针
    int rear;                    // 队尾指针
    int size;                    // 当前队列元素个数
    std::unique_ptr<qcar[]> wait; // 存储车辆的数组
};

using queue = std::unique_ptr<queueNode>; // 队列类型定义

int IsEmpty(const queue& q); // 检测队列是否为空
int IsFull(const queue& q);  // 检测队列是否满
queue createQueue();         // 创建一个队列
void MakeEmpty(queue& q);    // 将队列置空
void enter(queue& q, const QString& num, const QString& time); // 车辆进队列
qcar Front(const queue& q);  // 返回队首元素
void DeQueue(queue& q);      // 队首出队
qcar FrontAndDeQueue(queue& q); // 返回队首并出队
int search(const queue& q, const QString& num); // 查找队列中是否有指定车牌号的车辆

#endif // QUEUE_H
