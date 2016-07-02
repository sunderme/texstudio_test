#include "latexstyleparser.h"

LatexStyleParser::LatexStyleParser(QObject *parent, QString baseDirName, QString kpsecmd) :
	SafeThread(parent)
{
	baseDir = baseDirName;
	stopped = false;
	kpseWhichCmd = kpsecmd;
	mFiles.clear();
	//check if pdflatex is present
	texdefDir = kpsecmd.left(kpsecmd.length() - 9);
	QProcess myProc(0);
	//myProc.start(texdefDir+"texdef");
	myProc.start(texdefDir + "pdflatex --version");
	myProc.waitForFinished();
	texdefMode = (myProc.exitCode() == 0);
	if (texdefMode) {
		QString output = myProc.readAllStandardOutput();
		output = output.split("\n").first().trimmed();
		if (output.contains("MiKTeX")) {
			kpseWhichCmd.chop(9);
			kpseWhichCmd.append("findtexmf");
		}
	}
}

void LatexStyleParser::stop()
{
	stopped = true;
	mFilesAvailable.release();
}

void LatexStyleParser::run()
{
	forever {
		//wait for enqueued lines
		mFilesAvailable.acquire();
		if (stopped && mFiles.count() == 0) break;
		mFilesLock.lock();
		QString fn = mFiles.dequeue();
		mFilesLock.unlock();
		QString topPackage = "";
		QString dirName = "";
		if (fn.contains("/")) {
			int i = fn.indexOf("/");
			if (i > -1) {
				dirName = fn.mid(i + 1);
				fn = fn.left(i);
			}
		}
		if (fn.contains('#')) {
			QStringList lst = fn.split('#');
			if (!lst.isEmpty())
				topPackage = lst.takeFirst();
			if (!lst.isEmpty())
				fn = lst.last();
		} else {
			topPackage = fn;
			topPackage.chop(4);
		}
		QString fullName = kpsewhich(fn); // find file
		if (fullName.isEmpty()) {
			if (!dirName.isEmpty()) {
				fullName = kpsewhich(fn, dirName); // find file
			}
		}
		if (fullName.isEmpty())
			continue;

		QStringList results, parsedPackages;
		results = readPackage(fullName, parsedPackages); // parse package(s)
		results.sort();

		if (texdefMode) {
			QStringList appendList;
			//QStringList texdefResults=readPackageTexDef(fn); // parse package(s) by texdef as well for indirectly defined commands
			QStringList texdefResults = readPackageTracing(fn);
			texdefResults.sort();
			// add only additional commands to results
			if ( !results.isEmpty() && !texdefResults.isEmpty()) {
				QStringList::const_iterator texdefIterator;
				QStringList::const_iterator resultsIterator = results.constBegin();
				QString result = *resultsIterator;
				for (texdefIterator = texdefResults.constBegin(); texdefIterator != texdefResults.constEnd(); ++texdefIterator) {
					QString td = *texdefIterator;
					if (td.startsWith('#'))
						continue;
					int i = td.indexOf("#");
					if (i >= 0)
						td = td.left(i);
					while (result < td && resultsIterator != results.constEnd()) {
						++resultsIterator;
						if (resultsIterator != results.constEnd()) {
							result = *resultsIterator;
						} else {
							result.clear();
						}
					}
					//compare result/td
					bool addCommand = true;

					if (result.startsWith(td)) {
						if (result.length() > td.length()) {
							QChar c = result.at(td.length());
							switch (c.toLatin1()) {
							case '#':
								;
							case '{':
								;
							case '[':
								addCommand = false;
							default:
								break;
							}
						} else {
							addCommand = false; //exactly equal
						}
					}

					if (addCommand) {
						appendList << *texdefIterator;
						//qDebug()<<td;
					}
				}
			} else {
				if (results.isEmpty())
					results << texdefResults;
			}
			results << appendList;
		}


		// if included styles call for additional generation, do it.
		QStringList included = results.filter(QRegExp("#include:.+"));
		foreach (QString elem, included) {
			elem = elem.mid(9);
			if (!QFileInfo("cwl:" + elem + ".cwl").exists()) {
				QString hlp = kpsewhich(elem + ".sty");
				if (!hlp.isEmpty()) {
					if (!topPackage.isEmpty())
						elem = topPackage + "#" + elem + ".sty";
					addFile(elem);
				}
			}
		}

		// write results
		if (!results.isEmpty()) {
			QFileInfo info(fn);
			QString baseName = info.completeBaseName();
			if (!baseName.isEmpty()) {
				QFile data(joinPath(baseDir, "completion/autogenerated", baseName + ".cwl"));
				if (data.open(QFile::WriteOnly | QFile::Truncate)) {
					QTextStream out(&data);
					out << "# autogenerated by txs\n";
					foreach (const QString &elem, results) {
						out << elem << "\n";
					}
				}
				if (!topPackage.isEmpty() && topPackage != baseName)
					baseName = topPackage + "#" + baseName;
				emit scanCompleted(baseName);
			}
		}
	}
}

