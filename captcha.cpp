#include "captcha.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDebug>

//TODO: test anti-captcha support

Captcha::Captcha()
{
	ec['7']='0';
	ec['8']='1';
	ec['9']='2';
	ec['4']='3';
	ec['5']='4';
	ec['6']='5';
	ec['1']='6';
	ec['2']='7';
	ec['3']='8';

	//connect(timer,&QTimer::timeout,this,&Captcha::timeOut);
}

QString Captcha::easyCaptcha(QString answer){
	int len = answer.length();
	for(int i=0;i<len;i++){
		answer.replace(i,1,ec.value(answer.at(i)));
	}
	return answer;
}

//TODO use the CaptchaLinks object from api instead of locally defined variables
void Captcha::startUp(Chan *api){
	CaptchaLinks links = api->captchaLinks();
	siteKey = links.siteKey;
	server = links.server;
	lang = QString(links.lang);
	urlChallenge = links.challengeURL;
	urlImageBase = links.imageBaseURL;
	QSettings settings(QSettings::IniFormat,QSettings::UserScope,"qtchan","qtchan");
	if(settings.value("antiCaptcha/enable",false).toBool() == false){
		requestChallenge = QNetworkRequest(QUrl(urlChallenge));
		requestChallenge.setRawHeader(QByteArray("Referer"), QByteArray(links.refererURL.toUtf8()));
	}
	else{
		antiKey = settings.value("antiCaptcha/key","").toString();
		antiType = settings.value("antiCaptcha/type","NoCaptchaTakeProxyless").toString();
		antiMakeUrl = settings.value("antiCaptcha/url","https://api.anti-captcha.com/createTask").toString();
		antiCaptchaInfo =
					"{ "
					"\"clientKey\": \"" + antiKey + "\", "
					"\"task\": {"
						"\"type\": \"" + antiType + "\", "
						"\"websiteURL\": \"" + urlChallenge + "\", "
						"\"websiteKey\": \"" + siteKey + "\""
					" }";
		requestChallenge = QNetworkRequest(QUrl(antiMakeUrl));
		requestChallenge.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
		requestAntiSolution = QNetworkRequest(QUrl(antiGetUrl));
		requestAntiSolution.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
	}
}

Captcha::~Captcha(){
	disconnect(getConnection);
	disconnect(antiConnection);
	disconnect(anti2Connection);
	disconnect(imageConnection);
}

void Captcha::getCaptcha(){
	if(loading || loaded) return;
	loading = true;
	emit questionInfo(loadingString);
	replyChallenge = NULL;
	QSettings settings(QSettings::IniFormat,QSettings::UserScope,"qtchan","qtchan");
	if(settings.value("antiCaptcha/enable",false).toBool() == true){
		antiMake();
	}
	qDebug() << "getting captcha from " + urlChallenge;
	replyChallenge = nc.captchaManager->get(requestChallenge);
	getConnection = QObject::connect(replyChallenge,&QNetworkReply::finished,this,&Captcha::loadCaptcha,Qt::UniqueConnection);
}

void Captcha::antiMake(){
	replyChallenge = nc.captchaManager->post(requestAntiCaptcha,antiCaptchaInfo.toUtf8());
	antiConnection = QObject::connect(replyChallenge,&QNetworkReply::finished,this,&Captcha::antiMade,Qt::UniqueConnection);
}

void Captcha::antiMade(){
	if(!replyChallenge){
		emit fail();
		return;
	}
	else if(replyChallenge->error()) {
		qDebug().noquote() << "loading antiCaptcha error:" << replyChallenge->errorString();
		replyChallenge->deleteLater();
		return;
	}
	else{
		replyChallenge->deleteLater();
		QByteArray reply = replyChallenge->readAll();
		QJsonObject json = QJsonDocument::fromBinaryData(reply).object();
		qDebug() << json;
		if(json.value("errorId").toString() == QString('0')){
			antiTaskID = json.value("taskId").toString();
			antiGetInfo =	"{ "
							"\"clientKey\": \"" + antiKey + "\", "
							"\"taskId\": \"" + antiTaskID + "\""
						" }";
			antiFinish();
		}
		else{
			qDebug() << json.value("errorDescription").toString();
		}
	}
}

void Captcha::antiFinish(){
	QTimer::singleShot(10000,[=]{
		if(!antiGetInfo.isEmpty()){
			replyImage = nc.captchaManager->post(requestAntiSolution,antiGetInfo.toUtf8());
			anti2Connection = connect(replyImage,&QNetworkReply::finished,this,&Captcha::antiFinished,Qt::UniqueConnection);
		}
	});
}

void Captcha::antiFinished(){
	if(!replyImage){
		emit fail();
		return;
	}
	else if(replyImage->error()) {
		qDebug().noquote() << "solving antiCaptcha error:" << replyChallenge->errorString();
		replyImage->deleteLater();
		return;
	}
	else{
		replyImage->deleteLater();
		QByteArray reply = replyImage->readAll();
		QJsonObject json = QJsonDocument::fromBinaryData(reply).object();
		qDebug() << json;
		if(json.value("errorId").toString() == QString('0')){
			if(json.value("status").toString() == "ready"){
				antiGetInfo.clear();
				QString code = json.value("solution").toObject().value("gRecaptchaResponse").toString();
				emit captchaCode(code);
			}
			else{
				antiFinish();
			}
		}
		else{
			qDebug() << json.value("errorDescription").toString();
		}
	}
}

void Captcha::loadCaptcha(){
	qDebug() << "got captcha";
	loading = false;
	if(!replyChallenge){
		emit fail();
		return;
	}
	else if(replyChallenge->error()) {
		qDebug().noquote() << "loading post error:" << replyChallenge->errorString();
		replyChallenge->deleteLater();
		return;
	}
	QByteArray rep = replyChallenge->readAll();
	replyChallenge->deleteLater();
	if(rep.isEmpty()) return;
	QString replyString(rep);
	QString matchStart = "src=\"/recaptcha/api2/payload?c=";
	int start = replyString.indexOf(matchStart);
	if(start == -1) return;
	int end = replyString.indexOf("&",start+matchStart.length());
	if(end == -1) return;
	challenge = replyString.mid(start+matchStart.length(),end-start-matchStart.length());
	start = replyString.indexOf("<strong>");
	end = replyString.indexOf("</strong>",start+8);
	challengeQuestion = replyString.mid(start+8,end-start-8);
	emit questionInfo(challengeQuestion);
	getImage(challenge);
	//if(timer.isActive())timer.stop();
	//timer.start(120000);
}

void Captcha::getImage(QString challenge){
	qDebug() << "getting image";
	if(challenge.isEmpty()) return;
	QNetworkRequest imageChallenge(QUrl(urlImageBase+challenge+"&k="+siteKey));
	replyImage = nc.captchaManager->get(imageChallenge);
	imageConnection = connect(replyImage,&QNetworkReply::finished,this,&Captcha::loadImage,Qt::UniqueConnection);
}

void Captcha::loadImage(){
	qDebug() << "got image";
	if(!replyImage){
		emit fail();
		return;
	}
	else if(replyImage->error()) {
		qDebug().noquote() << "loading post error:" << replyImage->errorString();
		replyImage->deleteLater();
		return;
	}
	QByteArray rep = replyImage->readAll();
	replyImage->deleteLater();
	if(rep.isEmpty()) return;
	QPixmap challengeImage;
	challengeImage.loadFromData(rep);
	loaded = true;
	QTimer::singleShot(180000, [=](){loaded = false;});
	emit challengeInfo(challenge,challengeImage);
}
