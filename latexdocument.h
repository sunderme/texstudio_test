#ifndef LATEXDOCUMENT_H
#define LATEXDOCUMENT_H
//#undef QT_NO_DEBUG
#include "mostQtHeaders.h"
#include "bibtexparser.h"
#include "latexeditorview.h"
#include "configmanagerinterface.h"
#include "usermacro.h"

class QDocumentLineHandle;
class LatexEditorView;
class LatexDocument;
class LatexDocuments;
class QDocument;
class QDocumentCursor;
struct QDocumentSelection;

struct StructureEntry{
	enum Type {SE_DOCUMENT_ROOT,SE_OVERVIEW,SE_SECTION,SE_BIBTEX,SE_TODO,SE_MAGICCOMMENT,SE_INCLUDE,SE_LABEL,SE_BLOCK=SE_LABEL};
	enum Context {
		 InAppendix = 0x0001,
		 BeyondEnd = 0x0010
	};
	Q_DECLARE_FLAGS(Contexts, Context)
	Type type;
	QString title;
	QString tooltip; // optional because most tooltips are automatically generated.
	int level; //only used for section types!
	bool valid; //currently only used for includes and magic comments
	QList<StructureEntry*> children;
	StructureEntry* parent;
	LatexDocument* document;
	
	StructureEntry(LatexDocument* doc, Type newType);
	~StructureEntry();
	void add(StructureEntry* child);
	void insert(int pos, StructureEntry* child);

	int columnNumber; //position of the entry in the line, only used for correct sorting of structure update (TODO: use a local variable for it)
	void setLine(QDocumentLineHandle* handle, int lineNr = -1);
	QDocumentLineHandle* getLineHandle() const;
	int getCachedLineNumber() const;
	int getRealLineNumber() const;
	int getRealParentRow() const;

	bool hasContext(Context c) const { return m_contexts & c; }
	void setContext(Context c, bool b=true) { if (b) m_contexts |= c; else m_contexts &= ~c; }
	
	void debugPrint(const char* message) const;
private:
	mutable int parentRow;
	QDocumentLineHandle* lineHandle;
	mutable int lineNumber;
	Contexts m_contexts;
};

Q_DECLARE_METATYPE(StructureEntry*)

//iterator for fast traversal of a structure entry tree
class StructureEntryIterator{
public:
	StructureEntryIterator(StructureEntry* entry);
	bool hasNext();
	StructureEntry* next();
private:
	QList<StructureEntry*> entryHierarchy; //hierarchy of next element (all parents and element itself)
	QList<int> indexHierarchy; //for every element in entryHierarchy the index of this element in its parent children
};

struct FileNamePair{
	QString relative, absolute;
	FileNamePair(const QString& rel, const QString& abs);
};

struct ReferencePair{
	QString name;
	int start;
};

class LatexDocument: public QDocument
{
	Q_OBJECT
public:
	LatexDocument(QObject *parent=0);
	~LatexDocument();
	
	enum CookieType{
		CK_COLS=0
	};
	void setFileName(const QString& fileName);
	void setEditorView(LatexEditorView* edView);
	LatexEditorView *getEditorView() const;
	QString getFileName() const;
	QFileInfo getFileInfo() const;
	//QSet<QString> texFiles; //absolute file names, also contains fileName
	
	Q_PROPERTY(QString fileName READ getFileName);
	Q_PROPERTY(QFileInfo fileInfo READ getFileInfo);
	Q_PROPERTY(LatexEditorView* editorView READ getEditorView);

    bool isHidden();
	
	//	References containedLabels,containedReferences;
	QMultiHash<QDocumentLineHandle*,FileNamePair>& mentionedBibTeXFiles();
	const QMultiHash<QDocumentLineHandle*,FileNamePair>& mentionedBibTeXFiles() const;
    QStringList listOfMentionedBibTeXFiles() const;
	QSet<QString> lastCompiledBibTeXFiles;
	
	QList<Macro> localMacros;

