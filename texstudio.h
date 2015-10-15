/***************************************************************************
 *   copyright       : (C) 2003-2007 by Pascal Brachet                     *
 *   http://www.xm1math.net/texmaker/                                      *
 *                         2008-2009 by Benito van der Zander              *
 *   http://texstudio.sourceforge.net
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TEXSTUDIO_H
#define TEXSTUDIO_H

#include "mostQtHeaders.h"

#include "bibtexparser.h"
#include "buildmanager.h"
#include "bookmarks.h"
#include "codesnippet.h"
#include "configmanager.h"
#include "cursorhistory.h"
#include "latexlog.h"
#include "latexdocument.h"
#include "latexeditorview.h"
#include "latexcompleter.h"
#include "findGlobalDialog.h"
#include "symbolgridwidget.h"
#include "xmltagslistwidget.h"
#include "spellerdialog.h"
#include "textanalysis.h"
#include "toolwidgets.h"
#include "txstabwidget.h"
#include "searchresultwidget.h"
#include "unicodeinsertion.h"
#include "tablemanipulation.h"
#include "PDFDocument.h"
#include <QSplashScreen>
#include "session.h"
#include "sessionlist.h"
#include "qformatfactory.h"
#include "qlanguagefactory.h"
#include "qlinemarksinfocenter.h"
#include "latexstyleparser.h"
#include "kpathseaParser.h"
#include "diffoperations.h"

#include <QProgressDialog>

typedef QHash<QString, int> SymbolList;

typedef QSet<QString> StringSet;

enum SVNSTATUS {
	SVN_Unknown = 0,
	SVN_Unmanaged,
	SVN_Modified,
	SVN_Locked,
	SVN_CheckedIn,
	SVN_InConflict
};

class UserMenuDialog;
class GrammarCheck;
class FileSelector;
class LatexReference;

Q_DECLARE_METATYPE(QSet<QString>)

class Texstudio : public QMainWindow
{
	Q_OBJECT

public:
	Texstudio(QWidget *parent = 0, Qt::WindowFlags flags = 0, QSplashScreen *splash = 0);
	~Texstudio();

	Q_INVOKABLE QString getCurrentFileName(); //returns the absolute file name of the current file or "" if none is opene
	Q_INVOKABLE QString getAbsoluteFilePath(const QString &relName, const QString &extension = ""); //treats the path relative to the compiled .tex file
	Q_INVOKABLE QString getRelativeFileName(const QString &file, QString basepath, bool keepSuffix = false); //provide function for scripts
	QByteArray windowstate;
	bool tobemaximized, tobefullscreen;

public slots:
	LatexEditorView *load(const QString &f , bool asProject = false, bool hidden = false, bool recheck = true, bool dontAsk = false);
	void executeCommandLine(const QStringList &args, bool realCmdLine);
	void startupCompleted();
	void onOtherInstanceMessage(const QString &);  // For messages for the single instance

	void fuzzCursorHistory();
	void fuzzBackForward();


protected:
	//these are just wrappers around configManager so we don't have to type so much (todo??? move them to configmanager.h and use a singleton design???)
	Q_INVOKABLE inline QMenu *newManagedMenu(const QString &id, const QString &text) { return configManager.newManagedMenu(id, text); }
	Q_INVOKABLE inline QMenu *newManagedMenu(QMenu *menu, const QString &id, const QString &text) { return configManager.newManagedMenu(menu, id, text); }
	Q_INVOKABLE QAction *newManagedAction(QWidget *menu, const QString &id, const QString &text, const char *slotName = 0, const QKeySequence &shortCut = 0, const QString &iconFile = "", const QList<QVariant> &args = QList<QVariant>());
	Q_INVOKABLE QAction *newManagedAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QList<QKeySequence> &shortCuts, const QString &iconFile = "", const QList<QVariant> &args = QList<QVariant>());
	Q_INVOKABLE QAction *newManagedEditorAction(QWidget *menu, const QString &id, const QString &text, const char *slotName = 0, const QKeySequence &shortCut = 0, const QString &iconFile = "", const QList<QVariant> &args = QList<QVariant>());
	Q_INVOKABLE QAction *newManagedEditorAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QList<QKeySequence> &shortCuts, const QString &iconFile = "", const QList<QVariant> &args = QList<QVariant>());
	Q_INVOKABLE inline QAction *newManagedAction(QWidget *menu, const QString &id, QAction *act) { return configManager.newManagedAction(menu, id, act); }
	Q_INVOKABLE inline QMenu *getManagedMenu(const QString &id) { return configManager.getManagedMenu(id); }
	Q_INVOKABLE inline QAction *getManagedAction(const QString &id) { return configManager.getManagedAction(id); }
	Q_INVOKABLE inline QList<QAction *> getManagedActions(const QStringList &ids, const QString &commonPrefix = "") { return configManager.getManagedActions(ids, commonPrefix); }
	Q_INVOKABLE QAction *insertManagedAction(QAction *before, const QString &id, const QString &text, const char *slotName = 0, const QKeySequence &shortCut = 0, const QString &iconFile = "");

	SymbolGridWidget *addSymbolGrid(const QString &SymbolList,  const QString &iconName, const QString &text);
	void addTagList(const QString &id, const QString &iconName, const QString &text, const QString &tagFile);

	QVariantMap *MapForSymbols;

private slots:
	void updateToolBarMenu(const QString &menuName);
	void showTestProgress(const QString &message);
private:
	void executeTests(const QStringList &args);
	void generateAddtionalTranslations();
	void setupMenus();
	void setupDockWidgets();
	void setupToolBars();
	void createStatusBar();
	bool activateEditorForFile(QString f, bool checkTemporaryNames = false, bool setFocus = true);
	bool saveAllFilesForClosing(); // checks for unsaved files and asks the user if they should be saved
	bool saveFilesForClosing(const QList<LatexEditorView *> &editors); // checks for unsaved files and asks the user if they should be saved
	void closeAllFiles();
	bool canCloseNow(bool saveSettings = true); //asks the user and close all files, and prepares to exit txs
	void closeEvent(QCloseEvent *e);

	void updateUserMacros(bool updateMenu = true);

	void updateEmphasizedRegion(QDocumentCursor c, int sid);

	QFormatScheme *m_formats, *m_formatsOldDefault;
	QLanguageFactory *m_languages;
	LatexCompleter *completer;
	LatexReference *latexReference;

	QPointer<UnicodeInsertion> unicodeInsertionDialog;

	//gui
	TxsTabWidget *EditorTabs;
	QSplitter *mainHSplitter;
	QSplitter *centralVSplitter;
	QFrame *centralFrame;
	QToolBar *centralToolBar;
	CustomWidgetList *leftPanel;
	SymbolGridWidget *MostUsedSymbolWidget, *FavoriteSymbolWidget;
	QString hiddenLeftPanelWidgets;

	QTreeView *structureTreeView;
	LatexParser latexParser;
public:
	LatexDocuments documents;
private:
	OutputViewWidget *outputView; //contains output widgets (over OutputLayout)

	//menu
	QActionGroup *bibtexEntryActions;
	QActionGroup *biblatexEntryActions;
	QActionGroup *bibTypeActions;
	QActionGroup *highlightLanguageActions;
	QActionGroup *actgroupRootDocMode;
	QAction *actRootDocAutomatic;
	QAction *actRootDocExplicit;
	QAction *actRootDocSetExplicit;

	//toolbars
	QAction *ToggleRememberAct;
	QAction *actSave, *actUndo, *actRedo;

	QLabel *statusLabelMode, *statusLabelProcess;
	QToolButton *statusTbLanguage;
	QToolButton *statusTbEncoding;
	QActionGroup *spellLanguageActions;

	CursorHistory *cursorHistory;

	//settings
	ConfigManager configManager;
public:
	BuildManager buildManager;
private:
	QStringList struct_level;

	qreal pdfSplitterRel;

	SpellerManager spellerManager;
	SafeThread grammarCheckThread;
	GrammarCheck *grammarCheck;
	Bookmarks *bookmarks;
	SessionList *recentSessionList;

	//dialogs
	TextAnalysisDialog *textAnalysisDlg;
	SpellerDialog *spellDlg;

	//tools
	usercodelist symbolMostused;
	QStringList symbolFavorites;

	Q_INVOKABLE LatexEditorView *currentEditorView() const;
	Q_INVOKABLE QEditor *currentEditor() const;
	void configureNewEditorView(LatexEditorView *edit);
	void configureNewEditorViewEnd(LatexEditorView *edit, bool asMaster = false, bool hidden = false);
	LatexEditorView *getEditorViewFromFileName(const QString &fileName, bool checkTemporaryNames = false);

	QAction *fullscreenModeAction;

	int runningPDFCommands, runningPDFAsyncCommands;

	void updateUserToolMenu();
	void linkToEditorSlot(QAction *act, const char *slot, const QList<QVariant> &args);
private slots:
	void relayToEditorSlot();
	void relayToOwnSlot();
	void autoRunScripts();
	void runScripts(int trigger);
	void runScriptsInList(int trigger, const QList<Macro> &scripts);

protected slots:
	void fileNew(QString fileName = "");
	void fileNewFromTemplate();
	void fileMakeTemplate();
	void fileOpen();
	void fileRestoreSession(bool showProgress = true, bool warnMissing = true);
	void fileSave(const bool saveSilently = false);
	void fileSaveAll();
	void fileSaveAll(bool alsoUnnamedFiles, bool alwaysCurrentFile);
	void fileSaveAs(const QString &fileName = "") { fileSaveAs(fileName, false); }
private slots:
	void fileSaveAs(const QString &fileName, const bool saveSilently);
	void fileNewInternal(QString fileName = "");
protected slots:
	void fileUtilCopyMove(bool move);
	void fileUtilDelete();
	void fileUtilRevert();
	void fileUtilPermissions();
	void fileUtilCopyFileName();
	void fileUtilCopyMasterFileName();
	void fileClose();
	void fileCloseAll();
	void fileExit();
private:
	void centerFileSelector();
protected slots:
	void fileOpenRecent();
	void fileOpenAllRecent();
	void fileRecentList();
	void viewDocumentListHidden();
	void fileDocumentOpenFromChoosen(const QString &doc, int duplicate, int lineNr, int column);
	void viewDocumentList();
	void viewDocumentOpenFromChoosen(const QString &doc, int duplicate, int lineNr, int column);
	void fileOpenFirstNonOpen();
	void fileOpenRecentProject();
	void fileLoadSession();
	void loadSession(const QString &fileName);
	void fileSaveSession();
private slots:
	void restoreSession(const Session &s, bool showProgress = true, bool warnMissing = true);
	Session getCurrentSession();
protected slots:
	void MarkCurrentFileAsRecent();
private slots:
	void fileCheckin(QString filename = "");
	void fileLockPdf(QString filename = "");
	void fileCheckinPdf(QString filename = "");
	void fileUpdate(QString filename = "");
	void fileUpdateCWD(QString filename = "");
	void checkinAfterSave(QString filename);
	void checkin(QString fn, QString text = "txs auto checkin", bool blocking = false);
	bool svnadd(QString fn, int stage = 0);
	void svncreateRep(QString fn);
	void svnUndo(bool redo = false);
	void svnPatch(QEditor *ed, QString diff);
	void svnLock(QString fn);
	SVNSTATUS svnStatus(QString filename);
	void showOldRevisions();
	QStringList svnLog();
	void changeToRevision(QString rev, QString old_rev = "");
	void svnDialogClosed();
	void fileDiff();
	void fileDiff3();
	bool checkSVNConflicted(bool substituteContents = true);
	void removeDiffMarkers(bool theirs = false);
	void editChangeDiff(QPoint pt);
	void jumpNextDiff();
	void jumpPrevDiff();
	void fileDiffMerge();
	void declareConflictResolved();
protected slots:
	void openExternalFile();
	void openExternalFile(QString name, const QString &defaultExt = "tex", LatexDocument *doc = 0); // signaled by latexViewer to open specific file

	void editUndo();
	void editRedo();
	void editDebugUndoStack();
	void editCopy();
	void editPaste();
	void editPasteImage(QImage image);
	void editSectionCopy();
	void editSectionCopy(int startingLine, int endLine);
	void editSectionCut();
	void editSectionCut(int startingLine, int endLine);
	void editSectionPasteAfter();
	void editSectionPasteAfter(int line);
	void editSectionPasteBefore();
	void editSectionPasteBefore(int line);
	void editTextToLowercase();
	void editTextToUppercase();
	void editTextToTitlecase(bool smart = false);
	void editTextToTitlecaseSmart();
	void editFind();
	void editPasteLatex();
	void convertToLatex();
	void editPasteRef();
	void editIndentSection();
	void editUnIndentSection();
	void editHardLineBreak();
	void editHardLineBreakRepeat();
	void editDeleteLine();
	void editDeleteToEndOfLine();
	void editDeleteFromStartOfLine();
	void editMoveLineUp();
	void editMoveLineDown();
	void editDuplicateLine();
	void editEraseWordCmdEnv();
	void editGotoDefinition(QDocumentCursor c = QDocumentCursor());
	void editSpell();
	void editThesaurus(int line = -1, int col = -1);
	void editChangeLineEnding();
	void editSetupEncoding();
	void editInsertUnicode();
	void editInsertRefToNextLabel(const QString &refCmd = "\\ref", bool backward = false);
	void editInsertRefToPrevLabel(const QString &refCmd = "\\ref");
	void editFindGlobal();
	void runSearch(SearchQuery *query);
	void findLabelUsages();
	void findLabelUsages(LatexDocument *doc, const QString &labelText);
	SearchResultWidget *searchResultWidget();

	void findWordRepetions();
	void findNextWordRepetion();

	void StructureContextMenu(const QPoint &point);
	void structureContextMenuCloseDocument();
	void structureContextMenuSwitchMasterDocument();
	void structureContextMenuOpenAllRelatedDocuments();
	void structureContextMenuCloseAllRelatedDocuments();
	void structureContextMenuExpandSubitems();
	void structureContextMenuCollapseSubitems();
	void structureContextMenuExpandAllDocuments();
	void structureContextMenuCollapseAllDocuments();
	void symbolGridContextMenu(QWidget *widget, const QPoint &point);
	void mostUsedSymbolsTriggered(bool direct = false);
	void symbolAddFavorite();
	void symbolRemoveFavorite();
	void symbolRemoveAllFavorites();

	void editorTabContextMenu(const QPoint &point);

	void addDocToLoad(QString filename);

	void moveCursorTodlh();

private slots:
	void readSettings(bool reread = false);
	void saveSettings(const QString &configName = "");
	void restoreDefaultSettings();

protected slots:
	void showMarkTooltipForLogMessage(QList<int> errors);
	void newDocumentStatus();
	void newDocumentLineEnding();
	void updateCaption();
	void updateMasterDocumentCaption();
	void updateUndoRedoStatus();
	void currentEditorChanged();
	void editorTabMoved(int from, int to);
	void editorAboutToChangeByTabClick(LatexEditorView *edFrom, LatexEditorView *edTo);

	void getExpandedStructureEntries(const QModelIndex &index, QSet<QString> &expandedEntryTags, QString baseTag = QString());
	void expandStructureEntries(const QModelIndex index, const QSet<QString> &expandedEntryTags, QString baseTag = QString());
	void updateStructure(bool initial = false, LatexDocument *doc = 0, bool hidden = false);
	void showStructure();
	void clickedOnStructureEntry(const QModelIndex &index);
	void editRemovePlaceHolders();
	void editRemoveCurrentPlaceHolder();

	void normalCompletion();
	void insertEnvironmentCompletion();
	void insertTextCompletion();
	void insertTag(const QString &Entity, int dx = 0, int dy = 0);
	void insertCitation(const QString &text);
	void insertFormula(const QString &formula);
	void insertSymbol(QTableWidgetItem *item);
	void insertSymbolPressed(QTableWidgetItem *item);
	void insertXmlTag(QListWidgetItem *item);
	void insertXmlTagFromToolButtonAction();
	void callToolButtonAction();
	void insertFromAction();
	void insertBib();
	void closeEnvironment();

	void insertBibEntryFromAction();
	void insertBibEntry(const QString &id = "");
	void setBibTypeFromAction();

	void insertUserTag(const QString &macro, int triggerId = 0);
	void insertUserTag();
	void editMacros();
	void macroDialogAccepted();

	void insertRef(const QString &refCmd);
	void insertRef();
	void insertEqRef();
	void insertPageRef();
	void createLabelFromAction();

	void changeTextCodec();
	void updateAvailableLanguages();
	void editorSpellerChanged(const QString &name);
	void changeEditorSpeller();
	void insertSpellcheckMagicComment();
	void updateStatusBarEncoding();
	void addMagicRoot();
	void addMagicCoding();

	void quickTabular();
	void quickArray();
	void quickTabbing();
	void quickLetter();
	void quickDocument();
	void quickBeamer();
	void quickGraphics(const QString &graphicsFile = QString());
	void quickMath();

	bool checkProgramPermission(const QString &program, const QString &cmdId, LatexDocument *master);
	void runInternalPdfViewer(const QFileInfo &master, const QString &options);
	void runBibliographyIfNecessary(const QFileInfo &cmd);

	void searchExtendToggled(bool toggled);

	void changeIconSize(int value);
	void changeSecondaryIconSize(int value);
	void changeSymbolGridIconSize(int value, bool changePanel = true);

public slots:
	void connectSubCommand(ProcessX *p, bool showStdoutLocallyDefault);
private slots:
	void runInternalCommand(const QString &cmd, const QFileInfo &master, const QString &options);
	void commandLineRequested(const QString &cmdId, QString *result, bool *);
	void beginRunningCommand(const QString &commandMain, bool latex, bool pdf, bool async);
	void beginRunningSubCommand(ProcessX *p, const QString &commandMain, const QString &subCommand, const RunCommandFlags &flags);
	void endRunningSubCommand(ProcessX *p, const QString &commandMain, const QString &subCommand, const RunCommandFlags &flags);
	void endRunningCommand(const QString &commandMain, bool latex, bool pdf, bool async);


	bool runCommand(const QString &commandline, QString *buffer = 0, QTextCodec *codecForBuffer = 0);
protected slots:
	void processNotification(const QString &message);
	void openTerminal();
	void cleanAll();
	void checkShortcutChangeNotification(QAction *act);
	void commandFromAction();  //calls a command given by sender.data, doesn't wait

	void webPublish();
	void webPublishSource();
	void analyseText();
	void analyseTextFormDestroyed();
	void generateRandomText();

	bool loadLog();
	void setLogMarksVisible(bool visible);
	void clearLogEntriesInEditors();
	void updateLogEntriesInEditors();
	void showLog();
	void viewLog();
	void viewLogOrReRun(LatexCompileResult *result);
	bool gotoNearLogEntry(int lt, bool backward, QString notFoundMessage);
	bool hasLatexErrors();
	bool logExists();
	void clearMarkers();
	/////
	void latexHelp();
	void userManualHelp();
	void texdocHelp();
	void helpAbout();

	void generalOptions();
	void setAutomaticRootDetection();
	void setExplicitRootDocument(LatexDocument *doc);
	void setCurrentDocAsExplicitRoot();

	void gotoNextDocument();
	void gotoPrevDocument();
	void gotoOpenDocument();
	void updateOpenDocumentMenu(bool localChange = false);

	void focusEditor();
	void focusViewer();
	void enlargeEmbeddedPDFViewer();
	void shrinkEmbeddedPDFViewer(bool preserveConfig = false);

	void viewCloseSomething();
	void setFullScreenMode();
	void viewAlignWindows();
	void viewSetHighlighting(QAction *act);
	void showHighlightingMenu();

	void viewCollapseBlock();
	void viewExpandBlock();

	QObject *newPdfPreviewer(bool embedded = false);
	void pdfClosed();
	void restoreMacMenuBar();

	void masterDocumentChanged(LatexDocument *doc);
	void aboutToDeleteDocument(LatexDocument *doc);

	void setMostUsedSymbols(QTableWidgetItem *item);

	void updateCompleter(LatexEditorView *edView = 0);
	void completerNeedsUpdate();
	void needUpdatedCompleter();

	void outputPageChanged(const QString &id);

	void gotoLine(int line, int col = 0, LatexEditorView *edView = 0, QEditor::MoveFlags mflags = QEditor::Navigation, bool setFocus = true); // line is 0 based
	bool gotoLine(int line, const QString &fileName);  // line is 0 based, absolute file name
	void gotoLogEntryEditorOnly(int logEntryNumber);
	bool gotoLogEntryAt(int newLineNumber);
	bool gotoMark(bool backward, int id = -1);
	QList<int> findOccurencesApproximate(QString checkLine, const QString &guessedWord);
	void syncFromViewer(const QString &fileName, int lineNr, bool activate, const QString &guessedWord);  //0 based, absolute file name, raise window

	void goBack();
	void goForward();
	void setGlobalCursor(const QDocumentCursor &c);
	void saveCurrentCursorToHistory();
	void saveEditorCursorToHistory(LatexEditorView *edView);

	void previewLatex();
	void previewAvailable(const QString &imageFile, const PreviewSource &source);
	void clearPreview();
	void showPreview(const QString &text);
	void showPreview(const QDocumentCursor &c);
	void showPreview(const QDocumentCursor &c, bool addToList);
	void showPreviewQueue();
	void showImgPreview(const QString &fname);
	void showImgPreviewFinished(const QPixmap &pm, int page);

	void templateEdit(const QString &fname);

	void fileReloaded();
	void fileInConflict();
	void fileAutoReloading(QString fname);

	void jumpToSearchResult(QDocument *doc, int lineNumber, const SearchQuery *query);

	void cursorPositionChanged();
	void syncPDFViewer(QDocumentCursor cur, bool inForeground = true);
	//	void treeWidgetChanged();

	bool generateMirror(bool setCur = false);
	void generateBracketInverterMirror();
	void jumpToBracket();
	void selectBracket();
	void findMissingBracket();
	void cursorHovered();
	void loadProfile();
	void saveProfile();

	void addRowCB();
	void addColumnCB();
	void removeColumnCB();
	void removeRowCB();
	void cutColumnCB();
	void pasteColumnCB();
	void addHLineCB();
	void remHLineCB();
	void insertTableTemplate();
	void alignTableCols();

	void latexModelViewMode();
	void moveDocumentToFront();
	void moveDocumentToEnd();
	void moveDocumentToDest(int dest);

	void updateTexQNFA();
	void updateHighlighting();

	void toggleGrammar(int type);

	void openInternalDocViewer(QString package, const QString command = "");

private slots:
	void importPackage(QString name);
	void packageScanCompleted(QString name);
	void stopPackageParser();
	void packageParserFinished();
	void readinAllPackageNames();
	void packageListReadCompleted(QSet<QString> packages);
protected:
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
	virtual void changeEvent(QEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
#if (QT_VERSION > 0x050000) && (defined(Q_OS_MAC))
	bool eventFilter(QObject *obj, QEvent *event);
#endif

	Qt::MouseButtons mb;

	bool mCompleterNeedsUpdate;

	int currentLine;
	QBrush oldBackground;
	bool mDontScrollToItem;

	QByteArray stateFullScreen;

	QDialog *svndlg;
	UserMenuDialog *userMacroDialog;
	QComboBox *cmbLog;

	QStringList m_columnCutBuffer;

	QTimer autosaveTimer;

	bool completionBaseCommandsUpdated;

	QSet<int> previewQueue;
	LatexEditorView *previewQueueOwner;

	QString fileFilters;
	QString selectedFileFilter;

	QPointer<FileSelector> fileSelector;

	bool runBibliographyIfNecessaryEntered;

	LatexStyleParser *latexStyleParser;
	PackageScanner *packageListReader;

	QMap<QString, QString> detectedEnvironmentsForHighlighting;

	LatexDocument *diffLoadDocHidden(QString f);
	void showDiff3(const QString file1, const QString file2);

	void restoreBookmarks(LatexEditorView *edView);

	bool completerPreview;

	bool recheckLabels;

	LatexEditorView *editorViewForLabel(LatexDocument *doc, const QString &label);

	QSet<QString> latexPackageList, currentPackageList;

	findGlobalDialog *findDlg;

	QMap<QString, QString> *mReplacementList;

	StructureEntry *contextEntry;
	QModelIndex contextIndex;

public:
	Q_PROPERTY(QString clipboard READ clipboardText WRITE setClipboardText);
	Q_INVOKABLE QString clipboardText(const QClipboard::Mode &mode = QClipboard::Clipboard) const;
	Q_INVOKABLE void setClipboardText(const QString &text, const QClipboard::Mode &mode = QClipboard::Clipboard);
	Q_INVOKABLE int getVersion() const;
	Q_INVOKABLE void simulateKeyPress(const QString &shortcut);

	static void recoverFromCrash();

public slots:
	void threadCrashed();
	void iamalive();
	void checkLatexInstall();
	void checkCWLs();

	void slowOperationStarted();
	void slowOperationEnded();

signals:
	void infoNewFile();
	void infoNewFromTemplate();
	void infoLoadFile(const QString &filename);
	void infoFileSaved(const QString &filename);
	void infoFileClosed();
	void infoAfterTypeset();
	void imgPreview(const QString &fn);
};

Q_DECLARE_METATYPE(Texstudio *)

#endif

