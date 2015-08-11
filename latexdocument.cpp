#include "latexdocument.h"
#include "qdocument.h"
#include "qformatscheme.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"
#include "qdocumentcursor.h"
#include "qeditor.h"
#include "latexcompleter.h"
#include "latexcompleter_config.h"
#include "smallUsefulFunctions.h"
#include <QtConcurrentMap>

//FileNamePair::FileNamePair(const QString& rel):relative(rel){};
FileNamePair::FileNamePair(const QString& rel, const QString& abs):relative(rel),absolute(abs){};

LatexDocument::LatexDocument(QObject *parent):QDocument(parent),remeberAutoReload(false),mayHaveDiffMarkers(false),edView(0),mAppendixLine(0)
{
	baseStructure = new StructureEntry(this,StructureEntry::SE_DOCUMENT_ROOT);
	magicCommentList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	labelList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	todoList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	bibTeXList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	blockList = new StructureEntry(this, StructureEntry::SE_OVERVIEW);
	
	magicCommentList->title=tr("MAGIC_COMMENTS");
	labelList->title=tr("LABELS");
	todoList->title=tr("TODO");
	bibTeXList->title=tr("BIBLIOGRAPHY");
	blockList->title=tr("BLOCKS");
	mLabelItem.clear();
	mBibItem.clear();
	mUserCommandList.clear();
	mMentionedBibTeXFiles.clear();
	masterDocument=0;
	this->parent=0;

    unclosedEnv.id=-1;
    latexLikeChecking=true; //TODO: needs to bne changeable

    lp=LatexParser::getInstance();

    SynChecker.verbatimFormat=getFormatId("verbatim");
    SynChecker.setLtxCommands(LatexParser::getInstance());
    SynChecker.setErrFormat(syntaxErrorFormat);
    SynChecker.start();

    connect(&SynChecker, SIGNAL(checkNextLine(QDocumentLineHandle*,bool,int)), SLOT(checkNextLine(QDocumentLineHandle *,bool,int)), Qt::QueuedConnection);
}
LatexDocument::~LatexDocument(){
    SynChecker.stop();
    SynChecker.wait();
	if (!magicCommentList->parent) delete magicCommentList;
	if (!labelList->parent) delete labelList;
	if (!todoList->parent) delete todoList;
	if (!bibTeXList->parent) delete bibTeXList;
    if (!blockList->parent) delete blockList;

    foreach (QDocumentLineHandle *dlh, mLineSnapshot) {
        dlh->deref();
    }
    mLineSnapshot.clear();

	delete baseStructure;
}

void LatexDocument::setFileName(const QString& fileName){
	//clear all references to old editor
	if (this->edView){
		StructureEntryIterator iter(baseStructure);
		while (iter.hasNext()) iter.next()->setLine(0);
	}
	
	this->fileName=fileName;
	this->fileInfo=QFileInfo(fileName);
	this->edView=0;
}
void LatexDocument::setEditorView(LatexEditorView* edView){
	this->fileName=edView->editor->fileName();
	this->fileInfo=edView->editor->fileInfo();
	this->edView=edView;
	if(baseStructure){
		baseStructure->title=fileName;
		emit updateElement(baseStructure);
	}
}
LatexEditorView *LatexDocument::getEditorView() const{
	return this->edView;
}

QString LatexDocument::getFileName() const{
	return fileName;
}

bool LatexDocument::isHidden(){
    return parent->hiddenDocuments.contains(this);
}
QFileInfo LatexDocument::getFileInfo() const{
	return fileInfo;
}

QMultiHash<QDocumentLineHandle*,FileNamePair>& LatexDocument::mentionedBibTeXFiles(){
	return mMentionedBibTeXFiles;
}
const QMultiHash<QDocumentLineHandle*,FileNamePair>& LatexDocument::mentionedBibTeXFiles() const{
	return mMentionedBibTeXFiles;
}
QStringList LatexDocument::listOfMentionedBibTeXFiles() const{
  QStringList result;
  foreach(const FileNamePair& fnp,mMentionedBibTeXFiles.values())
    result<<fnp.absolute;
  return result;
}

QDocumentSelection LatexDocument::sectionSelection(StructureEntry* section){
	QDocumentSelection result = {-1, -1, -1, -1};
	
	if (section->type!=StructureEntry::SE_SECTION) return result;
	int startLine=section->getRealLineNumber();
	
	// find next section or higher
	StructureEntry* parent;
	int index;
	do {
		parent=section->parent;
		if (parent) {
			index=section->getRealParentRow();
			section=parent;
		} else index=-1;
	} while ((index>=0)&&(index>=parent->children.count()-1)&&(parent->type==StructureEntry::SE_SECTION));
	
	int endingLine=-1;
	if (index>=0 && index<parent->children.count()-1) {
		endingLine=parent->children.at(index+1)->getRealLineNumber();
	} else {
		// no ending section but end of document
		endingLine=findLineContaining("\\end{document}",startLine,Qt::CaseInsensitive);
		if (endingLine<0) endingLine=lines();
	}
	
	result.startLine=startLine;
	result.endLine=endingLine;
	result.end=0;
	result.start=0;
	return result;
}

void LatexDocument::initClearStructure() {
	mUserCommandList.clear();
	mLabelItem.clear();
	mBibItem.clear();
	mRefItem.clear();
	mMentionedBibTeXFiles.clear();
	
	mAppendixLine=0;
    mBeyondEnd=0;


	emit structureUpdated(this,0);

	const int CATCOUNT = 5;
	StructureEntry* categories[CATCOUNT] = {magicCommentList, labelList, todoList, bibTeXList, blockList};
	for (int i=0; i<CATCOUNT;i++)
		if (categories[i]->parent == baseStructure) {
			removeElementWithSignal(categories[i]);
			foreach (StructureEntry* se, categories[i]->children)
				delete se;
			categories[i]->children.clear();
		}

	for (int i=0;i<baseStructure->children.length();i++) {
		StructureEntry* temp = baseStructure->children[i];
		removeElementWithSignal(temp);
		delete temp;
	}
	
	baseStructure->title=fileName;
}

void LatexDocument::updateStructure() {
	initClearStructure();
	
    patchStructure(0, -1);
	
	emit structureLost(this);
}

/* Removes a deleted line from the structure view */
void LatexDocument::patchStructureRemoval(QDocumentLineHandle* dlh) {
	if(!baseStructure) return;
	bool completerNeedsUpdate=false;
	bool bibTeXFilesNeedsUpdate=false;
	bool updateSyntaxCheck=false;
	if (mLabelItem.contains(dlh)) {
		QList<ReferencePair> labels=mLabelItem.values(dlh);
		completerNeedsUpdate = true;
		mLabelItem.remove(dlh);
		foreach(const ReferencePair& rp,labels)
			updateRefsLabels(rp.name);
	}
	mRefItem.remove(dlh);
	if(mMentionedBibTeXFiles.remove(dlh))
		bibTeXFilesNeedsUpdate=true;
	if (mBibItem.contains(dlh)) {
		mBibItem.remove(dlh);
		bibTeXFilesNeedsUpdate=true;
	}

    QList<CodeSnippet> commands=mUserCommandList.values(dlh);
    foreach(CodeSnippet elem,commands){
        QString word=elem.word;
        int i=word.indexOf("{");
        if(i>=0) word=word.left(i);
        ltxCommands.possibleCommands["user"].remove(word);
		updateSyntaxCheck=true;
	}
	mUserCommandList.remove(dlh);

    QStringList removeIncludes=mIncludedFilesList.values(dlh);
    if(mIncludedFilesList.remove(dlh)>0){
        parent->removeDocs(removeIncludes);
        parent->updateMasterSlaveRelations(this);
    }
	
	QStringList removedUsepackages;
	removedUsepackages << mUsepackageList.values(dlh);
	mUsepackageList.remove(dlh);
	
	if(dlh==mAppendixLine){
		updateContext(mAppendixLine, 0, StructureEntry::InAppendix);
		mAppendixLine=0;
	}
	
	int linenr = indexOf(dlh);
	if (linenr==-1) linenr=lines();

	// check if line contains bookmark
	if(edView){
		for(int i=-1;i<10;i++){
			if(edView->hasBookmark(dlh,i)){
				emit bookmarkRemoved(dlh);
				edView->removeBookmark(dlh,i);
				break;
			}
		}
	}
	
	QList<StructureEntry*> categories=QList<StructureEntry*>() << magicCommentList << labelList << todoList << blockList << bibTeXList;
	foreach (StructureEntry* sec, categories) {
		int l=0;
		QMutableListIterator<StructureEntry*> iter(sec->children);
		while (iter.hasNext()){
			StructureEntry* se=iter.next();
			if(dlh==se->getLineHandle()) {
				emit removeElement(se,l);
				iter.remove();
				emit removeElementFinished();
				delete se;
			} else l++;
		}
	}

	LatexParser& latexParser = LatexParser::getInstance();
	QVector<StructureEntry*> parent_level(latexParser.structureDepth());
	
	QList<StructureEntry*> ls;
	mergeStructure(baseStructure, parent_level, ls, linenr, 1);
	
	// rehighlight current cursor position
	StructureEntry *newSection=0;
	if(edView){
		int i=edView->editor->cursor().lineNumber();
		if(i>=0) {
			newSection=findSectionForLine(i);
		}
	}
	
	emit structureUpdated(this,newSection);
	
	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();
	
	if (completerNeedsUpdate || bibTeXFilesNeedsUpdate)
		emit updateCompleter();
	
	if(!removedUsepackages.isEmpty() || updateSyntaxCheck){
        updateCompletionFiles(updateSyntaxCheck);
	}

}

// workaround to prevent false command recognition in definitions:
// Example: In \newcommand{\seeref}[1]{\ref{(see #1)}} the argument of \ref is not actually a label.
// Using this function we detect this case.
// TODO: a more general solution should make this dependent on if the command is inside a definition.
// However this requires a restructuring of the patchStructure. It would also allow categorizing
// the redefined command, e.g. as "%ref"
inline bool isDefinitionArgument(const QString &arg) {
	// equivalent to checking the regexp #[0-9], but faster:
	int pos = arg.indexOf("#");
	return (pos >= 0 && pos<arg.length()-1 && arg[pos+1].isDigit());
}

