#ifndef SMALLUSEFULFUNCTIONS_T_H
#define SMALLUSEFULFUNCTIONS_T_H
#ifndef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "smallUsefulFunctions.h"
#include "testutil.h"
#include <QtTest/QtTest>


/*QList<TestToken>& operator<< (QList<TestToken> & list, const char* str){
	return list << TestToken(str);
}*/

class SmallUsefulFunctionsTest: public QObject{
	Q_OBJECT
private slots:
	void test_getCommand_data(){
		QTest::addColumn<QString>("line");
		QTest::addColumn<int>("pos");
		QTest::addColumn<QString>("outCmd");
		QTest::addColumn<int>("out");

		QTest::newRow("before") << "test \\section{sec}" << 0 << QString() << 0;
		QTest::newRow("before 2") << "test \\section{sec}" << 3 << QString() << 3;
		QTest::newRow("before 3") << "test \\section{sec}" << 4 << QString() << 4;
		QTest::newRow("at cmd start") << "\\section{sec}" << 0 << "\\section" << 8;
		QTest::newRow("at cmd start 2") << "test \\section{sec}" << 5 << "\\section" << 13;
		QTest::newRow("at cmd end") << "\\section{sec} end" << 7 << "\\section" << 8;
		QTest::newRow("at cmd end2") << "\\section nothing" << 7 << "\\section" << 8;
		QTest::newRow("at cmd end3") << "\\section" << 7 << "\\section" << 8;
		QTest::newRow("on brace") << "\\section{sec}" << 8 << QString() << 8;
		QTest::newRow("on optional brace") << "\\section[opt]{sec}" << 8 << QString() << 8;
		QTest::newRow("in argument") << "\\section{sec}" << 10 << QString() << 10;
	}
	void test_getCommand(){
		QFETCH(QString, line);
		QFETCH(int, pos);
		QFETCH(QString, outCmd);
		QFETCH(int, out);
		QString retCmd;
		int ret = getCommand(line, retCmd, pos);
		QEQUAL(ret, out);
		QEQUAL(retCmd, outCmd);
	}
	void test_trimLeft_data(){
		QTest::addColumn<QString>("line");
		QTest::addColumn<QString>("trimmedLine");

		QTest::newRow("leftSpace") << " ab" << "ab";
		QTest::newRow("rightSpace") << "ab " << "ab ";
		QTest::newRow("bothSpace") << " ab " << "ab ";
	}
	void test_trimLeft(){
		QFETCH(QString, line);
		QFETCH(QString, trimmedLine);
		QEQUAL(trimLeft(line), trimmedLine);
	}
    void test_trimRight_data(){
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("trimmedLine");

        QTest::newRow("leftSpace") << " ab" << " ab";
        QTest::newRow("rightSpace") << "ab " << "ab";
        QTest::newRow("bothSpace") << " ab " << " ab";
    }
    void test_trimRight(){
        QFETCH(QString, line);
        QFETCH(QString, trimmedLine);
        QEQUAL(trimRight(line), trimmedLine);
    }
	void test_findCommandWithArgs_data(){
		QTest::addColumn<QString>("line");
		QTest::addColumn<int>("offset");
		QTest::addColumn<QString>("cmd");
		QTest::addColumn<QStringList>("args");
		QTest::addColumn<int>("col");

		QTest::newRow("cmd") << "\\section{foo}" << 0 << "\\section" << (QStringList() << "{foo}") << 0;
		QTest::newRow("text before") << "foo \\section{foo}" << 0 << "\\section" << (QStringList() << "{foo}") << 4;
		QTest::newRow("text remainder") << "\\section{foo} remainder" << 0 << "\\section" << (QStringList() << "{foo}") << 0;
		QTest::newRow("empty") << "" << 0 << "" << QStringList() << -1;
		QTest::newRow("no cmd") << "foo bar" << 0 << "" << QStringList() << -1;
		QTest::newRow("cmd without arg") << "foo \\pm bar" << 0 << "\\pm" << QStringList() << 4;
		QTest::newRow("cmd with opt") << "\\section[opt1]{foo}" << 0 << "\\section" << (QStringList() << "[opt1]" << "{foo}") << 0;
		QTest::newRow("cmd two opt") << "\\section[opt1][opt2]{foo}" << 0 << "\\section" << (QStringList() << "[opt1]" << "[opt2]" << "{foo}") << 0;
		QTest::newRow("cmd with opt without arg") << "\\section[opt1]" << 0 << "\\section" << (QStringList() << "[opt1]") << 0;
	}
	void test_findCommandWithArgs(){
		QFETCH(QString, line);
		QFETCH(int, offset);
		QFETCH(QString, cmd);
		QFETCH(QStringList, args);
		QFETCH(int, col);
		QString ret_cmd;
		QStringList ret_args;
		int ret_col = findCommandWithArgs(line, ret_cmd, ret_args, 0, offset);
		QEQUAL(ret_col, col);
		QEQUAL(ret_cmd, cmd);
		QEQUAL(ret_args.count(), args.count());
		for (int i=0; i<args.count(); i++) {
			QEQUAL2(ret_args[i], args[i], QString("in argument %1").arg(i));
		}
	}
	void test_latexToText_data(){
		QTest::addColumn<QString>("in");
		QTest::addColumn<QString>("out");

		QTest::newRow("basic") << "\\texorpdfstring{foo}{bar}" << "bar";
		QTest::newRow("context") << "spam \\texorpdfstring{foo}{bar} and eggs" << "spam bar and eggs";
		QTest::newRow("multiple") << "spam \\texorpdfstring{foo}{bar} and \\texorpdfstring{foo}{eggs}" << "spam bar and eggs";
		QTest::newRow("spaces") << "\\texorpdfstring  {foo}  {bar}" << "bar";
		QTest::newRow("tabs") << "\\texorpdfstring\t\t{foo}\t\t{bar} spam" << "bar spam";
		QTest::newRow("texorpdfstring-noarg") << "a \\texorpdfstring no arg" << "a \\texorpdfstring no arg";
		QTest::newRow("texorpdfstring-unbalanced") << "a \\texorpdfstring{unbalanced arg" << "a \\texorpdfstring{unbalanced arg";
		QTest::newRow("texorpdfstring-singlearg") << "a \\texorpdfstring{single} arg" << "a \\texorpdfstring{single} arg";
		QTest::newRow("texorpdfstring-unbalanced2") << "a \\texorpdfstring{second}{unbalanced arg" <<  "a \\texorpdfstring{second}{unbalanced arg" ;
		QTest::newRow("discretionary hyphen") << "Wurst\\-salat" << "Wurstsalat";
	}
	void test_latexToText(){
		QFETCH(QString, in);
		QFETCH(QString, out);
		QEQUAL(latexToText(in), out);
	}
	void test_joinLinesExceptCommentsAndEmptyLines_data(){
        QTest::addColumn<QStringList>("in");
		QTest::addColumn<QStringList>("out");

		QTest::newRow("simple")
				<< (QStringList() << "ab" << "cd")
				<< (QStringList() << "ab cd");
		QTest::newRow("spaces")
				<< (QStringList() << "\tab " << "\tcd\t" << "ef\n")
				<< (QStringList() << "\tab cd ef");
		QTest::newRow("lineAsSeparator")
				<< (QStringList() << "\tab" << "" << "ef\n")
				<< (QStringList() << "\tab" << "" << "ef");
	}
	void test_joinLinesExceptCommentsAndEmptyLines(){
		QFETCH(QStringList, in);
		QFETCH(QStringList, out);
		QStringList joinedLines = joinLinesExceptCommentsAndEmptyLines(in);
		QEQUAL(joinedLines.length(), out.length());
		for (int i=0; i<joinedLines.count(); i++) {
			QEQUAL2(joinedLines[i], out[i], QString("in join #%1").arg(i));
		}
	}
	void test_splitLines_data() {
		QTest::addColumn<int>("maxChars");
		QTest::addColumn<QStringList>("in");
		QTest::addColumn<QStringList>("out");

		QTest::newRow("splitPosition") << 10
				<< (QStringList() << "01234 6789 12")
				<< (QStringList() << "01234 6789" << "12");
		QTest::newRow("splitPosition2") << 9
				<< (QStringList() << "01234 6789 12")
				<< (QStringList() << "01234" << "6789 12");
		QTest::newRow("commentContinuation") << 24
				<< (QStringList() << "this is a wrap % inside a comment")
				<< (QStringList() << "this is a wrap % inside" << "% a comment");
		QTest::newRow("keepIndentSpace") << 11
				<< (QStringList() << "  01234 6789 12")
				<< (QStringList() << "  01234" << "  6789 12");
		QTest::newRow("keepIndentTab") << 10
				<< (QStringList() << "\t01234 6789 12")
				<< (QStringList() << "\t01234" << "\t6789 12");
		QTest::newRow("keepIndentComment") << 26
				<< (QStringList() << "  this is a wrap % inside a comment")
				<< (QStringList() << "  this is a wrap % inside" << "  % a comment");
	}
	void test_splitLines(){
		QFETCH(int, maxChars);
		QFETCH(QStringList, in);
		QFETCH(QStringList, out);
		QRegExp breakChars("[ \t\n\r]");
		QStringList splittedLines = splitLines(in, maxChars, breakChars);
		QEQUAL(splittedLines.length(), out.length());
		for (int i=0; i<splittedLines.count(); i++) {
			QEQUAL2(splittedLines[i], out[i], QString("in split #%1").arg(i));
		}
	}
    /*
	void test_getSimplifiedSVNVersion_data(){
		QTest::addColumn<QString>("versionString");
		QTest::addColumn<int>("out");

		QTest::newRow("plain") << "1314" << 1314;
		QTest::newRow("modified") << "1314M" << 1314;
		QTest::newRow("mixed") << "1314:1315" << 1314;
		QTest::newRow("unknown") << "Unknown" << 0;
	}
	void test_getSimplifiedSVNVersion(){
		QFETCH(QString, versionString);
		QFETCH(int, out);
		QEQUAL(getSimplifiedSVNVersion(versionString), out);
    }*/

