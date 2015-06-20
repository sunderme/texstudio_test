#ifndef CODESNIPPET_H
#define CODESNIPPET_H

#include "mostQtHeaders.h"

class QDocumentCursor;
class QEditor;
class Texmaker;

enum CompletionType { CT_COMMANDS,CT_NORMALTEXT,CT_CITATIONS,CT_CITATIONCOMMANDS};

struct CodeSnippetPlaceHolder{
	int offset, length;
	int id;
	enum Flag{AutoSelect = 1, Mirrored = 2, Mirror = 4, PreferredMultilineAutoSelect = 8, Persistent = 16, Translatable = 32};
	int flags;
	int offsetEnd() const;
};

class CodeSnippet
{
public:
    CodeSnippet():cursorLine(-1), cursorOffset(-1),anchorOffset(-1),usageCount(0),index(0),snippetLength(0),type(none) {}
    CodeSnippet(const CodeSnippet &cw):word(cw.word),sortWord(cw.sortWord),lines(cw.lines),cursorLine(cw.cursorLine),cursorOffset(cw.cursorOffset),anchorOffset(cw.anchorOffset),placeHolders(cw.placeHolders),usageCount(cw.usageCount),index(cw.index),snippetLength(cw.snippetLength),type(cw.type),name(cw.name){}
	CodeSnippet(const QString &newWord, bool replacePercentNewline = true);
	bool operator< (const CodeSnippet &cw) const;
	bool operator== (const CodeSnippet &cw) const;

	enum PlaceholderMode { PlacehodersActive, PlaceholdersToPlainText, PlaceholdersRemoved };

	QString word,sortWord;
	QStringList lines; 
	//TODO: Multirow selection
	int cursorLine;  //-1 => not defined
	int cursorOffset; //-1 => not defined
	int anchorOffset;
	QList<QList<CodeSnippetPlaceHolder> > placeHolders; //used to draw
	int usageCount;
	uint index;
	int snippetLength;
    enum Type {none,length};
    Type type;
	
	QString expandCode(const QString &code);
	QString environmentContent(const QString &envName);
	void insert(QEditor* editor) const;
	void insertAt(QEditor* editor, QDocumentCursor* cursor, PlaceholderMode placeholderMode=PlacehodersActive, bool byCompleter=false, bool isKeyVal=false) const;

	void setName(const QString& newName);
	QString getName() const;

	static bool autoReplaceCommands;

	static bool debugDisableAutoTranslate;
	
private:
	QString name;
	QDocumentCursor getCursor(QEditor* editor, const CodeSnippetPlaceHolder &ph, int snippetLine, int baseLine, int baseLineIndent, int lastLineRemainingLength) const;
};

Q_DECLARE_METATYPE(CodeSnippet);
Q_DECLARE_METATYPE(CodeSnippet::PlaceholderMode);

class CodeSnippetList: public QList<CodeSnippet> {
public:
    void unite(CodeSnippetList &lst);
    void unite(const QList<CodeSnippet> &lst);
    void insert(const QString &elem);
    CodeSnippetList::iterator insert(CodeSnippetList::iterator before,const CodeSnippet &cs){
        return QList<CodeSnippet>::insert(before,cs);
    }
};

#endif // CODESNIPPET_H