void LatexStyleParser::addFile(QString filename)
{
	mFilesLock.lock();
	mFiles.enqueue(filename);
	mFilesLock.unlock();
	mFilesAvailable.release();
}

/*!
 * \return "{arg1}..{argN}" where N=count. If with optional, return "[opt]{arg1}..{argN}"
 */
QString LatexStyleParser::makeArgString(int count, bool withOptional) const
{
	QString args;
	if (withOptional) {
		args.append("[opt]");
	}
	for (int j = 0; j < count; j++) {
		args.append(QString("{arg%1}").arg(j + 1));
	}
	return args;
}

/*!
 * \brief LatexStyleParser::parseLine
 * \param line: the text line to parse
 * \param inRequirePackage: context information: are we inside \\RequirePackage{ - This may be modified by the function.
 * \param parsedPackages: context information. - This may be modified by the function.
 * \param fileName: the file from which line comes
 * \return a list of cwl commands
 */
QStringList LatexStyleParser::parseLine(const QString &line, bool &inRequirePackage, QStringList &parsedPackages, const QString &fileName) const
{
	static const QRegExp rxDef("\\\\def\\s*(\\\\[\\w@]+)\\s*(#\\d+)?");
	static const QRegExp rxLet("\\\\let\\s*(\\\\[\\w@]+)");
	static const QRegExp rxCom("\\\\(newcommand|providecommand|DeclareRobustCommand)\\*?\\s*\\{(\\\\\\w+)\\}\\s*\\[?(\\d+)?\\]?\\s*\\[?(\\d+)?\\]?");
	static const QRegExp rxComNoBrace("\\\\(newcommand|providecommand|DeclareRobustCommand)\\*?\\s*(\\\\\\w+)\\s*\\[?(\\d+)?\\]?\\s*\\[?(\\d+)?\\]?");
	static const QRegExp rxEnv("\\\\newenvironment\\s*\\{(\\w+)\\}\\s*\\[?(\\d+)?\\]?");
	static const QRegExp rxInput("\\\\input\\s*\\{?([\\w._]+)");
	static QRegExp rxRequire("\\\\RequirePackage\\s*\\{(\\S+)\\}");
	rxRequire.setMinimal(true);
	static const QRegExp rxRequireStart("\\\\RequirePackage\\s*\\{(.+)");
	static const QRegExp rxDecMathSym("\\\\DeclareMathSymbol\\s*\\{\\\\(\\w+)\\}");
	static const QRegExp rxNewLength("\\\\newlength\\s*\\{\\\\(\\w+)\\}");
	static const QRegExp rxNewCounter("\\\\newcounter\\s*\\{(\\w+)\\}");
	static const QRegExp rxLoadClass("\\\\LoadClass\\s*\\{(\\w+)\\}");
	QStringList results;
	if (line.startsWith("\\endinput"))
		return results;
	int options = 0;
	if (inRequirePackage) {
		int col = line.indexOf('}');
		if (col > -1) {
			QString zw = line.left(col);
			foreach (QString elem, zw.split(',')) {
				QString package = elem.remove(' ');
				if (!package.isEmpty())
					results << "#include:" + package;
			}
			inRequirePackage = false;
		} else {
			foreach (QString elem, line.split(',')) {
				QString package = elem.remove(' ');
				if (!package.isEmpty())
					results << "#include:" + package;
			}
		}
		return results;
	}
	if (rxDef.indexIn(line) > -1) {
		QString name = rxDef.cap(1);
		if (name.contains("@"))
			return results;
		QString optionStr = rxDef.cap(2);
		//qDebug()<< name << ":"<< optionStr;
		options = optionStr.mid(1).toInt(); //returns 0 if conversion fails
		for (int j = 0; j < options; j++) {
			name.append(QString("{arg%1}").arg(j + 1));
		}
		name.append("#S");
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxLet.indexIn(line) > -1) {
		QString name = rxLet.cap(1);
		if (name.contains("@"))
			return results;
		name.append("#S");
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxCom.indexIn(line) > -1) {
		QString name = rxCom.cap(2);
		if (name.contains("@"))
			return results;
		int optionCount = rxCom.cap(3).toInt(); //returns 0 if conversion fails
		QString optionalArg = rxCom.cap(4);
		if (!optionalArg.isEmpty()) {
			optionCount--;
			QString nameWithOpt = name + makeArgString(optionCount, true) + "#S";
			if (!results.contains(nameWithOpt))
				results << nameWithOpt;
		}
		name += makeArgString(optionCount) + "#S";
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxComNoBrace.indexIn(line) > -1) {
		QString name = rxComNoBrace.cap(2);
		if (name.contains("@"))
			return results;
		int optionCount = rxComNoBrace.cap(3).toInt(); //returns 0 if conversion fails
		QString optionalArg = rxComNoBrace.cap(4);
		if (!optionalArg.isEmpty()) {
			optionCount--;
			QString nameWithOpt = name + makeArgString(optionCount, true) + "#S";
			if (!results.contains(nameWithOpt))
				results << nameWithOpt;
		}
		name += makeArgString(optionCount) + "#S";
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxEnv.indexIn(line) > -1) {
		QString name = rxEnv.cap(1);
		if (name.contains("@"))
			return results;
		QString optionStr = rxEnv.cap(2);
		//qDebug()<< name << ":"<< optionStr;
		QString zw = "\\begin{" + name + "}#S";
		if (!results.contains(zw))
			results << zw;
		zw = "\\end{" + name + "}#S";
		if (!results.contains(zw))
			results << zw;
		return results;
	}
	if (rxInput.indexIn(line) > -1) {
		QString name = rxInput.cap(1);
		name = kpsewhich(name);
		if (!name.isEmpty() && name != fileName) // avoid indefinite loops
			results << readPackage(name, parsedPackages);
		return results;
	}
	if (rxNewLength.indexIn(line) > -1) {
		QString name = "\\" + rxNewLength.cap(1);
		if (name.contains("@"))
			return results;
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxNewCounter.indexIn(line) > -1) {
		QString name = "\\the" + rxNewCounter.cap(1);
		if (name.contains("@"))
			return results;
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxDecMathSym.indexIn(line) > -1) {
		QString name = "\\" + rxDecMathSym.cap(1);
		if (name.contains("@"))
			return results;
		name.append("#Sm");
		if (!results.contains(name))
			results << name;
		return results;
	}
	if (rxRequire.indexIn(line) > -1) {
		QString arg = rxRequire.cap(1);
		foreach (QString elem, arg.split(',')) {
			QString package = elem.remove(' ');
			if (!package.isEmpty())
				results << "#include:" + package;
		}
		return results;
	}
	if (rxRequireStart.indexIn(line) > -1) {
		QString arg = rxRequireStart.cap(1);
		foreach (QString elem, arg.split(',')) {
			QString package = elem.remove(' ');
			if (!package.isEmpty())
				results << "#include:" + package;
		}
		inRequirePackage = true;
	}
	if (rxLoadClass.indexIn(line) > -1) {
		QString arg = rxLoadClass.cap(1);
		if (!arg.isEmpty()) {
			if (mPackageAliases.contains(arg))
				foreach (QString elem, mPackageAliases.values(arg)) {
					results << "#include:" + elem;
				}
			else
				results << "#include:" + arg;
		}
		return results;
	}
	return results;
}