bool LatexDocument::patchStructure(int linenr, int count,bool recheck) {
    /* true means a second run is suggested as packages are loadeed which change the outcome
     * e.g. definition of specialDef command, but packages are load at the end of this method.
     */
    //qDebug()<<"begin Patch"<<QTime::currentTime().toString("HH:mm:ss:zzz");
    if(!parent->patchEnabled())
        return false;

    if (!baseStructure) return false;

    bool reRunSuggested=false;
    bool recheckLabels=true;
    if(count<0){
        count=lineCount();
        recheckLabels=false;
    }
	
	emit toBeChanged();
	
	bool completerNeedsUpdate=false;
	bool bibTeXFilesNeedsUpdate=false;
	bool bibItemsChanged=false;
	
	QDocumentLineHandle *oldLine=mAppendixLine; // to detect a change in appendix position
    QDocumentLineHandle *oldLineBeyond=mBeyondEnd; // to detect a change in end document position

    // get remainder
    TokenStack remainder;
    int lineNrStart=linenr;
    int newCount=count;
    if(linenr>0){
        QDocumentLineHandle *previous=line(linenr-1).handle();
        remainder=previous->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
        if(!remainder.isEmpty() && remainder.top().subtype!=Tokens::none){
            QDocumentLineHandle *lh=remainder.top().dlh;
            lineNrStart=lh->document()->indexOf(lh);
            if(linenr-lineNrStart>10) // limit search depth
                lineNrStart=linenr;
        }
    }
	

	
	bool updateSyntaxCheck=false;
	
	QList<StructureEntry*> flatStructure;
	
	// usepackage list
	QStringList removedUsepackages;
	QStringList addedUsepackages;
    QStringList removedUserCommands,addedUserCommands;
    QStringList lstFilesToLoad;

    //first pass: lex
    TokenStack oldRemainder;
    if(!recheck){
        QList<QDocumentLineHandle*> l_dlh;
//#pragma omp parallel for shared(l_dlh)
        for (int i=linenr; i<linenr+count; i++) {
            l_dlh<<line(i).handle();
            simpleLexLatexLine(line(i).handle());
        }
        //QtConcurrent::blockingMap(l_dlh,simpleLexLatexLine);
    }
    QDocumentLineHandle *lastHandle=line(linenr-1).handle();
    if(lastHandle){
        oldRemainder=lastHandle->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
    }
    for (int i=linenr; i<lineCount() && i<linenr+count; i++) {
        bool remainderChanged=latexDetermineContexts2(line(i).handle(),oldRemainder,lp);
        if(remainderChanged && i+1==linenr+count && i+1<lineCount()){ // remainder changed in last line which is to be checked
            count++; // check also next line ...
        }
    }

    if(linenr>lineNrStart){
        newCount=linenr+count-lineNrStart;
    }

    QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfMagicComments;
    QMutableListIterator<StructureEntry*> iter_magicComment(magicCommentList->children);
    findStructureEntryBefore(iter_magicComment,MapOfMagicComments,lineNrStart,newCount);

    QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfLabels;
    QMutableListIterator<StructureEntry*> iter_label(labelList->children);
    findStructureEntryBefore(iter_label,MapOfLabels,lineNrStart,newCount);

    QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfTodo;
    QMutableListIterator<StructureEntry*> iter_todo(todoList->children);
    findStructureEntryBefore(iter_todo,MapOfTodo,lineNrStart,newCount);

    QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfBlock;
    QMutableListIterator<StructureEntry*> iter_block(blockList->children);
    findStructureEntryBefore(iter_block,MapOfBlock,lineNrStart,newCount);
    QMultiHash<QDocumentLineHandle*,StructureEntry*> MapOfBibtex;
    QMutableListIterator<StructureEntry*> iter_bibTeX(bibTeXList->children);
    findStructureEntryBefore(iter_bibTeX,MapOfBibtex,lineNrStart,newCount);


    //updateSubsequentRemaindersLatex(this,linenr,count,lp);
    // force command from all line of which the actual line maybe subsequent lines (multiline commands)
    for (int i=lineNrStart; i<linenr+count; i++) {
		//update bookmarks
		if(edView && edView->hasBookmark(i,-1)){
			emit bookmarkLineUpdated(i);
		}

        QString curLine = line(i).text();
        QDocumentLineHandle *dlh=line(i).handle();
        if(!dlh)
            continue; //non-existing line ...
        TokenList tl=dlh->getCookieLocked(QDocumentLine::LEXER_COOKIE).value<TokenList >();
        QLineFormatAnalyzer lineFormatAnaylzer(line(i).getFormats());
		
        /*for(int j=0;j<curLine.length() && j < fmts.size();j++){
			if(fmts[j]==verbatimFormat || (fmts[j]==commentFormat && !parent->showCommentedElementsInStructure) ){
				curLine[j]=QChar(' ');
			}
        }*/
		
		// remove command,bibtex,labels at from this line
        QList<CodeSnippet> commands=mUserCommandList.values(dlh);
        foreach(CodeSnippet cs,commands){
            QString elem=cs.word;
			int i=elem.indexOf("{");
			if(i>=0) elem=elem.left(i);
			ltxCommands.possibleCommands["user"].remove(elem);
			removedUserCommands << elem;
            //updateSyntaxCheck=true;
		}
		if (mLabelItem.contains(dlh)) {
			QList<ReferencePair> labels=mLabelItem.values(dlh);
			completerNeedsUpdate = true;
			mLabelItem.remove(dlh);
			foreach(const ReferencePair& rp,labels)
				updateRefsLabels(rp.name);
		}
		mRefItem.remove(dlh);
        QStringList removedIncludes=mIncludedFilesList.values(dlh);
        mIncludedFilesList.remove(dlh);

		if (mUserCommandList.remove(dlh)>0) completerNeedsUpdate = true;
		if(mBibItem.remove(dlh))
			bibTeXFilesNeedsUpdate=true;
		
		removedUsepackages << mUsepackageList.values(dlh);
		if (mUsepackageList.remove(dlh)>0) completerNeedsUpdate = true;
		
		//remove old bibs files from hash, but keeps a temporary copy
		QStringList oldBibs;
		while (mMentionedBibTeXFiles.contains(dlh)) {
			QMultiHash<QDocumentLineHandle*, FileNamePair>::iterator it = mMentionedBibTeXFiles.find(dlh);
			Q_ASSERT(it.key() == dlh);
			Q_ASSERT(it != mMentionedBibTeXFiles.end());
			if (it == mMentionedBibTeXFiles.end()) break;
			oldBibs.append(it.value().relative);
			mMentionedBibTeXFiles.erase(it);
		}

		int col;
		//// TODO marker
		col = lineFormatAnaylzer.firstCol(getFormatId("commentTodo"));
		if (col >= 0) {
			QString text = curLine.mid(col, lineFormatAnaylzer.formatLength(col));
			if (text.startsWith("%")) {  // other todos like \todo are handled by the tokenizer below.
				bool reuse=false;
				StructureEntry *newTodo;
				if(MapOfTodo.contains(dlh)){
					newTodo=MapOfTodo.value(dlh);
					//parent->add(newTodo);
					newTodo->type=StructureEntry::SE_TODO;
					MapOfTodo.remove(dlh,newTodo);
					reuse=true;
				}else{
					newTodo=new StructureEntry(this, StructureEntry::SE_TODO);
				}
				newTodo->title=text.mid(1).trimmed();
				newTodo->setLine(line(i).handle(), i);
				newTodo->parent=todoList;
				if(!reuse) emit addElement(todoList,todoList->children.size()); //todo: why here but not in label?
				iter_todo.insert(newTodo);
			}
		}
		
		//// parameter comment
		if (curLine.startsWith("%&")) {
			int start = curLine.indexOf("-job-name=");
			if (start>=0) {
				int end = start+10; // += "-job-name=".length;
				if (end<curLine.length() && curLine[end]=='"') {
					// quoted filename
					end = curLine.indexOf('"', end+1);
					if (end>=0) {
						end += 1;  // include closing quotation mark
						addMagicComment(curLine.mid(start, end-start), i, MapOfMagicComments, iter_magicComment);
					}
				} else {
					end = curLine.indexOf(' ', end+1);
					if (end>=0) {
						addMagicComment(curLine.mid(start, end-start), i, MapOfMagicComments, iter_magicComment);
					} else {
						addMagicComment(curLine.mid(start), i, MapOfMagicComments, iter_magicComment);
					}
				}
			}
		}

		//// magic comment
		col = lineFormatAnaylzer.firstCol(getFormatId("magicComment"));
		if (col >= 0) {
			QString text = curLine.mid(col);
			if (text.startsWith("% !TeX", Qt::CaseInsensitive)) {
				addMagicComment(text.mid(6).trimmed(), i, MapOfMagicComments, iter_magicComment);
			} else if (text.startsWith("% !BIB", Qt::CaseInsensitive)) {
				// workaround to also support "% !BIB program = biber" syntax used by TeXShop and TeXWorks
				text = text.mid(6).trimmed();
				QString name;
				QString val;
				splitMagicComment(text, name, val);
				if ((name=="TS-program" || name=="program") && (val=="biber" || val=="bibtex")) {
					addMagicComment(QString("TXS-program:bibliography = txs:///%1").arg(val), i, MapOfMagicComments, iter_magicComment);
				}
			}
		}

		// check also in command argument, als references might be put there as well...
		//// Appendix keyword
		if (curLine=="\\appendix") {
			oldLine=mAppendixLine;
			mAppendixLine=line(i).handle();
			
		}
		if(line(i).handle()==mAppendixLine && curLine!="\\appendix"){
			oldLine=mAppendixLine;
			mAppendixLine=0;
		}
        /// \end{document} keyword
        /// don't add section in structure view after passing \end{document} , this command must not contains spaces nor any additions in the same line
        if (curLine=="\\end{document}") {
            oldLineBeyond=mBeyondEnd;
            mBeyondEnd=line(i).handle();

        }
        if(line(i).handle()==mBeyondEnd && curLine!="\\end{document}"){
            oldLineBeyond=mBeyondEnd;
            mBeyondEnd=0;
        }

        int offset = 0;
        for(int j=0;j<tl.length();j++) {
            Tokens tk=tl.at(j);
            // break at comment start
            if(tk.type==Tokens::comment)
                break;
            // work special args
            ////Ref
            //for reference counting (can be placed in command options as well ...
            if(tk.type==Tokens::labelRef||tk.type==Tokens::labelRefList){
                ReferencePair elem;
                elem.name=tk.getText();
                elem.start=tk.start;
                mRefItem.insert(line(i).handle(),elem);
            }

            //// label ////
            if(tk.type==Tokens::label && tk.length>0){
                ReferencePair elem;
                elem.name=tk.getText();
                elem.start=tk.start;
                mLabelItem.insert(line(i).handle(),elem);
                completerNeedsUpdate=true;
                StructureEntry *newLabel;
                if(MapOfLabels.contains(dlh)){
                    newLabel=MapOfLabels.value(dlh);
                    newLabel->type=StructureEntry::SE_LABEL;
                    MapOfLabels.remove(dlh,newLabel);
                }else{
                    newLabel=new StructureEntry(this, StructureEntry::SE_LABEL);
                }
                newLabel->title=elem.name;
                newLabel->setLine(line(i).handle(), i);
                newLabel->parent=labelList;
                iter_label.insert(newLabel);
            }
            // work on general commands
            if(tk.type!=Tokens::command && tk.type!=Tokens::commandUnknown)
                continue; // not a command
            Tokens tkCmd;
            TokenList args;
            QString cmd;
            int cmdStart = findCommandWithArgsFromTL(tl, tkCmd, args, j, parent->showCommentedElementsInStructure);
			if (cmdStart < 0) break;
            cmd=curLine.mid(tkCmd.start,tkCmd.length);
			// offset is the starting point for the next search. Currently this is behind cmd.
			// It could be improved to be behind the arguments.
            offset = cmdStart+1;
            QString firstArg = getArg(args, dlh,0,ArgumentList::Mandatory);



            if (lp.possibleCommands["%todo"].contains(cmd)) {
				bool reuse=false;
				StructureEntry *newTodo;
				if(MapOfTodo.contains(dlh)){
					newTodo=MapOfTodo.value(dlh);
					//parent->add(newTodo);
					newTodo->type=StructureEntry::SE_TODO;
					MapOfTodo.remove(dlh,newTodo);
					reuse=true;
				}else{
					newTodo=new StructureEntry(this, StructureEntry::SE_TODO);
				}
				newTodo->title = firstArg;
				newTodo->setLine(line(i).handle(), i);
				newTodo->parent=todoList;
				if(!reuse) emit addElement(todoList,todoList->children.size()); //todo: why here but not in label?
				iter_todo.insert(newTodo);
			}
			
			//// newcommand ////
            if (lp.possibleCommands["%definition"].contains(cmd)||ltxCommands.possibleCommands["%definition"].contains(cmd)) {
                completerNeedsUpdate=true;
                Tokens cmdName;
                if(!args.isEmpty() && args.at(0).type==Tokens::braces){
                    //remove first arg, contains new command name, already saved into "firstArg"
                    cmdName=args.takeFirst();
                }
                int optionCount = getArg(args,dlh,0, ArgumentList::Optional).toInt();  // results in 0 if there is no optional argument or conversion fails
				if (optionCount>9 || optionCount<0) optionCount = 0;  // limit number of options
                bool def=!getArg(args,dlh,1, ArgumentList::Optional).isEmpty();

                ltxCommands.possibleCommands["user"].insert(firstArg);

                if(!removedUserCommands.removeAll(firstArg)){
					addedUserCommands << firstArg;
                }

				for (int j=0; j<optionCount; j++) {
					if (j==0) {
                        if(!def)
							firstArg.append("{%<arg1%|%>}");
						else
							firstArg.append("[%<opt. arg1%|%>]");
					} else
						firstArg.append(QString("{%<arg%1%>}").arg(j+1));
				}
                CodeSnippet cs(firstArg);
                if(cmdName.subtype==Tokens::defWidth)
                    cs.type=CodeSnippet::length;
                mUserCommandList.insert(line(i).handle(),cs);
				// remove obsolete Overlays (maybe this can be refined
                //updateSyntaxCheck=true;
				continue;
			}
			// special treatment \def
			if (cmd=="\\def" || cmd == "\\gdef" || cmd == "\\edef" || cmd == "\\xdef") {
                QString remainder = curLine.mid(cmdStart+cmd.length());
                completerNeedsUpdate=true;
				QRegExp rx("(\\\\\\w+)\\s*([^{%]*)");
				if(rx.indexIn(remainder)>-1){
					QString name=rx.cap(1);
					QString optionStr=rx.cap(2);
					//qDebug()<< name << ":"<< optionStr;
					ltxCommands.possibleCommands["user"].insert(name);
					if(!removedUserCommands.removeAll(name)) addedUserCommands << name;
					optionStr = optionStr.trimmed();
					if (optionStr.length()) {
						int lastArg = optionStr[optionStr.length()-1].toLatin1() - '0';
						if (optionStr.length() == lastArg * 2) { //#1#2#3...
							for (int j=1; j<=lastArg; j++)
								if (j==1) name.append("{%<arg1%|%>}");
								else name.append(QString("{%<arg%1%>}").arg(j));
						} else {
							QStringList args = optionStr.split('#'); //#1;#2#3:#4 => ["",1;,2,3:,4]
							args.removeAt(0);
							bool hadSeparator = true;
							for (int i=0;i<args.length();i++) {
								if (args[i].length() == 0) continue; //invalid
								bool hasSeparator = (args[i].length() != 1); //only single digit variables allowed. last arg also needs a sep
								if (!hadSeparator || !hasSeparator)
									args[i] = "{%<arg"+args[i][0]+"%>}" + args[i].mid(1);
								else
									args[i] = "%<arg"+args[i][0]+"%>" + args[i].mid(1); //no need to use {} for arguments that are separated anyways
								hadSeparator  = hasSeparator;
							}
							name.append(" ");
							name.append(args.join(""));
						}
					}
					mUserCommandList.insert(line(i).handle(),name);
					// remove obsolete Overlays (maybe this can be refined
                    //updateSyntaxCheck=true;
				}
				continue;
			}
			
			//// newenvironment ////
			static const QStringList envTokens = QStringList() << "\\newenvironment" << "\\renewenvironment";
			if (envTokens.contains(cmd)) {
				completerNeedsUpdate=true;
                int optionCount = getArg(args,dlh,0, ArgumentList::Optional).toInt(); // results in 0 if there is no optional argument or conversion fails
				if (optionCount>9 || optionCount<0) optionCount = 0;  // limit number of options
				firstArg.append("}");
				mUserCommandList.insert(line(i).handle(),"\\end{"+firstArg);
				QStringList lst;
				lst << "\\begin{"+firstArg << "\\end{"+firstArg;
				foreach(const QString& elem,lst){
					ltxCommands.possibleCommands["user"].insert(elem);
					if(!removedUserCommands.removeAll(elem)){
						addedUserCommands << elem;
					}
				}
				for (int j=0; j<optionCount; j++) {
					if (j==0) firstArg.append("{%<1%|%>}");
					else firstArg.append(QString("{%<%1%>}").arg(j+1));
				}
				//mUserCommandList.insert(line(i).handle(),firstArg);//???
				mUserCommandList.insert(line(i).handle(),"\\begin{"+firstArg);
				continue;
			}
			//// newtheorem ////
			if (cmd=="\\newtheorem" || cmd=="\\newtheorem*" || cmd=="\\declaretheorem") {
				completerNeedsUpdate=true;
				QStringList lst;
				lst << "\\begin{"+firstArg+"}" << "\\end{"+firstArg+"}";
				foreach(const QString& elem,lst){
					mUserCommandList.insert(line(i).handle(),elem);
					ltxCommands.possibleCommands["user"].insert(elem);
					if(!removedUserCommands.removeAll(elem)){
						addedUserCommands << elem;
					}
				}
				continue;
			}
			//// newcounter ////
			if (cmd=="\\newcounter") {
				completerNeedsUpdate=true;
				QStringList lst;
				lst << "\\the"+firstArg ;
				foreach(const QString& elem,lst){
					mUserCommandList.insert(line(i).handle(),elem);
                    ltxCommands.possibleCommands["user"].insert(elem);
					if(!removedUserCommands.removeAll(elem)){
						addedUserCommands << elem;
					}
				}
				continue;
			}
            /// specialDefinition ///
            /// e.g. definecolor
            if(ltxCommands.specialDefCommands.contains(cmd)){
                if(!args.isEmpty() ){
                    completerNeedsUpdate=true;
                    QString definition=ltxCommands.specialDefCommands.value(cmd);
                    Tokens::TokenType type=Tokens::braces;
                    if(definition.startsWith('(')){
                        definition.chop(1);
                        definition=definition.mid(1);
                        type=Tokens::bracket;
                    }
                    if(definition.startsWith('[')){
                        definition.chop(1);
                        definition=definition.mid(1);
                        type=Tokens::squareBracket;
                    }

                    foreach(Tokens mTk,args){
                        if(mTk.type!=type)
                            continue;
                        QString elem=mTk.getText();
                        elem=elem.mid(1,elem.length()-2); // strip braces
                        mUserCommandList.insert(line(i).handle(),definition+"%"+elem);
                        if(!removedUserCommands.removeAll(elem)){
                            addedUserCommands << elem;
                        }
                        break;
                    }
                }
            }
			/// bibitem ///
            if(lp.possibleCommands["%bibitem"].contains(cmd)){
				if(!firstArg.isEmpty() && !isDefinitionArgument(firstArg)){
					ReferencePair elem;
					elem.name=firstArg;
                    int optionStart = 0;
					for (int j=0; j<args.length(); j++) {
                        if (args[j].type == Tokens::braces) {
                            optionStart =args[j].start;
                            break;
                        }
                    }
					elem.start=optionStart;
					mBibItem.insert(line(i).handle(),elem);
					bibItemsChanged=true;
					continue;
				}
			}
			///usepackage
            if (lp.possibleCommands["%usepackage"].contains(cmd)) {
				completerNeedsUpdate=true;
				QStringList packagesHelper=firstArg.split(",");

                if(cmd.endsWith("theme")){ // special treatment for  \usetheme
					QString preambel=cmd;
					preambel.remove(0,4);
					preambel.prepend("beamer");
					packagesHelper.replaceInStrings(QRegExp("^"),preambel);
				}

                QString firstOptArg = getArg(args,dlh,0, ArgumentList::Optional);
                if(cmd=="\\documentclass"){
                    //special treatment for documentclass, especially for the class options
                    // at the moment a change here soes not automatically lead to an update of corresponding definitions, here babel
					mClassOptions=firstOptArg;
                }

				if(firstArg=="babel"){
                    //special treatment for babel
					if(firstOptArg.isEmpty()){
						firstOptArg=mClassOptions;
                    }
					if(!firstOptArg.isEmpty()){
						packagesHelper << firstOptArg.split(",");
                    }
                }

				QStringList packages;
                foreach(QString elem,packagesHelper){
                    elem=elem.simplified();
                    if(lp.packageAliases.contains(elem))
                        packages << lp.packageAliases.values(elem);
					else
						packages << elem;
                }

				foreach(const QString& elem,packages){
					if(!removedUsepackages.removeAll(firstOptArg+"#"+elem))
						addedUsepackages << firstOptArg+"#"+elem;
					mUsepackageList.insertMulti(dlh,firstOptArg+"#"+elem); // hand on option of usepackages for conditional cwl load ..., force load if option is changed
				}
				continue;
			}
			//// bibliography ////
            if (lp.possibleCommands["%bibliography"].contains(cmd)) {
				QStringList additionalBibPaths = ConfigManagerInterface::getInstance()->getOption("Files/Bib Paths").toString().split(getPathListSeparator());
				QStringList bibs=firstArg.split(',',QString::SkipEmptyParts);
				//add new bibs and set bibTeXFilesNeedsUpdate if there was any change
				foreach(const QString& elem,bibs){ //latex doesn't seem to allow any spaces in file names
					mMentionedBibTeXFiles.insert(line(i).handle(),FileNamePair(elem,getAbsoluteFilePath(elem,"bib",additionalBibPaths)));
					if (oldBibs.removeAll(elem) == 0)
						bibTeXFilesNeedsUpdate = true;
				}
				//write bib tex in tree
				foreach (const QString& bibFile, bibs) {
					StructureEntry *newFile;
					if(MapOfBibtex.contains(dlh)){
						newFile=MapOfBibtex.value(dlh);
						newFile->type=StructureEntry::SE_BIBTEX;
						MapOfBibtex.remove(dlh,newFile);
					}else{
						newFile=new StructureEntry(this, StructureEntry::SE_BIBTEX);
					}
					newFile->title=bibFile;
					newFile->setLine(line(i).handle(), i);
					newFile->parent=bibTeXList;
					iter_bibTeX.insert(newFile);
				}
				continue;
			}
			
			//// beamer blocks ////
			
			if (cmd=="\\begin" && firstArg=="block") {
				StructureEntry *newBlock;
				if(MapOfBlock.contains(dlh)){
					newBlock=MapOfBlock.value(dlh);
					newBlock->type=StructureEntry::SE_BLOCK;
					MapOfBlock.remove(dlh,newBlock);
				}else{
					newBlock=new StructureEntry(this, StructureEntry::SE_BLOCK);
				}
                newBlock->title=getArg(args,dlh,1, ArgumentList::Mandatory);
				newBlock->setLine(line(i).handle(), i);
				newBlock->parent=blockList;
				iter_block.insert(newBlock);
				continue;
			}
			
			//// include,input,import ////
            if (lp.possibleCommands["%include"].contains(cmd) && !isDefinitionArgument(firstArg)) {
				StructureEntry *newInclude=new StructureEntry(this, StructureEntry::SE_INCLUDE);
                newInclude->level = parent && !parent->indentIncludesInStructure ? 0 : lp.structureDepth() - 1;
				firstArg = removeQuote(firstArg);
				newInclude->title=firstArg;
				QString fname=findFileName(firstArg);
                removedIncludes.removeAll(fname);
                mIncludedFilesList.insert(line(i).handle(),fname);
				LatexDocument* dc=parent->findDocumentFromName(fname);
                if(dc){
                    childDocs.insert(dc);
                    dc->setMasterDocument(this,recheckLabels);
                }else {
                    lstFilesToLoad << fname;
                    //parent->addDocToLoad(fname);
                }

                newInclude->valid = !fname.isEmpty();
				newInclude->setLine(line(i).handle(), i);
				newInclude->columnNumber = cmdStart;
				flatStructure << newInclude;
                updateSyntaxCheck=true;
				continue;
			}
			
            if (lp.possibleCommands["%import"].contains(cmd) && !isDefinitionArgument(firstArg)) {
				StructureEntry *newInclude=new StructureEntry(this, StructureEntry::SE_INCLUDE);
                newInclude->level = parent && !parent->indentIncludesInStructure ? 0 : lp.structureDepth() - 1;
				QDir dir(firstArg);
                QFileInfo fi(dir, getArg(args,dlh,1, ArgumentList::Mandatory));
				QString file = fi.filePath();
				newInclude->title = file;
                QString fname=findFileName(file);
                removedIncludes.removeAll(fname);
                mIncludedFilesList.insert(line(i).handle(),fname);
				LatexDocument* dc=parent->findDocumentFromName(fname);
                if(dc){
                    childDocs.insert(dc);
                    dc->setMasterDocument(this,recheckLabels);
                } else {
                    lstFilesToLoad << fname;
                    //parent->addDocToLoad(fname);
                }

                newInclude->valid = !fname.isEmpty();
				newInclude->setLine(line(i).handle(), i);
				newInclude->columnNumber = cmdStart;
				flatStructure << newInclude;
                updateSyntaxCheck=true;
				continue;
			}

			//// all sections ////
			if(cmd.endsWith("*"))
				cmd=cmd.left(cmd.length()-1);
            int level = lp.structureCommandLevel(cmd);
            if (level>-1 && !firstArg.isEmpty() && tkCmd.subtype==Tokens::none) {
                StructureEntry *newSection = new StructureEntry(this,StructureEntry::SE_SECTION);
				if(mAppendixLine &&indexOf(mAppendixLine)<i) newSection->setContext(StructureEntry::InAppendix);
				if(mBeyondEnd &&indexOf(mBeyondEnd)<i) newSection->setContext(StructureEntry::BeyondEnd);
				newSection->title=parseTexOrPDFString(firstArg);
                newSection->level=level;
                newSection->setLine(line(i).handle(), i);
				newSection->columnNumber = cmdStart;
                flatStructure << newSection;
			}
		} // while(findCommandWithArgs())
		
		if (!oldBibs.isEmpty())
			bibTeXFilesNeedsUpdate = true; //file name removed
		
		if(!removedIncludes.isEmpty()){
            parent->removeDocs(removedIncludes);
            parent->updateMasterSlaveRelations(this);
        }
        if(latexLikeChecking) {
            StackEnvironment env;
            getEnv(i,env);
            QDocumentLineHandle *lastHandle=line(i-1).handle();
            TokenStack oldRemainder;
            if(lastHandle){
                oldRemainder=lastHandle->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
            }

            SynChecker.putLine(line(i).handle(),env,oldRemainder,true);
        }
	}//for each line handle
    QVector<StructureEntry*> parent_level(lp.structureDepth());
    if(!isHidden()){
        mergeStructure(baseStructure, parent_level, flatStructure, linenr, count);

        const QList<StructureEntry*> categories=
                QList<StructureEntry*>() << magicCommentList << blockList << labelList << todoList << bibTeXList;

        for (int i=categories.size()-1;i>=0;i--) {
            StructureEntry *cat = categories[i];
            if (cat->children.isEmpty() == (cat->parent == 0)) continue;
            if (cat->children.isEmpty()) removeElementWithSignal(cat);
            else insertElementWithSignal(baseStructure, 0, cat);
        }

        //update appendix change
        if(oldLine!=mAppendixLine){
			updateContext(oldLine, mAppendixLine, StructureEntry::InAppendix);
        }
        //update end document change
        if(oldLineBeyond!=mBeyondEnd){
			updateContext(oldLineBeyond,mBeyondEnd, StructureEntry::BeyondEnd);
        }

        // rehighlight current cursor position
        StructureEntry *newSection=0;
        if(edView){
            int i=edView->editor->cursor().lineNumber();
            if(i>=0) {
                newSection=findSectionForLine(i);
            }
        }

        emit structureUpdated(this,newSection);
    }
	StructureEntry* se;
	foreach(se,MapOfTodo.values())
		delete se;
	
	foreach(se,MapOfBibtex.values())
		delete se;
	
	foreach(se,MapOfBlock.values())
		delete se;
	
	foreach(se,MapOfLabels.values())
		delete se;
	
	foreach(se,MapOfMagicComments.values())
		delete se;
	
    bool updateLtxCommands=false;
	if(!addedUsepackages.isEmpty() || !removedUsepackages.isEmpty() || !addedUserCommands.isEmpty() || !removedUserCommands.isEmpty()){
		bool forceUpdate=!addedUserCommands.isEmpty() || !removedUserCommands.isEmpty();
        updateLtxCommands=updateCompletionFiles(forceUpdate,false,true);
        reRunSuggested=(count>1)&&(!addedUsepackages.isEmpty() || !removedUsepackages.isEmpty());
	}
	
	if (bibTeXFilesNeedsUpdate)
		emit updateBibTeXFiles();

    // force update on citation overlays
    if(bibItemsChanged||bibTeXFilesNeedsUpdate){
        parent->updateBibFiles(bibTeXFilesNeedsUpdate);
        // needs probably done asynchronously as bibteFiles needs to be loaded first ...
        foreach(LatexDocument* elem,getListOfDocs()){
            if(elem->edView)
                elem->edView->updateCitationFormats();
        }
    }

    if (completerNeedsUpdate || bibTeXFilesNeedsUpdate)
		emit updateCompleter();


    if(!recheck && (updateSyntaxCheck || updateLtxCommands)) {
            this->updateLtxCommands(true);
	}

	//update view
	if(edView)
		edView->documentContentChanged(linenr, count);
	
	
#ifndef QT_NO_DEBUG
    if(!isHidden())
        checkForLeak();
#endif
    foreach(QString fname,lstFilesToLoad){
        parent->addDocToLoad(fname);
    }
    //qDebug()<<"leave"<< QTime::currentTime().toString("HH:mm:ss:zzz");
    if(reRunSuggested && !recheck)
        patchStructure(0,-1,true); // expensive solution for handling changed packages (and hence command definitions)
    return reRunSuggested;
}

