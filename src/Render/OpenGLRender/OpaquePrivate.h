#pragma once

#include "../OpenGLRenderPrivate.h"
#include "../ARender.h"
#include "../../Player/APlayer.h"

class OpenGLOpaqueRenderPrivate :public OpenGLRenderPrivate
{
public:
	QSize inner;
	int format;
	GLuint frame[3];
	QMutex dataLock;
	QList<quint8 *> buffer;

	virtual void initialize() override
	{
		OpenGLRenderPrivate::initialize();
		glGenTextures(3, frame);
	}

	void loadTexture(int i, int c, int w, int h)
	{
		OpenGLRenderPrivate::loadTexture(frame[i], c, w, h, buffer[i]);
	}

	virtual void drawData(QPainter *painter, QRect rect) override
	{
		if (inner.isEmpty()){
			return;
		}
		painter->beginNativePainting();
		if (dirty){
			int w = inner.width(), h = inner.height();
			dataLock.lock();
			switch (format){
			case 0:
			case 1:
				loadTexture(0, 1, w, h);
				loadTexture(1, 1, w / 2, h / 2);
				loadTexture(2, 1, w / 2, h / 2);
				break;
			case 2:
			case 3:
				loadTexture(0, 1, w, h);
				loadTexture(1, 2, w / 2, h / 2);
				break;
			case 4:
				loadTexture(0, 4, w, h);
				break;
			}
			dirty = false;
			dataLock.unlock();
		}
		QRect dest = fitRect(ARender::instance()->getPreferSize(), rect);
		drawTexture(frame, format, dest, rect);
		painter->endNativePainting();
	}

	void paint(QPaintDevice *device)
	{
		QPainter painter(device);
		painter.setRenderHints(QPainter::SmoothPixmapTransform);
		QRect rect(QPoint(0, 0), ARender::instance()->getActualSize());
		if (APlayer::instance()->getState() == APlayer::Stop){
			drawStop(&painter, rect);
		}
		else{
			drawPlay(&painter, rect);
			drawTime(&painter, rect);
		}
	}

	virtual QList<quint8 *> getBuffer() override
	{
		dataLock.lock();
		return buffer;
	}

	virtual void releaseBuffer() override
	{
		dirty = true;
		dataLock.unlock();
	}

	virtual void setBuffer(QString &chroma, QSize size, QList<QSize> *bufferSize) override
	{
		if (chroma == "YV12"){
			format = 1;
		}
		else if (chroma == "NV12"){
			format = 2;
		}
		else if (chroma == "NV21"){
			format = 3;
		}
		else{
			format = 0;
			chroma = "I420";
		}
		inner = size;
		int s = size.width()*size.height();
		quint8 *alloc = nullptr;
		QList<QSize> plane;
		switch (format){
		case 0:
		case 1:
			alloc = new quint8[s * 3 / 2];
			plane.append(size);
			size /= 2;
			plane.append(size);
			plane.append(size);
			break;
		case 2:
		case 3:
			alloc = new quint8[s * 3 / 2];
			plane.append(size);
			size.rheight() /= 2;
			plane.append(size);
			break;
		}
		if (!buffer.isEmpty()){
			delete[]buffer[0];
		}
		buffer.clear();
		for (const QSize &s : plane){
			buffer.append(alloc);
			alloc += s.width()*s.height();
		}
		if (bufferSize)
			bufferSize->swap(plane);
	}

	virtual QSize getBufferSize() override
	{
		return inner;
	}

	virtual ~OpenGLOpaqueRenderPrivate()
	{
		if (!buffer.isEmpty()){
			delete[]buffer[0];
		}
	}
};
