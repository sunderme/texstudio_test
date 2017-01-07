#ifndef DEBUGHELPER_H
#define DEBUGHELPER_H

#ifndef __GLIBC__
#define NO_CRASH_HANDLER
#endif

#include "modifiedQObject.h"
#include "smallUsefulFunctions.h"


QString print_backtrace(const QString &message);

void recover(); //defined in texmaker.cpp

void initCrashHandler(int mode);
QString getLastCrashInformation(bool &wasLoop);
void catchUnhandledException();


class Guardian: public SafeThread
{
	Q_OBJECT

public:
	Guardian(): SafeThread(), slowOperations(0) {}
	static void summon();
	static void calm();
	static void shutdown();

	static void continueEndlessLoop();

	static Guardian *instance();
	
protected:
	void run();

public slots:
	void slowOperationStarted();
	void slowOperationEnded();

private:
	int slowOperations;
};


#endif // DEBUGHELPER_H
