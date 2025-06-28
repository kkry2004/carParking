#include <stdio.h>
#include "queue.h"
#include <stdlib.h>

// 判断队列是否为空
int IsEmpty(const queue& q){
   return q == nullptr || q->size == 0;
}

// 判断队列是否已满
int IsFull(const queue& q){
    return q != nullptr && q->size == q->capacity;
}

// 队列初始化/置空
void MakeEmpty(queue& q){
    if (q) {
        q->rear = 0;
        q->front = 0;
        q->size = 0;
    }
}

// 创建一个队列，分配空间并初始化
queue createQueue(){
    auto q = std::make_unique<queueNode>();
    q->capacity = Max;
    q->wait = std::make_unique<qcar[]>(q->capacity);
    MakeEmpty(q);
    return q;
}

// 车辆进队列
// 参数：q 队列，num 车牌号，time 进入时间
void enter(queue& q, const QString& num, const QString& time){
    if(IsFull(q))
       printf("Full queue\n");
    else{
       q->wait[q->rear].num = num;
       q->wait[q->rear].enterTime = time;
       q->size++;
       if(++q->rear == q->capacity)
          q->rear = 0; // 循环队列处理
    }
}

// 获取队首元素
qcar Front(const queue& q){
    return q->wait[q->front];
}

// 队首出队
void DeQueue(queue& q){
    if(IsEmpty(q))
        printf("empty queue!\n");
    else{
        q->size--;
        q->front++;
        if(q->front == q->capacity)
            q->front = 0; // 循环队列处理
    }
}

// 返回队首并出队
qcar FrontAndDeQueue(queue& q){
    if(!IsEmpty(q))
        return q->wait[q->front++];
    else{
        printf("empty queue!\n");
        return qcar{};
    }
}

// 查找队列中是否有指定车牌号的车辆
// 返回下标，未找到返回-1
int search(const queue& q, const QString& num){
    int sign = -1;
    if(IsEmpty(q))
      printf("empty table!\n");
    else{
        int n = q->size;
        for(int i = 0; i < n; i++){
            if(q->wait[i].num == num){
                printf("found it!!!\n");
                sign = i;
                break;
            }
        }
    }
    return sign;
}