QStringList LatexStyleParser::readPackage(QString fileName, QStringList &parsedPackages) const
{
	if (parsedPackages.contains(fileName))
		return QStringList();
	QFile data(fileName);
	QStringList results;
	if (data.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QTextStream stream(&data);
		parsedPackages << fileName;
		QString line;
		bool inRequirePackage = false;
		while (!stream.atEnd()) {
			line = LatexParser::cutComment(stream.readLine());
			results << parseLine(line, inRequirePackage, parsedPackages, fileName);
		}
	}
	return results;
}

QString LatexStyleParser::kpsewhich(QString name, QString dirName) const
{
	if (name.startsWith("."))
		return "";  // don't check .sty/.cls
	QString fn = name;
	if (!kpseWhichCmd.isEmpty()) {
		QProcess myProc(0);
		QStringList options;
		if (!dirName.isEmpty()) {
			options << "-path=" + dirName;
		}
		options << fn;
		myProc.start(kpseWhichCmd, options);
		myProc.waitForFinished();
		if (myProc.exitCode() == 0) {
			fn = myProc.readAllStandardOutput();
			fn = fn.split('\n').first().trimmed(); // in case more than one results are present
		} else
			fn.clear();
	}
	return fn;
}

QStringList LatexStyleParser::readPackageTexDef(QString fn) const
{
	if (!fn.endsWith(".sty"))
		return QStringList();

	QString fname = fn.left(fn.length() - 4);
	QProcess myProc(0);

	//add exec search path
	if (!texdefDir.isEmpty()) {
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		if (env.value("SHELL") == "/bin/bash") {
			env.insert("PATH", env.value("PATH") + ":" + texdefDir);
			myProc.setProcessEnvironment(env);
		}
	}
	QStringList args;
	QString result;
	args << "-t" << "latex" << "-l" << "-p" << fname;
	myProc.start(texdefDir + "texdef", args);
	args.clear();
	myProc.waitForFinished();
	if (myProc.exitCode() != 0)
		return QStringList();

	result = myProc.readAllStandardOutput();
	QStringList lines = result.split('\n');

	bool incl = false;
	for (int i = 0; i < lines.length(); i++) {
		if (lines.at(i).startsWith("Defined")) {
			QString name = lines.at(i);
			name = name.mid(17);
			name = name.left(name.length() - 2);
			incl = (name == fn);
			if (!incl)
				args << "#include:" + name;
		}
		if (incl && lines.at(i).startsWith("\\")) {
			if (lines.at(i).startsWith("\\\\")) {
				QString zw = lines.at(i) + "#S";
				zw.remove(0, 1);
				//args<<zw;
			} else {
				args << lines.at(i) + "#S";
			}
		}
	}
	// replace tex env def by latex commands
	QStringList zw = args.filter(QRegExp("\\\\end.+"));
	foreach (const QString &elem, zw) {
		QString begin = elem;
		begin.remove(1, 3);
		int i = args.indexOf(begin);
		if (i != -1) {
			QString env = begin.mid(1, begin.length() - 3);
			args.replace(i, "\\begin{" + env + "}");
			i = args.indexOf(elem);
			args.replace(i, "\\end{" + env + "}");
		}
	}

	return args;
}