    friend class SyntaxCheckTest;

private:	
	static QStringList someItems(const QMultiHash<QDocumentLineHandle*,ReferencePair>& list);
public:
	Q_INVOKABLE QStringList labelItems() const;
	Q_INVOKABLE QStringList refItems() const;
	Q_INVOKABLE QStringList bibItems() const;
    Q_INVOKABLE QList<CodeSnippet> userCommandList() const{
        QList<CodeSnippet> csl=mUserCommandList.values();
        qSort(csl);
        return csl;
	}
    Q_INVOKABLE CodeSnippetList additionalCommandsList();
    /*{
		return mCompleterWords;
    }*/
	void updateRefsLabels(const QString& ref);
	void recheckRefsLabels();
	Q_INVOKABLE int countLabels(const QString& name);
	Q_INVOKABLE int countRefs(const QString& name);
	Q_INVOKABLE bool bibIdValid(const QString& name);
	Q_INVOKABLE bool isBibItem(const QString& name);
	Q_INVOKABLE QString findFileFromBibId(const QString& name);
	Q_INVOKABLE QMultiHash<QDocumentLineHandle*,int> getLabels(const QString& name);
	Q_INVOKABLE QMultiHash<QDocumentLineHandle*,int> getRefs(const QString& name);
	Q_INVOKABLE QMultiHash<QDocumentLineHandle*,int> getBibItems(const QString& name);
	Q_INVOKABLE void replaceItems(QMultiHash<QDocumentLineHandle*,ReferencePair> items, const QString& newName, QDocumentCursor *cursor=0);
	Q_INVOKABLE void replaceLabel(const QString& name, const QString& newName, QDocumentCursor *cursor=0);
	Q_INVOKABLE void replaceRefs(const QString& name, const QString& newName, QDocumentCursor *cursor=0);
	Q_INVOKABLE void replaceLabelsAndRefs(const QString& name, const QString& newName);
	
	void patchLinesContaining(const QStringList cmds);
	
	StructureEntry* baseStructure;
	
	QDocumentSelection sectionSelection(StructureEntry* section);
	void clearAppendix(){
		mAppendixLine=0;
	}
	StructureEntry * findSectionForLine(int currentLine);
	
	LatexDocuments *parent;
	
	void setTemporaryFileName(const QString& fileName);
	Q_INVOKABLE QString getTemporaryFileName() const;
	Q_INVOKABLE QFileInfo getTemporaryFileInfo() const;
	Q_INVOKABLE QString getAbsoluteFilePath(const QString & relName, const QString &extension, const QStringList &additionalSearchPaths=QStringList()) const;
	
    void setMasterDocument(LatexDocument* doc, bool recheck=true);
    void addChild(LatexDocument* doc);
    void removeChild(LatexDocument* doc);
    bool containsChild(LatexDocument* doc) const;
	Q_INVOKABLE LatexDocument* getMasterDocument() const{
		return masterDocument;
	}
	const LatexDocument* getRootDocument(QSet<const LatexDocument*> *visitedDocs=0) const; 
	Q_INVOKABLE LatexDocument* getRootDocument(); 
	Q_INVOKABLE LatexDocument* getTopMasterDocument() {return getRootDocument();}   // DEPRECATED: only the for backward compatibility of user scripts
	
	Q_INVOKABLE QStringList includedFiles();
	Q_INVOKABLE QStringList includedFilesAndParent();
	Q_INVOKABLE QList<LatexDocument *> getListOfDocs(QSet<LatexDocument*> *visitedDocs=0);
	
    LatexParser ltxCommands,lp;
	
	Q_INVOKABLE bool containsPackage(const QString& name);
    Q_INVOKABLE QStringList containedPackages();
    bool updateCompletionFiles(bool forceUpdate, bool forceLabelUpdate=false, bool delayUpdate=false);
	
	Q_INVOKABLE QString spellingDictName() const {
		return mSpellingDictName;
	}
	Q_INVOKABLE QString getMagicComment(const QString& name) const;
	Q_INVOKABLE StructureEntry* getMagicCommentEntry(const QString &name) const;
	Q_INVOKABLE void updateMagicComment(const QString &name, const QString &val, bool createIfNonExisting=false);
	
	void updateMagicCommentScripts();
	static bool splitMagicComment(const QString &comment, QString &name, QString &val);

    QString findFileName(QString fname);
    bool fileExits(QString fname);

	void saveLineSnapshot();
	QDocumentLine lineFromLineSnapshot(int lineNumber);
	int lineToLineSnapshotLineNumber(const QDocumentLine &line);

    bool remeberAutoReload; //remember whether doc is auto reloaded while hidden (and auto reload is always activated).

    bool mayHaveDiffMarkers;

    void emitUpdateCompleter();

    static int syntaxErrorFormat;

    void reCheckSyntax(int linenr=0, int count=-1);
    QString getErrorAt(QDocumentLineHandle *dlh, int pos, StackEnvironment previous, TokenStack stack);

    void getEnv(int lineNumber,StackEnvironment &env); // get Environment for syntax checking, number of cols is now part of env
    Q_INVOKABLE QString getLastEnvName(int lineNumber); // special function to use with javascript (insert "\item" from menu)
	
    void enableSyntaxCheck(bool enable){
        latexLikeChecking=enable;
    }

private:
	QString fileName; //absolute
	QString temporaryFileName; //absolute, temporary
	QFileInfo fileInfo;
	
	LatexEditorView* edView;
	
	LatexDocument* masterDocument;
    QSet<LatexDocument*> childDocs;
	
	StructureEntry* magicCommentList;
	StructureEntry* labelList;
	StructureEntry* todoList;
	StructureEntry* bibTeXList;
	StructureEntry* blockList;
	