	void test_tokenizeCommandLine_data(){
		QTest::addColumn<QString>("cmdLine");
		QTest::addColumn<QStringList>("args");

		QTest::newRow("empty") << "" << QStringList();
		QTest::newRow("cmd") << "cmd" << (QStringList() << "cmd");
		QTest::newRow("cmd arg") << "cmd arg" << (QStringList() << "cmd" << "arg");
		QTest::newRow("cmd arg arg2") << "cmd arg arg2" << (QStringList() << "cmd" << "arg" << "arg2");
		QTest::newRow("\"enquoted cmd\" arg") << "\"enquoted cmd\" arg" << (QStringList() << "\"enquoted cmd\"" << "arg");
		QTest::newRow("cmd \"enquouted arg\"") << "cmd \"enquoted arg\"" << (QStringList() << "cmd" << "\"enquoted arg\"");
		QTest::newRow("redirect") << "cmd > file" << (QStringList() << "cmd" << ">" << "file");
		QTest::newRow("redirect nospace") << "cmd>file" << (QStringList() << "cmd" << ">" << "file");
		QTest::newRow("redirect stderr") << "cmd 2> file" << (QStringList() << "cmd" << "2>" << "file");
		QTest::newRow("redirect quote") << "cmd \\C \"type file1.txt > file2.txt\"" << (QStringList() << "cmd" << "\\C" << "\"type file1.txt > file2.txt\"");
	}
	void test_tokenizeCommandLine(){
		QFETCH(QString, cmdLine);
		QFETCH(QStringList, args);

		QStringList result = tokenizeCommandLine(cmdLine);
		QEQUAL(result.join("|||"), args.join("|||"));
	}

