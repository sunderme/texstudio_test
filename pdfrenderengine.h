#ifndef PDFRENDERENGINE_H
#define PDFRENDERENGINE_H

#ifndef NO_POPPLER_PREVIEW

#include "poppler-qt4.h"

#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QQueue>

class PDFQueue;

class RenderCommand {
public:
	explicit RenderCommand(int p,double xr=72.0,double yr=72.0,int x=-1,int y=-1,int w=-1,int h=-1);
	int pageNr;
	double xres;
	double yres;
	int x;
	int y;
	int w;
	int h;
	Poppler::Page::Rotation rotate;
	int ticket;
	bool priority;
};

class PDFRenderEngine : public QThread
{
	Q_OBJECT
public:
	explicit PDFRenderEngine(QObject *parent,PDFQueue *mQueue);
	~PDFRenderEngine();

	void setDocument(Poppler::Document *doc);
signals:
	void sendImage(QImage image,int page,int ticket);

public slots:

protected:
	void run();

private:
	Poppler::Document	*document;
	int cachedNumPages;
	PDFQueue *queue;

};

#endif // PDFRENDERENGINE_H

#endif