#ifndef QT_NO_DEBUG

void LatexDocument::checkForLeak(){
	StructureEntryIterator iter(baseStructure);
	QSet<StructureEntry*>zw=StructureContent;
	while (iter.hasNext()){
		zw.remove(iter.next());
	}
	
	// filter top level elements
	QMutableSetIterator<StructureEntry *> i(zw);
	while (i.hasNext())
		if(i.next()->type==StructureEntry::SE_OVERVIEW) i.remove();
	
	if(zw.count()>0){
		qDebug("Memory leak in structure");
	}
}

#endif

StructureEntry * LatexDocument::findSectionForLine(int currentLine){
	StructureEntryIterator iter(baseStructure);
	StructureEntry *newSection=0;
	
	while (/*iter.hasNext()*/true){
		StructureEntry *curSection=0;
		while (iter.hasNext()){
			curSection=iter.next();
			if (curSection->type==StructureEntry::SE_SECTION)
				break;
		}
		if (curSection==0 || curSection->type!=StructureEntry::SE_SECTION)
			break;
		
		if (curSection->getRealLineNumber() > currentLine) break; //curSection is after newSection where the cursor is
		else newSection=curSection;
	}
	if(newSection && newSection->getRealLineNumber()>currentLine) newSection=0;
	
	return newSection;
}

void LatexDocument::setTemporaryFileName(const QString& fileName){
	temporaryFileName=fileName;
}

QString LatexDocument::getTemporaryFileName() const{
	return temporaryFileName;
}

QFileInfo LatexDocument::getTemporaryFileInfo() const {
	return QFileInfo(temporaryFileName);
}

int LatexDocument::countLabels(const QString& name){
	int result=0;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QStringList items=elem->labelItems();
		result+=items.count(name);
	}
	return result;
}

int LatexDocument::countRefs(const QString& name){
	int result=0;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QStringList items=elem->refItems();
		result+=items.count(name);
	}
	return result;
}

bool LatexDocument::bibIdValid(const QString& name){
    bool result=!findFileFromBibId(name).isEmpty();
    if(!result){
        foreach(const LatexDocument* doc,getListOfDocs()){
             //if(doc->getEditorView()->containsBibTeXId(name)){
            if(doc->bibItems().contains(name)){
                 result=true;
                 break;
             }
        }
    }
    return result;
}

bool LatexDocument::isBibItem(const QString& name){
    bool result=false;
    foreach(const LatexDocument* doc,getListOfDocs()){
         //if(doc->getEditorView()->containsBibTeXId(name)){
        if(doc->bibItems().contains(name)){
             result=true;
             break;
         }
    }
    return result;
}

QString LatexDocument::findFileFromBibId(const QString& bibId){
  QStringList collected_mentionedBibTeXFiles;
  foreach(const LatexDocument* doc,getListOfDocs())
      collected_mentionedBibTeXFiles<<doc->listOfMentionedBibTeXFiles();
  const QMap<QString, BibTeXFileInfo>& bibtexfiles = parent->bibTeXFiles;
  foreach (const QString& file, collected_mentionedBibTeXFiles)
    if (bibtexfiles.value(file).ids.contains(bibId))
      return file;
  return QString();
}

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getBibItems(const QString& name){
  QHash<QDocumentLineHandle*,int> result;
  foreach(const LatexDocument *elem,getListOfDocs()){
    QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
    for (it = elem->mBibItem.constBegin(); it != elem->mBibItem.constEnd(); ++it){
      ReferencePair rp=it.value();
      if(rp.name==name && elem->indexOf(it.key()) >= 0){
        result.insert(it.key(),rp.start);
      }
    }
  }
  return result;
}