	void test_extractOutputRedirection_data(){
		QTest::addColumn<QStringList>("commandArgs");
		QTest::addColumn<QStringList>("extracted");
		QTest::addColumn<QString>("stdOut");
		QTest::addColumn<QString>("stdErr");

		QTest::newRow("empty") << QStringList() << QStringList() << QString() << QString();
		QTest::newRow("cmd arg") << QString("cmd arg").split(' ') << QString("cmd arg").split(' ') << QString() << QString();
		QTest::newRow("stdout") << QString("cmd > file").split(' ') << (QStringList() << "cmd") << "file" << QString();
		QTest::newRow("stdout2") << QString("cmd >file").split(' ') << (QStringList() << "cmd") << "file" << QString();
		QTest::newRow("stderr") << QString("cmd 2> file").split(' ') << (QStringList() << "cmd") << QString() << "file";
		QTest::newRow("stderr2") << QString("cmd 2>file").split(' ') << (QStringList() << "cmd") << QString() << "file";
		QTest::newRow("stdout stderr") << QString("cmd > out 2> err").split(' ') << (QStringList() << "cmd") << "out" << "err";
	}
	void test_extractOutputRedirection(){
		QFETCH(QStringList, commandArgs);
		QFETCH(QStringList, extracted);
		QFETCH(QString, stdOut);
		QFETCH(QString, stdErr);

		QString resultStdOut;
		QString resultStdErr;
		QStringList result = extractOutputRedirection(commandArgs, resultStdOut, resultStdErr);
		QEQUAL(result.join("|||"), extracted.join("|||"));
		QEQUAL(resultStdOut, stdOut);
		QEQUAL(resultStdErr, stdErr);
	}