	QMultiHash<QDocumentLineHandle*,ReferencePair> mLabelItem;
    QMultiHash<QDocumentLineHandle*,ReferencePair> mBibItem;
	QMultiHash<QDocumentLineHandle*,ReferencePair> mRefItem;
	QMultiHash<QDocumentLineHandle*,FileNamePair> mMentionedBibTeXFiles;
    QMultiHash<QDocumentLineHandle*,CodeSnippet> mUserCommandList;
	QMultiHash<QDocumentLineHandle*,QString> mUsepackageList;
    QMultiHash<QDocumentLineHandle*,QString> mIncludedFilesList;

	QList<QDocumentLineHandle *> mLineSnapshot;
	
	QSet<QString> mCompleterWords; // local list of completer words
    QSet<QString> mCWLFiles;
	
	QString mSpellingDictName;

    QString mClassOptions; // store class options, if they are defined in this doc
	
    QDocumentLineHandle *mAppendixLine,*mBeyondEnd;
	
	void updateContext(QDocumentLineHandle *oldLine,QDocumentLineHandle *newLine, StructureEntry::Context context);
	void setContextForLines(StructureEntry *se, int startLine, int endLine, StructureEntry::Context context, bool state);

	void findStructureEntryBefore(QMutableListIterator<StructureEntry*> &iter,QMultiHash<QDocumentLineHandle*,StructureEntry*> &MapOfElemnts,int linenr,int count);
	void mergeStructure(StructureEntry* se, QVector<StructureEntry*> &parent_level, QList<StructureEntry*>& flatStructure, const int linenr, const int count);
	
	void removeElementWithSignal(StructureEntry* se);
	void addElementWithSignal(StructureEntry* parent, StructureEntry* se);
	void insertElementWithSignal(StructureEntry* parent, int pos, StructureEntry* se);
	void moveElementWithSignal(StructureEntry* se, StructureEntry* parent, int pos);
	
	void updateParentVector(QVector<StructureEntry*> &parent_level, StructureEntry* se);
	StructureEntry* moveToAppropiatePositionWithSignal(QVector<StructureEntry*> &parent_level, StructureEntry* se);
	
	void addMagicComment(const QString &text, int lineNr, QMultiHash<QDocumentLineHandle *, StructureEntry *> &MapOfMagicComments, QMutableListIterator<StructureEntry *> &iter_magicComment);
	void parseMagicComment(const QString &name, const QString &val, StructureEntry* se);
	
	void gatherCompletionFiles(QStringList &files,QStringList &loadedFiles,LatexPackage &pck);

    SyntaxCheck SynChecker;
    Environment unclosedEnv;

    bool latexLikeChecking;


#ifndef QT_NO_DEBUG
public:
	QSet<StructureEntry*> StructureContent;
	void checkForLeak();
#endif
	
public slots:
	void updateStructure();
    bool patchStructure(int linenr, int count, bool recheck=false);
	void patchStructureRemoval(QDocumentLineHandle* dlh);
	void initClearStructure();
    void updateLtxCommands(bool updateAll=false);
    void setLtxCommands(const LatexParser& cmds);
    void updateSettings();
    void checkNextLine(QDocumentLineHandle *dlh,bool clearOverlay,int ticket);
	
signals:
	void hasBeenIncluded(const LatexDocument& newMasterDocument);
	void structureUpdated(LatexDocument* document,StructureEntry *highlight=0);
	void structureLost(LatexDocument* document);
	void removeElement(StructureEntry *se,int row);
	void removeElementFinished();
	void addElement(StructureEntry *se,int row);
	void addElementFinished();
	void updateElement(StructureEntry *se);
	void updateCompleter();
	void updateBibTeXFiles();
	void toBeChanged();
	void importPackage(QString name);
	void spellingDictChanged(const QString &name);
	void encodingChanged();
    void bookmarkLineUpdated(int lineNr);
};
Q_DECLARE_METATYPE(LatexDocument*)

class LatexDocumentsModel: public QAbstractItemModel{
	Q_OBJECT
private:
	LatexDocuments& documents;
	QIcon iconDocument, iconMasterDocument, iconBibTeX, iconInclude, iconWarning;
	QVector<QIcon> iconSection;
	QModelIndex mHighlightIndex;
	bool m_singleMode;
	
public:
	LatexDocumentsModel(LatexDocuments& docs);
	Qt::ItemFlags flags ( const QModelIndex & index ) const;
	QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
	QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
	int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
	int columnCount ( const QModelIndex & parent = QModelIndex() ) const;
	QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
	QModelIndex index ( StructureEntry* entry ) const;
	QModelIndex parent ( const QModelIndex & index ) const;
	