QMultiHash<QDocumentLineHandle*,int> LatexDocument::getLabels(const QString& name){
	QHash<QDocumentLineHandle*,int> result;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
		for (it = elem->mLabelItem.constBegin(); it != elem->mLabelItem.constEnd(); ++it){
			ReferencePair rp=it.value();
			if(rp.name==name && elem->indexOf(it.key()) >= 0){
				result.insert(it.key(),rp.start);
			}
		}
	}
	return result;
}

QMultiHash<QDocumentLineHandle*,int> LatexDocument::getRefs(const QString& name){
	QHash<QDocumentLineHandle*,int> result;
	foreach(const LatexDocument *elem,getListOfDocs()){
		QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
		for (it = elem->mRefItem.constBegin(); it != elem->mRefItem.constEnd(); ++it){
			ReferencePair rp=it.value();
			if(rp.name==name && elem->indexOf(it.key()) >= 0){
				result.insert(it.key(),rp.start);
			}
		}
	}
	return result;
}

/*!
 * replace all given items by newName
 * an optional QDocumentCursor may be passed in, if the operation should be 
 * part of a larger editBlock of that cursor.
 */
void LatexDocument::replaceItems(QMultiHash<QDocumentLineHandle *, ReferencePair> items, const QString &newName, QDocumentCursor *cursor)
{
	QDocumentCursor *cur = cursor;
	if (!cursor) {
		cur = new QDocumentCursor(this);
		cur->beginEditBlock();
	}
	QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
	for (it = items.constBegin(); it != items.constEnd(); ++it){
		QDocumentLineHandle *dlh = it.key();
		ReferencePair rp = it.value();
		int lineNo = indexOf(dlh);
		if(lineNo >= 0){
			cur->setLineNumber(lineNo);
			cur->setColumnNumber(rp.start);
			cur->movePosition(rp.name.length(), QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
			cur->replaceSelectedText(newName);
		}
	}
	if (!cursor) {
		cur->endEditBlock();
		delete cur;
	}
}

/*!
 * replace all labels name by newName
 * an optional QDocumentCursor may be passed in, if the operation should be 
 * part of a larger editBlock of that cursor.
 */
void LatexDocument::replaceLabel(const QString& name, const QString& newName, QDocumentCursor *cursor){
	QMultiHash<QDocumentLineHandle*,ReferencePair> labelItemsMatchingName;
	QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
	for (it = mLabelItem.constBegin(); it != mLabelItem.constEnd(); ++it) {
		if (it.value().name == name) {
			labelItemsMatchingName.insertMulti(it.key(), it.value());
		}
	}
	replaceItems(labelItemsMatchingName, newName, cursor);
}

/*!
 * replace all references name by newName
 * an optional QDocumentCursor may be passed in, if the operation should be 
 * part of a larger editBlock of that cursor.
 */
void LatexDocument::replaceRefs(const QString& name, const QString& newName, QDocumentCursor *cursor){
	QMultiHash<QDocumentLineHandle*,ReferencePair> refItemsMatchingName;
	QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
	for (it = mRefItem.constBegin(); it != mRefItem.constEnd(); ++it) {
		if (it.value().name == name) {
			refItemsMatchingName.insertMulti(it.key(), it.value());
		}
	}
	replaceItems(refItemsMatchingName, newName, cursor);
}

void LatexDocument::replaceLabelsAndRefs(const QString &name, const QString &newName){
	QDocumentCursor cursor(this);
	cursor.beginEditBlock();
	replaceLabel(name, newName, &cursor);
	replaceRefs(name, newName, &cursor);
	cursor.endEditBlock();
}

void LatexDocument::setMasterDocument(LatexDocument* doc,bool recheck){
	masterDocument=doc;
    if(recheck){
        QList<LatexDocument *>listOfDocs=getListOfDocs();
        foreach(LatexDocument *elem,listOfDocs){
            elem->recheckRefsLabels();
        }
    }
}

void LatexDocument::addChild(LatexDocument* doc){
    childDocs.insert(doc);
}

void LatexDocument::removeChild(LatexDocument* doc){
    childDocs.remove(doc);
}

bool LatexDocument::containsChild(LatexDocument *doc) const{
    return childDocs.contains(doc);
}


QList<LatexDocument *>LatexDocument::getListOfDocs(QSet<LatexDocument*> *visitedDocs){
	QList<LatexDocument *>listOfDocs;
	bool deleteVisitedDocs=false;
	if(parent->masterDocument){
        listOfDocs=parent->getDocuments();
	}else{
		LatexDocument *master=this;
		if(!visitedDocs){
			visitedDocs=new QSet<LatexDocument*>();
			deleteVisitedDocs=true;
		}
        foreach(LatexDocument *elem,parent->getDocuments()){ // check children
            if(elem!=master && !master->childDocs.contains(elem)) continue;

			if(visitedDocs && !visitedDocs->contains(elem)){
				listOfDocs << elem;
				visitedDocs->insert(elem);
				listOfDocs << elem->getListOfDocs(visitedDocs);
			}
		}
		if(masterDocument){ //check masters
			master=masterDocument;
			if(!visitedDocs->contains(master))
				listOfDocs << master->getListOfDocs(visitedDocs);
		}
	}
	if(deleteVisitedDocs)
		delete visitedDocs;
	return listOfDocs;
}

void LatexDocument::recheckRefsLabels(){
	// get occurences (refs)
	int referenceMultipleFormat=getFormatId("referenceMultiple");
	int referencePresentFormat=getFormatId("referencePresent");
	int referenceMissingFormat=getFormatId("referenceMissing");
	
	QMultiHash<QDocumentLineHandle*,ReferencePair>::const_iterator it;
	for(it=mLabelItem.constBegin();it!=mLabelItem.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const ReferencePair& rp,mLabelItem.values(dlh)){
			int cnt=countLabels(rp.name);
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
		}
	}
	for(it=mRefItem.constBegin();it!=mRefItem.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const ReferencePair& rp,mRefItem.values(dlh)){
			int cnt=countLabels(rp.name);
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(rp.start,rp.name.length(),referenceMissingFormat));
		}
	}
}

QStringList LatexDocument::someItems(const QMultiHash<QDocumentLineHandle*,ReferencePair>& list){
	QList<ReferencePair> lst=list.values();
	QStringList result;
	foreach(const ReferencePair& elem,lst){
		result << elem.name;
	}
	
	return result;
}

QStringList LatexDocument::labelItems() const{
	return someItems(mLabelItem);
}

QStringList LatexDocument::refItems() const{
	return someItems(mRefItem);
}

QStringList LatexDocument::bibItems() const{
    return someItems(mBibItem);
}

void LatexDocument::updateRefsLabels(const QString& ref){
	// get occurences (refs)
	int referenceMultipleFormat=getFormatId("referenceMultiple");
	int referencePresentFormat=getFormatId("referencePresent");
	int referenceMissingFormat=getFormatId("referenceMissing");
	
	int cnt=countLabels(ref);
	QMultiHash<QDocumentLineHandle*,int> occurences=getLabels(ref);
	occurences+=getRefs(ref);
	QMultiHash<QDocumentLineHandle*,int>::const_iterator it;
	for(it=occurences.constBegin();it!=occurences.constEnd();++it){
		QDocumentLineHandle* dlh=it.key();
		foreach(const int pos,occurences.values(dlh)){
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referenceMultipleFormat));
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referencePresentFormat));
			dlh->removeOverlay(QFormatRange(pos,ref.length(),referenceMissingFormat));
			if (cnt>1) {
				dlh->addOverlay(QFormatRange(pos,ref.length(),referenceMultipleFormat));
			} else if (cnt==1) dlh->addOverlay(QFormatRange(pos,ref.length(),referencePresentFormat));
			else dlh->addOverlay(QFormatRange(pos,ref.length(),referenceMissingFormat));
		}
	}
}

/*
void LatexDocument::includeDocument(LatexDocument* includedDocument){
 includedDocument->deleteLater();
 if (texFiles.contains(includedDocument->fileName))
  return; //this should never happen
  
 texFiles.unite(includedDocument->texFiles);
 containedLabels.unite(includedDocument->containedLabels);
 containedReferences.unite(includedDocument->containedReferences);
 mentionedBibTeXFiles.unite(includedDocument->mentionedBibTeXFiles);
 allBibTeXIds.unite(includedDocument->allBibTeXIds);
 
}
*/
StructureEntry::StructureEntry(LatexDocument* doc, Type newType):type(newType),level(0), parent(0), document(doc), parentRow(-1), lineHandle(0), lineNumber(-1), m_contexts(0) {
#ifndef QT_NO_DEBUG
	Q_ASSERT(document);
	document->StructureContent.insert(this);
#endif
}
StructureEntry::~StructureEntry(){
	level=-1; //invalidate entry
	foreach (StructureEntry* se, children)
		delete se;
#ifndef QT_NO_DEBUG
	Q_ASSERT(document);
	bool removed = document->StructureContent.remove(this);
	Q_ASSERT(removed); //prevent double deletion
#endif
}
void StructureEntry::add(StructureEntry* child){
	Q_ASSERT(child!=0);
	children.append(child);
	child->parent=this;
}
void StructureEntry::insert(int pos, StructureEntry* child){
	Q_ASSERT(child!=0);
	children.insert(pos,child);
	child->parent=this;
}
void StructureEntry::setLine(QDocumentLineHandle* handle, int lineNr){
	lineHandle = handle;
	lineNumber = lineNr;
}

QDocumentLineHandle* StructureEntry::getLineHandle() const{
	return lineHandle;
}

int StructureEntry::getCachedLineNumber() const{
	return lineNumber;
}
int StructureEntry::getRealLineNumber() const{
	lineNumber = document->indexOf(lineHandle, lineNumber);
	Q_ASSERT(lineNumber == -1 || document->line(lineNumber).handle() == lineHandle);
	return lineNumber;
}

template <typename T> inline int hintedIndexOf (const QList<T*>& list, const T* elem, int hint) {
	if (hint < 2) return list.indexOf(const_cast<T*>(elem));
	int backward = hint, forward = hint + 1;
	for (;backward >= 0 && forward < list.size();
			 backward--, forward++) {
		if (list[backward] == elem) return backward;
		if (list[forward] == elem) return forward;
	}
	if (backward >= list.size()) backward = list.size() - 1;
	for (;backward >= 0; backward--)
		if (list[backward] == elem) return backward;
	if (forward < 0) forward = 0;
	for (;forward < list.size(); forward++)
		if (list[forward] == elem) return forward;
	return -1;
}

int StructureEntry::getRealParentRow() const{
	REQUIRE_RET(parent, -1);
	parentRow = hintedIndexOf<StructureEntry>(parent->children, this, parentRow);
	return parentRow;
}

void StructureEntry::debugPrint(const char* message) const{
	qDebug("%s %p",message, this);
	if (!this) return;
	qDebug("   level: %i",level);
	qDebug("   type: %i",(int)type);
	qDebug("   line nr: %i", lineNumber);
	qDebug("   title: %s", qPrintable(title));
}

StructureEntryIterator::StructureEntryIterator(StructureEntry* entry){
	if (!entry) return;
	while (entry->parent){
		entryHierarchy.prepend(entry);
		indexHierarchy.prepend(entry->getRealParentRow());
		entry=entry->parent;
	}
	entryHierarchy.prepend(entry);
	indexHierarchy.prepend(0);
}
bool StructureEntryIterator::hasNext(){
	return !entryHierarchy.isEmpty();
}
StructureEntry* StructureEntryIterator::next(){
	if (!hasNext()) return 0;
	StructureEntry* result=entryHierarchy.last();
	if (!result->children.isEmpty()) { //first child is next element, go a level deeper
		entryHierarchy.append(result->children.at(0));
		indexHierarchy.append(0);
	} else { //no child, go to next on same level
		entryHierarchy.removeLast();
		indexHierarchy.last()++;
		while (!entryHierarchy.isEmpty() && indexHierarchy.last()>=entryHierarchy.last()->children.size()) {
			//doesn't exists, proceed to travel upwards
			entryHierarchy.removeLast();
			indexHierarchy.removeLast();
			indexHierarchy.last()++;
		}
		if (!entryHierarchy.isEmpty())
			entryHierarchy.append(entryHierarchy.last()->children.at(indexHierarchy.last()));
	}
	return result;
}

