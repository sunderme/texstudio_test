#ifndef KPATHSEAPARSER_H
#define KPATHSEAPARSER_H
#include "mostQtHeaders.h"
#include "smallUsefulFunctions.h"
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QQueue>

class PackageScanner : public SafeThread
{
	Q_OBJECT

public:
	void stop();

	static void savePackageList(QSet<QString> packages, const QString &filename);
	static QSet<QString> readPackageList(const QString &filename);

signals:
	void scanCompleted(QSet<QString> packages);

protected:
	explicit PackageScanner(QObject *parent = 0);
	bool stopped;
};


class KpathSeaParser : public PackageScanner
{
	Q_OBJECT

public:
	explicit KpathSeaParser(QString kpsecmd, QObject *parent = 0);

protected:
	void run();
	QString kpsewhich(const QString &arg);

private:
	QString kpseWhichCmd;
};


class MiktexPackageScanner : public PackageScanner
{
	Q_OBJECT

public:
	MiktexPackageScanner(QString mpmCmd, QString settingsDir, QObject *parent = 0);

protected:
	void run();
	QString mpm(const QString &arg);
	void savePackageMap(const QHash<QString, QStringList> &map);
	QHash<QString, QStringList> loadPackageMap();
	QStringList stysForPackage(const QString &pck);

private:
	QString mpmCmd;
	QString settingsDir;
};

#endif // LATEXSTYLEPARSER_H
