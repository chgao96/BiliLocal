﻿/*=======================================================================
*
*   Copyright (C) 2013-2015 Lysine.
*
*   Filename:    Interface.cpp
*   Time:        2013/03/18
*   Author:      Lysine
*   Contributor: Chaserhkj
*
*   Lysine is a student majoring in Software Engineering
*   from the School of Software, SUN YAT-SEN UNIVERSITY.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Interface.h"
#include "../Config.h"
#include "../Local.h"
#include "../Access/Load.h"
#include "../Access/Post.h"
#include "../Access/Seek.h"
#include "../Model/Danmaku.h"
#include "../Model/List.h"
#include "../Model/Shield.h"
#include "../Player/APlayer.h"
#include "../Render/ARender.h"
#include "../UI/Editor.h"
#include "../UI/Info.h"
#include "../UI/Jump.h"
#include "../UI/Menu.h"
#include "../UI/Prefer.h"
#include "../UI/Type.h"
#include <functional>

using namespace UI;

namespace
{
	void setSingle(QMenu *menu)
	{
		QActionGroup *g = new QActionGroup(menu);
		for (auto *a : menu->actions()){
			g->addAction(a)->setCheckable(true);
		}
	}

	QAction *addScale(QMenu *menu, double src, double dst)
	{
		QAction *s = menu->addAction(QString("%1:%2").arg(src).arg(dst));
		s->setEnabled(false);
		s->setData(dst / src);
		return s;
	}

	QAction *addRatio(QMenu *menu, double num, double den)
	{
		QAction *r = menu->addAction(QString("%1:%2").arg(num).arg(den));
		r->setData(num / den);
		return r;
	}
}

Interface::Interface(QWidget *parent) :
QWidget(parent)
{
	setObjectName("Interface");
	setMouseTracking(true);
	setAcceptDrops(true);
	setWindowIcon(QIcon(":/Picture/icon.png"));
	setMinimumSize(360 * logicalDpiX() / 72, 270 * logicalDpiY() / 72);
	Local::objects["Interface"] = this;

	aplayer = APlayer::instance();
	danmaku = Danmaku::instance();
	arender = ARender::instance();
	Local::objects["Danmaku"] = danmaku;
	Local::objects["APlayer"] = aplayer;
	Local::objects["ARender"] = arender;
	Local::objects["Config"] = Config::instance();
	Local::objects["Shield"] = Shield::instance();

	menu = new Menu(this);
	info = new Info(this);
	jump = new UI::Jump(this);
	type = new UI::Type(this);
	list = List::instance();
	load = Load::instance();
	post = Post::instance();
	seek = Seek::instance();
	Local::objects["Info"] = info;
	Local::objects["Menu"] = menu;
	Local::objects["Next"] = list;
	Local::objects["Load"] = load;
	Local::objects["Post"] = post;
	Local::objects["Seek"] = seek;

	timer = new QTimer(this);
	delay = new QTimer(this);
	connect(timer, &QTimer::timeout, [this](){
		QPoint cur = mapFromGlobal(QCursor::pos());
		int x = cur.x(), y = cur.y(), w = width(), h = height();
		if (isActiveWindow()){
			if (y<-60 || y>h + 60 || x<-100 || x>w + 100){
				menu->push();
				info->push();
			}
		}
	});
	delay->setSingleShot(true);
	connect(delay, &QTimer::timeout, APlayer::instance(), [this](){
		if (aplayer->getState() == APlayer::Play&&!menu->isVisible() && !info->isVisible()){
			setCursor(QCursor(Qt::BlankCursor));
		}
		if (!sliding){
			showprg = false;
			arender->setDisplayTime(0);
		}
	});

	connect(aplayer, &APlayer::begin, this, [this](){
		rat->defaultAction()->setChecked(true);
		spd->defaultAction()->setChecked(true);
		for (auto iter : sca->actions()){
			if (iter->data().type() == QVariant::Double){
				iter->setEnabled(true);
			}
		}
		if (!isFullScreen()){
			if (geo.isEmpty()){
				geo = saveGeometry();
				setCenter(arender->getPreferSize(), false);
			}
		}
		rat->setEnabled(true);
		spd->setEnabled(true);
		arender->setDisplayTime(0);
		setWindowFilePath(aplayer->getMedia());
	});
	connect(aplayer, &APlayer::reach, this, [this](bool m){
		danmaku->resetTime();
		danmaku->clearCurrent();
		rat->setEnabled(false);
		spd->setEnabled(false);
		for (auto iter : sca->actions()){
			if (iter->data().type() == QVariant::Double){
				iter->setEnabled(false);
			}
		}
		arender->setVideoAspectRatio(0);
		if (!geo.isEmpty() && (list->finished() || m)){
			fullA->setChecked(false);
			restoreGeometry(geo);
			geo.clear();
		}
		arender->setDisplayTime(0);
		setWindowFilePath(QString());
	});
	connect(aplayer, &APlayer::errorOccurred, [this](int code){
		QString text;
		switch (code){
		case APlayer::ResourceError:
			text = tr("A media resource couldn't be resolved.");
			break;
		case APlayer::FormatError:
			text = tr("The format of a media resource isn't (fully) supported. "
				"Playback may still be possible, "
				"but without an audio or video component.");
			break;
		case APlayer::NetworkError:
			text = tr("A network error occurred.");
			break;
		case APlayer::AccessDeniedError:
			text = tr("There are not the appropriate permissions to play a media resource.");
			break;
		case APlayer::ServiceMissingError:
			text = tr("A valid playback service was not found, playback cannot proceed.");
			break;
		default:
			text = tr("An error occurred.");
			break;
		}
		warning(tr("Warning"), text);
	});
	connect(aplayer, &APlayer::stateChanged, this, &Interface::setWindowFlags);

	auto alertNetworkError=[this](int code){
		QString info = tr("Network error occurred, error code: %1").arg(code);
		QString sugg;
		switch (code){
		case 3:
		case 4:
			sugg = tr("check your network connection");
			break;
		case 203:
		case 403:
		case -8:
			sugg = tr("access denied, try login");
			break;
		default:
			break;
		}
		warning(tr("Network Error"), sugg.isEmpty() ? info : (info + '\n' + sugg));
	};
	connect(load, &Load::progressChanged, this, &Interface::percent);
	connect(load, &Load::errorOccured, this, alertNetworkError);
	connect(post, &Post::errorOccured, this, alertNetworkError);
	connect(seek, &Seek::errorOccured, this, alertNetworkError);

	showprg = sliding = false;
	connect(aplayer, &APlayer::timeChanged, [this](qint64 t){
		if (!sliding&&aplayer->getState() != APlayer::Stop){
			arender->setDisplayTime(showprg ? t / (double)aplayer->getDuration() : -1);
		}
	});

	addActions(menu->actions());
	addActions(info->actions());

	quitA = new QAction(tr("Quit"), this);
	quitA->setObjectName("Quit");
	quitA->setShortcut(Config::getValue("/Shortcut/Quit", QString("Ctrl+Q")));
	addAction(quitA);
	connect(quitA, &QAction::triggered, this, &Interface::close);

	fullA = new QAction(tr("Full Screen"), this);
	fullA->setObjectName("Full");
	fullA->setCheckable(true);
	fullA->setShortcut(Config::getValue("/Shortcut/Full", QString("F")));
	addAction(fullA);
	connect(fullA, &QAction::toggled, [this](bool b){
		b ? showFullScreen() : showNormal();
	});

	confA = new QAction(tr("Config"), this);
	confA->setObjectName("Conf");
	confA->setShortcut(Config::getValue("/Shortcut/Conf", QString("Ctrl+I")));
	addAction(confA);
	connect(confA, &QAction::triggered, [](){
		Prefer::exec(lApp->mainWidget());
	});

	toggA = new QAction(tr("Block All"), this);
	toggA->setObjectName("Togg");
	toggA->setCheckable(true);
	QString wholeShield = QString("m=%1").arg(Shield::Whole);
	toggA->setChecked(Shield::instance()->contains(wholeShield));
	toggA->setShortcut(Config::getValue("/Shortcut/Togg", QString("Ctrl+T")));
	addAction(toggA);
	connect(toggA, &QAction::triggered, [=](bool b){
		if (b){
			Shield::instance()->insert(wholeShield);
		}
		else{
			Shield::instance()->remove(wholeShield);
		}
		danmaku->parse(0x2);
	});
	connect(Shield::instance(), &Shield::shieldChanged, [=](){
		toggA->setChecked(Shield::instance()->contains(wholeShield));
	});

	listA = new QAction(tr("Playlist"), this);
	listA->setObjectName("List");
	listA->setShortcut(Config::getValue("/Shortcut/List", QString("Ctrl+L")));
	addAction(listA);
	connect(listA, &QAction::triggered, [this](){
		Editor::exec(this);
	});

	postA = new QAction(tr("Post Danmaku"), this);
	postA->setObjectName("Post");
	postA->setEnabled(false);
	postA->setShortcut(Config::getValue("/Shortcut/Post", QString("Ctrl+P")));
	addAction(postA);
	connect(postA, &QAction::triggered, [this](){
		menu->push();
		info->push();
		type->show();
	});
	connect(danmaku, &Danmaku::modelReset, [this](){
		bool allow = false;
		for (const Record &r : danmaku->getPool()){
			if (post->canPost(r.access)){
				allow = true;
				break;
			}
		}
		postA->setEnabled(allow);
	});

	QAction *fwdA = new QAction(tr("Forward"), this);
	fwdA->setObjectName("Fowd");
	fwdA->setShortcut(Config::getValue("/Shortcut/Fowd", QString("Right")));
	connect(fwdA, &QAction::triggered, [this](){
		aplayer->setTime(aplayer->getTime() + Config::getValue("/Interface/Interval", 10) * 1000);
	});
	QAction *bwdA = new QAction(tr("Backward"), this);
	bwdA->setObjectName("Bkwd");
	bwdA->setShortcut(Config::getValue("/Shortcut/Bkwd", QString("Left")));
	connect(bwdA, &QAction::triggered, [this](){
		aplayer->setTime(aplayer->getTime() - Config::getValue("/Interface/Interval", 10) * 1000);
	});
	addAction(fwdA);
	addAction(bwdA);

	QAction *delA = new QAction(tr("Delay"), this);
	delA->setObjectName("Dely");
	delA->setShortcut(Config::getValue("/Shortcut/Dely", QString("Ctrl+Right")));
	connect(delA, &QAction::triggered, std::bind(&Danmaku::delayAll, Danmaku::instance(), +1000));
	QAction *ahdA = new QAction(tr("Ahead"), this);
	ahdA->setObjectName("Ahed");
	ahdA->setShortcut(Config::getValue("/Shortcut/Ahed", QString("Ctrl+Left")));
	connect(ahdA, &QAction::triggered, std::bind(&Danmaku::delayAll, Danmaku::instance(), -1000));
	addAction(delA);
	addAction(ahdA);

	QAction *escA = new QAction(this);
	escA->setShortcut(Qt::Key_Escape);
	connect(escA, &QAction::triggered, [this](){
		fullA->setChecked(false);
	});
	addAction(escA);

	QAction *vouA = new QAction(tr("VolUp"), this);
	vouA->setObjectName("VoUp");
	vouA->setShortcut(Config::getValue("/Shortcut/VoUp", QString("Up")));
	connect(vouA, &QAction::triggered, [this](){aplayer->setVolume(aplayer->getVolume() + 5); });
	QAction *vodA = new QAction(tr("VolDn"), this);
	vodA->setObjectName("VoDn");
	vodA->setShortcut(Config::getValue("/Shortcut/VoDn", QString("Down")));
	connect(vodA, &QAction::triggered, [this](){aplayer->setVolume(aplayer->getVolume() - 5); });
	addAction(vouA);
	addAction(vodA);

	QAction *pguA = new QAction(tr("Last Media"), this);
	pguA->setObjectName("PgUp");
	pguA->setShortcut(Config::getValue("/Shortcut/PgUp", QString("PgUp")));
	connect(pguA, &QAction::triggered, [this](){list->jumpToLast(); });
	QAction *pgdA = new QAction(tr("Next Media"), this);
	pgdA->setObjectName("PgDn");
	pgdA->setShortcut(Config::getValue("/Shortcut/PgDn", QString("PgDown")));
	connect(pgdA, &QAction::triggered, [this](){list->jumpToNext(); });
	addAction(pguA);
	addAction(pgdA);

	QAction *spuA = new QAction(tr("SpeedUp"), this);
	spuA->setObjectName("SpUp");
	spuA->setShortcut(Config::getValue("/Shortcut/SpUp", QString("Ctrl+Up")));
	connect(spuA, &QAction::triggered, [this](){aplayer->setRate(aplayer->getRate() + 0.1); });
	QAction *spdA = new QAction(tr("SpeedDn"), this);
	spdA->setObjectName("SpDn");
	spdA->setShortcut(Config::getValue("/Shortcut/SpDn", QString("Ctrl+Down")));
	connect(spdA, &QAction::triggered, [this](){aplayer->setRate(aplayer->getRate() - 0.1); });
	addAction(spuA);
	addAction(spdA);

	QAction *rstA = new QAction(tr("Reset"), this);
	rstA->setObjectName("Rest");
	rstA->setShortcut(Config::getValue("/Shortcut/Rest", QString("Home")));
	connect(rstA, &QAction::triggered, [this](){
		aplayer->setRate(1.0);
		rat->defaultAction()->trigger();
		sca->defaultAction()->trigger();
		spd->defaultAction()->trigger();
	});
	addAction(rstA);

	rat = new QMenu(tr("Ratio"), this);
	rat->setEnabled(false);
	rat->setDefaultAction(rat->addAction(tr("Default")));
	addRatio(rat, 4, 3);
	addRatio(rat, 16, 10);
	addRatio(rat, 16, 9);
	addRatio(rat, 1.85, 1);
	addRatio(rat, 2.35, 1);
	connect(rat, &QMenu::triggered, [this](QAction *action){
		arender->setVideoAspectRatio(action->data().toDouble());
		if (aplayer->getState() == APlayer::Pause){
			arender->draw();
		}
	});
	setSingle(rat);

	sca = new QMenu(tr("Scale"), this);
	connect(sca->addAction(fullA->text()), &QAction::toggled, fullA, &QAction::setChecked);
	sca->addAction(tr("Init Size"))->setData(QSize());
	sca->addAction(QStringLiteral("541×384"))->setData(QSize(540, 383));
	sca->addAction(QStringLiteral("672×438"))->setData(QSize(671, 437));
	sca->addSeparator();
	addScale(sca, 1, 4);
	addScale(sca, 1, 2);
	sca->setDefaultAction(addScale(sca, 1, 1));
	addScale(sca, 2, 1);
	addScale(sca, 4, 1);
	connect(sca, &QMenu::triggered, [this](QAction *action){
		QVariant d = action->data();
		switch (d.type()){
		case QVariant::Double:
			setCenter(arender->getPreferSize() * d.toDouble(), false);
			break;
		case QVariant::Size:
			setCenter(d.toSize(), false);
			break;
		default:
			break;
		}
	});
	setSingle(sca);

	spd = new QMenu(tr("Speed"), this);
	spd->setEnabled(false);
	spd->addAction(QStringLiteral("×5.0"))->setData(5.0);
	spd->addAction(QStringLiteral("×2.0"))->setData(2.0);
	spd->addAction(QStringLiteral("×1.5"))->setData(1.5);
	spd->setDefaultAction(spd->addAction(QStringLiteral("×1.0")));
	spd->addAction(QStringLiteral("×0.8"))->setData(0.8);
	spd->addAction(QStringLiteral("×0.5"))->setData(0.5);
	spd->addAction(QStringLiteral("×0.2"))->setData(0.2);
	connect(spd, &QMenu::triggered, [this](QAction *action){
		auto data = action->data();
		aplayer->setRate(data.isValid() ? data.toDouble() : 1.0);
	});
	setSingle(spd);

	setWindowFlags();
	checkForUpdate();
	setCenter(QSize(), true);
}

void Interface::closeEvent(QCloseEvent *e)
{
	if (aplayer->getState() == APlayer::Stop&&!isFullScreen() && !isMaximized()){
		QString conf = Config::getValue("/Interface/Size", QString("720,405"));
		QString size = QString("%1,%2").arg(width() * 72 / logicalDpiX()).arg(height() * 72 / logicalDpiY());
		Config::setValue("/Interface/Size", conf.endsWith(' ') ? conf.trimmed() : size);
	}
	QWidget::closeEvent(e);
	lApp->exit();
}

void Interface::dragEnterEvent(QDragEnterEvent *e)
{
	if (e->mimeData()->hasFormat("text/uri-list")){
		e->acceptProposedAction();
	}
	QWidget::dragEnterEvent(e);
}

void Interface::dropEvent(QDropEvent *e)
{
	if (e->mimeData()->hasFormat("text/uri-list")){
		for (const QString &item : QString(e->mimeData()->data("text/uri-list")).split('\n', QString::SkipEmptyParts)){
			tryLocal(QUrl(item).toLocalFile().trimmed());
		}
	}
}

void Interface::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (!menu->isShown() && !info->isShown()){
		fullA->toggle();
	}
	QWidget::mouseDoubleClickEvent(e);
}

void Interface::mouseMoveEvent(QMouseEvent *e)
{
	QPoint cur = e->pos();
	int x = cur.x(), y = cur.y(), w = width(), h = height();
	if (isActiveWindow()){
		if (x > 250){
			menu->push();
		}
		if (x < w - 250){
			info->push();
		}
		if (y <= h - 50){
			if (x >= 0 && x<50){
				menu->pop();
			}
			if (x>w - 50 && x <= w){
				info->pop();
			}
		}
	}
	if (!showprg&&aplayer->getState() != APlayer::Stop){
		arender->setDisplayTime(aplayer->getTime() / (double)aplayer->getDuration());
	}
	showprg = true;
	if (cursor().shape() == Qt::BlankCursor){
		unsetCursor();
	}
	delay->start(4000);
	if (sliding){
		arender->setDisplayTime(x / (double)w);
	}
	else if (!sta.isNull() && (windowFlags()&Qt::CustomizeWindowHint) != 0 && !isFullScreen()){
		move(wgd + e->globalPos() - sta);
	}
	QWidget::mouseMoveEvent(e);
}

void Interface::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton){
		if (sta.isNull()){
			sta = e->globalPos();
			wgd = pos();
		}
		QPoint p = e->pos();
		if (!info->geometry().contains(p) && !menu->geometry().contains(p)){
			if (height() - p.y() <= 25 && height() >= p.y()){
				sliding = true;
				arender->setDisplayTime(e->x() / (double)width());
			}
			else if (Config::getValue("/Interface/Sensitive", false)){
				aplayer->play();
			}
		}
	}
	QWidget::mousePressEvent(e);
}

void Interface::mouseReleaseEvent(QMouseEvent *e)
{
	if (!menu->geometry().contains(e->pos()) && !info->geometry().contains(e->pos())){
		menu->push(true);
		info->push(true);
		type->hide();
		jump->hide();
	}
	sta = wgd = QPoint();
	if (sliding&&e->button() == Qt::LeftButton){
		sliding = false;
		if (aplayer->getState() != APlayer::Stop){
			aplayer->setTime(e->x()*aplayer->getDuration() / (width() - 1));
		}
	}
	if (e->button() == Qt::RightButton){
		showContextMenu(e->pos());
	}
	QWidget::mouseReleaseEvent(e);
}

void Interface::resizeEvent(QResizeEvent *e)
{
	const QSize s = e->size();
	arender->resize(s);
	int w = s.width(), h = s.height();
	menu->terminate();
	info->terminate();
	int l = Config::getValue("/Interface/Popup/Width", 16 * font().pointSizeF())*logicalDpiX() / 72;
	menu->setGeometry(menu->isShown() ? 0 : 0 - l, 0, l, h);
	info->setGeometry(info->isShown() ? w - l : w, 0, l, h);
	type->move(w / 2 - type->width() / 2, h - type->height() - 2);
	jump->move(w / 2 - jump->width() / 2, 2);
	for (QAction *action : sca->actions()){
		if (action->isSeparator() || !action->isEnabled()){
			continue;
		}
		QVariant d = action->data();
		switch (d.type()){
		case QVariant::Double:
			action->setChecked(s == arender->getPreferSize() * d.toDouble());
			break;
		case QVariant::Size:
			action->setChecked(s == d.toSize());
			break;
		default:
			action->setChecked(fullA->isChecked());
			break;
		}
	}
	QWidget::resizeEvent(e);
}

void Interface::tryLocal(QString p)
{
	QFileInfo info(p);
	QString suffix = info.suffix().toLower();
	if (!info.exists()){
		return;
	}
	else if (Utils::getSuffix(Utils::Danmaku).contains(suffix)){
		load->loadDanmaku(p);
	}
	else if (Utils::getSuffix(Utils::Subtitle).contains(suffix) && aplayer->getState() != APlayer::Stop){
		aplayer->addSubtitle(p);
	}
	else{
		switch (Config::getValue("/Interface/Single", 1)){
		case 0:
		case 1:
			aplayer->setMedia(p);
			break;
		case 2:
			list->appendMedia(p);
			break;
		}
	}
}

void Interface::tryLocal(QStringList p)
{
	for (const QString &i : p){
		tryLocal(i);
	}
}

void Interface::setWindowFlags()
{
	QFlags<Qt::WindowType> flags = windowFlags();
	if (Config::getValue("/Interface/Frameless", false)){
		flags = Qt::CustomizeWindowHint;
	}
	if ((Config::getValue("/Interface/Top", 0) + (aplayer->getState() == APlayer::Play)) >= 2){
		flags |= Qt::WindowStaysOnTopHint;
	}
	else{
		flags &= ~Qt::WindowStaysOnTopHint;
	}
	if (!testAttribute(Qt::WA_WState_Created)){
		QWidget::setWindowFlags(flags);
	}
	else{
		emit windowFlagsChanged(flags);
	}
}

void Interface::warning(QString title, QString text)
{
	auto m = msg ? qobject_cast<QMessageBox     *>(msg) : nullptr;
	QWidget *active = qApp->activeWindow();
	if (!m || (m != active&&m->parent() != active&&active)){
		delete msg;
		m = new QMessageBox(active);
		m->setIcon(QMessageBox::Warning);
		msg = m;
	}
	m->setText(text);
	m->setWindowTitle(title);
	m->show();
}

void Interface::percent(double degree)
{
	auto p = msg ? qobject_cast<QProgressDialog *>(msg) : nullptr;
	QWidget *active = qApp->activeWindow();
	if (!p || (p != active&&p->parent() != active&&active)){
		if (msg){
			delete msg;
		}
		p = new QProgressDialog(active);
		p->setMaximum(1000);
		p->setWindowTitle(tr("Loading"));
		p->setFixedSize(p->sizeHint());
		p->show();
		connect(p, &QProgressDialog::canceled, Load::instance(), &Load::dequeue);
		msg = p;
	}
	p->setValue(1000 * degree);
}

void Interface::checkForUpdate()
{
	QString url("https://raw.githubusercontent.com/AncientLysine/BiliLocal/master/res/INFO");
	auto update = Config::getValue("/Interface/Update", QVariant(url));
	if (QVariant::Bool == update.type()){
		if (!update.toBool()){
			return;
		}
	}
	else{
		url = update.toString();
	}
	QNetworkAccessManager *manager = new QNetworkAccessManager(this);
	manager->get(QNetworkRequest(url));
	connect(manager, &QNetworkAccessManager::finished, [=](QNetworkReply *info){
		QUrl redirect = info->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
		if (redirect.isValid()){
			manager->get(QNetworkRequest(redirect));
			return;
		}
		manager->deleteLater();
		if (info->error() != QNetworkReply::NoError){
			return;
		}
		QFile local(":/Text/DATA");
		local.open(QIODevice::ReadOnly);
		QJsonObject l = QJsonDocument::fromJson(local.readAll()).object();
		QJsonObject r = QJsonDocument::fromJson(info->readAll()).object();
		if (!r.contains("Version") || l["Version"].toString() >= r["Version"].toString()){
			return;
		}
		QMessageBox::StandardButton button;
		QString information = r["String"].toString();
		button = QMessageBox::information(this,
			tr("Update"),
			information,
			QMessageBox::Ok | QMessageBox::Cancel);
		if (button == QMessageBox::Ok){
			QDesktopServices::openUrl(r["Url"].toString());
		}
	});
}

void Interface::setCenter(QSize _s, bool _f)
{
	double x = logicalDpiX() / 72.0, y = logicalDpiY() / 72.0;
	if (!_s.isValid()){
		QStringList l = Config::getValue("/Interface/Size", QString("720,405")).trimmed().split(',', QString::SkipEmptyParts);
		if (l.size() >= 2){
			_s = QSize(l[0].toInt()*x, l[1].toInt()*y);
		}
	}
	QSize m = minimumSize();
	QRect r;
	r.setSize(QSize(qMax(m.width(), _s.width()), qMax(m.height(), _s.height())));
	QRect s = QApplication::desktop()->screenGeometry(this);
	QRect t = _f ? s : geometry();
	if ((windowFlags()&Qt::CustomizeWindowHint) == 0){
		s.setTop(s.top() + style()->pixelMetric(QStyle::PM_TitleBarHeight));
	}
	bool flag = true;
	if (r.width() >= s.width() || r.height() >= s.height()){
		if (isVisible()){
			fullA->setChecked(true);
			flag = false;
		}
		else{
			r.setSize(QSize(720 * x, 405 * y));
		}
	}
	if (flag){
		r.moveCenter(t.center());
		if (r.top() < s.top()){
			r.moveTop(s.top());
		}
		if (r.bottom() > s.bottom()){
			r.moveBottom(s.bottom());
		}
		if (r.left() < s.left()){
			r.moveLeft(s.left());
		}
		if (r.right() > s.right()){
			r.moveRight(s.right());
		}
		setGeometry(r);
	}
}

namespace
{
	void addTrack(QMenu *menu, int type)
	{
		QMenu *sub = new QMenu(Interface::tr("Track"), menu);
		sub->addActions(APlayer::instance()->getTracks(type));
		sub->setEnabled(!sub->isEmpty());
		menu->addMenu(sub);
	}

	void addDelay(QMenu *menu, int type, qint64 step)
	{
		QMenu *sub = new QMenu(Interface::tr("Sync"), menu);
		auto fake = sub->addAction(QString());
		fake->setEnabled(false);
		auto edit = new QLineEdit(sub);
		edit->setFrame(false);
		edit->setText(Interface::tr("Delay: %1s").arg(APlayer::instance()->getDelay(type) / 1000.0));
		edit->setGeometry(sub->actionGeometry(fake).adjusted(24, 1, -1, -1));
		QObject::connect(edit, &QLineEdit::editingFinished, [=](){
			QString e = QRegularExpression("[\\d\\.\\+\\-\\(][\\s\\d\\.\\:\\+\\-\\*\\/\\(\\)]*").match(edit->text()).captured();
			APlayer::instance()->setDelay(type, e.isEmpty() ? 0 : Utils::evaluate(e) * 1000);
		});
		QObject::connect(sub->addAction(Interface::tr("%1s Faster").arg(step / 1000.0)), &QAction::triggered, [=](){
			APlayer::instance()->setDelay(type, APlayer::instance()->getDelay(type) + step);
		});
		QObject::connect(sub->addAction(Interface::tr("%1s Slower").arg(step / 1000.0)), &QAction::triggered, [=](){
			APlayer::instance()->setDelay(type, APlayer::instance()->getDelay(type) - step);
		});
		sub->setEnabled(APlayer::instance()->getState() != APlayer::Stop);
		menu->addMenu(sub);
	}
}

void Interface::showContextMenu(QPoint p)
{
	bool flag = true;
	flag = flag&&!(menu->isVisible() && menu->geometry().contains(p));
	flag = flag&&!(info->isVisible() && info->geometry().contains(p));
	if (flag){
		QMenu top(this);
		const Comment *cur = danmaku->commentAt(p);
		if (cur){
			QAction *text = new QAction(&top);
			top.addAction(text);
			int w = top.actionGeometry(text).width() - 24;
			text->setText(top.fontMetrics().elidedText(cur->string, Qt::ElideRight, w));
			top.addSeparator();
			connect(top.addAction(tr("Copy")), &QAction::triggered, [=](){
				qApp->clipboard()->setText(cur->string);
			});
			connect(top.addAction(tr("Eliminate The Sender")), &QAction::triggered, [=](){
				QString sender = cur->sender;
				if (!sender.isEmpty()){
					Shield::instance()->insert("u=" + sender);
					danmaku->parse(0x2);
				}
			});
			top.addSeparator();
		}
		top.addActions(info->actions());
		top.addAction(menu->findChild<QAction *>("File"));
		top.addAction(listA);
		QMenu vid(tr("Video"), this);
		vid.setFixedWidth(top.sizeHint().width());
		addTrack(&vid, Utils::Video);
		vid.addMenu(sca);
		vid.addMenu(rat);
		vid.addMenu(spd);
		vid.addAction(findChild<QAction *>("Rest"));
		top.addMenu(&vid);
		QMenu aud(tr("Audio"), this);
		aud.setFixedWidth(top.sizeHint().width());
		addTrack(&aud, Utils::Audio);
		addDelay(&aud, Utils::Audio, 100);
		aud.addAction(findChild<QAction *>("VoUp"));
		aud.addAction(findChild<QAction *>("VoDn"));
		top.addMenu(&aud);
		QMenu sub(tr("Subtitle"), this);
		sub.setFixedWidth(top.sizeHint().width());
		addTrack(&sub, Utils::Subtitle);
		addDelay(&sub, Utils::Subtitle, 500);
		connect(sub.addAction(tr("Load Subtitle")), &QAction::triggered, [this](){
			QFileInfo info(aplayer->getMedia());
			QString file = QFileDialog::getOpenFileName(this,
				tr("Open File"),
				info.absolutePath(),
				tr("Subtitle files (%1);;All files (*.*)").arg(Utils::getSuffix(Utils::Subtitle, "*.%1").join(' ')));
			if (!file.isEmpty()){
				aplayer->addSubtitle(file);
			}
		});
		top.addMenu(&sub);
		QMenu dmk(tr("Danmaku"), this);
		dmk.setFixedWidth(top.sizeHint().width());
		QMenu dts(tr("Track"), &dmk);
		for (const Record r : danmaku->getPool()){
			dts.addAction(r.string);
		}
		dts.setEnabled(!dts.isEmpty());
		dmk.addMenu(&dts);
		connect(dmk.addAction(tr("Sync")), &QAction::triggered, [](){
			Editor::exec(lApp->mainWidget(), 2);
		});
		QAction *loadA = menu->findChild<QAction *>("Danm");
		QAction *seekA = menu->findChild<QAction *>("Sech");
		dmk.addAction(toggA);
		dmk.addAction(tr("Load"), loadA, SLOT(trigger()), loadA->shortcut())->setEnabled(loadA->isEnabled());
		dmk.addAction(tr("Post"), postA, SLOT(trigger()), postA->shortcut())->setEnabled(postA->isEnabled());
		dmk.addAction(tr("Seek"), seekA, SLOT(trigger()), seekA->shortcut())->setEnabled(seekA->isEnabled());
		top.addMenu(&dmk);
		top.addAction(confA);
		top.addAction(quitA);
		top.exec(mapToGlobal(p));
	}
}