	void test_minimalJsonParse_data(){
		QTest::addColumn<QString>("jsonData");
		QTest::addColumn<bool>("retVal");
		QTest::addColumn<QStringList>("keys");
		QTest::addColumn<QStringList>("vals");

		QTest::newRow("empty") << "" << true << QStringList() << QStringList();
		QTest::newRow("empty") << "{}" << true << QStringList() << QStringList();
		QTest::newRow("single") << " { \"key\"  : \"val\" } " << true << (QStringList() << "key") << (QStringList() << "val");
		QTest::newRow("two") << "{\"key\":\"val\",\"key2\":\"val2\"} " << true << (QStringList() << "key" << "key2") << (QStringList() << "val" << "val2");
		QTest::newRow("escapedQoute") << "{\"key\":\"val\\\"more\"}" << true << (QStringList() << "key") << (QStringList() << "val\"more");
		QTest::newRow("missingClose") << "{\"key\":\"val\"" << false << (QStringList() << "key") << (QStringList() << "val");
		QTest::newRow("missingQuote1") << "{key:\"val\"}" << false << QStringList() << QStringList();
		QTest::newRow("missingQuote2") << "{\"key:\"val\"}" << false << QStringList() << QStringList();
		QTest::newRow("missingQuote3") << "{key\":\"val\"}" << false << QStringList() << QStringList();
	}
	void test_minimalJsonParse(){
		QFETCH(QString, jsonData);
		QFETCH(bool, retVal);
		QFETCH(QStringList, keys);
		QFETCH(QStringList, vals);

		QHash<QString, QString> data;
		QEQUAL(minimalJsonParse(jsonData, data), retVal);
		for (int i=0; i<keys.count(); i++) {
			QEQUAL2(data[keys[i]], vals[i], QString("for key: %1").arg(keys[i]));
		}
	}

	void test_enquoteDequoteString_data(){
		QTest::addColumn<QString>("in");
		QTest::addColumn<QString>("out");

		QTest::newRow("empty") << "" << "\"\"";
		QTest::newRow("plain") << "plain" << "\"plain\"";
		QTest::newRow("insideQuote") << "inside \"quote" << "\"inside \\\"quote\"";
		QTest::newRow("startQuote") << "\"startQuote" << "\"\\\"startQuote\"";
		QTest::newRow("endQuote") << "endQuote\"" << "\"endQuote\\\"\"";
	}
	void test_enquoteDequoteString(){
		QFETCH(QString, in);
		QFETCH(QString, out);

		QString quoted = enquoteStr(in);
		QEQUAL2(quoted, out, "while enqouting");
		QString unquoted = dequoteStr(quoted);
		QEQUAL2(unquoted, in, "while dequoting");
	}

	void test_joinUnicodeSurrogate_data(){
		QTest::addColumn<QChar>("highSurrogate");
		QTest::addColumn<QChar>("lowSurrogate");
		QTest::addColumn<unsigned int>("val");

		QTest::newRow("CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B") << QChar(55360) << QChar(56390) << 0x20046U;
		QTest::newRow("min") << QChar(0xD800) << QChar(0xDC00) << 0x10000U;
		QTest::newRow("rumi number fifty") << QChar(0xD803) << QChar(0xDE6D) << 0x10E6DU;
		QTest::newRow("musical symbol g clef") << QChar(0xD834) << QChar(0xDD1E) << 0x1D11EU;
		QTest::newRow("max") << QChar(0xDBFF) << QChar(0xDFFF) << 0x10FFFFU;
	}
	void test_joinUnicodeSurrogate(){
		QFETCH(QChar, lowSurrogate);
		QFETCH(QChar, highSurrogate);
		QFETCH(unsigned int, val);
		QEQUAL(joinUnicodeSurrogate(highSurrogate, lowSurrogate), val);
	}

