#include <stdio.h>
#include "table.h"
#include <stdlib.h>
#include <QCoreApplication>
#include <iostream>
#include <QDateTime>

// 判断停车场是否为空
int IsEmpty(const table& t){
    return t == nullptr || t->length == 0;
}

// 判断停车场是否已满
int IsFull(const table& t){
    return t != nullptr && t->length == t->size;
}

// 停车场置空
void MakeEmpty(table& t){
    if (t) t->length = 0;
}

// 创建一个停车场表，分配空间并初始化
// 参数：num 停车位总数
// 返回：新建的停车场表
table createTable(int num){
    auto t = std::make_unique<tableNode>();
    t->size = num;
    t->cars = std::make_unique<car[]>(t->size);
    MakeEmpty(t);
    return t;
}

// 车辆进入停车场
// 参数：t 停车场表，num 车牌号，etime 进入时间，pos 车位编号
void carEnter(table& t, const QString& num, const QString& etime, int pos){
    if(IsFull(t))
       printf("Parkinglot is Full!\n");
    else{
        t->cars[t->length].num = num;
        t->cars[t->length].entime = etime;
        t->cars[t->length].pos = pos;
        t->length++;
    }
}

// 车辆离开停车场
// 参数：t 停车场表，num 车牌号
void carOut(table& t, const QString& num){
    int index = search(t, num);
    if(index == -1)
       printf("not found!!!\n");
    else{
        for(int i = index; i < t->length - 1; i++){
            t->cars[i] = t->cars[i+1];
        }
        t->length--;
    }
}

// 查找停车场中是否有指定车牌号的车辆
// 返回下标，未找到返回-1
int search(const table& t, const QString& num){
    int sign = -1;
    if(IsEmpty(t))
      printf("empty table!\n");
    else{
        int n = t->length;
        for(int i = 0; i < n; i++){
            if(t->cars[i].num == num){
                printf("found it!!!\n");
                sign = i;
                break;
            }
        }
    }
    return sign;
}

// 计算停车费用
// 参数：etime 进入时间，outtime 离开时间
// 返回：费用（元）
float calculate(const QString& etime, const QString& outtime){
    QDateTime timeen, timeout;
    timeen = QDateTime::fromString(etime, "hh:mm:ss");
    timeout = QDateTime::fromString(outtime, "hh:mm:ss");
    int second = timeen.secsTo(timeout);
    if(second <= 5)
        return 0;
    else if(second > 5 && second <= 20)
        return 2;
    else if(second > 20 && second <= 60)
        return 5;
    else if(second > 60)
        return 10;
    return 0;
}

// 随机分配停车位
// 参数：position 停车位占用数组，maxnum 停车位总数
// 返回：分配到的车位编号
int createPos(int position[], int maxnum){
    int max = maxnum, pos = rand()%max + 1;
    if(position[pos-1] == 0){
        position[pos-1] = 1;
        return pos;
    }
    else{
        int i;
        for (i = 0; i < max; i++)
        {
            if(position[i] == 0){
                position[i] = 1;
                return i+1;
            }
        }
    }
    return -1;
}