	static StructureEntry* indexToStructureEntry(const QModelIndex & index );
	static LatexDocument* indexToDocument(const QModelIndex & index );
	static StructureEntry* labelForStructureEntry(const StructureEntry* entry);
	QModelIndex highlightedEntry();
	void setHighlightedEntry(StructureEntry* entry);
	
	void resetAll();
	void resetHighlight();
	void setSingleDocMode(bool singleMode);
	bool getSingleDocMode();
	void moveDocs(int from,int to);
private slots:
	void structureUpdated(LatexDocument* document,StructureEntry *highlight=0);
	void structureLost(LatexDocument* document);
	void removeElement(StructureEntry *se,int row);
	void removeElementFinished();
	void addElement(StructureEntry *se,int row);
	void addElementFinished();
	void updateElement(StructureEntry *se);
	
	friend class LatexDocuments;
};

class LatexDocuments: public QObject
{
	Q_OBJECT
public:
	LatexDocumentsModel* model;
	LatexDocument* masterDocument;
	LatexDocument* currentDocument;
	QList<LatexDocument*> documents;
    QList<LatexDocument*> hiddenDocuments;
	
	LatexDocuments();
	~LatexDocuments();
    void addDocument(LatexDocument* document, bool hidden = false);
    void deleteDocument(LatexDocument* document, bool hidden = false, bool purge = false);
    void move(int from,int to);
	Q_INVOKABLE void setMasterDocument(LatexDocument* document);
	Q_INVOKABLE LatexDocument* getCurrentDocument() const;
	Q_INVOKABLE LatexDocument* getMasterDocument() const;
	Q_INVOKABLE QList<LatexDocument*> getDocuments() const;
	
	Q_PROPERTY(LatexDocument* currentDocument READ getCurrentDocument);
	Q_PROPERTY(LatexDocument* masterDocument READ getMasterDocument);
	Q_PROPERTY(QList<LatexDocument*> documents READ getDocuments);  //<- semicolon necessary due to qt bug 22992
	
	Q_INVOKABLE LatexDocument *getRootDocumentForDoc(LatexDocument *doc = 0) const ; // no argument means current doc ...
	
	Q_INVOKABLE QString getCurrentFileName() const; //returns the absolute file name of the current file or "" if none is opened
	Q_INVOKABLE QString getCompileFileName() const; //returns the absolute file name of the file to be compiled (master or current)
	Q_INVOKABLE QString getTemporaryCompileFileName() const; //returns the absolute file name of the file to be compiled (master or current)
	Q_INVOKABLE QString getLogFileName() const;
	Q_INVOKABLE QString getAbsoluteFilePath(const QString & relName, const QString &extension="", const QStringList &additionalSearchPaths=QStringList()) const;
	
	Q_INVOKABLE LatexDocument* findDocument(const QString& fileName, bool checkTemporaryNames = false) const;
	Q_INVOKABLE LatexDocument* findDocument(const QDocument *qDoc) const;
	Q_INVOKABLE LatexDocument* findDocumentFromName(const QString& fileName) const;
	
	void settingsRead();
	
	Q_INVOKABLE bool singleMode() const;

	//support for included BibTeX-files
	QMap<QString, BibTeXFileInfo> bibTeXFiles; //bibtex files loaded by txs
	bool bibTeXFilesModified; //true iff the BibTeX files were changed after the last compilation
	QStringList mentionedBibTeXFiles; //bibtex files imported in the tex file (absolute after updateBibFiles)
    //QSet<QString> bibItems; // direct defined bibitems
    //QSet<QString> allBibTeXIds;
	void updateBibFiles(bool updateFiles=true);
	
	void updateStructure();
    void updateMasterSlaveRelations(LatexDocument *doc, bool recheckRefs=true, bool updateCompleterNow=false);
	
	bool showLineNumbersInStructure;
	int indentationInStructure;
	bool showCommentedElementsInStructure;
	bool markStructureElementsBeyondEnd;
	bool markStructureElementsInAppendix;
	bool indentIncludesInStructure; //pointer! those above should be changed as well


	QHash<QString,LatexPackage> cachedPackages;
    void addDocToLoad(QString filename);
    void removeDocs(QStringList removeIncludes);
    void hideDocInEditor(LatexEditorView *edView);
    QString findPackageByCommand(const QString command);
    void enablePatch(const bool enable);
    bool patchEnabled();
signals:
	void masterDocumentChanged(LatexDocument *masterDocument);
	void aboutToDeleteDocument(LatexDocument *document);
    void docToLoad(QString filename);
    void docToHide(LatexEditorView *edView);
private slots:
	void bibTeXFilesNeedUpdate();
public slots:
	void lineGrammarChecked(const void* doc,const void* line,int lineNr, const QList<GrammarError>& errors);
    void requestedClose();
private:
    bool m_patchEnabled;
};

#endif // LATEXDOCUMENT_H