	void test_replaceFileExtension_data(){
		QTest::addColumn<bool>("appendIfNoExtension");
		QTest::addColumn<QString>("file");
		QTest::addColumn<QString>("ext");
		QTest::addColumn<QString>("result");

		QTest::newRow("noExtension") << false << "c:/test" << "log" << QString();
		QTest::newRow("noExtensionAdd") << true << "c:/test" << "log" << "c:/test.log";
		QTest::newRow("simple") << false << "c:/test.tex" << "log" << "c:/test.log";
		QTest::newRow("relative") << false << "../dir/test.tex" << "log" << "../dir/test.log";
		QTest::newRow("doubleExtSrc") << false << "test.synctex.gz" << "log" << "test.synctex.log";
		QTest::newRow("doubleExtTarget") << false << "test.tex" << "synctex.gz" << "test.synctex.gz";
		QTest::newRow("dotExt") << false << "test.tex" << ".log" << "test.log";
		QTest::newRow("dotExtAdd") << true << "c:/test" << ".log" << "c:/test.log";
		QTest::newRow("multiDot") << false << "c:/a.b.c.tex" << ".log" << "c:/a.b.c.log";
	}
	void test_replaceFileExtension() {
		QFETCH(bool, appendIfNoExtension);
		QFETCH(QString, file);
		QFETCH(QString, ext);
		QFETCH(QString, result);

		QEQUAL(replaceFileExtension(file, ext, appendIfNoExtension), result);
	}

	void test_getParamItem_data() {
		QTest::addColumn<QString>("line");
		QTest::addColumn<int>("pos");
		QTest::addColumn<bool>("stopAtWhiteSpace");
		QTest::addColumn<QString>("result");

		QTest::newRow("singleStart") << "\\section{Single}" << 9 << false << "Single";
		QTest::newRow("singleMid") << "\\section{Single}" << 11 << false << "Single";
		QTest::newRow("singleEnd") << "\\section{Single}" << 15 << false << "Single";

		QTest::newRow("singleSpaced") << "\\section{With Space}" << 9 << false << "With Space";
		QTest::newRow("singleSpaceStop") << "\\section{With Space}" << 9 << true << "With";
		QTest::newRow("singleSpaceStop2") << "\\section{With Space}" << 16 << true << "Space";

		QTest::newRow("optArg") << "\\caption[short]{long}" << 9 << false << "short";
		QTest::newRow("optArg") << "\\caption[short]{long}" << 11 << false << "short";
		QTest::newRow("optArg") << "\\caption[short]{long}" << 14 << false << "short";

		QTest::newRow("firstListParam") << "\\cmd{one, two, three}" << 5 << false << "one";
		QTest::newRow("secondListParam") << "\\cmd{one, two, three}" << 10 << false << " two";
		QTest::newRow("secondListParam2") << "\\cmd{one, two, three}" << 10 << true << "two";
		QTest::newRow("thirdListParam") << "\\cmd{one, two, three}" << 16 << true << "three";

		QTest::newRow("internalArg") << "\\cmd{an \\internal[opt]{cmd}}" << 6 << false << "an \\internal[opt]{cmd}";
		QTest::newRow("internalArg2") << "\\cmd{an \\internal[opt]{cmd}}" << 12 << true << "\\internal[opt]{cmd}";
		QTest::newRow("internalArg3") << "{\\cmd{an \\internal[opt]{cmd}} end}" << 33 << false << "\\cmd{an \\internal[opt]{cmd}} end";

		// Behavior for pos outside of a parameter is not part of the specification and subject to later change.
		// The following tests just document the current behavior.
		QTest::newRow("outside") << "\\section{Single}" << 8 << false << "\\section{Single}";
		QTest::newRow("outsideEnd") << "\\section{Single}" << 16 << false << "\\section{Single}";
		QTest::newRow("betweenArg") << "\\caption[short]{long}" << 15 << false << "\\caption[short]{long}";
		QTest::newRow("betweenColonAndSpaceArg") << "\\cmd{one, two}" << 9 << false << " two";
		QTest::newRow("betweenColonAndSpaceArg") << "\\cmd{one, two}" << 9 << true << "";
	}

	void test_getParamItem() {
		QFETCH(QString, line);
		QFETCH(int, pos);
		QFETCH(bool, stopAtWhiteSpace);
		QFETCH(QString, result);

		QEQUAL(getParamItem(line, pos, stopAtWhiteSpace), result);
	}