LatexDocumentsModel::LatexDocumentsModel(LatexDocuments& docs):documents(docs),
  iconDocument(":/images/doc.png"), iconMasterDocument(":/images/masterdoc.png"), iconBibTeX(":/images/bibtex.png"), iconInclude(":/images/include.png"),
  iconWarning(getRealIconCached("warning")), m_singleMode(false){
	mHighlightIndex=QModelIndex();

    QStringList structureIconNames = QStringList() << "part" << "chapter" << "section" << "subsection" << "subsubsection" << "paragraph" << "subparagraph";
	iconSection.resize(structureIconNames.length());
	for (int i=0; i<structureIconNames.length(); i++)
		iconSection[i] = getRealIconCached(structureIconNames[i]);
}
Qt::ItemFlags LatexDocumentsModel::flags ( const QModelIndex & index ) const{
	if (index.isValid()) return Qt::ItemIsEnabled|Qt::ItemIsSelectable;
	else return 0;
}
QVariant LatexDocumentsModel::data ( const QModelIndex & index, int role) const{
	if (!index.isValid()) return QVariant();
	StructureEntry* entry = (StructureEntry*) index.internalPointer();
	if (!entry) return QVariant();
	QString result;
	switch (role) {
	case Qt::DisplayRole:
		if (entry->type==StructureEntry::SE_DOCUMENT_ROOT){ //show only base file name
			QString title=entry->title.mid(1+qMax(entry->title.lastIndexOf("/"), entry->title.lastIndexOf(QDir::separator())));
			if(title.isEmpty()) title=tr("untitled");
			return QVariant(title);
		}
		//show full title in other cases
		if(documents.showLineNumbersInStructure && entry->getCachedLineNumber()>-1){
			result=entry->title+QString(tr(" (Line %1)").arg(entry->getRealLineNumber()+1));
		}else{
			result=entry->title;
		}
		return QVariant(result);
	case Qt::ToolTipRole:
		//qDebug("data %x",entry);
		if (!entry->tooltip.isNull()) {
			return QVariant(entry->tooltip);
		}
		if (entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
			return QVariant(QDir::toNativeSeparators(entry->document->getFileName()));
		}
		if (entry->type==StructureEntry::SE_SECTION) {
			QString tooltip(entry->title);
			if (entry->getCachedLineNumber()>-1)
				tooltip.append("\n"+tr("Line")+QString(": %1").arg(entry->getRealLineNumber()+1));
			StructureEntry *se = LatexDocumentsModel::labelForStructureEntry(entry);
			if (se)
				tooltip.append("\n"+tr("Label")+": "+se->title);
			return QVariant(tooltip);
		}
		if (entry->getCachedLineNumber()>-1)
			return QVariant(entry->title+QString(tr(" (Line %1)").arg(entry->getRealLineNumber()+1)));
		else
			return QVariant();
	case Qt::DecorationRole:
		switch (entry->type){
		case StructureEntry::SE_BIBTEX: return iconBibTeX;
		case StructureEntry::SE_INCLUDE: return iconInclude;
		case StructureEntry::SE_MAGICCOMMENT:
			if (entry->valid)
				return QVariant();
			else
				return iconWarning;
		case StructureEntry::SE_SECTION:
			if (entry->level>=0 && entry->level<iconSection.count())
				return iconSection[entry->level];
			else
				return QVariant();
		case StructureEntry::SE_DOCUMENT_ROOT:
			if (documents.masterDocument==entry->document)
				return iconMasterDocument;
			else
				return iconDocument;
		default: return QVariant();
		}
	case Qt::BackgroundRole:
        if (index==mHighlightIndex) return QVariant(QColor(Qt::lightGray));
		if (documents.markStructureElementsBeyondEnd && entry->hasContext(StructureEntry::BeyondEnd)) return QVariant(QColor(255,170,0));
		if (documents.markStructureElementsInAppendix && entry->hasContext(StructureEntry::InAppendix)) return QVariant(QColor(200,230,200));
        return QVariant();
	case Qt::ForegroundRole:
        if(entry->type==StructureEntry::SE_INCLUDE) {
            return entry->valid ? QVariant() : QVariant(QColor(Qt::red)); // not found files marked red, else black (green is not easily readable)
		}else return QVariant();
	case Qt::FontRole:
		if(entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
			QFont f=QApplication::font();
			if(entry->document==documents.currentDocument) f.setBold(true);
			if(entry->title.isEmpty()) f.setItalic(true);
			return QVariant(f);
		}
		return QVariant();
	default:
		return QVariant();
	}
}
QVariant LatexDocumentsModel::headerData ( int section, Qt::Orientation orientation, int role ) const{
	Q_UNUSED(orientation);
	if (section!=0) return QVariant();
	if (role!=Qt::DisplayRole) return QVariant();
	return QVariant("Structure");
}
int LatexDocumentsModel::rowCount ( const QModelIndex & parent ) const{
	if (!parent.isValid()) return documents.documents.count();
	else {
		StructureEntry* entry = (StructureEntry*) parent.internalPointer();
		if (!entry) return 0;
		return entry->children.size();
	}
}
int LatexDocumentsModel::columnCount ( const QModelIndex & parent ) const{
	Q_UNUSED(parent);
	return 1;
}
QModelIndex LatexDocumentsModel::index ( int row, int column, const QModelIndex & parent ) const{
	if (column!=0) return QModelIndex(); //one column
	if (row<0) return QModelIndex(); //shouldn't happen
	if (parent.isValid()) {
		const StructureEntry* entry = (StructureEntry*) parent.internalPointer();
		if (!entry) return QModelIndex(); //should never happen
		if (row>=entry->children.size()) return QModelIndex(); //shouldn't happen in a correct view
		return createIndex(row,column, entry->children.at(row));
	} else {
		if (row>=documents.documents.size()) return QModelIndex();
		if(m_singleMode){
			if(row!=0 || !documents.currentDocument )
				return QModelIndex();
			else
				return createIndex(row, column, documents.currentDocument->baseStructure);
		}else{
			return createIndex(row, column, documents.documents.at(row)->baseStructure);
		}
	}
}
QModelIndex LatexDocumentsModel::index ( StructureEntry* entry ) const{
	if (!entry) return QModelIndex();
	if (entry->parent==0 && entry->type==StructureEntry::SE_DOCUMENT_ROOT) {
		int row=documents.documents.indexOf(entry->document);
		if(m_singleMode){
			row=0;
		}
		if (row<0) return QModelIndex();
		return createIndex(row, 0, entry);
	} else if (entry->parent!=0 && entry->type!=StructureEntry::SE_DOCUMENT_ROOT) {
		int row=entry->getRealParentRow();
		if (row<0) return QModelIndex(); //shouldn't happen
		return createIndex(row, 0, entry);
	} else return QModelIndex(); //shouldn't happen
}

