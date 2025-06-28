#ifndef TABLE_H
#define TABLE_H

#include <stdio.h>
#include <memory>
#include "mainwindow.h"
#define COL 5 // 停车场每行车位数
#define ROW 2 // 停车场行数

// 停车场中车辆信息结构体
struct car
{
    int pos;           // 停车位编号
    QString num;       // 车牌号
    QString entime;    // 进入停车场时间
};

// 停车场表结构体
struct tableNode
{
    int length, size;              // length: 当前车辆数，size: 停车场总车位数
    std::unique_ptr<car[]> cars;   // 存储车辆的数组
};

using table = std::unique_ptr<tableNode>; // 停车场类型定义

table createTable(int); // 创建一个停车场表
int IsEmpty(const table& t); // 检测停车场是否为空
int IsFull(const table& t);  // 检测停车场是否满
void MakeEmpty(table& t);    // 将停车场表置空
void carEnter(table& t, const QString& num, const QString& etime, int pos); // 车辆进入停车场
void carOut(table& t, const QString& num); // 车辆离开停车场
int search(const table& t, const QString& num); // 查找停车场中是否有指定车牌号的车辆
float calculate(const QString& etime, const QString& outtime); // 计算停车费用
int createPos(int position[], int); // 随机分配停车位

#endif // TABLE_H
