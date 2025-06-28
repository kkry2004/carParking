#include <QCoreApplication>
#include <QTcpSocket>
#include <QTimer>
#include <QRandomGenerator>
#include <QTextStream>
#include <QSet>
#include <QDebug>

QString generateLicensePlate() {
    QStringList provinces = { "京", "沪", "川", "渝", "鄂", "粤", "鲁", "晋", "蒙", "桂", "甘", "贵", "黑", "吉", "辽", "闽", "赣", "闽", "青", "琼", "藏", "新", "苏" };
    QString province = provinces[QRandomGenerator::global()->bounded(provinces.size())];
    QString letter = QString(QChar('A' + QRandomGenerator::global()->bounded(10)));
    QString numberPart;
    numberPart.append(QString::number(QRandomGenerator::global()->bounded(10)));
    return province + letter + numberPart;
}

class Client : public QObject {
    Q_OBJECT
public:
    Client(QObject* parent = nullptr) : QObject(parent) {
        connect(&socket, &QTcpSocket::connected, this, &Client::onConnected);
        connect(&socket, &QTcpSocket::readyRead, this, &Client::onReadyRead);
        connect(&timer, &QTimer::timeout, this, &Client::onTimeout);
    }

    void start(const QString& host, quint16 port) {
        socket.connectToHost(host, port);
    }

public slots:
    void onConnected() {
        qDebug() << "已连接到服务器";
        timer.start(2000); // 每2秒自动进出车
    }

    void onTimeout() {
        // 随机决定进车还是离开车
        bool isEnter = (QRandomGenerator::global()->bounded(3) != 0);
        QString plate;
        if (isEnter || carSet.isEmpty()) {
            // 进车
            plate = generateLicensePlate();
            carSet.insert(plate);
            QString msg = "IN:" + plate + "\n";
            socket.write(msg.toUtf8());
            socket.flush();
            qDebug() << "[自动] 进车:" << plate;
        } else {
            // 离开车
            int idx = QRandomGenerator::global()->bounded(carSet.size());
            auto it = carSet.begin();
            std::advance(it, idx);
            plate = *it;
            QString msg = "OUT:" + plate + "\n";
            socket.write(msg.toUtf8());
            socket.flush();
            carSet.remove(plate);
            qDebug() << "[自动] 离开:" << plate;
        }
    }

    void onReadyRead() {
        while (socket.bytesAvailable()) {
            QByteArray data = socket.readLine();
            QString msg = QString::fromUtf8(data).trimmed();
            if (msg.startsWith("HELP_OUT:")) {
                QString plate = msg.section(':', 1, 1);
                if (carSet.contains(plate)) {
                    carSet.remove(plate);
                    qDebug() << "[服务端帮助离场] 车牌:" << plate;
                } else {
                    qDebug() << "[服务端帮助离场] 车牌:" << plate << "(本地无记录)";
                }
            } else {
                qDebug() << "[收到服务端消息]:" << msg;
            }
        }
    }

private:
    QTcpSocket socket;
    QTimer timer;
    QSet<QString> carSet; // 记录已进场的车牌
};

#include "main.moc"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Client client;
    client.start("127.0.0.1", 12345); // 服务器IP和端口
    return a.exec();
}