QModelIndex LatexDocumentsModel::parent ( const QModelIndex & index ) const{
	if (!index.isValid()) return QModelIndex();
	const StructureEntry* entry = (StructureEntry*) index.internalPointer();
#ifndef QT_NO_DEBUG
	const LatexDocument* found = 0;
	foreach (const LatexDocument* ld, documents.documents)
		if (ld->StructureContent.contains(const_cast<StructureEntry*>(entry))) {
			found = ld;
			break;
		}
	if (!found) entry->debugPrint("No document for entry:");
	//Q_ASSERT(found);
	//Q_ASSERT(entry->document == found);
#endif
	if (!entry) return QModelIndex();
	if (!entry->parent) return QModelIndex();
	
	if(entry->level>LatexParser::getInstance().structureDepth() || entry->level<0){
		entry->debugPrint("Structure broken!");
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if(entry->parent->level>LatexParser::getInstance().structureDepth() || entry->parent->level<0){
		entry->debugPrint("Structure broken (b)!");
		//qDebug("Title %s",qPrintable(entry->title));
		return QModelIndex();
	}
	if (entry->parent->parent)
		return createIndex(entry->parent->getRealParentRow(), 0, entry->parent);
	else {
		if(m_singleMode)
			return createIndex(0, 0, entry->parent);
		for (int i=0; i < documents.documents.count(); i++)
			if (documents.documents.at(i)->baseStructure==entry->parent)
				return createIndex(i, 0, entry->parent);
		return QModelIndex();
	}
}

StructureEntry* LatexDocumentsModel::indexToStructureEntry(const QModelIndex & index ){
	if (!index.isValid()) return 0;
	StructureEntry* result=(StructureEntry*)index.internalPointer();
	if (!result || !result->document) return 0;
	return result;
}

LatexDocument* LatexDocumentsModel::indexToDocument(const QModelIndex & index ){
	StructureEntry* se = indexToStructureEntry(index);
	return se ? se->document : 0;
}

/*!
 Returns an associated SE_LABEL entry for a structure element if one exists, otherwise 0.
 TODO: currently association is checked, by checking, if the label is on the same line or on the next.
 This is not necessarily correct. It fails if:
  - there are multiple labels on one line (always the first label is chosen)
  - the label is more than one line after the entry (label not detected)
*/
StructureEntry *LatexDocumentsModel::labelForStructureEntry(const StructureEntry *entry)
{
	REQUIRE_RET(entry && entry->document,0 );
	QDocumentLineHandle *dlh = entry->getLineHandle();
	if (!dlh) return 0;
	QDocumentLineHandle *nextDlh = entry->document->line(entry->getRealLineNumber()+1).handle();
	StructureEntryIterator iter(entry->document->baseStructure);

	while (iter.hasNext()){
		StructureEntry *se = iter.next();
		if (se->type==StructureEntry::SE_LABEL) {
			QDocumentLineHandle *labelDlh = se->getLineHandle();
			if (labelDlh == dlh || labelDlh == nextDlh) {
				return se;
			}
		}
	}
	return 0;
}

QModelIndex LatexDocumentsModel::highlightedEntry(){
	return mHighlightIndex;
}
void LatexDocumentsModel::setHighlightedEntry(StructureEntry* entry){
	
	QModelIndex i1=mHighlightIndex;
	QModelIndex i2=index(entry);
	if (i1==i2) return;
	emit dataChanged(i1,i1);
	mHighlightIndex=i2;
	emit dataChanged(i2,i2);
}

void LatexDocumentsModel::resetAll(){
#if QT_VERSION<0x050000
#else
    beginResetModel();
#endif

	mHighlightIndex=QModelIndex();

#if QT_VERSION<0x050000
    reset();
#else
    endResetModel();
#endif
}

void LatexDocumentsModel::resetHighlight(){
	mHighlightIndex=QModelIndex();
}

void LatexDocumentsModel::structureUpdated(LatexDocument* document,StructureEntry *highlight){
	Q_UNUSED(document);
	//resetAll();
	if(highlight){
		mHighlightIndex=index(highlight);
	}else{
		mHighlightIndex=QModelIndex();
	}
	emit layoutChanged();
	//emit resetAll();
}
void LatexDocumentsModel::structureLost(LatexDocument* document){
	Q_UNUSED(document);
	resetAll();
}

void LatexDocumentsModel::removeElement(StructureEntry *se,int row){
	REQUIRE(se);
	if (!se->parent)
		beginRemoveRows(QModelIndex(),row,row); // remove from root (documents)
	else {
		if(row<0) row=se->getRealParentRow();
        else {Q_ASSERT(row < se->parent->children.size());
              Q_ASSERT(se->parent->children[row] == se);
        }
		beginRemoveRows(index(se->parent),row,row);
	}
}

void LatexDocumentsModel::removeElementFinished(){
	endRemoveRows();
}

void LatexDocumentsModel::addElement(StructureEntry *se, int row){
	beginInsertRows(index(se),row,row);
}

void LatexDocumentsModel::addElementFinished(){
	endInsertRows();
}

void LatexDocumentsModel::updateElement(StructureEntry *se){
	emit dataChanged(index(se),index(se));
}
void LatexDocumentsModel::setSingleDocMode(bool singleMode){
	if(m_singleMode!=singleMode){
		//resetAll();
		if(singleMode){
			foreach(LatexDocument *doc,documents.documents){
				changePersistentIndex(index(doc->baseStructure),createIndex(0,0,doc->baseStructure));
			}
		}else{
			for(int i=0;i<documents.documents.count();i++){
				StructureEntry *se=documents.documents.at(i)->baseStructure;
				changePersistentIndex(index(se),createIndex(i,0,se));
			}
		}
		m_singleMode=singleMode;
	}
	structureUpdated(documents.currentDocument,0);
}

void LatexDocumentsModel::moveDocs(int from,int to){
	REQUIRE(from >= 0 && to >= 0
		   && from < documents.documents.length()
		   && to < documents.documents.length() );
	QModelIndexList fl, tl;
	int d = from < to ? 1 : -1;
	for (int i=from; i != to + d ; i += d )
		fl.append(createIndex(i, 0, documents.documents.at(i)->baseStructure));
	tl.append(createIndex(to, 0, documents.documents.at(from)->baseStructure));
	for (int i=from + d; i != to + d; i += d )
		tl.append(createIndex(i - d, 0, documents.documents.at(i)->baseStructure));
	changePersistentIndexList(fl, tl);
}

bool LatexDocumentsModel::getSingleDocMode(){
  return m_singleMode;
}

LatexDocuments::LatexDocuments(): model(new LatexDocumentsModel(*this)), masterDocument(0), currentDocument(0), bibTeXFilesModified(false){
	showLineNumbersInStructure=false;
	indentationInStructure=-1;
	showCommentedElementsInStructure = false;
	markStructureElementsBeyondEnd = true;
	markStructureElementsInAppendix = true;
	indentIncludesInStructure = false;
    m_patchEnabled=true;
}

LatexDocuments::~LatexDocuments(){
	delete model;
}

void LatexDocuments::addDocument(LatexDocument* document,bool hidden){
    if(hidden){
        hiddenDocuments.append(document);
        LatexEditorView *edView=document->getEditorView();
        if (edView) {
            QEditor* ed=edView->getEditor();
            if(ed){
                document->remeberAutoReload=ed->silentReloadOnExternalChanges();
                ed->setSilentReloadOnExternalChanges(true);
                ed->setHidden(true);
            }
        }
    }else{
        documents.append(document);
    }
	connect(document, SIGNAL(updateBibTeXFiles()), SLOT(bibTeXFilesNeedUpdate()));
	connect(document,SIGNAL(structureLost(LatexDocument*)),model,SLOT(structureLost(LatexDocument*)));
	connect(document,SIGNAL(structureUpdated(LatexDocument*,StructureEntry*)),model,SLOT(structureUpdated(LatexDocument*,StructureEntry*)));
	connect(document,SIGNAL(toBeChanged()),model,SIGNAL(layoutAboutToBeChanged()));
	connect(document,SIGNAL(removeElement(StructureEntry*,int)),model,SLOT(removeElement(StructureEntry*,int)));
	connect(document,SIGNAL(removeElementFinished()),model,SLOT(removeElementFinished()));
	connect(document,SIGNAL(addElement(StructureEntry*,int)),model,SLOT(addElement(StructureEntry*,int)));
	connect(document,SIGNAL(addElementFinished()),model,SLOT(addElementFinished()));
	connect(document,SIGNAL(updateElement(StructureEntry*)),model,SLOT(updateElement(StructureEntry*)));
	document->parent=this;
	if(masterDocument){
		// repaint all docs
		foreach(const LatexDocument *doc,documents){
			LatexEditorView *edView=doc->getEditorView();
			if (edView) edView->documentContentChanged(0,edView->editor->document()->lines());
		}
	}
    if(!hidden)
        model->structureUpdated(document,0);
}


void LatexDocuments::deleteDocument(LatexDocument* document,bool hidden,bool purge){
    if(!hidden)
        emit aboutToDeleteDocument(document);
	LatexEditorView *view=document->getEditorView();
	if(view)
		view->closeCompleter();
	if (document!=masterDocument) {
		// get list of all affected documents
		QList<LatexDocument*> lstOfDocs=document->getListOfDocs();
        // special treatment to remove document in purge mode (hidden doc was deleted on disc)
        if(purge){
            Q_ASSERT(hidden); //purging non-hidden doc crashes.
            LatexDocument *rootDoc=document->getRootDocument();
            hiddenDocuments.removeAll(document);
            foreach(LatexDocument *elem,getDocuments()){
                if(elem->containsChild(document)){
                       elem->removeChild(document);
                }
            }
            //update children (connection to parents is severed)
            foreach(LatexDocument *elem,lstOfDocs){
                if(elem->getMasterDocument()==document){
                    if(elem->isHidden())
                        deleteDocument(elem,true,true);
                    else
                        elem->setMasterDocument(0);
                }
            }
            delete document;
            if (rootDoc != document) {
                // update parents
                lstOfDocs=rootDoc->getListOfDocs();
                int n=0;
                foreach(LatexDocument* elem,lstOfDocs){
                    if(!elem->isHidden()){
                        n++;
                        break;
                    }
                }
                if(n==0)
                    deleteDocument(rootDoc,true,true);
                else
                    updateMasterSlaveRelations(rootDoc,true,true);
            }
            return;
        }
        // count open related (child/parent) documents
        int n=0;
        foreach(LatexDocument* elem,lstOfDocs){
            if(!elem->isHidden())
                n++;
        }
        if(hidden){
            hiddenDocuments.removeAll(document);
            return;
        }
        if(n>1){ // at least one related document will be open after removal
            hiddenDocuments.append(document);
            LatexEditorView *edView=document->getEditorView();
            if (edView) {
                QEditor* ed=edView->getEditor();
                if(ed){
                    document->remeberAutoReload=ed->silentReloadOnExternalChanges();
                    ed->setSilentReloadOnExternalChanges(true);
                    ed->setHidden(true);
                }
            }
        }else{
            // no open document remains, remove all others as well
            foreach(LatexDocument *elem,getDocuments()){
                if(elem->containsChild(document)){
                       elem->removeChild(document);
                }
            }
            foreach(LatexDocument* elem,lstOfDocs){
                if(elem->isHidden()){
                    hiddenDocuments.removeAll(elem);
                    delete elem->getEditorView();
                    delete elem;
                }
            }
        }

		int row=documents.indexOf(document);
		//qDebug()<<document->getFileName()<<row;
		if (!document->baseStructure) row = -1; //may happen directly after reload (but won't)
		if(model->getSingleDocMode()){
			row=0;
		}
		if(row>=0 ){//&& !model->getSingleDocMode()){
			model->resetHighlight();
			model->removeElement(document->baseStructure,row); //remove from root
		}
		documents.removeAll(document);
		if (document==currentDocument){
			currentDocument=0;
		}
		if(row>=0 ){//&& !model->getSingleDocMode()){
			model->removeElementFinished();
		}
		//model->resetAll();
        if(n>1) {// don't remove document, stays hidden instead
            hideDocInEditor(document->getEditorView());
            return;
        }
		if (view) delete view;
		delete document;
	} else {
        if(hidden){
            hiddenDocuments.removeAll(document);
            return;
        }
		document->setFileName(document->getFileName());
		model->resetAll();
		document->clearAppendix();
		if (view) delete view;
		if (document==currentDocument)
			currentDocument=0;
	}
}

void LatexDocuments::requestedClose(){
    QEditor *editor = qobject_cast<QEditor*>(sender());
    LatexDocument *doc=qobject_cast<LatexDocument*>(editor->document());
    deleteDocument(doc,true,true);
}

void LatexDocuments::setMasterDocument(LatexDocument* document){
	if (document==masterDocument) return;
	if (masterDocument!=0 && masterDocument->getEditorView()==0){
        QString fn=masterDocument->getFileName();
        addDocToLoad(fn);
        LatexDocument *doc=masterDocument;
        masterDocument=0;
        deleteDocument(doc);
        //documents.removeAll(masterDocument);
        //delete masterDocument;
	}
	masterDocument=document;
	if (masterDocument!=0) {
		documents.removeAll(masterDocument);
		documents.prepend(masterDocument);
		// repaint doc
		foreach(LatexDocument *doc,documents){
			LatexEditorView *edView=doc->getEditorView();
			if (edView) edView->documentContentChanged(0,edView->editor->document()->lines());
		}
	}
	model->resetAll();
	emit masterDocumentChanged(masterDocument);
}
LatexDocument* LatexDocuments::getCurrentDocument() const{
	return currentDocument;
}
LatexDocument* LatexDocuments::getMasterDocument() const{
	return masterDocument;
}
QList<LatexDocument*> LatexDocuments::getDocuments() const{
    QList<LatexDocument*> docs=documents+hiddenDocuments;
    return docs;
}

void LatexDocuments::move(int from, int to){
  model->layoutAboutToBeChanged();
  model->moveDocs(from,to);
  documents.move(from,to);
  model->layoutChanged();
}

QString LatexDocuments::getCurrentFileName() const {
	if (!currentDocument) return "";
	return currentDocument->getFileName();
}
QString LatexDocuments::getCompileFileName() const {
	if (masterDocument)
		return masterDocument->getFileName();
	if (!currentDocument)
		return "";
    // check for magic comment
    QString curDocFile=currentDocument->getMagicComment("root");
    if(curDocFile.isEmpty())
        curDocFile=currentDocument->getMagicComment("texroot");
    if(!curDocFile.isEmpty()){
        return currentDocument->findFileName(curDocFile);
    }
    //
	const LatexDocument* rootDoc=currentDocument->getRootDocument();
    curDocFile = currentDocument->getFileName();
	if(rootDoc)
		curDocFile=rootDoc->getFileName();
	return curDocFile;
}
QString LatexDocuments::getTemporaryCompileFileName() const {
	QString temp = getCompileFileName();
	if (!temp.isEmpty()) return temp;
	if (masterDocument) return masterDocument->getTemporaryFileName();
	else if (currentDocument) return currentDocument->getTemporaryFileName();
	return "";
}

QString LatexDocuments::getLogFileName() const {
	if (!currentDocument) return QString();
	LatexDocument *rootDoc = currentDocument->getRootDocument();
	QString jobName = rootDoc->getMagicComment("-job-name");
	if (!jobName.isEmpty()) {
		return ensureTrailingDirSeparator(rootDoc->getFileInfo().absolutePath()) + jobName + ".log";
	} else {
		return replaceFileExtension(getTemporaryCompileFileName(), ".log");
	}
}

QString LatexDocuments::getAbsoluteFilePath(const QString & relName, const QString &extension, const QStringList &additionalSearchPaths) const {
	if (!currentDocument) return relName;
	return currentDocument->getAbsoluteFilePath(relName, extension, additionalSearchPaths);
}

LatexDocument* LatexDocuments::findDocumentFromName(const QString& fileName) const {
    QList<LatexDocument*> docs=getDocuments();
    foreach(LatexDocument *doc,docs){
		if(doc->getFileName()==fileName) return doc;
	}
	return 0;
}

LatexDocument* LatexDocuments::findDocument(const QDocument *qDoc) const {
    QList<LatexDocument*> docs=getDocuments();
    foreach(LatexDocument *doc,docs){
		LatexEditorView *edView=doc->getEditorView();
		if(edView && edView->editor->document()==qDoc) return doc;
	}
	return 0;
}

LatexDocument* LatexDocuments::findDocument(const QString& fileName, bool checkTemporaryNames) const {
	if (fileName=="") return 0;
	if (checkTemporaryNames) {
		LatexDocument* temp = findDocument(fileName, false);
		if (temp) return temp;
	}

	QFileInfo fi(fileName);
	if (fi.exists()) {
		foreach (LatexDocument* document, documents) {
			if (document->getFileInfo() == fi) {
				return document;
			}
		}
		if (checkTemporaryNames) {
			foreach (LatexDocument* document, documents) {
				if (document->getFileName().isEmpty() && document->getTemporaryFileInfo() == fi) {
					return document;
				}
			}
		}
	}
	
	//check for relative file names
	fi.setFile(getAbsoluteFilePath(fileName));
	if (!fi.exists()) {
		fi.setFile(getAbsoluteFilePath(fileName), ".tex");
	}
	if (!fi.exists()) {
		fi.setFile(getAbsoluteFilePath(fileName), ".bib");
	}
	if (fi.exists()) {
		foreach (LatexDocument* document, documents) {
			if (document->getFileInfo().exists() && document->getFileInfo()==fi) {
				return document;
			}
		}
	}
	
	return 0;
}
void LatexDocuments::settingsRead(){
	return; // currently unused
}
bool LatexDocuments::singleMode() const {
	return !masterDocument;
}

void LatexDocuments::updateBibFiles(bool updateFiles){
  mentionedBibTeXFiles.clear();
  QStringList additionalBibPaths = ConfigManagerInterface::getInstance()->getOption("Files/Bib Paths").toString().split(getPathListSeparator());
  foreach (LatexDocument* doc, getDocuments() ) {
    if(updateFiles){
      QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator it = doc->mentionedBibTeXFiles().begin();
      QMultiHash<QDocumentLineHandle*,FileNamePair>::iterator itend = doc->mentionedBibTeXFiles().end();
	  for (; it != itend; ++it){
		it.value().absolute = getAbsoluteFilePath(it.value().relative,".bib",additionalBibPaths).replace(QDir::separator(), "/"); // update absolute path
        mentionedBibTeXFiles << it.value().absolute;
      }
    }
  }

  //bool changed=false;
  if(updateFiles){
    QString bibFileEncoding = ConfigManagerInterface::getInstance()->getOption("Bibliography/BibFileEncoding").toString();
    QTextCodec *defaultCodec = QTextCodec::codecForName(bibFileEncoding.toLatin1());
    for (int i=0; i<mentionedBibTeXFiles.count();i++){
      QString &fileName=mentionedBibTeXFiles[i];
      QFileInfo fi(fileName);
      if (!fi.isReadable()) continue; //ups...
      if (!bibTeXFiles.contains(fileName))
        bibTeXFiles.insert(fileName,BibTeXFileInfo());
      BibTeXFileInfo& bibTex=bibTeXFiles[mentionedBibTeXFiles[i]];
      // TODO: allow to use the encoding of the tex file which mentions the bib file (need to port this information from above)
      bibTex.codec = defaultCodec;
      bibTex.loadIfModified(fileName);

      /*if (bibTex.loadIfModified(fileName))
        changed = true;*/
      if (bibTex.ids.empty() && !bibTex.linksTo.isEmpty())
        //handle obscure bib tex feature, a just line containing "link fileName"
        mentionedBibTeXFiles.append(bibTex.linksTo);
    }
  }
  /*
  if (changed || (newBibItems!=bibItems)) {
    allBibTeXIds.clear();
    bibItems=newBibItems;
    for (QMap<QString, BibTeXFileInfo>::const_iterator it=bibTeXFiles.constBegin(); it!=bibTeXFiles.constEnd();++it)
      foreach (const QString& s, it.value().ids)
        allBibTeXIds << s;
    allBibTeXIds.unite(bibItems);
    for (int i=0;i<documents.size();i++)
      if (documents[i]->getEditorView())
        documents[i]->getEditorView()->setBibTeXIds(&allBibTeXIds);
    bibTeXFilesModified=true;
  }*/
}


void LatexDocuments::removeDocs(QStringList removeIncludes){
    foreach(QString fname,removeIncludes){
        LatexDocument* dc=findDocumentFromName(fname);
        if(dc){
            foreach(LatexDocument *elem,getDocuments()){
                if(elem->containsChild(dc)){
                    elem->removeChild(dc);
                }
            }
        }
        if(dc && dc->isHidden()){
            QStringList toremove=dc->includedFiles();
            dc->setMasterDocument(0);
            hiddenDocuments.removeAll(dc);
            //qDebug()<<fname;
            delete dc->getEditorView();
            delete dc;
            if(!toremove.isEmpty())
                removeDocs(toremove);
        }
    }
}

void LatexDocuments::addDocToLoad(QString filename){
    emit docToLoad(filename);
}
void LatexDocuments::hideDocInEditor(LatexEditorView *edView){
    emit docToHide(edView);
}

void LatexDocument::findStructureEntryBefore(QMutableListIterator<StructureEntry*> &iter,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElements,int linenr,int count){
	bool goBack=false;
	int l=0;
	while(iter.hasNext()){
		StructureEntry* se=iter.next();
		int realline = se->getRealLineNumber();
		Q_ASSERT(realline >= 0);
		if(realline>=linenr && (realline<linenr+count) ){
			emit removeElement(se,l);
			iter.remove();
			emit removeElementFinished();
			MapOfElements.insert(se->getLineHandle(),se);
			l--;
		}
		if(realline>=linenr+count) {
			goBack=true;
			break;
		}
		l++;
	}
	if(goBack && iter.hasPrevious()) iter.previous();
}


void LatexDocument::mergeStructure(StructureEntry* se, QVector<StructureEntry*> &parent_level, QList<StructureEntry*>& flatStructure, const int linenr, const int count){
	if (!se) return;
	if (se->type != StructureEntry::SE_DOCUMENT_ROOT && se->type != StructureEntry::SE_SECTION && se->type != StructureEntry::SE_INCLUDE) return;
	if (se == baseStructure) parent_level.fill(se);
	int se_line = se->getRealLineNumber();
	if (se_line < linenr || se == baseStructure) {
		//se is before updated region, but children might still be in it
		updateParentVector(parent_level, se);
		
		//if (!se->children.isEmpty() && se->children.last()->getRealLineNumber() >= linenr) {
		int start = -1;
		for (int i=0;i<se->children.size();i++){
			StructureEntry* c = se->children[i];
			if (c->type != StructureEntry::SE_SECTION && c->type != StructureEntry::SE_INCLUDE) continue;
			if (c->getRealLineNumber() < linenr)
				updateParentVector(parent_level, c);
			start = i;
			break;
		}
		if (start >=0) {
			if (start > 0) start--;

			QList<StructureEntry*> oldChildren = se->children;
			for (int i=start;i<oldChildren.size();i++)
				mergeStructure(oldChildren[i], parent_level, flatStructure, linenr, count);
		}
	} else {
		//se is within or after the region
		// => insert flatStructure.first() before se or replace se with it (don't insert after since there might be another "se" to replace)
		while (!flatStructure.isEmpty() && se->getRealLineNumber() >= flatStructure.first()->getRealLineNumber() ) {
			if (se->getRealLineNumber() == flatStructure.first()->getRealLineNumber()) {
				flatStructure.first()->parent = se->parent;
				flatStructure.first()->children = se->children;
				*se = *flatStructure.first();
				flatStructure.first()->children.clear();
				delete flatStructure.takeFirst();
				emit updateElement(se);
				moveToAppropiatePositionWithSignal(parent_level, se);
				//	qDebug()<<"a"<<se->children.size() << ":"<<se->title<<" von "<<linenr<<count;
				QList<StructureEntry*> oldChildren = se->children;
				foreach (StructureEntry* next, oldChildren)
					mergeStructure(next, parent_level, flatStructure, linenr, count);
				return;
			}
			moveToAppropiatePositionWithSignal(parent_level, flatStructure.takeFirst());
		}

		if (se_line < linenr + count) {
			//se is within the region (delete if necessary and then merge children)
			if (flatStructure.isEmpty() || se->getRealLineNumber() < flatStructure.first()->getRealLineNumber()) {
				QList<StructureEntry*> oldChildren = se->children;
				int oldrow = se->getRealParentRow();
				for (int i=se->children.size()-1;i>=0;i--)
					moveElementWithSignal(se->children[i], se->parent, oldrow);
				removeElementWithSignal(se);
				delete se;
				for (int i=1;i<parent_level.size();i++)
					if (parent_level[i] == se)
						parent_level[i] = parent_level[i-1];
				foreach (StructureEntry* next, oldChildren)
					mergeStructure(next, parent_level, flatStructure, linenr, count);
				return;
			}
		}
		
		//se not replaced or deleted => se is after everything the region => keep children
		moveToAppropiatePositionWithSignal(parent_level, se);
		QList<StructureEntry*> oldChildren = se->children;
		foreach (StructureEntry* c, oldChildren)
			moveToAppropiatePositionWithSignal(parent_level, c);
		
	}
	
	//insert unprocessed elements of flatStructure at the end of the structure
	if (se == baseStructure && !flatStructure.isEmpty()) {
		foreach (StructureEntry* s, flatStructure){
			addElementWithSignal(parent_level[s->level], s);
			updateParentVector(parent_level, s);
		}
		flatStructure.clear();
	}
	
	
	return;
}

void LatexDocument::removeElementWithSignal(StructureEntry* se){
	int parentRow = se->getRealParentRow();
	REQUIRE(parentRow >= 0);
	emit removeElement(se, parentRow);
	se->parent->children.removeAt(parentRow);
	se->parent = 0;
	emit removeElementFinished();
}

void LatexDocument::addElementWithSignal(StructureEntry* parent, StructureEntry* se){
	emit addElement(parent, parent->children.size());
	parent->children.append(se);
	se->parent = parent;
	emit addElementFinished();
}

void LatexDocument::insertElementWithSignal(StructureEntry* parent, int pos, StructureEntry* se){
	emit addElement(parent, pos);
	parent->children.insert(pos, se);
	se->parent = parent;
	emit addElementFinished();
}

void LatexDocument::moveElementWithSignal(StructureEntry* se, StructureEntry* parent, int pos){
	removeElementWithSignal(se);
	insertElementWithSignal(parent, pos, se);
}


void LatexDocument::updateParentVector(QVector<StructureEntry*> &parent_level, StructureEntry* se){
	REQUIRE(se);
	if (se->type == StructureEntry::SE_DOCUMENT_ROOT
			|| (se->type == StructureEntry::SE_INCLUDE && parent && !parent->indentIncludesInStructure))
		parent_level.fill(baseStructure);
	else if (se->type == StructureEntry::SE_SECTION)
		for (int j=se->level+1;j<parent_level.size();j++)
			parent_level[j] = se;
}

class LessThanRealLineNumber
{
public:
	inline bool operator()(const StructureEntry * const se1, const StructureEntry * const se2) const
	{
		int l1 = se1->getRealLineNumber();
		int l2 = se2->getRealLineNumber();
		if (l1 < l2) return true;
		if (l1 == l2 && (se1->columnNumber < se2->columnNumber)) return true;
		return false;
	}
};


StructureEntry* LatexDocument::moveToAppropiatePositionWithSignal(QVector<StructureEntry*> &parent_level, StructureEntry* se){
	REQUIRE_RET(se, 0);
	StructureEntry* newParent = parent_level[se->level];
	if (se->parent == newParent) {
		updateParentVector(parent_level, se);
		return 0;
	}

	int newPos = newParent->children.size();
	if (newParent->children.size() > 0 &&
			newParent->children.last()->getRealLineNumber() >= se->getRealLineNumber())
		newPos = qUpperBound(newParent->children.begin(), newParent->children.end(), se, LessThanRealLineNumber()) - newParent->children.begin();
	
	
	//qDebug() << "auto insert at " << newPos;
	if (se->parent) moveElementWithSignal(se, newParent, newPos);
	else insertElementWithSignal(newParent, newPos, se);
	
	updateParentVector(parent_level, se);
	return newParent;
}

/*!
  Splits a [name] = [val] string into \a name and \a val removing extra spaces.
  
  \return true if splitting successful, false otherwise (in that case name and val are empty)
 */
bool LatexDocument::splitMagicComment(const QString &comment, QString &name, QString &val) {
	int sep = comment.indexOf("=");
	if (sep < 0) return false;
	name = comment.left(sep).trimmed();
	val = comment.mid(sep+1).trimmed();
	return true;
}

/*!
  Used by the parser to add a magic comment

\a text is the comment without the leading "! TeX" declaration. e.g. "spellcheck = DE-de"
\a lineNr - line number of the magic comment
\a MapOfMagicComments - MutliHashTable of all magic comments
\a iter_magicComment - iterator over all magic comments placed at the last entry before line
  */
void LatexDocument::addMagicComment(const QString &text, int lineNr, QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfMagicComments, QMutableListIterator<StructureEntry*>& iter_magicComment) {
	bool reuse=false;
	StructureEntry *newMagicComment;
	QDocumentLineHandle *dlh = line(lineNr).handle();
	if(MapOfMagicComments.contains(dlh)){
		newMagicComment=MapOfMagicComments.value(dlh);
		newMagicComment->type=StructureEntry::SE_MAGICCOMMENT;
		MapOfMagicComments.remove(dlh,newMagicComment);
		reuse=true;
	}else{
		newMagicComment=new StructureEntry(this, StructureEntry::SE_MAGICCOMMENT);
	}
	QString name;
	QString val;
	splitMagicComment(text, name, val);

	parseMagicComment(name, val, newMagicComment);
	newMagicComment->title=text;
	newMagicComment->setLine(dlh, lineNr);
	newMagicComment->parent=magicCommentList;
	if(!reuse) emit addElement(magicCommentList,magicCommentList->children.size()); //todo: why here but not in label?
	iter_magicComment.insert(newMagicComment);
}

/*!
  Formats the StructureEntry and modifies the document according to the MagicComment contents
  */
void LatexDocument::parseMagicComment(const QString &name, const QString &val, StructureEntry* se) {
	se->valid = false;
	se->tooltip = QString();

	QString lowerName = name.toLower();
	if (lowerName == "spellcheck") {
		mSpellingDictName = val;
		emit spellingDictChanged(mSpellingDictName);
		se->valid = true;
	} else if ((lowerName == "texroot")||(lowerName == "root")){
		QString fname=findFileName(val);
		LatexDocument* dc=parent->findDocumentFromName(fname);
        if(dc){
            dc->childDocs.insert(this);
            setMasterDocument(dc);
        } else {
			parent->addDocToLoad(fname);
		}
		se->valid = true;
	} else if (lowerName == "encoding") {
		QTextCodec *codec = QTextCodec::codecForName(val.toLatin1());
		if (!codec) {
			se->tooltip = tr("Invalid codec");
			return;
		}
		setCodecDirect(codec);
		emit encodingChanged();
		se->valid = true;
	} else if (lowerName == "txs-script") {
		se->valid = true;
	} else if (lowerName == "program" || lowerName.startsWith("txs-program:")) {
		se->valid = true;
	} else if (lowerName == "-job-name") {
		if (!val.isEmpty()) {
			se->valid = true;
		} else {
			se->tooltip = tr("Missing value for -job-name");
		}
	} else {
		se->tooltip = tr("Unknown magic comment");
	}
}

void LatexDocument::updateContext(QDocumentLineHandle *oldLine,QDocumentLineHandle *newLine, StructureEntry::Context context){
	int endLine=newLine?indexOf(newLine):-1 ;
	int startLine=-1;
	if(oldLine){
		startLine=indexOf(oldLine);
		if(endLine<0 || endLine>startLine){
			// remove appendic marker
			StructureEntry *se=baseStructure;
			setContextForLines(se,startLine,endLine,context,false);
		}
	}

	if(endLine>-1 && (endLine<startLine || startLine<0)){
		StructureEntry *se=baseStructure;
		setContextForLines(se,endLine,startLine,context,true);
	}
}

void LatexDocument::setContextForLines(StructureEntry *se, int startLine, int endLine, StructureEntry::Context context, bool state){
	bool first=false;
	for(int i=0;i<se->children.size();i++){
		StructureEntry *elem=se->children[i];
		if(endLine>=0 && elem->getLineHandle() && elem->getRealLineNumber()>endLine) break;
		if(elem->type==StructureEntry::SE_SECTION && elem->getRealLineNumber()>startLine){
			if(!first && i>0) setContextForLines(se->children[i-1],startLine,endLine,context,state);
			elem->setContext(context, state);
			emit updateElement(elem);
			setContextForLines(se->children[i],startLine,endLine,context,state);
			first=true;
		}
	}
	if(!first && !se->children.isEmpty()) {
		StructureEntry *elem=se->children.last();
		if(elem->type==StructureEntry::SE_SECTION) setContextForLines(elem,startLine,endLine,context,state);
	}
}

bool LatexDocument::fileExits(QString fname){
	QString curPath=ensureTrailingDirSeparator(getFileInfo().absolutePath());
	bool exist=QFile(getAbsoluteFilePath(fname,".tex")).exists();
	if (!exist) exist=QFile(getAbsoluteFilePath(curPath+fname,".tex")).exists();
	if (!exist) exist=QFile(getAbsoluteFilePath(curPath+fname,"")).exists();
	return exist;
}

/*
 * A line snapshot is a list of DocumentLineHandles at a given time.
 * For example, this is used to reconstruct the line number at latex compile time
 * allowing syncing from PDF to the correct source line also after altering the source document
 */
void LatexDocument::saveLineSnapshot() {
	foreach (QDocumentLineHandle *dlh, mLineSnapshot) {
		dlh->deref();
	}
	mLineSnapshot.clear();
#if (QT_VERSION >= 0x040700)
	mLineSnapshot.reserve(lineCount());
#endif
	QDocumentConstIterator it = begin(), e = end();
	while (it != e) {
		mLineSnapshot.append(*it);
		(*it)->ref();
		it++;
	}
}

// get the line with given lineNumber (0-based) from the snapshot
QDocumentLine LatexDocument::lineFromLineSnapshot(int lineNumber) {
	if (lineNumber < 0 || lineNumber >= mLineSnapshot.count()) return QDocumentLine();
	return QDocumentLine(mLineSnapshot.at(lineNumber));
}

// returns the 0-based number of the line in the snapshot, or -1 if line is not in the snapshot
int LatexDocument::lineToLineSnapshotLineNumber(const QDocumentLine &line) {
	return mLineSnapshot.indexOf(line.handle());
}

QString LatexDocument::findFileName(QString fname){
	QString curPath=ensureTrailingDirSeparator(getFileInfo().absolutePath());
	QString result;
	if(QFile(getAbsoluteFilePath(fname,".tex")).exists())
		result=QFileInfo(getAbsoluteFilePath(fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(getAbsoluteFilePath(curPath+fname,".tex")).exists())
		result=QFileInfo(getAbsoluteFilePath(curPath+fname,".tex")).absoluteFilePath();
	if (result.isEmpty() && QFile(getAbsoluteFilePath(curPath+fname,"")).exists())
		result=QFileInfo(getAbsoluteFilePath(curPath+fname,"")).absoluteFilePath();
	return result;
}

void LatexDocuments::updateStructure(){
	foreach(const LatexDocument* doc,documents){
		model->updateElement(doc->baseStructure);
	}
	if(model->getSingleDocMode()){
		model->structureUpdated(currentDocument,0);
	}
}

void LatexDocuments::bibTeXFilesNeedUpdate(){
	bibTeXFilesModified=true;
}

void LatexDocuments::updateMasterSlaveRelations(LatexDocument *doc, bool recheckRefs,bool updateCompleterNow){
	//update Master/Child relations
	//remove old settings ...
    doc->setMasterDocument(0,false);
    QList<LatexDocument*> docs=getDocuments();
    foreach(LatexDocument* elem,docs){
		if(elem->getMasterDocument()==doc){
            elem->setMasterDocument(0,recheckRefs);
            doc->removeChild(elem);
            //elem->recheckRefsLabels();
		}
	}
	
	//check whether document is child of other docs
	QString fname=doc->getFileName();
    foreach(LatexDocument* elem,docs){
		if(elem==doc)
			continue;
		QStringList includedFiles=elem->includedFiles();
		if(includedFiles.contains(fname)){
            elem->addChild(doc);
            doc->setMasterDocument(elem,recheckRefs);
		}
	}
	
	// check for already open child documents (included in this file)
    QStringList includedFiles=doc->includedFiles();
	foreach(const QString& fname,includedFiles){
		LatexDocument* child=this->findDocumentFromName(fname);
		if(child){
            doc->addChild(child);
            child->setMasterDocument(doc,recheckRefs);
            if(recheckRefs)
                child->reCheckSyntax(); // redo syntax checking (in case of defined commands)
		}
	}
	
	//recheck references
    if(recheckRefs)
        doc->recheckRefsLabels();

    if(updateCompleterNow)
        doc->emitUpdateCompleter();
}

const LatexDocument* LatexDocument::getRootDocument(QSet<const LatexDocument*> *visitedDocs) const {
	const LatexDocument *result=this;
	bool deleteVisitedDocs=false;
	if(!visitedDocs){
		visitedDocs=new QSet<const LatexDocument*>();
		deleteVisitedDocs=true;
	}
	visitedDocs->insert(this);
	if(masterDocument && !visitedDocs->contains(masterDocument))
		result=masterDocument->getRootDocument(visitedDocs);
	if (result->getFileName().endsWith("bib"))
		foreach (const LatexDocument* d, parent->documents) {
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator it = d->mentionedBibTeXFiles().constBegin();
			QMultiHash<QDocumentLineHandle*,FileNamePair>::const_iterator itend = d->mentionedBibTeXFiles().constEnd();
			for (; it != itend; ++it) {
				//qDebug() << it.value().absolute << " <> "<<result->getFileName();
				if (it.value().absolute == result->getFileName()){
					result = d->getRootDocument(visitedDocs);
					break;
				}
			}
			if (result == d) break;
		}
	if(deleteVisitedDocs)
		delete visitedDocs;
	return result;
}

LatexDocument* LatexDocument::getRootDocument(){
	return const_cast<LatexDocument*>(getRootDocument(0));
}

QStringList LatexDocument::includedFiles(){
    QStringList helper=mIncludedFilesList.values();
    QStringList result;
    foreach(const QString elem,helper){
        if(!elem.isEmpty() && !result.contains(elem))
            result<<elem;
    }

    return result;
}

QStringList LatexDocument::includedFilesAndParent(){
     QStringList result = includedFiles();
     QString t = getMagicComment("root");
     if (!t.isEmpty() && !result.contains(t)) result << t;
     t = getMagicComment("texroot");
     if (!t.isEmpty() && !result.contains(t)) result << t;
     if (masterDocument && !result.contains(masterDocument->getFileName()))
          result << masterDocument->getFileName();
     return result;
}

CodeSnippetList LatexDocument::additionalCommandsList(){
    LatexPackage pck;
    QStringList loadedFiles,files;
    files=mCWLFiles.toList();
    gatherCompletionFiles(files,loadedFiles,pck,true);
    return pck.completionWords;
}

bool LatexDocument::updateCompletionFiles(bool forceUpdate,bool forceLabelUpdate,bool delayUpdate){

    QStringList files=mUsepackageList.values();
	bool update=forceUpdate;
	
	//recheck syntax of ALL documents ...
	LatexPackage pck;
	QStringList loadedFiles;
	for(int i=0;i<files.count();i++){
		if(!files.at(i).endsWith(".cwl"))
			files[i]=files[i]+".cwl";
	}
    //files.append(completerConfig->getLoadedFiles());
    gatherCompletionFiles(files,loadedFiles,pck);
	update=true;
	
    /*
    // special treatment for special defs
    QHash<QString,QSet<QString> > possibleCommands;
    foreach(const QString elem,ltxCommands.specialDefCommands.values()){
        possibleCommands.insert(elem,ltxCommands.possibleCommands[elem]);
    }*/

    //completerConfig->words=pck.completionWords;
    //mCompleterWords=pck.completionWords.toSet();
    mCWLFiles=loadedFiles.toSet();
	ltxCommands.optionCommands=pck.optionCommands;
    ltxCommands.specialTreatmentCommands=pck.specialTreatmentCommands;
    ltxCommands.specialDefCommands=pck.specialDefCommands;
	ltxCommands.possibleCommands=pck.possibleCommands;
	ltxCommands.environmentAliases=pck.environmentAliases;
    ltxCommands.commandDefs=pck.commandDescriptions;
	
	// user commands
    QList<CodeSnippet> commands=mUserCommandList.values();
    foreach(CodeSnippet cs,commands){
        QString elem=cs.word;
        if(elem.startsWith("%")){ // insert specialArgs
            int i=elem.indexOf('%',1);
            QString category=elem.left(i);
            elem=elem.mid(i+1);
            ltxCommands.possibleCommands[category].insert(elem);
            continue;
        }
		if(!elem.startsWith("\\begin{")&&!elem.startsWith("\\end{")){
			int i=elem.indexOf("{");
            int j=elem.indexOf("[");
			if(i>=0) elem=elem.left(i);
            if(j>=0 && j<i) elem=elem.left(j);
		}
		ltxCommands.possibleCommands["user"].insert(elem);
	}
    /*
    // special treatment for special defs
    foreach(const QString elem,ltxCommands.specialDefCommands.values()){
        ltxCommands.possibleCommands.insert(elem,ltxCommands.possibleCommands[elem].unite(possibleCommands[elem]));
    }*/

	//patch lines for new commands (ref,def, etc)
	LatexParser& latexParser = LatexParser::getInstance();
	QStringList categories;
    categories<< "%ref" << "%label" << "%definition" << "%cite" << "%citeExtended" << "%citeExtendedCommand" << "%usepackage" << "%graphics" << "%file" << "%bibliography" << "%include" << "%url" << "%todo" <<"%replace";
	QStringList newCmds;
	foreach(const QString elem,categories){
		QStringList cmds=ltxCommands.possibleCommands[elem].values();
		foreach(const QString cmd,cmds){
			if(!latexParser.possibleCommands[elem].contains(cmd) || forceLabelUpdate){
				newCmds << cmd;
				latexParser.possibleCommands[elem]<< cmd;
			}
		}
	}

    if(!newCmds.isEmpty()){
		patchLinesContaining(newCmds);
    }
	
    if(delayUpdate)
        return update;

	if(update){
        updateLtxCommands(true);
	}
    return false;
}

void LatexDocument::emitUpdateCompleter(){
	emit updateCompleter();
}

void LatexDocument::gatherCompletionFiles(QStringList &files,QStringList &loadedFiles,LatexPackage &pck,bool gatherForCompleter){
	LatexPackage zw;
	LatexCompleterConfig *completerConfig=edView->getCompleter()->getConfig();
	foreach(const QString& elem, files){
		if(loadedFiles.contains(elem))
			continue;
        if(parent->cachedPackages.contains(elem)){
			zw=parent->cachedPackages.value(elem);
		}else{
			QString fileName = LatexPackage::keyToCwlFilename(elem);
			QStringList options = LatexPackage::keyToOptions(elem).split(',');
            zw=loadCwlFile(fileName,completerConfig,options);
			if(!zw.notFound){
				parent->cachedPackages.insert(elem,zw); // cache package
			}else{
				LatexPackage zw;
				zw.packageName=elem;
				parent->cachedPackages.insert(elem,zw); // cache package as empty/not found package
			}
		}
		if(zw.notFound){
            QString name=elem;
            LatexDocument *masterDoc=getRootDocument();
            if(masterDoc){
                QString fn=masterDoc->getFileInfo().absolutePath();
				name+="/"+fn;
				// TODO: oha, the key can be even more complex: option#filename.cwl/masterfile
				// consider this in the key-handling functions of LatexPackage
            }
            emit importPackage(name);
		} else {
            pck.unite(zw,gatherForCompleter);
			loadedFiles.append(elem);
			if(!zw.requiredPackages.isEmpty())
				gatherCompletionFiles(zw.requiredPackages,loadedFiles,pck);
		}
	}
}

QString LatexDocument::getMagicComment(const QString& name) const{
	QString seName;
	QString val;
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		splitMagicComment(se->title, seName, val);
        if (seName.toLower()==name.toLower())
			return val;
	}
	return QString();
}

StructureEntry* LatexDocument::getMagicCommentEntry(const QString& name) const{
	QString seName;
	QString val;
	
	if(!magicCommentList) return NULL;
	
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		splitMagicComment(se->title, seName, val);
		if (seName==name) return se;
	}
	return NULL;
}

