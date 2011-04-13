#include "latexstyleparser.h"

LatexStyleParser::LatexStyleParser(QObject *parent,QString baseDirName,QString kpsecmd) :
    QThread(parent)
{
    baseDir=baseDirName;
    stopped=false;
    kpseWhichCmd=kpsecmd;
    mFiles.clear();
}

void LatexStyleParser::stop(){
	stopped=true;
	mFilesAvailable.release();
}

void LatexStyleParser::run(){
	forever {
		//wait for enqueued lines
		mFilesAvailable.acquire();
		if(stopped && mFiles.count()==0) break;
		mFilesLock.lock();
		QString fn=mFiles.dequeue();
		mFilesLock.unlock();
		fn=kpsewhich(fn); // find file
		if(fn.isEmpty())
		    continue;
		QStringList results;
		results=readPackage(fn); // parse package(s)
		// write results
		if(!results.isEmpty()){
		    QFileInfo info(fn);
		    QString baseName=info.completeBaseName();
		    if(!baseName.isEmpty()){
			QFile data(baseDir+"/"+baseName+".cwl");
			if(data.open(QFile::WriteOnly|QFile::Truncate)){
			    QTextStream out(&data);
			    out << "# autogenerated by tmx\n";
			    foreach(QString elem,results){
				out << elem << "\n";
			    }
			}
			emit scanCompleted(baseName);
		    }
		}
	}
}

void LatexStyleParser::addFile(QString filename){
	mFilesLock.lock();
	mFiles.enqueue(filename);
	mFilesLock.unlock();
	mFilesAvailable.release();
}

QStringList LatexStyleParser::readPackage(QString fn){
    QFile data(fn);
    QStringList results;
    if(data.open(QFile::ReadOnly)){
	    QTextStream stream(&data);
	    QString line;
	    QRegExp rxDef("\\\\def\\s*(\\\\[\\w@]+)\\s*(#\\d+)?");
	    QRegExp rxCom("\\\\(newcommand|providecommad)\\s*\\{(\\\\\\w+)\\}\\s*\\[?(\\d+)?\\]?");
	    QRegExp rxInput("^\\\\input\\s*\\{?([\\w._]+)");
	    QRegExp rxRequire("^\\\\RequirePackage\\s*\\{(\\w+,?)+\\}");
	    while(!stream.atEnd()) {
		line = stream.readLine();
		int options=0;
		if(rxDef.indexIn(line)>-1){
		    QString name=rxDef.cap(1);
		    if(name.contains("@"))
			continue;
		    QString optionStr=rxDef.cap(2);
		    //qDebug()<< name << ":"<< optionStr;
		    options=optionStr.mid(1).toInt(); //returns 0 if conversion fails
		    for (int j=0; j<options; j++) {
			name.append(QString("{arg%1}").arg(j+1));
		    }
		    name.append("#*");
		    if(!results.contains(name))
			results << name;
		    continue;
		}
		if(rxCom.indexIn(line)>-1){
		    QString name=rxCom.cap(2);
		    if(name.contains("@"))
			continue;
		    QString optionStr=rxCom.cap(3);
		    //qDebug()<< name << ":"<< optionStr;
		    options=optionStr.toInt(); //returns 0 if conversion fails
		    for (int j=0; j<options; j++) {
			name.append(QString("{arg%1}").arg(j+1));
		    }
		    name.append("#*");
		    if(!results.contains(name))
			results << name;
		    continue;
		}
		if(rxInput.indexIn(line)>-1){
		    QString name=rxInput.cap(1);
		    name=kpsewhich(name);
		    results << readPackage(name);
		    continue;
		}
		if(rxRequire.indexIn(line)>-1){
		    int cnt=rxRequire.captureCount();
		    for(int i=1;i<=cnt;i++){
			QString name=rxRequire.cap(i);
			if(name.endsWith(","))
			    name.chop(1);
			results << "#include:"+name;
		    }
		    continue;
		}

	    }
    }
    return results;

}

QString LatexStyleParser::kpsewhich(QString name){
    QString fn=name;
    if(!kpseWhichCmd.isEmpty()){
	QProcess myProc(0);
	myProc.start(kpseWhichCmd,QStringList(fn));
	myProc.waitForFinished();
	if(myProc.exitCode()==0){
	    fn=myProc.readAllStandardOutput();
	    fn=fn.split('\n').first(); // in case more than one results are present
	}else
	    fn.clear();
    }
    return fn;
}