	void test_addMostRecent_data() {
		QTest::addColumn<QString>("item");
		QTest::addColumn<QStringList>("list");
		QTest::addColumn<int>("maxLength");
		QTest::addColumn<QStringList>("result");

		QTest::newRow("addEmpty") << "item1" << QStringList() << 2 << (QStringList() << "item1");
		QTest::newRow("addMax") << "item2" << (QStringList() << "item1") << 2 << (QStringList() << "item2" << "item1");
		QTest::newRow("addBeyondMax") << "item3" << (QStringList() << "item1" << "item2") << 2 << (QStringList() << "item3" << "item1");
		QTest::newRow("addExisting1") << "item1" << (QStringList() << "item1" << "item2") << 2 << (QStringList() << "item1" << "item2");
		QTest::newRow("addExisting2") << "item2" << (QStringList() << "item1" << "item2") << 2 << (QStringList() << "item2" << "item1");
	}

	void test_addMostRecent() {
		QFETCH(QString, item);
		QFETCH(QStringList, list);
		QFETCH(int, maxLength);
		QFETCH(QStringList, result);

		addMostRecent(item, list, maxLength);
		QEQUAL(list.length(), result.length());
		for (int i=0; i<list.length(); i++) {
			QEQUAL(list[i], result[i]);
		}
	}

	void test_indicesOf_data() {
		QTest::addColumn<QString>("line");
		QTest::addColumn<QString>("word");
		QTest::addColumn<bool>("caseSensitive");
		QTest::addColumn<QList<int> >("result");

		QTest::newRow("noMatch") << "A test, a test, a test" << "foo" << false << (QList<int>());
		QTest::newRow("multiCS") << "A test, a test, a test" << "a" << true << (QList<int>() << 8 << 16);
		QTest::newRow("multiCSI") << "A test, a test, a test" << "a" << false << (QList<int>() << 0 << 8 << 16);
		QTest::newRow("multiCS_end") << "A test, a test, a test" << "test" << true << (QList<int>() << 2 << 10 << 18);
	}

	void test_indicesOf() {
		QFETCH(QString, line);
		QFETCH(QString, word);
		QFETCH(bool, caseSensitive);
		QFETCH(QList<int>, result);

		QList<int> indices = indicesOf(line, word, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
		QEQUAL(indices.length(), result.length());
		for (int i=0; i<indices.length(); i++) {
			QEQUAL(indices[i], result[i]);
		}
	}

	void test_indicesOf_RegExp_data() {
		QTest::addColumn<QString>("line");
		QTest::addColumn<QString>("rxString");
		QTest::addColumn<QList<int> >("result");

		QTest::newRow("noMatch") << "A test, a test, a test" << "foo" << (QList<int>());
		QTest::newRow("multi1") << "A test, a test, a test" << "a\\s+t" << (QList<int>() << 8 << 16);
		QTest::newRow("multi2") << "A test, a test, a test" << "[aA]\\s+t" << (QList<int>() << 0 << 8 << 16);
		QTest::newRow("multi3") << "A test, a test, a test" << "test" << (QList<int>() << 2 << 10 << 18);
	}

	void test_indicesOf_RegExp() {
		QFETCH(QString, line);
		QFETCH(QString, rxString);
		QFETCH(QList<int>, result);

		QList<int> indices = indicesOf(line, QRegExp(rxString));
		QEQUAL(indices.length(), result.length());
		for (int i=0; i<indices.length(); i++) {
			QEQUAL(indices[i], result[i]);
		}
	}
	
	void test_indexMin_data() {
		QTest::addColumn<int>("i");
		QTest::addColumn<int>("j");
		QTest::addColumn<int>("result");

		QTest::newRow("i<j") << 3 << 5 << 3;
		QTest::newRow("i=j") << 3 << 3 << 3;
		QTest::newRow("i>j") << 5 << 3 << 3;
		QTest::newRow("i<0") << -1 << 5 << 5;
		QTest::newRow("j<0") << 5 << -1 << 5;
		QTest::newRow("ij<0") << -1 << -1 << -1;
	}

	void test_indexMin() {
		QFETCH(int, i);
		QFETCH(int, j);
		QFETCH(int, result);
		QEQUAL(indexMin(i, j), result);
	}

    void test_simpleLexing_data();
    void test_simpleLexing();
    void test_latexLexing_data();
    void test_latexLexing();
};


#endif
#endif

