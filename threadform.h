#ifndef THREADFORM_H
#define THREADFORM_H

#include <QWidget>
#include <QNetworkReply>
#include <QByteArray>
#include <QString>

namespace Ui {
class ThreadForm;
}

enum PostType { Thread, Reply };

class ThreadForm : public QWidget
{
    Q_OBJECT

public:
    explicit ThreadForm(PostType type = Reply, QWidget *parent = 0);
    ~ThreadForm();
    void setText(QString text);
    void setImage(QByteArray img);
    //void getImage(QNetworkAccessManager *manager, QString *img);
    void load(QJsonObject &p);
    //void setThread(QString threadName);
    PostType type;
    QString threadNum;
    QString board;
    QNetworkReply *reply;
    QNetworkReply *replyImage;
    QMetaObject::Connection connectionPost;
    QMetaObject::Connection connectionImage;
    QString htmlParse(QString &html);
    //void setImage(QString text);

signals:
    void loadThreadTab(ThreadForm*, QJsonArray&);
    void loadThread(ThreadForm*,QString&,QString&);
private:
    Ui::ThreadForm *ui;

public slots:
    void getImageFinished();
    void imageClicked();
    void getThreadFinished();
};

#endif // THREADFORM_H