/*!
  replaces the value of the magic comment
 */
void LatexDocument::updateMagicComment(const QString &name, const QString &val, bool createIfNonExisting) {
	QString line(QString("\% !TeX %1 = %2").arg(name).arg(val));
	
	StructureEntry* se = getMagicCommentEntry(name);
	QDocumentLineHandle* dlh = se ? se->getLineHandle() : NULL;
	if(dlh) {
		QString n, v;
		splitMagicComment(se->title, n, v);
		if (v != val) {
			QDocumentCursor cur(this, indexOf(dlh));
			cur.select(QDocumentCursor::LineUnderCursor);
			cur.replaceSelectedText(line);
		}
	} else {
		if (createIfNonExisting) {
			QDocumentCursor cur(this);
			cur.insertText(line+"\n");
		}
	}
}

void LatexDocument::updateMagicCommentScripts(){
	if (!magicCommentList) return;
	
	localMacros.clear();
	
	QRegExp rxTrigger(" *// *(Trigger) *[:=](.*)");
	
	StructureEntryIterator iter(magicCommentList);
	while (iter.hasNext()) {
		StructureEntry *se = iter.next();
		QString seName, val;
		splitMagicComment(se->title, seName, val);
		if (seName=="TXS-SCRIPT") {
			QString name = val;
			QString trigger = "";
			QString tag = "%SCRIPT\n";
			
			int l = se->getRealLineNumber() + 1;
			for (; l < lineCount(); l++) {
				QString lt = line(l).text().trimmed();
				if (lt.endsWith("TXS-SCRIPT-END") || !(lt.isEmpty() || lt.startsWith("%"))  ) break;
				lt.remove(0,1);
				tag += lt + "\n";
				if (rxTrigger.exactMatch(lt))
					trigger = rxTrigger.cap(2).trimmed();
			}
			
			Macro newMacro(name, tag, "", trigger);
			newMacro.document = this;
			localMacros.append(newMacro);
		}
	}

}