QStringList LatexStyleParser::readPackageTracing(QString fn) const
{
	if (!fn.endsWith(".sty"))
		return QStringList();

	fn.chop(4);

	QString tempPath = QDir::tempPath() + QDir::separator() + "." + QDir::separator();
	QTemporaryFile *tf = new QTemporaryFile(tempPath + "XXXXXX.tex");
	if (!tf) return QStringList();
	tf->open();

	QTextStream out(tf);
	out << "\\documentclass{article}\n";
	out << "\\usepackage{filehook}\n";
	out << "\\usepackage{currfile}\n";
	out << "\\AtBeginOfEveryFile{\\message{^^Jentering file \\currfilename ^^J}}\n";
	out << "\\AtEndOfEveryFile{\\message{^^Jleaving file \\currfilename ^^J}}\n";
	out << "\\tracingonline=1\n";
	out << "\\tracingassigns=1\n";
	out << "\\usepackage{" << fn << "}\n";
	out << "\\tracingassigns=0\n";
	out << "\\AtBeginOfEveryFile{}\n";
	out << "\\AtEndOfEveryFile{}\n";
	out << "\\begin{document}\n";
	out << "\\end{document}";
	tf->close();


	QProcess myProc(0);
	//add exec search path
	if (!texdefDir.isEmpty()) {
		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		if (env.value("SHELL") == "/bin/bash") {
			env.insert("PATH", env.value("PATH") + ":" + texdefDir);
			myProc.setProcessEnvironment(env);
		}
	}
	QStringList args;
	QString result;
	args << "-draftmode" << "-interaction=nonstopmode" << "--disable-installer" << tf->fileName();
	myProc.setWorkingDirectory(QFileInfo(tf->fileName()).absoluteDir().absolutePath());
	myProc.start(texdefDir + "pdflatex", args);
	args.clear();
	myProc.waitForFinished();
	if (myProc.exitCode() != 0)
		return QStringList();

	result = myProc.readAllStandardOutput();
	QStringList lines = result.split('\n');

	QStack<QString> stack;
	foreach (const QString elem, lines) {
		if (elem.startsWith("entering file")) {
			QString name = elem.mid(14);
			stack.push(name);
			if (stack.size() == 2) { // first level of include
				args << "#include:" + name;
			}
		}
		if (elem.startsWith("leaving file")) {
			stack.pop();
			if (stack.isEmpty())
				break;
		}
		if (stack.size() == 1 && elem.endsWith("=undefined}")) {
			if (elem.startsWith("{changing ")  && !elem.contains("@")) {
				QString zw = elem.mid(10);
				zw.chop(11);
				if (!args.contains(zw + "#S"))
					args << zw + "#S";
			}
		}
	}

	// replace tex env def by latex commands
	QStringList zw = args.filter(QRegExp("\\\\end.+"));
	foreach (const QString &elem, zw) {
		QString begin = elem;
		begin.remove(1, 3);
		int i = args.indexOf(begin);
		if (i != -1) {
			QString env = begin.mid(1, begin.length() - 3);
			args.replace(i, "\\begin{" + env + "}");
			i = args.indexOf(elem);
			args.replace(i, "\\end{" + env + "}");
		}
	}

	delete tf;
	return args;
}