QStringList LatexDocument::containedPackages(){
    QStringList helper=mUsepackageList.values();
    for(int l=0;l<helper.size();++l){
        QString elem=helper.value(l);
        if(elem.contains('#')){
            int i=elem.indexOf('#');
            helper[l]=elem.mid(i+1);
        }
    }

    return helper;
}

bool LatexDocument::containsPackage(const QString& name){
    QStringList helper=containedPackages();
    return helper.contains(name);
}

LatexDocument *LatexDocuments::getRootDocumentForDoc(LatexDocument *doc) const { // doc==0 means current document
	if(masterDocument)
		return masterDocument;
	LatexDocument *current=currentDocument;
	if(doc)
		current=doc;
	if(!current)
		return current;
	return current->getRootDocument();
}

QString LatexDocument::getAbsoluteFilePath(const QString & relName, const QString &extension, const QStringList &additionalSearchPaths) const {
	QStringList searchPaths;
	const LatexDocument* rootDoc = getRootDocument();
	QString compileFileName = rootDoc->getFileName();
	if (compileFileName.isEmpty()) compileFileName = rootDoc->getTemporaryFileName();
	QString fallbackPath;
	if (!compileFileName.isEmpty()) {
		fallbackPath = QFileInfo(compileFileName).absolutePath(); //when the file does not exist, resolve it relative to document (e.g. to create it there)
		searchPaths << fallbackPath;
	}
	searchPaths << additionalSearchPaths;
	return findAbsoluteFilePath(relName, extension, searchPaths, fallbackPath);
}

void LatexDocuments::lineGrammarChecked(const void* doc,const void* line,int lineNr, const QList<GrammarError>& errors){
	int d = documents.indexOf(static_cast<LatexDocument*>(const_cast<void*>(doc)));
	if (d == -1) return;
	if (!documents[d]->getEditorView()) return;
	documents[d]->getEditorView()->lineGrammarChecked(doc,line,lineNr,errors);
}

void LatexDocument::patchLinesContaining(const QStringList cmds){
  foreach(LatexDocument *elem,getListOfDocs()){
    // search all cmds in all lines, patch line if cmd is found
    for (int i=0;i<elem->lines();i++){
      QString text=elem->line(i).text();
      foreach(const QString cmd,cmds){
        if(text.contains(cmd)){
          //elem->patchStructure(i,1);
          patchStructure(i,1);
          break;
        }
      }
    }
  }
}

void LatexDocuments::enablePatch(const bool enable){
    m_patchEnabled=enable;
}

bool LatexDocuments::patchEnabled(){
    return m_patchEnabled;
}

QString LatexDocuments::findPackageByCommand(const QString command){
    // go through all cached packages (cwl) and find command in one of them
    QString result;
    foreach(const QString key,cachedPackages.keys()){
        const LatexPackage pck=cachedPackages.value(key);
        foreach(const QString envs,pck.possibleCommands.keys()){
            if(pck.possibleCommands.value(envs).contains(command)){
				result=LatexPackage::keyToCwlFilename(key); //pck.packageName;
                break;
            }
        }
        if(!result.isEmpty())
            break;
    }
    return result;
}


void LatexDocument::updateLtxCommands(bool updateAll){
    lp.init();
    lp.append(LatexParser::getInstance()); // append commands set in config
    QList<LatexDocument *>listOfDocs=getListOfDocs();
    foreach(const LatexDocument *elem,listOfDocs){
        lp.append(elem->ltxCommands);
    }

    if(updateAll){
        foreach(LatexDocument *elem,listOfDocs){
            elem->setLtxCommands(lp);
            elem->reCheckSyntax();
        }
        // check if other document have this doc as child as well (reused doc...)
        LatexDocuments* docs=parent;
        QList<LatexDocument*>lstOfAllDocs=docs->getDocuments();
        foreach(LatexDocument* elem,lstOfAllDocs){
            if(listOfDocs.contains(elem))
                continue; // already handled
            if(elem->containsChild(this)){
                // unhandled parent/child
                LatexParser lp;
                lp.init();
                lp.append(LatexParser::getInstance()); // append commands set in config
                QList<LatexDocument *>listOfDocs=elem->getListOfDocs();
                foreach(const LatexDocument *elem,listOfDocs){
                    lp.append(elem->ltxCommands);
                }
                foreach(LatexDocument *elem,listOfDocs){
                    elem->setLtxCommands(lp);
                    elem->reCheckSyntax();
                }
            }
        }
    }else{
        SynChecker.setLtxCommands(lp);
    }
}

void LatexDocument::setLtxCommands(const LatexParser& cmds){
     SynChecker.setLtxCommands(cmds);
     lp=cmds;

     LatexEditorView *view=getEditorView();
     if(view){
         view->updateReplamentList(cmds,true);
     }
}

void LatexDocument::updateSettings()
{
    SynChecker.setErrFormat(syntaxErrorFormat);
}
void LatexDocument::checkNextLine(QDocumentLineHandle *dlh,bool clearOverlay,int ticket){
    Q_ASSERT_X(dlh!=0,"checkNextLine","empty dlh used in checkNextLine");
    if(dlh->getRef()>1 && dlh->getCurrentTicket()==ticket){
        StackEnvironment env;
        QVariant envVar=dlh->getCookieLocked(QDocumentLine::STACK_ENVIRONMENT_COOKIE);
        if(envVar.isValid())
            env=envVar.value<StackEnvironment>();
        int index = indexOf(dlh);
        if (index == -1) return; //deleted
        REQUIRE(dlh->document() == this);
        if (index + 1 >= lines()) {
            //remove old errror marker
            if(unclosedEnv.id!=-1){
                unclosedEnv.id = -1;
                int unclosedEnvIndex = indexOf(unclosedEnv.dlh);
                if (unclosedEnvIndex >= 0 && unclosedEnv.dlh->getCookieLocked(QDocumentLine::UNCLOSED_ENVIRONMENT_COOKIE).isValid()){
                    StackEnvironment env;
                    Environment newEnv;
                    newEnv.name="normal";
                    newEnv.id=1;
                    env.push(newEnv);
                    TokenStack remainder;
                    if (unclosedEnvIndex >= 1) {
                        QDocumentLineHandle *prev = line(unclosedEnvIndex-1).handle();
                        QVariant result=prev->getCookieLocked(QDocumentLine::STACK_ENVIRONMENT_COOKIE);
                        if(result.isValid())
                            env=result.value<StackEnvironment>();
                        remainder=prev->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
                    }
                    SynChecker.putLine(unclosedEnv.dlh, env,remainder, true);
                }
            }
            if(env.size()>1){
                //at least one env has not been closed
                Environment environment=env.top();
                unclosedEnv=env.top();
                SynChecker.markUnclosedEnv(environment);
            }
            return;
        }
        TokenStack remainder=dlh->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
        SynChecker.putLine(line(index+1).handle(), env,remainder, clearOverlay);
    }
    dlh->deref();
}

void LatexDocument::reCheckSyntax(int linenr, int count){

    if(linenr<0 || linenr>=lineCount()) linenr=0;
    //patchStructure(0,-1,true);

    QDocumentLine line=this->line(linenr);
    QDocumentLine prev;
    if (linenr > 0) prev = this->line(linenr - 1);
    int lineNrEnd = count < 0 ? lineCount() : qMin(count + linenr, lineCount());
    for (int i=linenr; i < lineNrEnd; i++) {
        Q_ASSERT(line.isValid());
        StackEnvironment env;
        getEnv(i,env);
        TokenStack remainder;
        if(prev.handle())
            remainder=prev.handle()->getCookieLocked(QDocumentLine::LEXER_REMAINDER_COOKIE).value<TokenStack >();
        SynChecker.putLine(line.handle(),env,remainder,true);
        prev = line;
        line = this->line(i+1);
    }
}

QString LatexDocument::getErrorAt(QDocumentLineHandle *dlh, int pos, StackEnvironment previous,TokenStack stack)
{
    return SynChecker.getErrorAt(dlh,pos,previous,stack);
}

int LatexDocument::syntaxErrorFormat;

void LatexDocument::getEnv(int lineNumber,StackEnvironment &env){
    Environment newEnv;
    newEnv.name="normal";
    newEnv.id=1;
    env.push(newEnv);
    if (lineNumber > 0) {
        QDocumentLine prev = this->line(lineNumber - 1);
        REQUIRE(prev.isValid());
        QVariant result=prev.getCookie(QDocumentLine::STACK_ENVIRONMENT_COOKIE);
        if(result.isValid())
            env=result.value<StackEnvironment>();
    }
}

QString LatexDocument::getLastEnvName(int lineNumber){
    StackEnvironment env;
    getEnv(lineNumber,env);
    if(env.isEmpty())
        return "";
    return env.top().name;
}
