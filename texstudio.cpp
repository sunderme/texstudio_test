/***************************************************************************
 *   copyright       : (C) 2003-2008 by Pascal Brachet                     *
 *   addons by Luis Silvestre                                              *
 *   http://www.xm1math.net/texmaker/                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation  either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
//#include <stdlib.h>
//#include "/usr/include/valgrind/callgrind.h"

#include "texstudio.h"
#include "latexeditorview.h"

#include "smallUsefulFunctions.h"

#include "cleandialog.h"

#include "debughelper.h"

#include "dblclickmenubar.h"
#include "filechooser.h"
#include "tabdialog.h"
#include "arraydialog.h"
#include "bibtexdialog.h"
#include "tabbingdialog.h"
#include "letterdialog.h"
#include "quickdocumentdialog.h"
#include "quickbeamerdialog.h"
#include "mathassistant.h"
#include "maketemplatedialog.h"
#include "templateselector.h"
#include "templatemanager.h"
#include "usermenudialog.h"
#include "usertooldialog.h"
#include "aboutdialog.h"
#include "encodingdialog.h"
#include "encoding.h"
#include "randomtextgenerator.h"
#include "webpublishdialog.h"
#include "thesaurusdialog.h"
#include "qsearchreplacepanel.h"
#include "latexcompleter_config.h"
#include "universalinputdialog.h"
#include "insertgraphics.h"
#include "latexeditorview_config.h"
#include "scriptengine.h"
#include "grammarcheck.h"
#include "qmetautils.h"
#include "updatechecker.h"
#include "session.h"
#include "help.h"
#include "searchquery.h"
#include "fileselector.h"
#include "utilsUI.h"
#include "utilsSystem.h"
#include "minisplitter.h"
#include "latexparser/latextokens.h"
#include "latexparser/latexparser.h"
#include "latexstructure.h"


#ifndef QT_NO_DEBUG
#include "tests/testmanager.h"
#endif

#include "qdocument.h"
#include "qdocumentcursor.h"
#include "qdocumentline.h"
#include "qdocumentline_p.h"

#include "qnfadefinition.h"

/*! \file texstudio.cpp
 * contains the GUI definition as well as some helper functions
 */

/*!
    \defgroup txs Mainwindow
	\ingroup txs
	@{
*/

/*! \class Texstudio
 * This class sets up the GUI and handles the GUI interaction (menus and toolbar).
 * It uses QEditor with LatexDocument as actual text editor and PDFDocument for viewing pdf.
 *
 * \see QEditor
 * \see LatexDocument
 * \see PDFDocument
 */



const QString APPICON(":appicon");

bool programStopped = false;
Texstudio *txsInstance = 0;
QCache<QString, QIcon> iconCache;

// workaround needed on OSX due to https://bugreports.qt.io/browse/QTBUG-49576
void hideSplash()
{
#ifdef Q_OS_MAC
	if (txsInstance)
		txsInstance->hideSplash();
#endif
}
/*!
 * \brief constructor
 *
 * set-up GUI
 *
 * \param parent
 * \param flags
 * \param splash
 */
Texstudio::Texstudio(QWidget *parent, Qt::WindowFlags flags, QSplashScreen *splash)
	: QMainWindow(parent, flags), textAnalysisDlg(0), spellDlg(0), mDontScrollToItem(false), runBibliographyIfNecessaryEntered(false)
{

	splashscreen = splash;
	programStopped = false;
	spellLanguageActions = 0;
	MapForSymbols = 0;
	currentLine = -1;
	svndlg = 0;
	userMacroDialog = 0;
	mCompleterNeedsUpdate = false;
	latexStyleParser = 0;
	packageListReader = 0;
	bibtexEntryActions = 0;
	biblatexEntryActions = 0;
	bibTypeActions = 0;
	highlightLanguageActions = 0;
	runningPDFCommands = runningPDFAsyncCommands = 0;
	completerPreview = false;
	recheckLabels = true;
	findDlg = 0;
	cursorHistory = 0;
	recentSessionList = 0;
	editors = 0;
	contextEntry = 0;
	m_languages = 0; //initial state to avoid crash on OSX

	connect(&buildManager, SIGNAL(hideSplash()), this, SLOT(hideSplash()));

	readSettings();

#if (QT_VERSION > 0x050000) && (QT_VERSION <= 0x050700) && (defined(Q_OS_MAC))
	QCoreApplication::instance()->installEventFilter(this);
#endif

	latexReference = new LatexReference();
	latexReference->setFile(findResourceFile("latex2e.html"));

	qRegisterMetaType<QSet<QString> >();

	txsInstance = this;
	static int crashHandlerType = 1;
	configManager.registerOption("Crash Handler Type", &crashHandlerType, 1);

	initCrashHandler(crashHandlerType);

	QTimer *t  = new QTimer(this);
	connect(t, SIGNAL(timeout()), SLOT(iamalive()));
	t->start(9500);

	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	QFile styleSheetFile(configManager.configBaseDir + "stylesheet.qss");
	if (styleSheetFile.exists()) {
		styleSheetFile.open(QFile::ReadOnly);
		setStyleSheet(styleSheetFile.readAll());
		styleSheetFile.close();
	}

	setWindowIcon(QIcon(":/images/logo128.png"));

	int iconSize = qMax(16, configManager.guiToolbarIconSize);
	setIconSize(QSize(iconSize, iconSize));

	leftPanel = 0;
	sidePanel = 0;
	structureTreeView = 0;
	outputView = 0;

	qRegisterMetaType<LatexParser>();
	latexParser.importCwlAliases(findResourceFile("completion/cwlAliases.dat"));

	m_formatsOldDefault = QDocument::defaultFormatScheme();
	QDocument::setDefaultFormatScheme(m_formats);
	SpellerUtility::spellcheckErrorFormat = m_formats->id("spellingMistake");

	qRegisterMetaType<QList<LineInfo> >();
	qRegisterMetaType<QList<GrammarError> >();
	qRegisterMetaType<LatexParser>();
	qRegisterMetaType<GrammarCheckerConfig>();

	grammarCheck = new GrammarCheck();
	grammarCheck->moveToThread(&grammarCheckThread);
	GrammarCheck::staticMetaObject.invokeMethod(grammarCheck, "init", Qt::QueuedConnection, Q_ARG(LatexParser, latexParser), Q_ARG(GrammarCheckerConfig, *configManager.grammarCheckerConfig));
	connect(grammarCheck, SIGNAL(checked(const void *, const void *, int, QList<GrammarError>)), &documents, SLOT(lineGrammarChecked(const void *, const void *, int, QList<GrammarError>)));
	if (configManager.autoLoadChildren)
		connect(&documents, SIGNAL(docToLoad(QString)), this, SLOT(addDocToLoad(QString)));
	connect(&documents, SIGNAL(updateQNFA()), this, SLOT(updateTexQNFA()));

	grammarCheckThread.start();

	if (configManager.autoDetectEncodingFromLatex || configManager.autoDetectEncodingFromChars) QDocument::setDefaultCodec(0);
	else QDocument::setDefaultCodec(configManager.newFileEncoding);
	if (configManager.autoDetectEncodingFromLatex)
		QDocument::addGuessEncodingCallback(&Encoding::guessEncoding); // encodingcallbacks before restoer session !!!
	if (configManager.autoDetectEncodingFromChars)
		QDocument::addGuessEncodingCallback(&ConfigManager::getDefaultEncoding);

	QString qxsPath = QFileInfo(findResourceFile("qxs/tex.qnfa")).path();
	m_languages = new QLanguageFactory(m_formats, this);
	m_languages->addDefinitionPath(qxsPath);
	m_languages->addDefinitionPath(configManager.configBaseDir + "languages");  // definitions here overwrite previous ones

	// custom evironments & structure commands
	updateTexQNFA();

	QLineMarksInfoCenter::instance()->loadMarkTypes(qxsPath + "/marks.qxm");
	QList<QLineMarkType> &marks = QLineMarksInfoCenter::instance()->markTypes();
	for (int i = 0; i < marks.size(); i++)
		if (m_formats->format("line:" + marks[i].id).background.isValid())
			marks[i].color = m_formats->format("line:" + marks[i].id).background;
		else
			marks[i].color = Qt::transparent;

	LatexEditorView::updateFormatSettings();

	// TAB WIDGET EDITEUR
	documents.indentationInStructure = configManager.indentationInStructure;
	documents.showCommentedElementsInStructure = configManager.showCommentedElementsInStructure;
	documents.indentIncludesInStructure = configManager.indentIncludesInStructure;
	documents.markStructureElementsBeyondEnd = configManager.markStructureElementsBeyondEnd;
	documents.markStructureElementsInAppendix = configManager.markStructureElementsInAppendix;
	documents.showLineNumbersInStructure = configManager.showLineNumbersInStructure;
	connect(&documents, SIGNAL(masterDocumentChanged(LatexDocument *)), SLOT(masterDocumentChanged(LatexDocument *)));
	connect(&documents, SIGNAL(aboutToDeleteDocument(LatexDocument *)), SLOT(aboutToDeleteDocument(LatexDocument *)));

	centralFrame = new QFrame(this);
	centralFrame->setLineWidth(0);
	centralFrame->setFrameShape(QFrame::NoFrame);
	centralFrame->setFrameShadow(QFrame::Plain);

	//edit
	centralToolBar = new QToolBar(tr("Central"), this);
	centralToolBar->setFloatable(false);
	centralToolBar->setOrientation(Qt::Vertical);
	centralToolBar->setMovable(false);
	iconSize = qMax(16, configManager.guiSecondaryToolbarIconSize);
	centralToolBar->setIconSize(QSize(iconSize, iconSize));

	editors = new Editors(centralFrame);
	editors->setFocus();

	TxsTabWidget *leftEditors = new TxsTabWidget(this);
	leftEditors->setActive(true);
	editors->addTabWidget(leftEditors);
	TxsTabWidget *rightEditors = new TxsTabWidget(this);
	editors->addTabWidget(rightEditors);

	connect(&documents, SIGNAL(docToHide(LatexEditorView *)), editors, SLOT(removeEditor(LatexEditorView *)));
	connect(editors, SIGNAL(currentEditorChanged()), SLOT(currentEditorChanged()));
	connect(editors, SIGNAL(listOfEditorsChanged()), SLOT(updateOpenDocumentMenu()));
	connect(editors, SIGNAL(editorsReordered()), SLOT(onEditorsReordered()));
	connect(editors, SIGNAL(closeCurrentEditorRequested()), this, SLOT(fileClose()));
	connect(editors, SIGNAL(editorAboutToChangeByTabClick(LatexEditorView *, LatexEditorView *)), this, SLOT(editorAboutToChangeByTabClick(LatexEditorView *, LatexEditorView *)));

	cursorHistory = new CursorHistory(&documents);
	bookmarks = new Bookmarks(&documents, this);

	QLayout *centralLayout = new QHBoxLayout(centralFrame);
	centralLayout->setSpacing(0);
	centralLayout->setMargin(0);
	centralLayout->addWidget(centralToolBar);
	centralLayout->addWidget(editors);

	centralVSplitter = new MiniSplitter(Qt::Vertical, this);
	centralVSplitter->setChildrenCollapsible(false);
	centralVSplitter->addWidget(centralFrame);
	centralVSplitter->setStretchFactor(0, 1);  // all stretch goes to the editor (0th widget)

	sidePanelSplitter = new MiniSplitter(Qt::Horizontal, this);
	sidePanelSplitter->addWidget(centralVSplitter);

	mainHSplitter = new MiniSplitter(Qt::Horizontal, this);
	mainHSplitter->addWidget(sidePanelSplitter);
	mainHSplitter->setChildrenCollapsible(false);
	setCentralWidget(mainHSplitter);

	setContextMenuPolicy(Qt::ActionsContextMenu);

	symbolMostused.clear();
	setupDockWidgets();

	setMenuBar(new DblClickMenuBar());
	setupMenus();
	TitledPanelPage *logPage = outputView->pageFromId(outputView->LOG_PAGE);
	if (logPage) {
		logPage->addToolbarAction(getManagedAction("main/tools/logmarkers"));
		logPage->addToolbarAction(getManagedAction("main/edit2/goto/errorprev"));
		logPage->addToolbarAction(getManagedAction("main/edit2/goto/errornext"));
	}
	setupToolBars();
	connect(&configManager, SIGNAL(watchedMenuChanged(QString)), SLOT(updateToolBarMenu(QString)));

	restoreState(windowstate, 0);
	//workaround as toolbar central seems not be be handled by windowstate
	centralToolBar->setVisible(configManager.centralVisible);
	if (tobemaximized) showMaximized();
	if (tobefullscreen) {
		showFullScreen();
		restoreState(stateFullScreen, 1);
		fullscreenModeAction->setChecked(true);
	}

	createStatusBar();
	completer = 0;
	updateCaption();
	updateMasterDocumentCaption();
	statusLabelProcess->setText(QString(" %1 ").arg(tr("Ready")));

	show();
	if (splash)
		splash->raise();

	setAcceptDrops(true);
	//installEventFilter(this);

	UpdateChecker::instance()->autoCheck();

	completer = new LatexCompleter(latexParser, this);
	completer->setConfig(configManager.completerConfig);
	completer->setPackageList(&latexPackageList);
	connect(completer, SIGNAL(showImagePreview(QString)), this, SLOT(showImgPreview(QString)));
	connect(completer, SIGNAL(showPreview(QString)), this, SLOT(showPreview(QString)));
	connect(this, SIGNAL(imgPreview(QString)), completer, SLOT(bibtexSectionFound(QString)));
	//updateCompleter();
	LatexEditorView::setCompleter(completer);
	completer->setLatexReference(latexReference);
	completer->updateAbbreviations();

	TemplateManager::setConfigBaseDir(configManager.configBaseDir);
	TemplateManager::ensureUserTemplateDirExists();
	TemplateManager::checkForOldUserTemplates();

	/* The encoding detection works as follow:
		If QDocument detects the file is UTF16LE/BE, use that encoding
		Else If QDocument detects UTF-8 {
	If LatexParser::guessEncoding finds an encoding, use that
	Else use UTF-8
	} Else {
	  If LatexParser::guessEncoding finds an encoding use that
	  Else if QDocument detects ascii (only 7bit characters) {
	if default encoding == utf16: use utf-8 as fallback (because utf16 can be reliable detected and the user seems to like unicode)
	else use default encoding
	  }
	  Else {
	if default encoding == utf16/8: use latin1 (because the file contains invalid unicode characters )
	else use default encoding
	  }
	}

	*/

	QStringList filters;
	filters << tr("TeX files") + " (*.tex *.bib *.sty *.cls *.mp *.dtx *.cfg *.ins *.ltx *.tikz *.pdf_tex)";
	filters << tr("LilyPond files") + " (*.lytex)";
	filters << tr("Plaintext files") + " (*.txt)";
	filters << tr("Pweave files") + " (*.Pnw)";
	filters << tr("Sweave files") + " (*.Snw *.Rnw)";
	filters << tr("Asymptote files") + " (*.asy)";
	filters << tr("PDF files") + " (*.pdf)";
	filters << tr("All files") + " (*)";
	fileFilters = filters.join(";;");
	if (!configManager.rememberFileFilter)
		selectedFileFilter = filters.first();


	//setup autosave timer
	connect(&autosaveTimer, SIGNAL(timeout()), this, SLOT(fileSaveAll()));
	if (configManager.autosaveEveryMinutes > 0) {
		autosaveTimer.start(configManager.autosaveEveryMinutes * 1000 * 60);
	}

	connect(this, SIGNAL(infoFileSaved(QString, int)), this, SLOT(checkinAfterSave(QString, int)));

	//script things
	setProperty("applicationName", TEXSTUDIO);
	QTimer::singleShot(500, this, SLOT(autoRunScripts()));
	connectWithAdditionalArguments(this, SIGNAL(infoNewFile()), this, "runScripts", QList<QVariant>() << Macro::ST_NEW_FILE);
	connectWithAdditionalArguments(this, SIGNAL(infoNewFromTemplate()), this, "runScripts", QList<QVariant>() << Macro::ST_NEW_FROM_TEMPLATE);
	connectWithAdditionalArguments(this, SIGNAL(infoLoadFile(QString)), this, "runScripts", QList<QVariant>() << Macro::ST_LOAD_FILE);
	connectWithAdditionalArguments(this, SIGNAL(infoFileSaved(QString)), this, "runScripts", QList<QVariant>() << Macro::ST_FILE_SAVED);
	connectWithAdditionalArguments(this, SIGNAL(infoFileClosed()), this, "runScripts", QList<QVariant>() << Macro::ST_FILE_CLOSED);
	connectWithAdditionalArguments(&documents, SIGNAL(masterDocumentChanged(LatexDocument *)), this, "runScripts", QList<QVariant>() << Macro::ST_MASTER_CHANGED);
	connectWithAdditionalArguments(this, SIGNAL(infoAfterTypeset()), this, "runScripts", QList<QVariant>() << Macro::ST_AFTER_TYPESET);
	connectWithAdditionalArguments(&buildManager, SIGNAL(endRunningCommands(QString, bool, bool, bool)), this, "runScripts", QList<QVariant>() << Macro::ST_AFTER_COMMAND_RUN);

	if (configManager.sessionRestore && !ConfigManager::dontRestoreSession) {
		fileRestoreSession(false, false);
	}
	splashscreen = 0;
}
/*!
 * \brief destructor
 */
Texstudio::~Texstudio()
{

	iconCache.clear();
	QDocument::setDefaultFormatScheme(m_formatsOldDefault); //prevents crash when deleted latexeditorview accesses the default format scheme, as m_format is going to be deleted

	programStopped = true;

	Guardian::shutdown();

	delete MapForSymbols;
	delete findDlg;

	if (latexStyleParser) latexStyleParser->stop();
	if (packageListReader) packageListReader->stop();
	GrammarCheck::staticMetaObject.invokeMethod(grammarCheck, "shutdown", Qt::BlockingQueuedConnection);
	grammarCheckThread.quit();

	if (latexStyleParser) latexStyleParser->wait();
	if (packageListReader) packageListReader->wait();
	grammarCheckThread.wait(5000); //TODO: timeout causes sigsegv, is there any better solution?
}

/*!
 * \brief code to be executed at end of start-up
 *
 * Check for Latex installation.
 * Read in all package names for usepackage completion.
 */
void Texstudio::startupCompleted()
{
	if (configManager.checkLatexConfiguration) {
		bool noWarnAgain = false;
		buildManager.checkLatexConfiguration(noWarnAgain);
		configManager.checkLatexConfiguration = !noWarnAgain;
	}
	// package reading (at least with Miktex) apparently slows down the startup
	// the first rendering of lines in QDocumentPrivate::draw() gets very slow
	// therefore we defer it until the main window is completely loaded
	readinAllPackageNames(); // asynchrnous read in of all available sty/cls
}

QAction *Texstudio::newManagedAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QKeySequence &shortCut, const QString &iconFile, const QList<QVariant> &args)
{
	QAction *tmp = configManager.newManagedAction(menu, id, text, args.isEmpty() ? slotName : SLOT(relayToOwnSlot()), QList<QKeySequence>() << shortCut, iconFile);
	if (!args.isEmpty()) {
		QString slot = QString(slotName).left(QString(slotName).indexOf("("));
		Q_ASSERT(staticMetaObject.indexOfSlot(createMethodSignature(qPrintable(slot), args)) != -1);
		tmp->setProperty("slot", qPrintable(slot));
		tmp->setProperty("args", QVariant::fromValue<QList<QVariant> >(args));
	}
	return tmp;
}

QAction *Texstudio::newManagedAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QList<QKeySequence> &shortCuts, const QString &iconFile, const QList<QVariant> &args)
{
	QAction *tmp = configManager.newManagedAction(menu, id, text, args.isEmpty() ? slotName : SLOT(relayToOwnSlot()), shortCuts, iconFile);
	if (!args.isEmpty()) {
		QString slot = QString(slotName).left(QString(slotName).indexOf("("));
		Q_ASSERT(staticMetaObject.indexOfSlot(createMethodSignature(qPrintable(slot), args)) != -1);
		tmp->setProperty("slot", qPrintable(slot));
		tmp->setProperty("args", QVariant::fromValue<QList<QVariant> >(args));
	}
	return tmp;
}

QAction *Texstudio::newManagedEditorAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QKeySequence &shortCut, const QString &iconFile, const QList<QVariant> &args)
{
	QAction *tmp = configManager.newManagedAction(menu, id, text, 0, QList<QKeySequence>() << shortCut, iconFile);
	linkToEditorSlot(tmp, slotName, args);
	return tmp;
}

QAction *Texstudio::newManagedEditorAction(QWidget *menu, const QString &id, const QString &text, const char *slotName, const QList<QKeySequence> &shortCuts, const QString &iconFile, const QList<QVariant> &args)
{
	QAction *tmp = configManager.newManagedAction(menu, id, text, 0, shortCuts, iconFile);
	linkToEditorSlot(tmp, slotName, args);
	return tmp;
}

QAction *Texstudio::insertManagedAction(QAction *before, const QString &id, const QString &text, const char *slotName, const QKeySequence &shortCut, const QString &iconFile)
{
	QMenu *menu = before->menu();
	REQUIRE_RET(menu, 0);
	QAction *inserted = newManagedAction(menu, id, text, slotName, shortCut, iconFile);
	menu->removeAction(inserted);
	menu->insertAction(before, inserted);
	return inserted;
}

SymbolGridWidget *Texstudio::addSymbolGrid(const QString &SymbolList,  const QString &iconName, const QString &text)
{
	SymbolGridWidget *list = qobject_cast<SymbolGridWidget *>(leftPanel->widget(SymbolList));
	if (!list) {
		list = new SymbolGridWidget(this, SymbolList, MapForSymbols);
		list->setSymbolSize(configManager.guiSymbolGridIconSize);
		list->setProperty("isSymbolGrid", true);
		connect(list, SIGNAL(itemClicked(QTableWidgetItem *)), this, SLOT(insertSymbol(QTableWidgetItem *)));
		connect(list, SIGNAL(itemPressed(QTableWidgetItem *)), this, SLOT(insertSymbolPressed(QTableWidgetItem *)));
		leftPanel->addWidget(list, SymbolList, text, getRealIconFile(iconName));
	} else {
		leftPanel->setWidgetText(list, text);
		leftPanel->setWidgetIcon(list, getRealIconFile(iconName));
	}
	return list;
}
/*!
 * \brief add TagList to side panel
 *
 * add Taglist to side panel.
 *
 * \param id
 * \param iconName icon used for selecting taglist
 * \param text name of taglist
 * \param tagFile file to be read as tag list
 */
void Texstudio::addTagList(const QString &id, const QString &iconName, const QString &text, const QString &tagFile)
{
	XmlTagsListWidget *list = qobject_cast<XmlTagsListWidget *>(leftPanel->widget(id));
	if (!list) {
		list = new XmlTagsListWidget(this, ":/tags/" + tagFile);
		list->setObjectName("tags/" + tagFile.left(tagFile.indexOf("_tags.xml")));
		connect(list, SIGNAL(itemClicked(QListWidgetItem *)), this, SLOT(insertXmlTag(QListWidgetItem *)));
		leftPanel->addWidget(list, id, text, iconName);
		//(*list)->setProperty("mType",2);
	} else leftPanel->setWidgetText(list, text);
}
/*! set-up side- and bottom-panel
 */
void Texstudio::setupDockWidgets()
{
	//to allow retranslate this function must be able to be called multiple times

	if (!sidePanel) {
		sidePanel = new SidePanel(this);
		sidePanel->toggleViewAction()->setIcon(getRealIcon("sidebar"));
		sidePanel->toggleViewAction()->setText(tr("Side Panel"));
		sidePanel->toggleViewAction()->setChecked(configManager.getOption("GUI/sidePanel/visible", true).toBool());
		addAction(sidePanel->toggleViewAction());

		sidePanelSplitter->insertWidget(0, sidePanel);
		sidePanelSplitter->setStretchFactor(0, 0);  // panel does not get rescaled
		sidePanelSplitter->setStretchFactor(1, 1);
	}

	//Structure panel
	if (!leftPanel) {
		leftPanel = new CustomWidgetList(this);
		leftPanel->setObjectName("leftPanel");
		TitledPanelPage *page = new TitledPanelPage(leftPanel, "leftPanel", "TODO");
		sidePanel->appendPage(page);
		if (hiddenLeftPanelWidgets != "") {
			leftPanel->setHiddenWidgets(hiddenLeftPanelWidgets);
			hiddenLeftPanelWidgets = ""; //not needed anymore after the first call
		}
		connect(leftPanel, SIGNAL(widgetContextMenuRequested(QWidget *, QPoint)), this, SLOT(symbolGridContextMenu(QWidget *, QPoint)));
		connect(leftPanel, SIGNAL(titleChanged(QString)), page, SLOT(setTitle(QString)));
	}

	if (!structureTreeView) {
		structureTreeView = new QTreeView(this);
		structureTreeView->header()->hide();
		if (configManager.indentationInStructure > 0)
			structureTreeView->setIndentation(configManager.indentationInStructure);
		structureTreeView->setModel(documents.model);
		//disabled because it also reacts to expand, connect(structureTreeView, SIGNAL(activated(const QModelIndex &)), SLOT(clickedOnStructureEntry(const QModelIndex &))); //enter or double click (+single click on some platforms)
		connect(structureTreeView, SIGNAL(pressed(const QModelIndex &)), SLOT(clickedOnStructureEntry(const QModelIndex &))); //single click
		//		connect(structureTreeView, SIGNAL(expanded(const QModelIndex &)), SLOT(treeWidgetChanged()));
		//		connect(structureTreeView, SIGNAL(collapsed(const QModelIndex &)), SLOT(treeWidgetChanged()));
		//-- connect( StructureTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *,int )), SLOT(DoubleClickedOnStructure(QTreeWidgetItem *,int))); // qt4 bugs - don't use it ?? also true for view??

		leftPanel->addWidget(structureTreeView, "structureTreeView", tr("Structure"), getRealIconFile("structure"));
	} else leftPanel->setWidgetText(structureTreeView, tr("Structure"));
	if (!leftPanel->widget("bookmarks")) {
		QListWidget *bookmarksWidget = bookmarks->widget();
		connect(bookmarks, SIGNAL(loadFileRequest(QString)), this, SLOT(load(QString)));
		connect(bookmarks, SIGNAL(gotoLineRequest(int, int, LatexEditorView *)), this, SLOT(gotoLine(int, int, LatexEditorView *)));
		leftPanel->addWidget(bookmarksWidget, "bookmarks", tr("Bookmarks"), getRealIconFile("bookmarks"));
	} else leftPanel->setWidgetText("bookmarks", tr("Bookmarks"));

	int cnt = modernStyle ? 11 : 1;
	addSymbolGrid("operators", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Operator symbols"));
	addSymbolGrid("relation", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Relation symbols"));
	addSymbolGrid("arrows", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Arrow symbols"));
	addSymbolGrid("delimiters", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Delimiters"));
	addSymbolGrid("greek", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Greek letters"));
	addSymbolGrid("cyrillic", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Cyrillic letters"));
	addSymbolGrid("misc-math", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Miscellaneous math symbols"));
	addSymbolGrid("misc-text", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Miscellaneous text symbols"));
	addSymbolGrid("wasysym", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Miscellaneous text symbols (wasysym)"));
	addSymbolGrid("special", QString(":/symbols-ng/icons/img0%1icons.png").arg(cnt++, 2, 10, QLatin1Char('0')), tr("Accented letters"));

	MostUsedSymbolWidget = addSymbolGrid("!mostused", getRealIconFile("math6"), tr("Most used symbols"));
	MostUsedSymbolWidget->loadSymbols(MapForSymbols->keys(), MapForSymbols);
	FavoriteSymbolWidget = addSymbolGrid("!favorite", getRealIconFile("math7"), tr("Favorites"));
	FavoriteSymbolWidget->loadSymbols(symbolFavorites);

	addTagList("brackets", getRealIconFile("leftright"), tr("Left/Right Brackets"), "brackets_tags.xml");
	addTagList("pstricks", getRealIconFile("pstricks"), tr("Pstricks Commands"), "pstricks_tags.xml");
	addTagList("metapost", getRealIconFile("metapost"), tr("MetaPost Commands"), "metapost_tags.xml");
	addTagList("tikz", getRealIconFile("tikz"), tr("Tikz Commands"), "tikz_tags.xml");
	addTagList("asymptote", getRealIconFile("asymptote"), tr("Asymptote Commands"), "asymptote_tags.xml");

	leftPanel->showWidgets();

	// update MostOftenUsed
	mostUsedSymbolsTriggered(true);
	// OUTPUT WIDGETS
	if (!outputView) {
		outputView = new OutputViewWidget(this);
		outputView->setObjectName("OutputView");
		centralVSplitter->addWidget(outputView);
		outputView->toggleViewAction()->setChecked(configManager.getOption("GUI/outputView/visible", true).toBool());
		centralVSplitter->setStretchFactor(1, 0);
		centralVSplitter->restoreState(configManager.getOption("centralVSplitterState").toByteArray());

		connect(outputView->getLogWidget(), SIGNAL(logEntryActivated(int)), this, SLOT(gotoLogEntryEditorOnly(int)));
		connect(outputView->getLogWidget(), SIGNAL(logLoaded()), this, SLOT(updateLogEntriesInEditors()));
		connect(outputView->getLogWidget(), SIGNAL(logResetted()), this, SLOT(clearLogEntriesInEditors()));
		connect(outputView, SIGNAL(pageChanged(QString)), this, SLOT(outputPageChanged(QString)));
		connect(outputView->getSearchResultWidget(), SIGNAL(jumpToSearchResult(QDocument *, int, const SearchQuery *)), this, SLOT(jumpToSearchResult(QDocument *, int, const SearchQuery *)));
		connect(outputView->getSearchResultWidget(), SIGNAL(runSearch(SearchQuery *)), this, SLOT(runSearch(SearchQuery *)));

		connect(&buildManager, SIGNAL(previewAvailable(const QString &, const PreviewSource &)), this, SLOT(previewAvailable(const QString &, const PreviewSource &)));
		connect(&buildManager, SIGNAL(processNotification(QString)), SLOT(processNotification(QString)));
        connect(&buildManager, SIGNAL(clearLogs()), SLOT(clearLogs()));

		connect(&buildManager, SIGNAL(beginRunningCommands(QString, bool, bool, bool)), SLOT(beginRunningCommand(QString, bool, bool, bool)));
		connect(&buildManager, SIGNAL(beginRunningSubCommand(ProcessX *, QString, QString, RunCommandFlags)), SLOT(beginRunningSubCommand(ProcessX *, QString, QString, RunCommandFlags)));
		connect(&buildManager, SIGNAL(endRunningSubCommand(ProcessX *, QString, QString, RunCommandFlags)), SLOT(endRunningSubCommand(ProcessX *, QString, QString, RunCommandFlags)));
		connect(&buildManager, SIGNAL(endRunningCommands(QString, bool, bool, bool)), SLOT(endRunningCommand(QString, bool, bool, bool)));
		connect(&buildManager, SIGNAL(latexCompiled(LatexCompileResult *)), SLOT(viewLogOrReRun(LatexCompileResult *)));
		connect(&buildManager, SIGNAL(runInternalCommand(QString, QFileInfo, QString)), SLOT(runInternalCommand(QString, QFileInfo, QString)));
		connect(&buildManager, SIGNAL(commandLineRequested(QString, QString *, bool *)), SLOT(commandLineRequested(QString, QString *, bool *)));

		addAction(outputView->toggleViewAction());
		QAction *temp = new QAction(this);
		temp->setSeparator(true);
		addAction(temp);
	}
	sidePanelSplitter->restoreState(configManager.getOption("GUI/sidePanelSplitter/state").toByteArray());
}

void Texstudio::updateToolBarMenu(const QString &menuName)
{
	QMenu *menu = configManager.getManagedMenu(menuName);
	if (!menu) return;
	LatexEditorView *edView = currentEditorView();
	foreach (const ManagedToolBar &tb, configManager.managedToolBars)
		if (tb.toolbar && tb.actualActions.contains(menuName))
			foreach (QObject *w, tb.toolbar->children())
				if (w->property("menuID").toString() == menuName) {
					QToolButton *combo = qobject_cast<QToolButton *>(w);
					REQUIRE(combo);

					QFontMetrics fontMetrics(tb.toolbar->font());
					QStringList actionTexts;
					QList<QIcon> actionIcons;
					int defaultIndex = -1;
					foreach (const QAction *act, menu->actions())
						if (!act->isSeparator()) {
							actionTexts.append(act->text());
							actionIcons.append(act->icon());
							if (menuName == "main/view/documents" && edView == act->data().value<LatexEditorView *>()) {
								defaultIndex = actionTexts.length() - 1;
							}
						}

					//qDebug() << "**" << actionTexts;
					createComboToolButton(tb.toolbar, actionTexts, actionIcons, -1, this, SLOT(callToolButtonAction()), defaultIndex, combo);

					if (menuName == "main/view/documents") {
						// workaround to select the current document
						// combobox uses separate actions. So we have to get the current action from the menu (by comparing its data()
						// attribute to the currentEditorView(). Then map it to a combobox action using the index.
						// TODO: should this menu be provided by Editors?
						LatexEditorView *edView = currentEditorView();
						foreach (QAction* act, menu->actions()) {
							qDebug() << act->data().value<LatexEditorView *>() << combo;
							if (edView == act->data().value<LatexEditorView *>()) {
								int i = menu->actions().indexOf(act);
								qDebug() << i << combo->menu()->actions().length();
								if (i < 0 || i>= combo->menu()->actions().length()) continue;
								foreach (QAction *act, menu->actions()) {
									qDebug() << "menu" << act->text();
								}
								foreach (QAction *act, combo->menu()->actions()) {
									qDebug() << "cmb" << act->text();
								}

								combo->setDefaultAction(combo->menu()->actions()[i]);
							}
						}
					}
				}
}

// we different native shortcuts on OSX and Win/Linux
// note: in particular many key combination with arrows are reserved for text navigation in OSX
// and we already set them in QEditor. Don't overwrite them here.
#ifdef Q_OS_MAC
#define MAC_OTHER(shortcutMac, shortcutOther) shortcutMac
#else
#define MAC_OTHER(shortcutMac, shortcutOther) shortcutOther
#endif
/*! \brief set-up all menus in the menu-bar
 *
 * This function is called whenever the menu changes (= start and retranslation)
 * This means if you call it repeatedly with the same language setting it should not change anything
 * Currently this is not true, because it adds additional separator, which are invisible
 * creates new action groups and new context menu, although all invisible, they are a memory leak
 * But not a bad one, because no one is expected to change the language multiple times
 */
void Texstudio::setupMenus()
{
	//This function is called whenever the menu changes (= start and retranslation)
	//This means if you call it repeatedly with the same language setting it should not change anything
	//Currently this is not true, because it adds additional separator, which are invisible
	//creates new action groups and new context menu, although all invisible, they are a memory leak
	//But not a bad one, because no one is expected to change the language multiple times
	//TODO: correct somewhen

	configManager.menuParent = this;
    if(configManager.menuParents.isEmpty())
        configManager.menuParents.append(this);
	configManager.menuParentsBar = menuBar();

	//file
	QMenu *menu = newManagedMenu("main/file", tr("&File"));
    //getManagedMenu("main/file");
	newManagedAction(menu, "new", tr("&New"), SLOT(fileNew()), QKeySequence::New, "document-new");
	newManagedAction(menu, "newfromtemplate", tr("New From &Template..."), SLOT(fileNewFromTemplate()));
	newManagedAction(menu, "open", tr("&Open..."), SLOT(fileOpen()), QKeySequence::Open, "document-open");

	QMenu *submenu = newManagedMenu(menu, "openrecent", tr("Open &Recent")); //only create the menu here, actions are created by config manager

	submenu = newManagedMenu(menu, "session", tr("Session"));
	newManagedAction(submenu, "loadsession", tr("Load Session..."), SLOT(fileLoadSession()));
	newManagedAction(submenu, "savesession", tr("Save Session..."), SLOT(fileSaveSession()));
	newManagedAction(submenu, "restoresession", tr("Restore Previous Session"), SLOT(fileRestoreSession()));
	submenu->addSeparator();
	if (!recentSessionList) {
		recentSessionList = new SessionList(submenu, this);
		connect(recentSessionList, SIGNAL(loadSessionRequest(QString)), this, SLOT(loadSession(QString)));
	}
	recentSessionList->updateMostRecentMenu();

	menu->addSeparator();
	actSave = newManagedAction(menu, "save", tr("&Save"), SLOT(fileSave()), QKeySequence::Save, "document-save");
	newManagedAction(menu, "saveas", tr("Save &As..."), SLOT(fileSaveAs()), filterLocaleShortcut(Qt::CTRL + Qt::ALT + Qt::Key_S));
	newManagedAction(menu, "saveall", tr("Save A&ll"), SLOT(fileSaveAll()), Qt::CTRL + Qt::SHIFT + Qt::ALT + Qt::Key_S);
	newManagedAction(menu, "maketemplate", tr("&Make Template..."), SLOT(fileMakeTemplate()));


	submenu = newManagedMenu(menu, "utilities", tr("Fifi&x"));
	newManagedAction(submenu, "rename", tr("Save renamed/&moved file..."), "fileUtilCopyMove", 0, QString(), QList<QVariant>() << true);
	newManagedAction(submenu, "copy", tr("Save copied file..."), "fileUtilCopyMove", 0, QString(), QList<QVariant>() << false);
	newManagedAction(submenu, "delete", tr("&Delete file"), SLOT(fileUtilDelete()));
	newManagedAction(submenu, "chmod", tr("Set &permissions..."), SLOT(fileUtilPermissions()));
	submenu->addSeparator();
	newManagedAction(submenu, "revert", tr("&Revert to saved..."), SLOT(fileUtilRevert()));
	submenu->addSeparator();
	newManagedAction(submenu, "copyfilename", tr("Copy filename to &clipboard"), SLOT(fileUtilCopyFileName()));
	newManagedAction(submenu, "copymasterfilename", tr("Copy master filename to clipboard"), SLOT(fileUtilCopyMasterFileName()));

	QMenu *svnSubmenu = newManagedMenu(menu, "svn", tr("S&VN..."));
	newManagedAction(svnSubmenu, "checkin", tr("Check &in..."), SLOT(fileCheckin()));
	newManagedAction(svnSubmenu, "svnupdate", tr("SVN &update..."), SLOT(fileUpdate()));
	newManagedAction(svnSubmenu, "svnupdatecwd", tr("SVN update &work directory"), SLOT(fileUpdateCWD()));
	newManagedAction(svnSubmenu, "showrevisions", tr("Sh&ow old Revisions"), SLOT(showOldRevisions()));
	newManagedAction(svnSubmenu, "lockpdf", tr("Lock &PDF"), SLOT(fileLockPdf()));
	newManagedAction(svnSubmenu, "checkinpdf", tr("Check in P&DF"), SLOT(fileCheckinPdf()));
	newManagedAction(svnSubmenu, "difffiles", tr("Show difference between two files"), SLOT(fileDiff()));
	newManagedAction(svnSubmenu, "diff3files", tr("Show difference between two files in relation to base file"), SLOT(fileDiff3()));
	newManagedAction(svnSubmenu, "checksvnconflict", tr("Check SVN Conflict"), SLOT(checkSVNConflicted()));
	newManagedAction(svnSubmenu, "mergediff", tr("Try to merge differences"), SLOT(fileDiffMerge()));
	newManagedAction(svnSubmenu, "removediffmakers", tr("Remove Difference-Markers"), SLOT(removeDiffMarkers()));
	newManagedAction(svnSubmenu, "declareresolved", tr("Declare Conflict Resolved"), SLOT(declareConflictResolved()));
	newManagedAction(svnSubmenu, "nextdiff", tr("Jump to next difference"), SLOT(jumpNextDiff()), 0, "go-next-diff");
	newManagedAction(svnSubmenu, "prevdiff", tr("Jump to previous difference"), SLOT(jumpPrevDiff()), 0, "go-previous-diff");

	menu->addSeparator();
	newManagedAction(menu, "close", tr("&Close"), SLOT(fileClose()), Qt::CTRL + Qt::Key_W, "document-close");
	newManagedAction(menu, "closeall", tr("Clos&e All"), SLOT(fileCloseAll()));

	menu->addSeparator();
	newManagedEditorAction(menu, "print", tr("Print Source Code..."), "print", Qt::CTRL + Qt::Key_P);

	menu->addSeparator();
	newManagedAction(menu, "exit", tr("Exit"), SLOT(fileExit()), Qt::CTRL + Qt::Key_Q)->setMenuRole(QAction::QuitRole);

	//edit
	menu = newManagedMenu("main/edit", tr("&Edit"));
	actUndo = newManagedAction(menu, "undo", tr("&Undo"), SLOT(editUndo()), QKeySequence::Undo, "edit-undo");
	actRedo = newManagedAction(menu, "redo", tr("&Redo"), SLOT(editRedo()), QKeySequence::Redo, "edit-redo");
#ifndef QT_NO_DEBUG
	newManagedAction(menu, "debughistory", tr("Debug undo stack"), SLOT(editDebugUndoStack()));
#endif
	menu->addSeparator();
	newManagedAction(menu, "copy", tr("&Copy"), SLOT(editCopy()), QKeySequence::Copy, "edit-copy");
	newManagedEditorAction(menu, "cut", tr("C&ut"), "cut", QKeySequence::Cut, "edit-cut");
	newManagedAction(menu, "paste", tr("&Paste"), SLOT(editPaste()), QKeySequence::Paste, "edit-paste");

	submenu = newManagedMenu(menu, "selection", tr("&Selection"));
	newManagedEditorAction(submenu, "selectAll", tr("Select &All"), "selectAll", Qt::CTRL + Qt::Key_A);
	newManagedEditorAction(submenu, "selectAllOccurences", tr("Select All &Occurences"), "selectAllOccurences");
	newManagedEditorAction(submenu, "expandSelectionToWord", tr("Expand Selection to Word"), "selectExpandToNextWord", Qt::CTRL + Qt::Key_D);
	newManagedEditorAction(submenu, "expandSelectionToLine", tr("Expand Selection to Line"), "selectExpandToNextLine", Qt::CTRL + Qt::Key_L);

	submenu = newManagedMenu(menu, "lineoperations", tr("&Line Operations"));
	newManagedAction(submenu, "deleteLine", tr("Delete &Line"), SLOT(editDeleteLine()), Qt::CTRL + Qt::Key_K);
	newManagedAction(submenu, "deleteToEndOfLine", tr("Delete To &End Of Line"), SLOT(editDeleteToEndOfLine()), MAC_OTHER(Qt::CTRL + Qt::Key_Delete,  Qt::AltModifier + Qt::Key_K));
	newManagedAction(submenu, "deleteFromStartOfLine", tr("Delete From &Start Of Line"), SLOT(editDeleteFromStartOfLine()), MAC_OTHER(Qt::CTRL + Qt::Key_Backspace, 0));
	newManagedAction(submenu, "moveLineUp", tr("Move Line &Up"), SLOT(editMoveLineUp()));
	newManagedAction(submenu, "moveLineDown", tr("Move Line &Down"), SLOT(editMoveLineDown()));
	newManagedAction(submenu, "duplicateLine", tr("Du&plicate Line"), SLOT(editDuplicateLine()));
	newManagedAction(submenu, "alignMirrors", tr("&Align Cursors"), SLOT(editAlignMirrors()));

	submenu = newManagedMenu(menu, "textoperations", tr("&Text Operations"));
	newManagedAction(submenu, "textToLowercase", tr("To Lowercase"), SLOT(editTextToLowercase()));
	newManagedAction(submenu, "textToUppercase", tr("To Uppercase"), SLOT(editTextToUppercase()));
	newManagedAction(submenu, "textToTitlecaseStrict", tr("To Titlecase (strict)"), SLOT(editTextToTitlecase()));
	newManagedAction(submenu, "textToTitlecaseSmart", tr("To Titlecase (smart)"), SLOT(editTextToTitlecaseSmart()));


	menu->addSeparator();
	submenu = newManagedMenu(menu, "searching", tr("&Searching"));
	newManagedAction(submenu, "find", tr("&Find"), SLOT(editFind()), QKeySequence::Find);
	newManagedEditorAction(submenu, "findnext", tr("Find &Next"), "findNext", MAC_OTHER(Qt::CTRL + Qt::Key_G, Qt::Key_F3));
	newManagedEditorAction(submenu, "findprev", tr("Find &Prev"), "findPrev", MAC_OTHER(Qt::CTRL + Qt::SHIFT + Qt::Key_G, Qt::SHIFT + Qt::Key_F3));
	newManagedEditorAction(submenu, "findinsamedir", tr("Continue F&ind"), "findInSameDir", Qt::CTRL + Qt::Key_M);
	newManagedEditorAction(submenu, "findcount", tr("&Count"), "findCount");
	newManagedEditorAction(submenu, "select", tr("&Select all matches..."), "selectAllMatches");
	//newManagedAction(submenu,"findglobal",tr("Find &Dialog..."), SLOT(editFindGlobal()));
	submenu->addSeparator();
	newManagedEditorAction(submenu, "replace", tr("&Replace"), "replacePanel", Qt::CTRL + Qt::Key_R);
	newManagedEditorAction(submenu, "replacenext", tr("Replace Next"), "replaceNext");
	newManagedEditorAction(submenu, "replaceprev", tr("Replace Prev"), "replacePrev");
	newManagedEditorAction(submenu, "replaceall", tr("Replace &All"), "replaceAll");

	menu->addSeparator();
	submenu = newManagedMenu(menu, "goto", tr("Go to"));

	newManagedEditorAction(submenu, "line", tr("Line"), "gotoLine", MAC_OTHER(Qt::CTRL + Qt::Key_L, Qt::CTRL + Qt::Key_G), "goto");
	newManagedEditorAction(submenu, "lastchange", tr("Previous Change"), "jumpChangePositionBackward", Qt::CTRL + Qt::Key_H);
	newManagedEditorAction(submenu, "nextchange", tr("Next Change"), "jumpChangePositionForward", Qt::CTRL + Qt::SHIFT + Qt::Key_H);
	submenu->addSeparator();
	newManagedAction(submenu, "markprev", tr("Previous mark"), "gotoMark", MAC_OTHER(0, Qt::CTRL + Qt::Key_Up), "", QList<QVariant>() << true << -1); //, ":/images/errorprev.png");
	newManagedAction(submenu, "marknext", tr("Next mark"), "gotoMark", MAC_OTHER(0, Qt::CTRL + Qt::Key_Down), "", QList<QVariant>() << false << -1); //, ":/images/errornext.png");
	submenu->addSeparator();
	if (cursorHistory) {
		cursorHistory->setBackAction(newManagedAction(submenu, "goback", tr("Go Back"), SLOT(goBack()), MAC_OTHER(0, Qt::ALT + Qt::Key_Left), "back"));
		cursorHistory->setForwardAction(newManagedAction(submenu, "goforward", tr("Go Forward"), SLOT(goForward()), MAC_OTHER(0, Qt::ALT + Qt::Key_Right), "forward"));
	}

	submenu = newManagedMenu(menu, "gotoBookmark", tr("Goto Bookmark"));
	QList<int> bookmarkIndicies = QList<int>() << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << 0;
	foreach (int i, bookmarkIndicies)
		newManagedEditorAction(submenu, QString("bookmark%1").arg(i), tr("Bookmark %1").arg(i), "jumpToBookmark", Qt::CTRL + Qt::Key_0 + i, "", QList<QVariant>() << i);


	submenu = newManagedMenu(menu, "toggleBookmark", tr("Toggle Bookmark"));
	newManagedEditorAction(submenu, QString("bookmark"), tr("Unnamed Bookmark"), "toggleBookmark", Qt::CTRL + Qt::SHIFT + Qt::Key_B, "", QList<QVariant>() << -1);
	foreach (int i, bookmarkIndicies)
		newManagedEditorAction(submenu, QString("bookmark%1").arg(i), tr("Bookmark %1").arg(i), "toggleBookmark", Qt::CTRL + Qt::SHIFT + Qt::Key_0 + i, "", QList<QVariant>() << i);


	menu->addSeparator();
	submenu = newManagedMenu(menu, "lineend", tr("Line Ending"));
	QActionGroup *lineEndingGroup = new QActionGroup(this);
	QAction *act = newManagedAction(submenu, "crlf", tr("DOS/Windows (CR LF)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Windows);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);
	act = newManagedAction(submenu, "lf", tr("Unix (LF)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Unix);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);
	act = newManagedAction(submenu, "cr", tr("Old Mac (CR)"), SLOT(editChangeLineEnding()));
	act->setData(QDocument::Mac);
	act->setCheckable(true);
	lineEndingGroup->addAction(act);


	newManagedAction(menu, "encoding", tr("Setup Encoding..."), SLOT(editSetupEncoding()))->setMenuRole(QAction::NoRole); // with the default "QAction::TextHeuristicRole" this was interperted as Preferences on OSX
	newManagedAction(menu, "unicodeChar", tr("Insert Unicode Character..."), SLOT(editInsertUnicode()), filterLocaleShortcut(Qt::ALT + Qt::CTRL + Qt::Key_U));



	//Edit 2 (for LaTeX related things)
	menu = newManagedMenu("main/edit2", tr("&Idefix"));
	newManagedAction(menu, "eraseWord", tr("Erase &Word/Cmd/Env"), SLOT(editEraseWordCmdEnv()), MAC_OTHER(0, Qt::ALT + Qt::Key_Delete));

	menu->addSeparator();
	newManagedAction(menu, "pasteAsLatex", tr("Pas&te as LaTeX"), SLOT(editPasteLatex()), Qt::CTRL + Qt::SHIFT + Qt::Key_V, "editpaste");
	newManagedAction(menu, "convertTo", tr("Co&nvert to LaTeX"), SLOT(convertToLatex()));
	newManagedAction(menu, "previewLatex", tr("Pre&view Selection/Parentheses"), SLOT(previewLatex()), Qt::ALT + Qt::Key_P);
	newManagedAction(menu, "removePreviewLatex", tr("C&lear Inline Preview"), SLOT(clearPreview()));

	menu->addSeparator();
	newManagedEditorAction(menu, "togglecomment", tr("Toggle &Comment"), "toggleCommentSelection", Qt::CTRL + Qt::Key_T);
	newManagedEditorAction(menu, "comment", tr("&Comment"), "commentSelection");
	newManagedEditorAction(menu, "uncomment", tr("&Uncomment"), "uncommentSelection", Qt::CTRL + Qt::Key_U);
	newManagedEditorAction(menu, "indent", tr("&Indent"), "indentSelection");
	newManagedEditorAction(menu, "unindent", tr("Unin&dent"), "unindentSelection");
	newManagedAction(menu, "hardbreak", tr("Hard Line &Break..."), SLOT(editHardLineBreak()));
	newManagedAction(menu, "hardbreakrepeat", tr("R&epeat Hard Line Break"), SLOT(editHardLineBreakRepeat()));

	menu->addSeparator();
	submenu = newManagedMenu(menu, "goto", tr("&Go to"));

	newManagedAction(submenu, "errorprev", tr("Previous Error"), "gotoNearLogEntry", MAC_OTHER(0, Qt::CTRL + Qt::SHIFT + Qt::Key_Up), "errorprev", QList<QVariant>() << LT_ERROR << true << tr("No LaTeX errors detected !"));
	newManagedAction(submenu, "errornext", tr("Next Error"), "gotoNearLogEntry", MAC_OTHER(0, Qt::CTRL + Qt::SHIFT + Qt::Key_Down), "errornext", QList<QVariant>() << LT_ERROR << false << tr("No LaTeX errors detected !"));
	newManagedAction(submenu, "warningprev", tr("Previous Warning"), "gotoNearLogEntry", QKeySequence(), "", QList<QVariant>() << LT_WARNING << true << tr("No LaTeX warnings detected !")); //, ":/images/errorprev.png");
	newManagedAction(submenu, "warningnext", tr("Next Warning"), "gotoNearLogEntry", QKeySequence(), "", QList<QVariant>() << LT_WARNING << false << tr("No LaTeX warnings detected !")); //, ":/images/errornext.png");
	newManagedAction(submenu, "badboxprev", tr("Previous Bad Box"), "gotoNearLogEntry", MAC_OTHER(0, Qt::SHIFT + Qt::ALT + Qt::Key_Up), "", QList<QVariant>() << LT_BADBOX << true << tr("No bad boxes detected !")); //, ":/images/errorprev.png");
	newManagedAction(submenu, "badboxnext", tr("Next Bad Box"), "gotoNearLogEntry", MAC_OTHER(0, Qt::SHIFT + Qt::ALT + Qt::Key_Down), "", QList<QVariant>() << LT_BADBOX << true << tr("No bad boxes detected !")); //, ":/images/errornext.png");
	submenu->addSeparator();
	newManagedAction(submenu, "definition", tr("Definition"), SLOT(editGotoDefinition()), filterLocaleShortcut(Qt::CTRL + Qt::ALT + Qt::Key_F));

	menu->addSeparator();
	newManagedAction(menu, "generateMirror", tr("Re&name Environment"), SLOT(generateMirror()));

	submenu = newManagedMenu(menu, "parens", tr("Parenthesis"));
	newManagedAction(submenu, "jump", tr("Jump to Match"), SLOT(jumpToBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_J));
	newManagedAction(submenu, "selectBracketInner", tr("Select Inner"), SLOT(selectBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_I))->setProperty("type", "inner");
	newManagedAction(submenu, "selectBracketOuter", tr("Select Outer"), SLOT(selectBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_O))->setProperty("type", "outer");
	newManagedAction(submenu, "selectBracketCommand", tr("Select Command"), SLOT(selectBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_C))->setProperty("type", "command");
	newManagedAction(submenu, "selectBracketLine", tr("Select Line"), SLOT(selectBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_L))->setProperty("type", "line");
	newManagedAction(submenu, "generateInvertedBracketMirror", tr("Select Inverting"), SLOT(generateBracketInverterMirror()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_S));

	submenu->addSeparator();
	newManagedAction(submenu, "findMissingBracket", tr("Find Mismatch"), SLOT(findMissingBracket()), QKeySequence(Qt::SHIFT + Qt::CTRL + Qt::Key_P, Qt::Key_M));

	submenu = newManagedMenu(menu, "complete", tr("Complete"));
	newManagedAction(submenu, "normal", tr("Normal"), SLOT(normalCompletion()), MAC_OTHER(Qt::META + Qt::Key_Space, Qt::CTRL + Qt::Key_Space));
	newManagedAction(submenu, "environment", tr("\\begin{ Completion"), SLOT(insertEnvironmentCompletion()), Qt::CTRL + Qt::ALT + Qt::Key_Space);
	newManagedAction(submenu, "text", tr("Normal Text"), SLOT(insertTextCompletion()), Qt::SHIFT + Qt::ALT + Qt::Key_Space);
	newManagedAction(submenu, "closeEnvironment", tr("Close latest open environment"), SLOT(closeEnvironment()), Qt::ALT + Qt::Key_Return);

	menu->addSeparator();
	newManagedAction(menu, "reparse", tr("Refresh Structure"), SLOT(updateStructure()));
	act = newManagedAction(menu, "refreshQNFA", tr("Refresh Language Model"), SLOT(updateTexQNFA()));
	act->setStatusTip(tr("Force an update of the dynamic language model used for highlighting and folding. Likely, you do not need to call this because updates are usually automatic."));
	newManagedAction(menu, "removePlaceHolders", tr("Remove Placeholders"), SLOT(editRemovePlaceHolders()), Qt::CTRL + Qt::SHIFT + Qt::Key_K);
	newManagedAction(menu, "removeCurrentPlaceHolder", tr("Remove Current Placeholder"), SLOT(editRemoveCurrentPlaceHolder()));

	//tools


	menu = newManagedMenu("main/tools", tr("&Tools"));
	menu->setProperty("defaultSlot", QByteArray(SLOT(commandFromAction())));
	newManagedAction(menu, "quickbuild", tr("&Build && View"), SLOT(commandFromAction()), (QList<QKeySequence>() << Qt::Key_F5 << Qt::Key_F1), "build")->setData(BuildManager::CMD_QUICK);
	newManagedAction(menu, "compile", tr("&Compile"), SLOT(commandFromAction()), Qt::Key_F6, "compile")->setData(BuildManager::CMD_COMPILE);
	newManagedAction(menu, "stopcompile", buildManager.stopBuildAction())->setText(buildManager.tr("Stop Compile")); // resetting text necessary for language updates
	buildManager.stopBuildAction()->setParent(menu);  // actions need to be a child of the menu in order to be configurable in toolbars
	newManagedAction(menu, "view", tr("&View"), SLOT(commandFromAction()), Qt::Key_F7, "viewer")->setData(BuildManager::CMD_VIEW);
	newManagedAction(menu, "bibtex", tr("&Bibliography"), SLOT(commandFromAction()), Qt::Key_F8)->setData(BuildManager::CMD_BIBLIOGRAPHY);
	newManagedAction(menu, "glossary", tr("&Glossary"), SLOT(commandFromAction()), Qt::Key_F9)->setData(BuildManager::CMD_GLOSSARY);
	newManagedAction(menu, "index", tr("&Index"), SLOT(commandFromAction()))->setData(BuildManager::CMD_INDEX);

	menu->addSeparator();
	submenu = newManagedMenu(menu, "commands", tr("&Commands", "menu"));
	newManagedAction(submenu, "latexmk", tr("&Latexmk"), SLOT(commandFromAction()))->setData(BuildManager::CMD_LATEXMK);
	submenu->addSeparator();
	newManagedAction(submenu, "latex", tr("&LaTeX"), SLOT(commandFromAction()), QKeySequence(), "compile-latex")->setData(BuildManager::CMD_LATEX);
	newManagedAction(submenu, "pdflatex", tr("&PDFLaTeX"), SLOT(commandFromAction()), QKeySequence(), "compile-pdf")->setData(BuildManager::CMD_PDFLATEX);
	newManagedAction(submenu, "xelatex", "&XeLaTeX", SLOT(commandFromAction()), QKeySequence(), "compile-xelatex")->setData(BuildManager::CMD_XELATEX);
	newManagedAction(submenu, "lualatex", "L&uaLaTeX", SLOT(commandFromAction()), QKeySequence(), "compile-lua")->setData(BuildManager::CMD_LUALATEX);
	submenu->addSeparator();
	newManagedAction(submenu, "dvi2ps", tr("DVI->PS"), SLOT(commandFromAction()), QKeySequence(), "convert-dvips")->setData(BuildManager::CMD_DVIPS);
	newManagedAction(submenu, "ps2pdf", tr("P&S->PDF"), SLOT(commandFromAction()), QKeySequence(), "convert-pspdf")->setData(BuildManager::CMD_PS2PDF);
	newManagedAction(submenu, "dvipdf", tr("DV&I->PDF"), SLOT(commandFromAction()), QKeySequence(), "convert-dvipdf")->setData(BuildManager::CMD_DVIPDF);
	submenu->addSeparator();
	newManagedAction(submenu, "viewdvi", tr("View &DVI"), SLOT(commandFromAction()), QKeySequence(), "view-doc-dvi")->setData(BuildManager::CMD_VIEW_DVI);
	newManagedAction(submenu, "viewps", tr("Vie&w PS"), SLOT(commandFromAction()), QKeySequence(), "view-doc-ps")->setData(BuildManager::CMD_VIEW_PS);
	newManagedAction(submenu, "viewpdf", tr("View PD&F"), SLOT(commandFromAction()), QKeySequence(), "view-doc-pdf")->setData(BuildManager::CMD_VIEW_PDF);
	submenu->addSeparator();
	newManagedAction(submenu, "bibtex", tr("&Bibtex"), SLOT(commandFromAction()))->setData(BuildManager::CMD_BIBTEX);
	newManagedAction(submenu, "bibtex8", tr("&Bibtex 8-Bit"), SLOT(commandFromAction()))->setData(BuildManager::CMD_BIBTEX8);
	newManagedAction(submenu, "biber", tr("Bibe&r"), SLOT(commandFromAction()))->setData(BuildManager::CMD_BIBER);
	submenu->addSeparator();
	newManagedAction(submenu, "makeindex", tr("&MakeIndex"), SLOT(commandFromAction()))->setData(BuildManager::CMD_MAKEINDEX);
	newManagedAction(submenu, "texindy", tr("&TexIndy"), SLOT(commandFromAction()), QKeySequence())->setData(BuildManager::CMD_TEXINDY);
	newManagedAction(submenu, "makeglossaries", tr("&Makeglossaries"), SLOT(commandFromAction()), QKeySequence())->setData(BuildManager::CMD_MAKEGLOSSARIES);
	submenu->addSeparator();
	newManagedAction(submenu, "metapost", tr("&MetaPost"), SLOT(commandFromAction()))->setData(BuildManager::CMD_METAPOST);
	newManagedAction(submenu, "asymptote", tr("&Asymptote"), SLOT(commandFromAction()))->setData(BuildManager::CMD_ASY);

	submenu = newManagedMenu(menu, "user", tr("&User", "menu"));
	updateUserToolMenu();
	menu->addSeparator();
	newManagedAction(menu, "clean", tr("Cle&an Auxiliary Files..."), SLOT(cleanAll()));
	newManagedAction(menu, "terminal", tr("Open &Terminal"), SLOT(openTerminal()));
	menu->addSeparator();
	newManagedAction(menu, "viewlog", tr("View &Log"), SLOT(commandFromAction()), QKeySequence(), "viewlog")->setData(BuildManager::CMD_VIEW_LOG);
	act = newManagedAction(menu, "logmarkers", tr("Show Log Markers"), 0, 0, "logmarkers");
	act->setCheckable(true);
	connect(act, SIGNAL(triggered(bool)), SLOT(setLogMarksVisible(bool)));
	menu->addSeparator();
	newManagedAction(menu, "htmlexport", tr("C&onvert to Html..."), SLOT(webPublish()));
	newManagedAction(menu, "htmlsourceexport", tr("C&onvert Source to Html..."), SLOT(webPublishSource()));
	menu->addSeparator();
	newManagedAction(menu, "analysetext", tr("A&nalyse Text..."), SLOT(analyseText()));
	newManagedAction(menu, "generaterandomtext", tr("Generate &Random Text..."), SLOT(generateRandomText()));
	menu->addSeparator();
	newManagedAction(menu, "spelling", tr("Check Spelling..."), SLOT(editSpell()), MAC_OTHER(Qt::CTRL + Qt::SHIFT + Qt::Key_F7, Qt::CTRL + Qt::Key_Colon));
	newManagedAction(menu, "thesaurus", tr("Thesaurus..."), SLOT(editThesaurus()), Qt::CTRL + Qt::SHIFT + Qt::Key_F8);
	newManagedAction(menu, "wordrepetions", tr("Find Word Repetitions..."), SLOT(findWordRepetions()));

	//  Latex/Math external
	configManager.loadManagedMenus(":/uiconfig.xml");
	// add some additional items
	menu = newManagedMenu("main/latex", tr("&LaTeX"));
	menu->setProperty("defaultSlot", QByteArray(SLOT(insertFromAction())));
	newManagedAction(menu, "insertrefnextlabel", tr("Insert \\ref to Next Label"), SLOT(editInsertRefToNextLabel()), Qt::ALT + Qt::CTRL + Qt::Key_R);
	newManagedAction(menu, "insertrefprevlabel", tr("Insert \\ref to Previous Label"), SLOT(editInsertRefToPrevLabel()));
	submenu = newManagedMenu(menu, "tabularmanipulation", tr("Manipulate Tables", "table"));
	newManagedAction(submenu, "addRow", tr("Add Row", "table"), SLOT(addRowCB()), QKeySequence(), "addRow");
	newManagedAction(submenu, "addColumn", tr("Add Column", "table"), SLOT(addColumnCB()), QKeySequence(), "addCol");
	newManagedAction(submenu, "removeRow", tr("Remove Row", "table"), SLOT(removeRowCB()), QKeySequence(), "remRow");
	newManagedAction(submenu, "removeColumn", tr("Remove Column", "table"), SLOT(removeColumnCB()), QKeySequence(), "remCol");
	newManagedAction(submenu, "cutColumn", tr("Cut Column", "table"), SLOT(cutColumnCB()), QKeySequence(), "cutCol");
	newManagedAction(submenu, "pasteColumn", tr("Paste Column", "table"), SLOT(pasteColumnCB()), QKeySequence(), "pasteCol");
	newManagedAction(submenu, "addHLine", tr("Add \\hline", "table"), SLOT(addHLineCB()));
	newManagedAction(submenu, "remHLine", tr("Remove \\hline", "table"), SLOT(remHLineCB()));
	newManagedAction(submenu, "insertTableTemplate", tr("Remodel Table Using Template", "table"), SLOT(insertTableTemplate()));
	newManagedAction(submenu, "alignColumns", tr("Align Columns"), SLOT(alignTableCols()), QKeySequence(), "alignCols");
	submenu = newManagedMenu(menu, "magicComments", tr("Add magic comments ..."));
	newManagedAction(submenu, "addMagicRoot", tr("Insert root document name as TeX comment"), SLOT(addMagicRoot()));
	newManagedAction(submenu, "addMagicLang", tr("Insert language as TeX comment"), SLOT(insertSpellcheckMagicComment()));
	newManagedAction(submenu, "addMagicCoding", tr("Insert document coding as TeX comment"), SLOT(addMagicCoding()));

	menu = newManagedMenu("main/math", tr("&Math"));
	menu->setProperty("defaultSlot", QByteArray(SLOT(insertFromAction())));
	//wizards

	menu = newManagedMenu("main/wizards", tr("&Wizards"));
	newManagedAction(menu, "start", tr("Quick &Start..."), SLOT(quickDocument()));
	newManagedAction(menu, "beamer", tr("Quick &Beamer Presentation..."), SLOT(quickBeamer()));
	newManagedAction(menu, "letter", tr("Quick &Letter..."), SLOT(quickLetter()));

	menu->addSeparator();
	newManagedAction(menu, "tabular", tr("Quick &Tabular..."), SLOT(quickTabular()));
	newManagedAction(menu, "tabbing", tr("Quick T&abbing..."), SLOT(quickTabbing()));
	newManagedAction(menu, "array", tr("Quick &Array..."), SLOT(quickArray()));
	newManagedAction(menu, "graphic", tr("Insert &Graphic..."), SLOT(quickGraphics()), QKeySequence(), "image");
#ifdef Q_OS_WIN
	if (QSysInfo::windowsVersion() >= QSysInfo::WV_WINDOWS7) {
		newManagedAction(menu, "math", tr("Math Assistant..."), SLOT(quickMath()), QKeySequence(), "TexTablet");
	}
#endif

	menu = newManagedMenu("main/bibliography", tr("&Bibliography"));
	if (!bibtexEntryActions) {
		bibtexEntryActions = new QActionGroup(this);
		foreach (const BibTeXType &bt, BibTeXDialog::getPossibleEntryTypes(BibTeXDialog::BIBTEX)) {
			QAction *act = newManagedAction(menu, "bibtex/" + bt.name.mid(1), bt.description, SLOT(insertBibEntryFromAction()));
			act->setData(bt.name);
			act->setActionGroup(bibtexEntryActions);
		}
	} else {
		foreach (const BibTeXType &bt, BibTeXDialog::getPossibleEntryTypes(BibTeXDialog::BIBTEX))
			newManagedAction(menu, "bibtex/" + bt.name.mid(1), bt.description, SLOT(insertBibEntryFromAction()))->setData(bt.name);
	}

	if (!biblatexEntryActions) {
		biblatexEntryActions = new QActionGroup(this);
		foreach (const BibTeXType &bt, BibTeXDialog::getPossibleEntryTypes(BibTeXDialog::BIBLATEX)) {
			QAction *act = newManagedAction(menu, "biblatex/" + bt.name.mid(1), bt.description, SLOT(insertBibEntryFromAction()));
			act->setData(bt.name);
			act->setActionGroup(biblatexEntryActions);
		}
	} else {
		foreach (const BibTeXType &bt, BibTeXDialog::getPossibleEntryTypes(BibTeXDialog::BIBLATEX))
			newManagedAction(menu, "biblatex/" + bt.name.mid(1), bt.description, SLOT(insertBibEntryFromAction()))->setData(bt.name);
	}
	menu->addSeparator();
	newManagedEditorAction(menu, "clean", tr("&Clean"), "cleanBib");
	menu->addSeparator();
	newManagedAction(menu, "dialog", tr("&Insert Bibliography Entry..."), SLOT(insertBibEntry()));
	menu->addSeparator();
	QMenu *bibTypeMenu = newManagedMenu(menu, "type", tr("Type"));
	if (!bibTypeActions) {
		bibTypeActions = new QActionGroup(this);
		bibTypeActions->setExclusive(true);
		act = newManagedAction(bibTypeMenu, "bibtex", tr("BibTeX"), SLOT(setBibTypeFromAction()));
		act->setData("bibtex");
		act->setCheckable(true);
		act->setChecked(true);
		bibTypeActions->addAction(act);
		act = newManagedAction(bibTypeMenu, "biblatex", tr("BibLaTeX"), SLOT(setBibTypeFromAction()));
		act->setData("biblatex");
		act->setCheckable(true);
		bibTypeActions->addAction(act);
	}
	act = newManagedAction(bibTypeMenu, "bibtex", tr("BibTeX"), SLOT(setBibTypeFromAction()));
	act = newManagedAction(bibTypeMenu, "biblatex", tr("BibLaTeX"), SLOT(setBibTypeFromAction()));
	act->trigger(); // initialize menu for specified type




	//  User
	menu = newManagedMenu("main/macros", tr("Ma&cros"));
	updateUserMacros();
	scriptengine::macros = &configManager.completerConfig->userMacros;

	//---view---
	menu = newManagedMenu("main/view", tr("&View"));
	newManagedAction(menu, "prevdocument", tr("Previous Document"), SLOT(gotoPrevDocument()), QList<QKeySequence>() << Qt::CTRL + Qt::Key_PageUp << Qt::CTRL + Qt::SHIFT + Qt::Key_Tab);
	newManagedAction(menu, "nextdocument", tr("Next Document"), SLOT(gotoNextDocument()), QList<QKeySequence>() << Qt::CTRL + Qt::Key_PageDown << Qt::CTRL + Qt::Key_Tab);
	newManagedMenu(menu, "documents", tr("Open Documents"));
	newManagedAction(menu, "documentlist", tr("List Of Open Documents"), SLOT(viewDocumentList()));
	newManagedAction(menu, "documentlisthidden", tr("List Of Hidden Documents"), SLOT(viewDocumentListHidden()));

	newManagedAction(menu, "focuseditor", tr("Focus Editor"), SLOT(focusEditor()), QList<QKeySequence>() << Qt::ALT + Qt::CTRL + Qt::Key_Left);
	newManagedAction(menu, "focusviewer", tr("Focus Viewer"), SLOT(focusViewer()), QList<QKeySequence>() << Qt::ALT + Qt::CTRL + Qt::Key_Right);

	menu->addSeparator();
	submenu = newManagedMenu(menu, "show", tr("Show"));
	newManagedAction(submenu, "structureview", sidePanel->toggleViewAction());
	newManagedAction(submenu, "outputview", outputView->toggleViewAction());
	act = newManagedAction(submenu, "statusbar", tr("Statusbar"), SLOT(showStatusbar()));
	act->setCheckable(true);
	act->setChecked(configManager.getOption("View/ShowStatusbar").toBool());

	newManagedAction(menu, "enlargePDF", tr("Show embedded PDF large"), SLOT(enlargeEmbeddedPDFViewer()));
	newManagedAction(menu, "shrinkPDF", tr("Show embedded PDF small"), SLOT(shrinkEmbeddedPDFViewer()));

	newManagedAction(menu, "closesomething", tr("Close Something"), SLOT(viewCloseSomething()), Qt::Key_Escape);

	menu->addSeparator();
	submenu = newManagedMenu(menu, "collapse", tr("Collapse"));
	newManagedEditorAction(submenu, "all", tr("Everything"), "foldEverything", 0, "", QList<QVariant>() << false);
	newManagedAction(submenu, "block", tr("Nearest Block"), SLOT(viewCollapseBlock()));
	for (int i = 1; i <= 4; i++)
		newManagedEditorAction(submenu, QString::number(i), tr("Level %1").arg(i), "foldLevel", 0, "", QList<QVariant>() << false << i);
	submenu = newManagedMenu(menu, "expand", tr("Expand"));
	newManagedEditorAction(submenu, "all", tr("Everything"), "foldEverything", 0, "", QList<QVariant>() << true);
	newManagedAction(submenu, "block", tr("Nearest Block"), SLOT(viewExpandBlock()));
	for (int i = 1; i <= 4; i++)
		newManagedEditorAction(submenu, QString::number(i), tr("Level %1").arg(i), "foldLevel", 0, "", QList<QVariant>() << true << i);

	submenu = newManagedMenu(menu, "grammar", tr("Grammar errors"));
	static bool showGrammarType[8] = {false};
	for (int i = 0; i < 8; i++) configManager.registerOption(QString("Grammar/Display Error %1").arg(i), &showGrammarType[i], true);
	newManagedAction(submenu, "0", tr("Word Repetition"), "toggleGrammar", 0, "", QList<QVariant>() << 0);
	newManagedAction(submenu, "1", tr("Long-range Word Repetition"), "toggleGrammar", 0, "", QList<QVariant>() << 1);
	newManagedAction(submenu, "2", tr("Bad words"), "toggleGrammar", 0, "", QList<QVariant>() << 2);
	newManagedAction(submenu, "3", tr("Grammar Mistake"), "toggleGrammar", 0, "", QList<QVariant>() << 3);
	for (int i = 4; i < 8; i++)
		newManagedAction(submenu, QString("%1").arg(i), tr("Grammar Mistake Special %1").arg(i - 3), "toggleGrammar", 0, "", QList<QVariant>() << i);
	for (int i = 0; i < submenu->actions().size(); i++)
		if (!submenu->actions()[i]->isCheckable()) {
			submenu->actions()[i]->setCheckable(true);
			configManager.linkOptionToObject(&showGrammarType[i], submenu->actions()[i], 0);
			LatexEditorView::setGrammarOverlayDisabled(i, !submenu->actions()[i]->isChecked());
		}

	menu->addSeparator();
	submenu = newManagedMenu(menu, "editorZoom", tr("Editor Zoom"));
	newManagedEditorAction(submenu, "zoomIn", tr("Zoom In"), "zoomIn", Qt::CTRL + Qt::Key_Plus);
	newManagedEditorAction(submenu, "zoomOut", tr("Zoom Out"), "zoomOut", Qt::CTRL + Qt::Key_Minus);
	newManagedEditorAction(submenu, "resetZoom", tr("Reset Zoom"), "resetZoom");

#if QT_VERSION>=0x050000
	fullscreenModeAction = newManagedAction(menu, "fullscreenmode", tr("Full &Screen"), 0, QKeySequence::FullScreen);
#else
	fullscreenModeAction = newManagedAction(menu, "fullscreenmode", tr("Full &Screen"), 0, MAC_OTHER(Qt::CTRL + Qt::META + Qt::Key_F, Qt::Key_F11));
#endif
	fullscreenModeAction->setCheckable(true);
	connectUnique(fullscreenModeAction, SIGNAL(toggled(bool)), this, SLOT(setFullScreenMode()));
	connectUnique(menuBar(), SIGNAL(doubleClicked()), fullscreenModeAction, SLOT(toggle()));

	menu->addSeparator();
	QMenu *hlMenu = newManagedMenu(menu, "highlighting", tr("Highlighting"));
	if (!highlightLanguageActions) {
		highlightLanguageActions = new QActionGroup(this);
		highlightLanguageActions->setExclusive(true);
		connect(highlightLanguageActions, SIGNAL(triggered(QAction *)), this, SLOT(viewSetHighlighting(QAction *)));
		connect(hlMenu, SIGNAL(aboutToShow()), this, SLOT(showHighlightingMenu()));
		int id = 0;
		foreach (const QString &s, m_languages->languages()) {
#ifdef QT_NO_DEBUG
			if (s == "TXS Test Results") continue;
#endif
			QAction *act = newManagedAction(hlMenu, QString::number(id++), tr(qPrintable(s)));
			act->setData(s);
			act->setCheckable(true);
			hlMenu->addAction(act);
			highlightLanguageActions->addAction(act);
		}
	} else {
		int id = 0;
		foreach (const QString &s, m_languages->languages())
			newManagedAction(hlMenu, QString::number(id++), tr(qPrintable(s)));
	}

	//---options---
	menu = newManagedMenu("main/options", tr("&Options"));
	newManagedAction(menu, "config", tr("&Configure TeXstudio..."), SLOT(generalOptions()), 0, "configure")->setMenuRole(QAction::PreferencesRole);

	menu->addSeparator();
	newManagedAction(menu, "loadProfile", tr("Load &Profile..."), SLOT(loadProfile()));
	newManagedAction(menu, "saveProfile", tr("S&ave Profile..."), SLOT(saveProfile()));
	newManagedAction(menu, "saveSettings", tr("Save &Current Settings", "menu"), SLOT(saveSettings()));
	newManagedAction(menu, "restoreDefaultSettings", tr("Restore &Default Settings..."), SLOT(restoreDefaultSettings()));
	menu->addSeparator();

	submenu = newManagedMenu(menu, "rootdoc", tr("Root Document", "menu"));
	actgroupRootDocMode = new QActionGroup(this);
	actgroupRootDocMode->setExclusive(true);
	actRootDocAutomatic = newManagedAction(submenu, "auto", tr("Detect &Automatically"), SLOT(setAutomaticRootDetection()));
	actRootDocAutomatic->setCheckable(true);
	actRootDocAutomatic->setChecked(true);
	actgroupRootDocMode->addAction(actRootDocAutomatic);
	actRootDocExplicit = newManagedAction(submenu, "currentExplicit", "Shows Current Explicit Root");
	actRootDocExplicit->setCheckable(true);
	actRootDocExplicit->setVisible(false);
	actgroupRootDocMode->addAction(actRootDocExplicit);
	actRootDocSetExplicit = newManagedAction(submenu, "setExplicit", tr("Set Current Document As Explicit Root"), SLOT(setCurrentDocAsExplicitRoot()));

	//---help---
	menu = newManagedMenu("main/help", tr("&Help"));
	newManagedAction(menu, "latexreference", tr("LaTeX Reference..."), SLOT(latexHelp()), 0, "help-contents");
	newManagedAction(menu, "usermanual", tr("User Manual..."), SLOT(userManualHelp()), 0, "help-contents");
	newManagedAction(menu, "texdocdialog", tr("Packages Help..."), SLOT(texdocHelp()));

	menu->addSeparator();
	newManagedAction(menu, "checkinstall", tr("Check LaTeX Installation"), SLOT(checkLatexInstall()));
	newManagedAction(menu, "checkcwls", tr("Check Active Completion Files"), SLOT(checkCWLs()));
	newManagedAction(menu, "appinfo", tr("About TeXstudio..."), SLOT(helpAbout()), 0, APPICON)->setMenuRole(QAction::AboutRole);

	//additional elements for development


	//-----context menus-----
	if (LatexEditorView::getBaseActions().empty()) { //only called at first menu created
		QList<QAction *> baseContextActions;
		QAction *sep = new QAction(menu);
		sep->setSeparator(true);
		baseContextActions << getManagedActions(QStringList() << "copy" << "cut" << "paste", "main/edit/");
		baseContextActions << getManagedActions(QStringList() << "main/edit2/pasteAsLatex" << "main/edit2/convertTo" << "main/edit/selection/selectAll");
		baseContextActions << sep;
		baseContextActions << getManagedActions(QStringList() << "previewLatex" << "removePreviewLatex", "main/edit2/");
		LatexEditorView::setBaseActions(baseContextActions);
	}

	structureTreeView->setObjectName("StructureTree");
	structureTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	newManagedAction(structureTreeView, "CopySection", tr("Copy"), SLOT(editSectionCopy()));
	newManagedAction(structureTreeView, "CutSection", tr("Cut"), SLOT(editSectionCut()));
	newManagedAction(structureTreeView, "PasteBefore", tr("Paste Before"), SLOT(editSectionPasteBefore()));
	newManagedAction(structureTreeView, "PasteAfter", tr("Paste After"), SLOT(editSectionPasteAfter()));
	QAction *sep = new QAction(structureTreeView);
	sep->setSeparator(true);
	structureTreeView->addAction(sep);
	newManagedAction(structureTreeView, "IndentSection", tr("Indent Section"), SLOT(editIndentSection()));
	newManagedAction(structureTreeView, "UnIndentSection", tr("Unindent Section"), SLOT(editUnIndentSection()));
	connect(structureTreeView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(StructureContextMenu(QPoint)));

	configManager.updateRecentFiles(true);

	configManager.modifyMenuContents();
	configManager.modifyManagedShortcuts();
}
/*! \brief set-up all tool-bars
 */
void Texstudio::setupToolBars()
{
	//This method will be called multiple times and must not create something if this something already exists

	configManager.watchedMenus.clear();

	//customizable toolbars
	//first apply custom icons
	QMap<QString, QVariant>::const_iterator i = configManager.replacedIconsOnMenus.constBegin();
	while (i != configManager.replacedIconsOnMenus.constEnd()) {
		QString id = i.key();
		QString iconFilename = configManager.parseDir(i.value().toString());
		QObject *obj = configManager.menuParent->findChild<QObject *>(id);
		QAction *act = qobject_cast<QAction *>(obj);
		if (act && !iconFilename.isEmpty()) act->setIcon(QIcon(iconFilename));
		++i;
	}
	//setup customizable toolbars
	for (int i = 0; i < configManager.managedToolBars.size(); i++) {
		ManagedToolBar &mtb = configManager.managedToolBars[i];
		if (!mtb.toolbar) { //create actual toolbar on first call
			if (mtb.name == "Central") mtb.toolbar = centralToolBar;
			else mtb.toolbar = addToolBar(tr(qPrintable(mtb.name)));
			mtb.toolbar->setObjectName(mtb.name);
			addAction(mtb.toolbar->toggleViewAction());
			if (mtb.name == "Spelling") addToolBarBreak();
		} else mtb.toolbar->clear();
		foreach (const QString &actionName, mtb.actualActions) {
			if (actionName == "separator") mtb.toolbar->addSeparator(); //Case 1: Separator
			else if (actionName.startsWith("tags/")) {
				//Case 2: One of the xml tag widgets mapped on a toolbutton
				int tagCategorySep = actionName.indexOf("/", 5);
				XmlTagsListWidget *tagsWidget = findChild<XmlTagsListWidget *>(actionName.left(tagCategorySep));
				if (!tagsWidget) continue;
				if (!tagsWidget->isPopulated())
					tagsWidget->populate();
				QStringList list = tagsWidget->tagsTxtFromCategory(actionName.mid(tagCategorySep + 1));
				if (list.isEmpty()) continue;
				QToolButton *combo = createComboToolButton(mtb.toolbar, list, QList<QIcon>(), 0, this, SLOT(insertXmlTagFromToolButtonAction()));
				combo->setProperty("tagsID", actionName);
				mtb.toolbar->addWidget(combo);
			} else {
				QObject *obj = configManager.menuParent->findChild<QObject *>(actionName);
				QAction *act = qobject_cast<QAction *>(obj);
				if (act) {
					//Case 3: A normal QAction
					if (act->icon().isNull())
						act->setIcon(QIcon(APPICON));
					updateToolTipWithShortcut(act, configManager.showShortcutsInTooltips);
					mtb.toolbar->addAction(act);
				} else {
					QMenu *menu = qobject_cast<QMenu *>(obj);
					if (!menu) {
						qWarning("Unknown toolbar command %s", qPrintable(actionName));
						continue;
					}
					//Case 4: A submenu mapped on a toolbutton
					configManager.watchedMenus << actionName;
					QStringList list;
					QList<QIcon> icons;
					foreach (const QAction *act, menu->actions())
						if (!act->isSeparator()) {
							list.append(act->text());
							icons.append(act->icon());
						}
					//TODO: Is the callToolButtonAction()-slot really needed? Can't we just add the menu itself as the menu of the qtoolbutton, without creating a copy? (should be much faster)
					QToolButton *combo = createComboToolButton(mtb.toolbar, list, icons, 0, this, SLOT(callToolButtonAction()));
					combo->setProperty("menuID", actionName);
					mtb.toolbar->addWidget(combo);
				}
			}
		}
		if (mtb.actualActions.empty()) mtb.toolbar->setVisible(false);
	}
}

void Texstudio::updateAvailableLanguages()
{
	if (spellLanguageActions) delete spellLanguageActions;

	spellLanguageActions = new QActionGroup(statusTbLanguage);
	spellLanguageActions->setExclusive(true);

	foreach (const QString &s, spellerManager.availableDicts()) {
		QAction *act = new QAction(spellLanguageActions);
		act->setText(spellerManager.prettyName(s));
		act->setData(QVariant(s));
		act->setCheckable(true);
		connect(act, SIGNAL(triggered()), this, SLOT(changeEditorSpeller()));
	}

	QAction *act = new QAction(spellLanguageActions);
	act->setSeparator(true);
	act = new QAction(spellLanguageActions);
	act->setText(tr("Default") + QString(": %1").arg(spellerManager.prettyName(spellerManager.defaultSpellerName())));
	act->setData(QVariant("<default>"));
	connect(act, SIGNAL(triggered()), this, SLOT(changeEditorSpeller()));
	act->setCheckable(true);
	act->setChecked(true);

	act = new QAction(spellLanguageActions);
	act->setSeparator(true);
	act = new QAction(spellLanguageActions);
	act->setText(tr("Insert language as TeX comment"));
	connect(act, SIGNAL(triggered()), this, SLOT(insertSpellcheckMagicComment()));

	statusTbLanguage->addActions(spellLanguageActions->actions());

	if (currentEditorView()) {
		editorSpellerChanged(currentEditorView()->getSpeller());
	} else {
		editorSpellerChanged("<default>");
	}
}

void Texstudio::updateLanguageToolStatus()
{
	QIcon icon = getRealIconCached("languagetool");
	QSize iconSize = QSize(configManager.guiSecondaryToolbarIconSize, configManager.guiSecondaryToolbarIconSize);
	switch (grammarCheck->languageToolStatus()) {
		case GrammarCheck::LTS_Working:
			statusLabelLanguageTool->setPixmap(icon.pixmap(iconSize));
			statusLabelLanguageTool->setToolTip(QString(tr("Connected to LanguageTool at %1")).arg(grammarCheck->serverUrl()));
			break;
		case GrammarCheck::LTS_Error:
			statusLabelLanguageTool->setPixmap(icon.pixmap(iconSize, QIcon::Disabled));
			statusLabelLanguageTool->setToolTip(QString(tr("No LanguageTool server found at %1")).arg(grammarCheck->serverUrl()));
			break;
		case GrammarCheck::LTS_Unknown:
		default:
			statusLabelLanguageTool->setPixmap(icon.pixmap(iconSize, QIcon::Disabled));
			statusLabelLanguageTool->setToolTip(tr("LanguageTool status unknown"));
	}
}

/*! \brief set-up status bar
 */
void Texstudio::createStatusBar()
{
	QStatusBar *status = statusBar();
	status->setContextMenuPolicy(Qt::PreventContextMenu);
	status->setVisible(configManager.getOption("View/ShowStatusbar").toBool());

	QSize iconSize = QSize(configManager.guiSecondaryToolbarIconSize, configManager.guiSecondaryToolbarIconSize);
	QAction *act;
	QToolButton *tb;
	act = getManagedAction("main/view/show/structureview");
	if (act) {
		tb = new QToolButton(status);
		tb->setCheckable(true);
		tb->setChecked(act->isChecked());
		tb->setAutoRaise(true);
		tb->setIcon(act->icon());
		tb->setIconSize(iconSize);
		tb->setToolTip(act->toolTip());
		connect(tb, SIGNAL(clicked()), act, SLOT(trigger()));
		connect(act, SIGNAL(toggled(bool)), tb, SLOT(setChecked(bool)));
		status->addPermanentWidget(tb, 0);
	}
	act = getManagedAction("main/view/show/outputview");
	if (act) {
		tb = new QToolButton(status);
		tb->setCheckable(true);
		tb->setChecked(act->isChecked());
		tb->setAutoRaise(true);
		tb->setIcon(act->icon());
		tb->setIconSize(iconSize);
		tb->setToolTip(act->toolTip());
		connect(tb, SIGNAL(clicked()), act, SLOT(trigger()));
		connect(act, SIGNAL(toggled(bool)), tb, SLOT(setChecked(bool)));
		status->addPermanentWidget(tb, 0);
	}

	// spacer eating up all the space between "left" and "right" permanent widgets.
	QLabel *messageArea = new QLabel(status);
	connect(status, SIGNAL(messageChanged(QString)), messageArea, SLOT(setText(QString)));
	status->addPermanentWidget(messageArea, 1);

	// LanguageTool
	connect(grammarCheck, SIGNAL(languageToolStatusChanged()), this, SLOT(updateLanguageToolStatus()));
	statusLabelLanguageTool = new QLabel();
	updateLanguageToolStatus();
	status->addPermanentWidget(statusLabelLanguageTool);

	// language
	statusTbLanguage = new QToolButton(status);
	statusTbLanguage->setToolTip(tr("Language"));
	statusTbLanguage->setPopupMode(QToolButton::InstantPopup);
	statusTbLanguage->setAutoRaise(true);
	statusTbLanguage->setMinimumWidth(status->fontMetrics().width("OOOOOOO"));
	connect(&spellerManager, SIGNAL(dictPathChanged()), this, SLOT(updateAvailableLanguages()));
	connect(&spellerManager, SIGNAL(defaultSpellerChanged()), this, SLOT(updateAvailableLanguages()));
	updateAvailableLanguages();
	statusTbLanguage->setText(spellerManager.defaultSpellerName());
	status->addPermanentWidget(statusTbLanguage, 0);

	// encoding
	statusTbEncoding = new QToolButton(status);
	statusTbEncoding->setToolTip(tr("Encoding"));
	statusTbEncoding->setText(tr("Encoding") + "  ");
	statusTbEncoding->setPopupMode(QToolButton::InstantPopup);
	statusTbEncoding->setAutoRaise(true);
	statusTbEncoding->setMinimumWidth(status->fontMetrics().width("OOOOO"));

	QSet<int> encodingMibs;
	foreach (const QString &s, configManager.commonEncodings) {
		QTextCodec *codec = QTextCodec::codecForName(s.toLocal8Bit());
		if (!codec) continue;
		encodingMibs.insert(codec->mibEnum());
	}
	foreach (int mib, encodingMibs) {
		QAction *act = new QAction(statusTbEncoding);
		act->setText(QTextCodec::codecForMib(mib)->name());
		act->setData(mib);
		statusTbEncoding->addAction(act);
		connect(act, SIGNAL(triggered()), this, SLOT(changeTextCodec()));
	}
	act = new QAction(statusTbEncoding);
	act->setSeparator(true);
	statusTbEncoding->addAction(act);
	act = new QAction(statusTbEncoding);
	act->setText(tr("More Encodings..."));
	statusTbEncoding->addAction(act);
	connect(act, SIGNAL(triggered()), this, SLOT(editSetupEncoding()));
	act = new QAction(statusTbEncoding);
	act->setSeparator(true);
	statusTbEncoding->addAction(act);

	act = new QAction(statusTbEncoding);
	act->setText(tr("Insert encoding as TeX comment"));
	statusTbEncoding->addAction(act);
	connect(act, SIGNAL(triggered()), this, SLOT(addMagicCoding()));

	status->addPermanentWidget(statusTbEncoding, 0);


	statusLabelMode = new QLabel(status);
	statusLabelProcess = new QLabel(status);
	status->addPermanentWidget(statusLabelProcess, 0);
	status->addPermanentWidget(statusLabelMode, 0);
	for (int i = 1; i <= 3; i++) {
		QPushButton *pb = new QPushButton(getRealIcon(QString("bookmark%1").arg(i)), "", status);
		pb->setIconSize(iconSize);
		pb->setToolTip(tr("Go to bookmark") + QString(" %1").arg(i));
		connect(pb, SIGNAL(clicked()), getManagedAction(QString("main/edit/gotoBookmark/bookmark%1").arg(i)), SIGNAL(triggered()));
		pb->setFlat(true);
		status->addPermanentWidget(pb, 0);
	}
}

void Texstudio::updateCaption()
{
	if (!currentEditorView()) documents.currentDocument = 0;
	else {
		documents.currentDocument = currentEditorView()->document;
		documents.updateStructure();
		structureTreeView->setExpanded(documents.model->index(documents.currentDocument->baseStructure), true);
	}
	if (completer && completer->isVisible()) completer->close();
	QString title;
	if (!currentEditorView())	{
		title = TEXSTUDIO;
	} else {
		QString file = QDir::toNativeSeparators(getCurrentFileName());
		if (file.isEmpty())
			file = currentEditorView()->displayNameForUI();
		title = file + " - " + TEXSTUDIO;
		updateStatusBarEncoding();
		updateOpenDocumentMenu(true);
		newDocumentLineEnding();
	}
	setWindowTitle(title);
	//updateStructure();
	updateUndoRedoStatus();
	cursorPositionChanged();
	if (documents.singleMode()) {
		//outputView->resetMessagesAndLog();
		if (currentEditorView()) completerNeedsUpdate();
	}
	QString finame = getCurrentFileName();
	if (finame != "") configManager.lastDocument = finame;
}

void Texstudio::updateMasterDocumentCaption()
{
	if (documents.singleMode()) {
		actRootDocAutomatic->setChecked(true);
		actRootDocExplicit->setVisible(false);
		statusLabelMode->setText(QString(" %1 ").arg(tr("Automatic")));
		statusLabelMode->setToolTip(tr("Automatic root document detection active"));
	} else {
		QString shortName = documents.masterDocument->getFileInfo().fileName();
		actRootDocExplicit->setChecked(true);
		actRootDocExplicit->setVisible(true);
		actRootDocExplicit->setText(tr("&Explicit") + ": " + shortName);
		statusLabelMode->setText(QString(" %1 ").arg(tr("Root", "explicit root document") + ": " + shortName));
		statusLabelMode->setToolTip(QString(tr("Explict root document:\n%1")).arg(shortName));
	}
}

void Texstudio::currentEditorChanged()
{
	updateCaption();
	if (!currentEditorView()) return;
	if (configManager.watchedMenus.contains("main/view/documents"))
		updateToolBarMenu("main/view/documents");
	editorSpellerChanged(currentEditorView()->getSpeller());
	currentEditorView()->lastUsageTime = QDateTime::currentDateTime();
	currentEditorView()->checkRTLLTRLanguageSwitching();
}

/*!
 * \brief called when a editor tab is moved in position
 * \param from starting position
 * \param to ending position
 */
void Texstudio::editorTabMoved(int from, int to)
{
	//documents.aboutToUpdateLayout();
	documents.move(from, to);
	//documents.updateLayout();
}

void Texstudio::editorAboutToChangeByTabClick(LatexEditorView *edFrom, LatexEditorView *edTo)
{
	Q_UNUSED(edTo);
	saveEditorCursorToHistory(edFrom);
}

void Texstudio::showMarkTooltipForLogMessage(QList<int> errors)
{
	if (!currentEditorView()) return;
	REQUIRE(outputView->getLogWidget());
	REQUIRE(outputView->getLogWidget()->getLogModel());
	QString msg = outputView->getLogWidget()->getLogModel()->htmlErrorTable(errors);
	currentEditorView()->setLineMarkToolTip(msg);
}

void Texstudio::newDocumentLineEnding()
{
	if (!currentEditorView()) return;
	QDocument::LineEnding le = currentEditorView()->editor->document()->lineEnding();
	if (le == QDocument::Conservative) le = currentEditorView()->editor->document()->originalLineEnding();
	switch (le) {
#ifdef Q_OS_WIN32
	case QDocument::Local:
#endif
	case QDocument::Windows:
		getManagedAction("main/edit/lineend/crlf")->setChecked(true);
		break;
	case QDocument::Mac:
		getManagedAction("main/edit/lineend/cr")->setChecked(true);
		break;
	default:
		getManagedAction("main/edit/lineend/lf")->setChecked(true);
	}
}

void Texstudio::updateUndoRedoStatus()
{
	if (currentEditor()) {
		actSave->setEnabled(!currentEditor()->document()->isClean() || currentEditor()->fileName().isEmpty());
		bool canUndo = currentEditor()->document()->canUndo();
		if (!canUndo && configManager.svnUndo) {
			QVariant zw = currentEditor()->property("undoRevision");
			int undoRevision = zw.canConvert<int>() ? zw.toInt() : 0;
			if (undoRevision >= 0)
				canUndo = true;
		}
		actUndo->setEnabled(canUndo);
		actRedo->setEnabled(currentEditor()->document()->canRedo());
	} else {
		actSave->setEnabled(false);
		actUndo->setEnabled(false);
		actRedo->setEnabled(false);
	}
}
/*!
 * \brief return current editor
 *
 * return current editorview
 * \return current editor (LatexEditorView)
 */
LatexEditorView *Texstudio::currentEditorView() const
{
	return editors->currentEditor();
}

/*!
 * \brief return current editor
 *
 * return current editor
 * \return current editor (QEditor)
 */
QEditor *Texstudio::currentEditor() const
{
	LatexEditorView *edView = currentEditorView();
	if (!edView) return 0;
	return edView->editor;
}

void Texstudio::configureNewEditorView(LatexEditorView *edit)
{
	REQUIRE(m_languages);
	REQUIRE(edit->codeeditor);
	m_languages->setLanguage(edit->codeeditor->editor(), ".tex");

	//edit->setFormats(m_formats->id("environment"),m_formats->id("referenceMultiple"),m_formats->id("referencePresent"),m_formats->id("referenceMissing"));

	connect(edit->editor, SIGNAL(undoAvailable(bool)), this, SLOT(updateUndoRedoStatus()));
	connect(edit->editor, SIGNAL(requestClose()), &documents, SLOT(requestedClose()));
	connect(edit->editor, SIGNAL(redoAvailable(bool)), this, SLOT(updateUndoRedoStatus()));
	connect(edit->editor->document(), SIGNAL(lineEndingChanged(int)), this, SLOT(newDocumentLineEnding()));
	connect(edit->editor, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
	connect(edit->editor, SIGNAL(cursorHovered()), this, SLOT(cursorHovered()));
	connect(edit->editor, SIGNAL(emitWordDoubleClicked()), this, SLOT(cursorHovered()));
	connect(edit, SIGNAL(showMarkTooltipForLogMessage(QList<int>)), this, SLOT(showMarkTooltipForLogMessage(QList<int>)));
	connect(edit, SIGNAL(needCitation(const QString &)), this, SLOT(insertBibEntry(const QString &)));
	connect(edit, SIGNAL(showPreview(QString)), this, SLOT(showPreview(QString)));
	connect(edit, SIGNAL(showImgPreview(QString)), this, SLOT(showImgPreview(QString)));
	connect(edit, SIGNAL(showPreview(QDocumentCursor)), this, SLOT(showPreview(QDocumentCursor)));
	connect(edit, SIGNAL(gotoDefinition(QDocumentCursor)), this, SLOT(editGotoDefinition(QDocumentCursor)));
	connect(edit, SIGNAL(findLabelUsages(LatexDocument *, QString)), this, SLOT(findLabelUsages(LatexDocument *, QString)));
	connect(edit, SIGNAL(syncPDFRequested(QDocumentCursor)), this, SLOT(syncPDFViewer(QDocumentCursor)));
	connect(edit, SIGNAL(openFile(QString)), this, SLOT(openExternalFile(QString)));
	connect(edit, SIGNAL(openFile(QString, QString)), this, SLOT(openExternalFile(QString, QString)));
	connect(edit, SIGNAL(bookmarkRemoved(QDocumentLineHandle *)), bookmarks, SLOT(bookmarkDeleted(QDocumentLineHandle *)));
	connect(edit, SIGNAL(bookmarkAdded(QDocumentLineHandle *, int)), bookmarks, SLOT(bookmarkAdded(QDocumentLineHandle *, int)));
	connect(edit, SIGNAL(mouseBackPressed()), this, SLOT(goBack()));
	connect(edit, SIGNAL(mouseForwardPressed()), this, SLOT(goForward()));
	connect(edit, SIGNAL(cursorChangeByMouse()), this, SLOT(saveCurrentCursorToHistory()));
	connect(edit, SIGNAL(openCompleter()), this, SLOT(normalCompletion()));
	connect(edit, SIGNAL(openInternalDocViewer(QString, QString)), this, SLOT(openInternalDocViewer(QString, QString)));
	connect(edit, SIGNAL(showExtendedSearch()), this, SLOT(showExtendedSearch()));
	connect(edit, SIGNAL(execMacro(Macro, MacroExecContext)), this, SLOT(execMacro(Macro, MacroExecContext)));

	connect(edit->editor, SIGNAL(fileReloaded()), this, SLOT(fileReloaded()));
	connect(edit->editor, SIGNAL(fileInConflict()), this, SLOT(fileInConflict()));
	connect(edit->editor, SIGNAL(fileAutoReloading(QString)), this, SLOT(fileAutoReloading(QString)));

	if (Guardian::instance()) { // Guardian is not yet there when this is called at program startup
		connect(edit->editor, SIGNAL(slowOperationStarted()), Guardian::instance(), SLOT(slowOperationStarted()));
		connect(edit->editor, SIGNAL(slowOperationEnded()), Guardian::instance(), SLOT(slowOperationEnded()));
	}
	connect(edit, SIGNAL(linesChanged(QString, const void *, QList<LineInfo>, int)), grammarCheck, SLOT(check(QString, const void *, QList<LineInfo>, int)));

	connect(edit, SIGNAL(spellerChanged(QString)), this, SLOT(editorSpellerChanged(QString)));
	connect(edit->editor, SIGNAL(focusReceived()), edit, SIGNAL(focusReceived()));
	edit->setSpellerManager(&spellerManager);
	edit->setSpeller("<default>");

}

/*!
 * \brief complete the new editor view configuration (edit->document is set)
 * \param edit used editorview
 * \param reloadFromDoc
 * \param hidden if editor is not shown
 */
void Texstudio::configureNewEditorViewEnd(LatexEditorView *edit, bool reloadFromDoc, bool hidden)
{
	REQUIRE(edit->document);
	//patch Structure
	//disconnect(edit->editor->document(),SIGNAL(contentsChange(int, int))); // force order of contentsChange update
	connect(edit->editor->document(), SIGNAL(contentsChange(int, int)), edit->document, SLOT(patchStructure(int, int)));
	//connect(edit->editor->document(),SIGNAL(contentsChange(int, int)),edit,SLOT(documentContentChanged(int,int))); now directly called by patchStructure
	connect(edit->editor->document(), SIGNAL(lineRemoved(QDocumentLineHandle *)), edit->document, SLOT(patchStructureRemoval(QDocumentLineHandle *)));
	connect(edit->editor->document(), SIGNAL(lineDeleted(QDocumentLineHandle *)), edit->document, SLOT(patchStructureRemoval(QDocumentLineHandle *)));
	connect(edit->document, SIGNAL(updateCompleter()), this, SLOT(completerNeedsUpdate()));
	connect(edit->editor, SIGNAL(needUpdatedCompleter()), this, SLOT(needUpdatedCompleter()));
	connect(edit->document, SIGNAL(importPackage(QString)), this, SLOT(importPackage(QString)));
	connect(edit->document, SIGNAL(bookmarkLineUpdated(int)), bookmarks, SLOT(updateLineWithBookmark(int)));
	connect(edit->document, SIGNAL(encodingChanged()), this, SLOT(updateStatusBarEncoding()));
	connect(edit, SIGNAL(thesaurus(int, int)), this, SLOT(editThesaurus(int, int)));
	connect(edit, SIGNAL(changeDiff(QPoint)), this, SLOT(editChangeDiff(QPoint)));
	connect(edit, SIGNAL(saveCurrentCursorToHistoryRequested()), this, SLOT(saveCurrentCursorToHistory()));
	edit->document->saveLineSnapshot(); // best guess of the lines used during last latex compilation

	if (!hidden) {
		int index = reloadFromDoc ? documents.documents.indexOf(edit->document, 0) : -1; // index: we still assume here that the order of documents and editors is synchronized
		editors->insertEditor(edit, index);
		edit->editor->setFocus();
		updateCaption();
	}
}
/*!
 * \brief get editor which handles FileName
 *
 * get editor which handles FileName
 *
 * \param fileName
 * \param checkTemporaryNames
 * \return editorview, 0 if no editor matches
 */
LatexEditorView *Texstudio::getEditorViewFromFileName(const QString &fileName, bool checkTemporaryNames)
{
	LatexDocument *document = documents.findDocument(fileName, checkTemporaryNames);
	if (!document) return 0;
	return document->getEditorView();
}
/*!
 * \brief get filename of current editor
 *
 * get filename of current editor
 * \return filename
 */
QString Texstudio::getCurrentFileName()
{
	return documents.getCurrentFileName();
}

QString Texstudio::getAbsoluteFilePath(const QString &relName, const QString &extension)
{
	return documents.getAbsoluteFilePath(relName, extension);
}

QString Texstudio::getRelativeFileName(const QString &file, QString basepath, bool keepSuffix)
{
	return getRelativeBaseNameToPath(file, basepath, true, keepSuffix);
}

bool Texstudio::activateEditorForFile(QString f, bool checkTemporaryNames, bool setFocus)
{
	LatexEditorView *edView = getEditorViewFromFileName(f, checkTemporaryNames);
	if (!edView) return false;
	saveCurrentCursorToHistory();
	if (!editors->containsEditor(edView)) return false;
	editors->setCurrentEditor(edView);
	if (setFocus) {
		edView->editor->setFocus();
	}
	return true;
}

///////////////////FILE//////////////////////////////////////

LatexEditorView *Texstudio::editorViewForLabel(LatexDocument *doc, const QString &label)
{
	// doc can be any document, in which the label is valid
	REQUIRE_RET(doc, 0);
	QMultiHash<QDocumentLineHandle *, int> result = doc->getLabels(label);
	if (result.count() <= 0) return 0;
	QDocumentLine line(result.keys().first());
	LatexDocument *targetDoc = qobject_cast<LatexDocument *>(line.document());
	REQUIRE_RET(targetDoc, 0);
	return qobject_cast<LatexEditorView *>(targetDoc->getEditorView());
}

void guessLanguageFromContent(QLanguageFactory *m_languages, QEditor *e)
{
	QDocument *doc = e->document();
	if (doc->lineCount() == 0) return;
	if (doc->line(0).text().startsWith("<?xml") ||
	        doc->line(0).text().startsWith("<!DOCTYPE"))
		m_languages->setLanguage(e, ".xml");
}
/*!
 * \brief load file
 *
 * load file from disc
 * \param f filename
 * \param asProject load file as master-file
 * \param hidden hide editor
 * \param recheck
 * \param dontAsk
 * \return
 */
LatexEditorView *Texstudio::load(const QString &f , bool asProject, bool hidden, bool recheck, bool dontAsk)
{
	QString f_real = f;
#ifdef Q_OS_WIN32
	QRegExp regcheck("/([a-zA-Z]:[/\\\\].*)");
	if (regcheck.exactMatch(f)) f_real = regcheck.cap(1);
#endif

#ifndef NO_POPPLER_PREVIEW
	if (f_real.endsWith(".pdf", Qt::CaseInsensitive)) {
		if (PDFDocument::documentList().isEmpty())
			newPdfPreviewer();
		PDFDocument::documentList().first()->loadFile(f_real, QString(f_real).replace(".pdf", ".tex"));
		PDFDocument::documentList().first()->show();
		PDFDocument::documentList().first()->setFocus();
		return 0;
	}
	if ((f_real.endsWith(".synctex.gz", Qt::CaseInsensitive) ||
	        f_real.endsWith(".synctex", Qt::CaseInsensitive))
	        && txsConfirm(tr("Do you want to debug a SyncTeX file?"))) {
		fileNewInternal();
		currentEditor()->document()->setText(PDFDocument::debugSyncTeX(f_real), false);
		return currentEditorView();
	}
#endif

	if (f_real.endsWith(".log", Qt::CaseInsensitive) &&
	        txsConfirm(QString("Do you want to load file %1 as LaTeX log file?").arg(QFileInfo(f).completeBaseName()))) {
		outputView->getLogWidget()->loadLogFile(f, documents.getTemporaryCompileFileName(), QTextCodec::codecForName(configManager.logFileEncoding.toLatin1()));
		setLogMarksVisible(true);
		return 0;
	}

	if (!hidden)
		raise();

	//test is already opened
	LatexEditorView *existingView = getEditorViewFromFileName(f_real);
	LatexDocument *doc;
	if (!existingView) {
		doc = documents.findDocumentFromName(f_real);
		if (doc) existingView = doc->getEditorView();
	}
	if (existingView) {
		if (hidden)
			return existingView;
		if (asProject) documents.setMasterDocument(existingView->document);
		if (existingView->document->isHidden()) {
			existingView->editor->setLineWrapping(configManager.editorConfig->wordwrap > 0);
			documents.deleteDocument(existingView->document, true);
			existingView->editor->setSilentReloadOnExternalChanges(existingView->document->remeberAutoReload);
			existingView->editor->setHidden(false);
			documents.addDocument(existingView->document, false);
			editors->addEditor(existingView);
			updateStructure(false, existingView->document, true);
			existingView->editor->setFocus();
			updateCaption();
			return existingView;
		}
		editors->setCurrentEditor(existingView);
		return existingView;
	}

	// find closed master doc
	if (doc) {
		LatexEditorView *edit = new LatexEditorView(0, configManager.editorConfig, doc);
		edit->setLatexPackageList(&latexPackageList);
		edit->document = doc;
		edit->editor->setFileName(doc->getFileName());
		disconnect(edit->editor->document(), SIGNAL(contentsChange(int, int)), edit->document, SLOT(patchStructure(int, int)));
		configureNewEditorView(edit);
		if (edit->editor->fileInfo().suffix().toLower() != "tex")
			m_languages->setLanguage(edit->editor, f_real);
		if (!edit->editor->languageDefinition())
			guessLanguageFromContent(m_languages, edit->editor);

		doc->setLineEnding(edit->editor->document()->originalLineEnding());
		doc->setEditorView(edit); //update file name (if document didn't exist)

		configureNewEditorViewEnd(edit, !hidden, hidden);
		//edit->document->initStructure();
		//updateStructure(true);
		if (!hidden) {
			showStructure();
			bookmarks->restoreBookmarks(edit);
		}
		return edit;
	}

	//load it otherwise
	if (!QFile::exists(f_real)) return 0;
	QFile file(f_real);
	if (!file.open(QIODevice::ReadOnly)) {
		if (!hidden && !dontAsk)
			QMessageBox::warning(this, tr("Error"), tr("You do not have read permission to the file %1.").arg(f_real));
		return 0;
	}
	file.close();

	bool bibTeXmodified = documents.bibTeXFilesModified;

	doc = new LatexDocument(this);
	doc->enableSyntaxCheck(configManager.editorConfig->inlineSyntaxChecking);
	LatexEditorView *edit = new LatexEditorView(0, configManager.editorConfig, doc);
	edit->setLatexPackageList(&latexPackageList);
	if (hidden) {
		edit->editor->setLineWrapping(false); //disable linewrapping in hidden docs to speed-up updates
		doc->clearWidthConstraint();
	}
	configureNewEditorView(edit);

	edit->document = documents.findDocument(f_real);
	if (!edit->document) {
		edit->document = doc;
		edit->document->setEditorView(edit);
		documents.addDocument(edit->document, hidden);
	} else edit->document->setEditorView(edit);

	if (configManager.recentFileHighlightLanguage.contains(f_real))
		m_languages->setLanguage(edit->editor, configManager.recentFileHighlightLanguage.value(f_real));
	else if (edit->editor->fileInfo().suffix().toLower() != "tex")
		m_languages->setLanguage(edit->editor, f_real);

	//QTime time;
	//time.start();
	edit->editor->load(f_real, QDocument::defaultCodec());

	if (!edit->editor->languageDefinition())
		guessLanguageFromContent(m_languages, edit->editor);


	//qDebug() << "Load time: " << time.elapsed();
	edit->editor->document()->setLineEndingDirect(edit->editor->document()->originalLineEnding());

	edit->document->setEditorView(edit); //update file name (if document didn't exist)

	configureNewEditorViewEnd(edit, asProject, hidden);

	//check for svn conflict
	if (!hidden) {
		checkSVNConflicted();

		MarkCurrentFileAsRecent();
	}

	documents.updateMasterSlaveRelations(doc, recheck);

	if (recheck) {
		doc->updateLtxCommands();
	}

	if (!hidden) {
		if (QFile::exists(f_real + ".recover.bak~")
		        && QFileInfo(f_real + ".recover.bak~").lastModified() > QFileInfo(f_real).lastModified()) {
			if (txsConfirm(tr("A crash recover file from %1 has been found for \"%2\".\nDo you want to restore it?").arg(QFileInfo(f_real + ".recover.bak~").lastModified().toString()).arg(f_real))) {
				QFile f(f_real + ".recover.bak~");
				if (f.open(QFile::ReadOnly)) {
					QByteArray ba = f.readAll();
					QString recovered = QTextCodec::codecForName("UTF-8")->toUnicode(ba); //TODO: chunk loading?
					edit->document->setText(recovered, true);
				} else txsWarning(tr("Failed to open recover file \"%1\".").arg(f_real + ".recover.bak~"));
			}
		}

	}

	updateStructure(true, doc, true);

	if (!hidden)
		showStructure();
	bookmarks->restoreBookmarks(edit);

	if (asProject) documents.setMasterDocument(edit->document);

	if (outputView->getLogWidget()->logPresent()) {
		updateLogEntriesInEditors();
		setLogMarksVisible(true);
	}
	if (!bibTeXmodified)
		documents.bibTeXFilesModified = false; //loading a file can change the list of included bib files, but we won't consider that as a modification of them, because then they don't have to be recompiled
	LatexDocument *rootDoc = edit->document->getRootDocument();
	if (rootDoc) foreach (const FileNamePair &fnp, edit->document->mentionedBibTeXFiles().values()) {
			Q_ASSERT(!fnp.absolute.isEmpty());
			rootDoc->lastCompiledBibTeXFiles.insert(fnp.absolute);
		}

#ifndef Q_OS_MAC
	if (!hidden) {
		if (windowState() == Qt::WindowMinimized || !isVisible() || !QApplication::activeWindow()) {
			show();
			if (windowState() == Qt::WindowMinimized)
				setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
			show();
			raise();
			QApplication::setActiveWindow(this);
			activateWindow();
			setFocus();
			edit->editor->setFocus();
		}
	}
	//raise();
	//#ifdef Q_OS_WIN32
	//        if (IsIconic (this->winId())) ShowWindow(this->winId(), SW_RESTORE);
	//#endif
#endif

	runScriptsInList(Macro::ST_LOAD_THIS_FILE, doc->localMacros);

	emit infoLoadFile(f_real);

	return edit;
}

void Texstudio::completerNeedsUpdate()
{
	mCompleterNeedsUpdate = true;
}

void Texstudio::needUpdatedCompleter()
{
	if (mCompleterNeedsUpdate)
		updateCompleter();
}

void Texstudio::updateUserToolMenu()
{
	CommandMapping cmds = buildManager.getAllCommands();
	QStringList order = buildManager.getCommandsOrder();
	QStringList ids;
	QStringList displayName;
	for (int i = 0; i < order.size(); i++) {
		const CommandInfo &ci = cmds.value(order[i]);
		if (!ci.user) continue;
		ids << ci.id;
		displayName << ci.displayName;
	}
	configManager.updateListMenu("main/tools/user", displayName, "cmd", true, SLOT(commandFromAction()), Qt::ALT + Qt::SHIFT + Qt::Key_F1, false, 0);
	QMenu *m = getManagedMenu("main/tools/user");
	REQUIRE(m);
	QList<QAction *> actions = m->actions();
	for (int i = 0; i < actions.size(); i++)
		actions[i]->setData(BuildManager::TXS_CMD_PREFIX + ids[i]);
}

#include "QMetaMethod"
void Texstudio::linkToEditorSlot(QAction *act, const char *methodName, const QList<QVariant> &args)
{
	REQUIRE(act);
	connectUnique(act, SIGNAL(triggered()), this, SLOT(relayToEditorSlot()));
	act->setProperty("primarySlot", QString(SLOT(relayToEditorSlot())));
	QByteArray signature = createMethodSignature(methodName, args);
	if (!args.isEmpty())
		act->setProperty("args", QVariant::fromValue<QList<QVariant> >(args));
	for (int i = 0; i < LatexEditorView::staticMetaObject.methodCount(); i++)
#if QT_VERSION>=0x050000
		if (signature == LatexEditorView::staticMetaObject.method(i).methodSignature()) {
			act->setProperty("editorViewSlot", methodName);
			return;
		} //else qDebug() << LatexEditorView::staticMetaObject.method(i).signature();
	for (int i = 0; i < QEditor::staticMetaObject.methodCount(); i++)
		if (signature == QEditor::staticMetaObject.method(i).methodSignature()) {
			act->setProperty("editorSlot", methodName);
			return;
		}
#else
		if (signature == LatexEditorView::staticMetaObject.method(i).signature()) {
			act->setProperty("editorViewSlot", methodName);
			return;
		} //else qDebug() << LatexEditorView::staticMetaObject.method(i).signature();
	for (int i = 0; i < QEditor::staticMetaObject.methodCount(); i++)
		if (signature == QEditor::staticMetaObject.method(i).signature()) {
			act->setProperty("editorSlot", methodName);
			return;
		}
#endif

	qDebug() << methodName << signature;
	Q_ASSERT(false);
}

void Texstudio::relayToEditorSlot()
{
	if (!currentEditorView()) return;
	QAction *act = qobject_cast<QAction *>(sender());
	REQUIRE(act);
	if (act->property("editorViewSlot").isValid()) QMetaObjectInvokeMethod(currentEditorView(), qPrintable(act->property("editorViewSlot").toString()), act->property("args").value<QList<QVariant> >());
	else if (act->property("editorSlot").isValid()) QMetaObjectInvokeMethod(currentEditor(), qPrintable(act->property("editorSlot").toString()), act->property("args").value<QList<QVariant> >());
}

void Texstudio::relayToOwnSlot()
{
	QAction *act = qobject_cast<QAction *>(sender());
	REQUIRE(act && act->property("slot").isValid());
	QMetaObjectInvokeMethod(this, qPrintable(act->property("slot").toString()), act->property("args").value<QList<QVariant> >());
}

void Texstudio::autoRunScripts()
{
	QStringList vers = QString(QT_VERSION_STR).split('.');
	Q_ASSERT(vers.length() >= 2);
	int major = vers.at(0).toInt();
	int minor = vers.at(1).toInt();
	if (!hasAtLeastQt(major, minor))
		txsWarning(tr("%1 has been compiled with Qt %2, but is running with Qt %3.\nPlease get the correct runtime library (e.g. .dll or .so files).\nOtherwise there might be random errors and crashes.")
		           .arg(TEXSTUDIO).arg(QT_VERSION_STR).arg(qVersion()));
	runScripts(Macro::ST_TXS_START);
}

void Texstudio::runScripts(int trigger)
{
	runScriptsInList(trigger, configManager.completerConfig->userMacros);
}

void Texstudio::runScriptsInList(int trigger, const QList<Macro> &scripts)
{
	foreach (const Macro &macro, scripts) {
		if (macro.type == Macro::Script && macro.isActiveForTrigger((Macro::SpecialTrigger) trigger))
			runScript(macro.script(), MacroExecContext(trigger));
	}
}

void Texstudio::fileNewInternal(QString fileName)
{
	LatexDocument *doc = new LatexDocument(this);
	doc->enableSyntaxCheck(configManager.editorConfig->inlineSyntaxChecking);
	LatexEditorView *edit = new LatexEditorView (0, configManager.editorConfig, doc);
	edit->setLatexPackageList(&latexPackageList);
	if (configManager.newFileEncoding)
		edit->editor->setFileCodec(configManager.newFileEncoding);
	else
		edit->editor->setFileCodec(QTextCodec::codecForName("utf-8"));
	doc->clearUndo(); // inital file codec setting should not be undoable
	edit->editor->setFileName(fileName);

	configureNewEditorView(edit);

	edit->document = doc;
	edit->document->setEditorView(edit);
	documents.addDocument(edit->document);

	configureNewEditorViewEnd(edit);
	doc->updateLtxCommands();
	if (!fileName.isEmpty())
		fileSave(true);
}

void Texstudio::fileNew(QString fileName)
{
	fileNewInternal(fileName);
	emit infoNewFile();
}

void Texstudio::fileAutoReloading(QString fname)
{
	LatexDocument *document = documents.findDocument(fname);
	if (!document) return;
	document->initClearStructure();
}
/* \brief called when file has been reloaded from disc
 */
void Texstudio::fileReloaded()
{
	QEditor *mEditor = qobject_cast<QEditor *>(sender());
	if (mEditor == currentEditor()) {
		currentEditorView()->document->initClearStructure();
		updateStructure(true);
	} else {
		LatexDocument *document = documents.findDocument(mEditor->fileName());
		if (!document) return;
		document->initClearStructure();
		document->patchStructure(0, -1);
	}
}
/*!
 * \brief make a template from current editor
 */
void Texstudio::fileMakeTemplate()
{
	if (!currentEditorView())
		return;

	MakeTemplateDialog templateDialog(TemplateManager::userTemplateDir(), currentEditor()->fileName());
	if (templateDialog.exec()) {
		// save file
		QString fn = templateDialog.suggestedFile();
		LatexDocument *doc = currentEditorView()->document;
		QString txt = doc->text();
		//txt.replace("%","%%"); not necessary any more
		QFile file_txt(fn);
		if (!file_txt.open(QIODevice::WriteOnly | QIODevice::Text)) {
			txsInformation(tr("Could not write template data:") + "\n" + fn);
			return;
		} else {
			QTextStream out(&file_txt);
			out.setCodec("UTF-8");
			out << txt;
			file_txt.close();
		}

		/* alternate code in order to double %

		QString old_name=currentEditor()->fileName();
		QTextCodec *mCodec=currentEditor()->getFileCodec();
		currentEditor()->setFileCodec(QTextCodec::codecForName("utf-8"));
		currentEditor()->save(fn);
		currentEditor()->setFileName(old_name);
		currentEditor()->setFileCodec(mCodec);
		*/

		// save metaData
		QFileInfo fi(fn);
		fn = fi.absoluteFilePath();
		fn.remove(fn.lastIndexOf('.'), 4);
		fn.append(".json");
		QFile file(fn);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			txsInformation(tr("Could not write template meta data:") + "\n" + fn);
		} else {
			QTextStream out(&file);
			out.setCodec("UTF-8");
			out << templateDialog.generateMetaData();
			file.close();
		}
	}
}
/*!
 * \brief load template file for editing
 * \param fname filename
 */
void Texstudio::templateEdit(const QString &fname)
{
	load(fname, false);
}
/*!
 * \brief generate new file from template
 */
void Texstudio::fileNewFromTemplate()
{
	TemplateManager tmplMgr;
	connectUnique(&tmplMgr, SIGNAL(editRequested(QString)), this, SLOT(templateEdit(QString)));

	TemplateSelector *dialog = tmplMgr.createLatexTemplateDialog();
	if (!dialog->exec()) return;

	TemplateHandle th = dialog->selectedTemplate();
	if (!th.isValid()) return;

	if (dialog->createInFolder()) {
		th.createInFolder(dialog->creationFolder());
		if (th.isMultifile()) {
			QDir dir(dialog->creationFolder());
			foreach (const QString &f, th.filesToOpen()) {
				QFileInfo fi(dir, f);
				if (fi.exists() && fi.isFile())
					load(fi.absoluteFilePath());
			}
		} else {
			QDir dir(dialog->creationFolder());
			QFileInfo fi(dir, QFileInfo(th.file()).fileName());
			load(fi.absoluteFilePath());
		}
	} else {
		QString fname = th.file();
		QFile file(fname);
		if (!file.exists()) {
			txsWarning(tr("File not found:") + QString("\n%1").arg(fname));
			return;
		}
		if (!file.open(QIODevice::ReadOnly)) {
			txsWarning(tr("You do not have read permission to this file:") + QString("\n%1").arg(fname));
			return;
		}

		//set up new editor with template
		fileNewInternal();
		LatexEditorView *edit = currentEditorView();

		QString mTemplate;
		bool loadAsSnippet = false;
		QTextStream in(&file);
		in.setCodec(QTextCodec::codecForName("UTF-8"));
		QString line = in.readLine();
		if (line.contains(QRegExp("^%\\s*!TXS\\s+template"))) {
			loadAsSnippet = true;
		} else {
			mTemplate += line + '\n';
		}
		while (!in.atEnd()) {
			line = in.readLine();
			mTemplate += line + "\n";
		}
		if (loadAsSnippet) {
			bool flag = edit->editor->flag(QEditor::AutoIndent);
			edit->editor->setFlag(QEditor::AutoIndent, false);
			CodeSnippet toInsert(mTemplate, false);
			toInsert.insert(edit->editor);
			edit->editor->setFlag(QEditor::AutoIndent, flag);
			edit->editor->setCursorPosition(0, 0, false);
			edit->editor->nextPlaceHolder();
			edit->editor->ensureCursorVisible(QEditor::KeepSurrounding);
		} else {
			edit->editor->setText(mTemplate, false);
		}

		emit infoNewFromTemplate();
	}
	delete dialog;
}
/*!
 * \brief insert table template
 */
void Texstudio::insertTableTemplate()
{
	QEditor *m_edit = currentEditor();
	if (!m_edit)
		return;
	QDocumentCursor c = m_edit->cursor();
	if (!LatexTables::inTableEnv(c))
		return;

	// select Template
	TemplateManager tmplMgr;
	connectUnique(&tmplMgr, SIGNAL(editRequested(QString)), this, SLOT(templateEdit(QString)));
	if (tmplMgr.tableTemplateDialogExec()) {
		QString fname = tmplMgr.selectedTemplateFile();
		QFile file(fname);
		if (!file.exists()) {
			txsWarning(tr("File not found:") + QString("\n%1").arg(fname));
			return;
		}
		if (!file.open(QIODevice::ReadOnly)) {
			txsWarning(tr("You do not have read permission to this file:") + QString("\n%1").arg(fname));
			return;
		}
		QString tableDef = LatexTables::getSimplifiedDef(c);
		QString tableText = LatexTables::getTableText(c);
		QString widthDef;
		//remove table
		c.removeSelectedText();
		m_edit->setCursor(c);
		// split table text into line/column list
		QStringList values;
		QList<int> starts;
		QString env;
		//tableText.remove("\n");
		tableText.remove("\\hline");
		if (tableText.startsWith("\\begin")) {
			LatexParser::resolveCommandOptions(tableText, 0, values, &starts);
			env = values.takeFirst();
			env.remove(0, 1);
			env.remove(env.length() - 1, 1);
			// special treatment for tabu/longtabu
			if ((env == "tabu" || env == "longtabu") && values.count() < 1) {
				int startExtra = env.length() + 8;
				int endExtra = tableText.indexOf("{", startExtra);
				if (endExtra >= 0 && endExtra > startExtra) {
					QString textHelper = tableText;
					widthDef = textHelper.mid(startExtra, endExtra - startExtra);
					textHelper.remove(startExtra, endExtra - startExtra); // remove to/spread definition
					values.clear();
					starts.clear();
					LatexParser::resolveCommandOptions(textHelper, 0, values, &starts);
					for (int i = 1; i < starts.count(); i++) {
						starts[i] += endExtra - startExtra;
					}
				}
				if (!values.isEmpty())
					values.takeFirst();
			}
			if (LatexTables::tabularNames.contains(env)) {
				if (!values.isEmpty()) {
					int i = starts.at(1);
					i += values.first().length();
					tableText.remove(0, i);
				}
			}
			if (LatexTables::tabularNamesWithOneOption.contains(env)) {
				if (values.size() > 1) {
					int i = starts.at(2);
					i += values.at(1).length();
					tableText.remove(0, i);
				}
			}
			tableText.remove(QRegExp("\\\\end\\{" + env + "\\}$"));
		}
		tableText.replace("\\endhead", "\\\\");
		QStringList lines = tableText.split("\\\\");
		QList<QStringList> tableContent;
		foreach (QString line, lines) {
			//line=line.simplified();
			if (line.simplified().isEmpty())
				continue;
			QStringList elems = line.split(QRegExp("&"));
			if (elems.count() > 0) {
				if (elems[0].startsWith("\n"))
					elems[0] = elems[0].mid(1);
			}
			//QList<QString>::iterator i;
			/*for(i=elems.begin();i!=elems.end();i++){
				QString elem=*i;
			    *i=elem.simplified();
			}*/

			// handle \& correctly
			for (int i = elems.size() - 1; i >= 0; --i) {
				if (elems.at(i).endsWith("\\")) {
					QString add = elems.at(i) + elems.at(i + 1);
					elems.replace(i, add);
					elems.removeAt(i + 1);
				}
			}
			tableContent << elems;
		}
		LatexTables::generateTableFromTemplate(currentEditorView(), fname, tableDef, tableContent, env, widthDef);
	}
}
/*! \brief align columns of latex table in editor
 */
void Texstudio::alignTableCols()
{
	if (!currentEditor()) return;
	QDocumentCursor cur(currentEditor()->cursor());
	int linenr = cur.lineNumber();
	int col = cur.columnNumber();
	if (!cur.isValid())
		return;
	LatexTables::alignTableCols(cur);
	cur.setLineNumber(linenr);
	cur.setColumnNumber(col);
	currentEditor()->setCursor(cur);
}
/*! \brief open file
 *
 * open file is triggered from menu action.
 * It opens a file dialog and lets the user to select a file.
 * If the file is already open, the apropriate editor subwindow is brought to front.
 * If the file is open as hidden, an editor is created and brought to front.
 * pdf files are handled as well and they are forwarded to the pdf viewer.
 */
void Texstudio::fileOpen()
{
	QString currentDir = QDir::homePath();
	if (!configManager.lastDocument.isEmpty()) {
		QFileInfo fi(configManager.lastDocument);
		if (fi.exists() && fi.isReadable()) {
			currentDir = fi.absolutePath();
		}
	}
	QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Files"), currentDir, fileFilters,  &selectedFileFilter);

	recheckLabels = false; // impede label rechecking on hidden docs
	QList<LatexEditorView *>listViews;
	foreach (const QString &fn, files)
		listViews << load(fn);

	// update ref/labels in one go;
	QList<LatexDocument *> completedDocs;
	foreach (LatexEditorView *edView, listViews) {
		if (!edView)
			continue;
		LatexDocument *docBase = edView->getDocument();
		foreach (LatexDocument *doc, docBase->getListOfDocs()) {
			doc->recheckRefsLabels();
			if (completedDocs.contains(doc))
				continue;
			doc->updateLtxCommands(true);
			completedDocs << doc->getListOfDocs();
		}
	}
	recheckLabels = true;
	// update completer
	if (currentEditorView())
		updateCompleter(currentEditorView());
}

void Texstudio::fileRestoreSession(bool showProgress, bool warnMissing)
{

	QFileInfo f(QDir(configManager.configBaseDir), "lastSession.txss");
	Session s;
	if (f.exists()) {
		if (!s.load(f.filePath())) {
			txsCritical(tr("Loading of last session failed."));
		}
	}
	restoreSession(s, showProgress, warnMissing);
}
/*!
 * \brief save current editor content
 *
 * \param saveSilently
 */
void Texstudio::fileSave(const bool saveSilently)
{
	if (!currentEditor())
		return;

	if (currentEditor()->fileName() == "" || !QFileInfo(currentEditor()->fileName()).exists()) {
		removeDiffMarkers();// clean document from diff markers first
		fileSaveAs(currentEditor()->fileName(), saveSilently);
	} else {
		/*QFile file( *filenames.find( currentEditorView() ) );
		if ( !file.open( QIODevice::WriteOnly ) )
		{
		QMessageBox::warning( this,tr("Error"),tr("The file could not be saved. Please check if you have write permission."));
		return;
		}*/
		removeDiffMarkers();// clean document from diff markers first
		currentEditor()->save();
		currentEditor()->document()->markViewDirty();//force repaint of line markers (yellow -> green)
		MarkCurrentFileAsRecent();
		int checkIn = (configManager.autoCheckinAfterSaveLevel > 0 && !saveSilently) ? 2 : 1;
		emit infoFileSaved(currentEditor()->fileName(), checkIn);
	}
	updateCaption();
	//updateStructure(); (not needed anymore for autoupdate)
}
/*!
 * \brief save current editor content to new filename
 *
 * save current editor content to new filename
 * \param fileName
 * \param saveSilently don't ask for new filename if fileName is empty
 */
void Texstudio::fileSaveAs(const QString &fileName, const bool saveSilently)
{
	if (!currentEditorView())
		return;

	// select a directory/filepath
	QString currentDir = QDir::homePath();
	if (fileName.isEmpty()) {
		if (currentEditor()->fileInfo().isFile()) {
			currentDir = currentEditor()->fileInfo().absoluteFilePath();
		} else {
			if (!configManager.lastDocument.isEmpty())
				currentDir = configManager.lastDocument;
			static QString saveAsDefault;
			configManager.registerOption("Files/Save As Default", &saveAsDefault, "?a)/document");
			currentDir = buildManager.parseExtendedCommandLine(saveAsDefault, currentDir, currentDir).value(0, currentDir);
		}
	} else {
		currentDir = fileName;
		/*QFileInfo currentFile(fileName);
		if (currentFile.absoluteDir().exists())
		currentDir = fileName;*/
	}

	// get a file name
	QString fn = fileName;
	if (!saveSilently || fn.isEmpty()) {
		fn = QFileDialog::getSaveFileName(this, tr("Save As"), currentDir, fileFilters, &selectedFileFilter);
		if (!fn.isEmpty()) {
			static QRegExp fileExt("\\*(\\.[^ )]+)");
			if (fileExt.indexIn(selectedFileFilter) > -1) {
				//add
				int lastsep = qMax(fn.lastIndexOf("/"), fn.lastIndexOf("\\"));
				int lastpoint = fn.lastIndexOf(".");
				if (lastpoint <= lastsep) //if both aren't found or point is in directory name
					fn.append(fileExt.cap(1));
			}
		}
	}
	if (!fn.isEmpty()) {
		if (getEditorViewFromFileName(fn) && getEditorViewFromFileName(fn) != currentEditorView()) {
			// trying to save with same name as another already existing file
			LatexEditorView *otherEdView = getEditorViewFromFileName(fn);
			if (!otherEdView->document->isClean()) {
				txsWarning(tr("Saving under the name\n"
				              "%1\n"
				              "is currently not possible because a modified version of a file\n"
				              "with this name is open in TeXstudio. You have to save or close\n"
				              "this other file before you can overwrite it.").arg(fn));
				return;
			}
			// other editor does not contain unsaved changes, so it can be closed
			LatexEditorView *currentEdView = currentEditorView();
			editors->setCurrentEditor(otherEdView);
			fileClose();
			editors->setCurrentEditor(currentEdView);
		}
#ifndef NO_POPPLER_PREVIEW
		// show message in viewer
		if (currentEditor()->fileInfo() != QFileInfo(fn)) {
			foreach (PDFDocument *viewer, PDFDocument::documentList())
				if (viewer->getMasterFile() == currentEditor()->fileInfo())
					viewer->showMessage(tr("This pdf cannot be synchronized with the tex source any more because the source file has been renamed due to a Save As operation. You should recompile the renamed file and view its result."));
		}
#endif
		// save file
		removeDiffMarkers();// clean document from diff markers first
		currentEditor()->save(fn);
		currentEditorView()->document->setEditorView(currentEditorView()); //update file name
		MarkCurrentFileAsRecent();

		//update Master/Child relations
		LatexDocument *doc = currentEditorView()->document;
		documents.updateMasterSlaveRelations(doc);

		updateOpenDocumentMenu(true);  // TODO: currently duplicate functionality with updateCaption() below
		if (currentEditor()->fileInfo().suffix().toLower() != "tex")
			m_languages->setLanguage(currentEditor(), fn);

		emit infoFileSaved(currentEditor()->fileName());
	}

	updateCaption();
}
/*!
 * \brief save all files
 *
 * This functions is called from menu-action.
 */
void Texstudio::fileSaveAll()
{
	fileSaveAll(true, true);
}
/*!
 * \brief save all files
 *
 * \param alsoUnnamedFiles
 * \param alwaysCurrentFile
 */
void Texstudio::fileSaveAll(bool alsoUnnamedFiles, bool alwaysCurrentFile)
{
	//LatexEditorView *temp = new LatexEditorView(EditorView,colorMath,colorCommand,colorKeyword);
	//temp=currentEditorView();
	REQUIRE(editors);

	LatexEditorView *currentEdView = currentEditorView();

	foreach (LatexEditorView *edView, editors->editors()) {
		REQUIRE(edView->editor);

		if (edView->editor->fileName().isEmpty()) {
			if ((alsoUnnamedFiles || (alwaysCurrentFile && edView == currentEdView) ) ) {
				editors->setCurrentEditor(edView);
				fileSaveAs();
			} else if (!edView->document->getTemporaryFileName().isEmpty())
				edView->editor->saveCopy(edView->document->getTemporaryFileName());
			//else if (edView->editor->isInConflict()) {
			//edView->editor->save();
			//}
		} else if (edView->editor->isContentModified() || edView->editor->isInConflict()) {
			removeDiffMarkers();// clean document from diff markers first
			edView->editor->save(); //only save modified documents

			if (edView->editor->fileName().endsWith(".bib")) {
				QString temp = edView->editor->fileName();
				temp = temp.replace(QDir::separator(), "/");
				documents.bibTeXFilesModified = documents.bibTeXFilesModified  || documents.mentionedBibTeXFiles.contains(temp);//call bibtex on next compilation (this would also set as soon as the user switch to a tex file, but he could compile before switching)
			}

			emit infoFileSaved(edView->editor->fileName());
		}
	}

	if (currentEditorView() != currentEdView)
		editors->setCurrentEditor(currentEdView);
	documents.updateStructure(); //remove italics status from previous unsaved files
	updateUndoRedoStatus();
}

//TODO: handle svn in all these methods

void Texstudio::fileUtilCopyMove(bool move)
{
	QString fn = documents.getCurrentFileName();
	if (fn.isEmpty()) return;
	QString newfn = QFileDialog::getSaveFileName(this, move ? tr("Rename/Move") : tr("Copy"), fn, fileFilters, &selectedFileFilter);
	if (newfn.isEmpty()) return;
	if (fn == newfn) return;
	QFile::Permissions permissions = QFile(fn).permissions();
	if (move) fileSaveAs(newfn, true);
	else currentEditor()->saveCopy(newfn);
	if (documents.getCurrentFileName() != newfn) return;
	if (move) QFile(fn).remove();
	QFile(newfn).setPermissions(permissions); //keep permissions. (better: actually move the file, keeping the inode. but then all that stuff (e.g. master/slave) has to be updated here
}

void Texstudio::fileUtilDelete()
{
	QString fn = documents.getCurrentFileName();
	if (fn.isEmpty()) return;
	if (txsConfirmWarning(tr("Do you really want to delete the file \"%1\"?").arg(fn)))
		QFile(fn).remove();
}

void Texstudio::fileUtilRevert()
{
	if (!currentEditor()) return;
	QString fn = documents.getCurrentFileName();
	if (fn.isEmpty()) return;
	if (txsConfirmWarning(tr("Do you really want to revert the file \"%1\"?").arg(documents.getCurrentFileName())))
		currentEditor()->reload();
}

void Texstudio::fileUtilPermissions()
{
	QString fn = documents.getCurrentFileName();
	if (fn.isEmpty()) return;

	QFile f(fn);

	int permissionsRaw = f.permissions();
	int permissionsUnixLike = ((permissionsRaw & 0x7000) >> 4) | (permissionsRaw & 0x0077); //ignore qt "user" flags
	QString permissionsUnixLikeHex = QString::number(permissionsUnixLike, 16);
	QString permissions;
	const QString PERMISSIONSCODES = "rwx";
	int flag = QFile::ReadUser;
	REQUIRE(QFile::ReadUser == 0x400);
	int temp = permissionsUnixLike;
	for (int b = 0; b < 3; b++) {
		for (int i = 0; i < 3; i++, flag >>= 1)
			permissions += (temp & flag) ? PERMISSIONSCODES[i] : QChar('-');
		flag >>= 1;
	}
	QString oldPermissionsUnixLikeHex = permissionsUnixLikeHex, oldPermissions = permissions;

	UniversalInputDialog uid;
	uid.addVariable(&permissionsUnixLikeHex, tr("Numeric permissions"));
	uid.addVariable(&permissions, tr("Verbose permissions"));
	if (uid.exec() == QDialog::Accepted && (permissionsUnixLikeHex != oldPermissionsUnixLikeHex || permissions != oldPermissions)) {
		if (permissionsUnixLikeHex != oldPermissionsUnixLikeHex)
			permissionsRaw = permissionsUnixLikeHex.toInt(0, 16);
		else {
			permissionsRaw = 0;
			int flag = QFile::ReadUser;
			int p = 0;
			for (int b = 0; b < 3; b++) {
				for (int i = 0; i < 3; i++, flag >>= 1) {
					if (permissions[p] == '-') p++;
					else if (permissions[p] == PERMISSIONSCODES[i]) {
						permissionsRaw |= flag;
						p++;
					} else if (!QString("rwx").contains(permissions[p])) {
						txsWarning("invalid character in permission: " + permissions[p]);
						return;
					}
					if (p >= permissions.length()) p = 0; //wrap around
				}
				flag >>= 1;
			}
		}
		permissionsRaw = ((permissionsRaw << 4) & 0x7000) | permissionsRaw; //use qt "user" as owner flags
		f.setPermissions(QFile::Permissions(permissionsRaw)) ;
	}
}

void Texstudio::fileUtilCopyFileName()
{
	QApplication::clipboard()->setText(documents.getCurrentFileName());
}

void Texstudio::fileUtilCopyMasterFileName()
{
	QApplication::clipboard()->setText(documents.getCompileFileName());
}

void Texstudio::fileClose()
{
	if (!currentEditorView())	return;
	bookmarks->updateBookmarks(currentEditorView());
	QFileInfo fi = currentEditorView()->document->getFileInfo();

repeatAfterFileSavingFailed:
	if (currentEditorView()->editor->isContentModified()) {
		switch (QMessageBox::warning(this, TEXSTUDIO,
		                             tr("The document \"%1\" contains unsaved work. "
		                                "Do you want to save it before closing?").arg(currentEditorView()->displayName()),
		                             tr("Save and Close"), tr("Close without Saving"), tr("Cancel"),
		                             0,
		                             2)) {
		case 0:
			fileSave();
			if (currentEditorView()->editor->isContentModified())
				goto repeatAfterFileSavingFailed;
			documents.deleteDocument(currentEditorView()->document);
			break;
		case 1:
			documents.deleteDocument(currentEditorView()->document);
			break;
		case 2:
		default:
			return;
			break;
		}
	} else documents.deleteDocument(currentEditorView()->document);
	//UpdateCaption(); unnecessary as called by tabChanged (signal)

#ifndef NO_POPPLER_PREVIEW
	//close associated embedded pdf viewer
	foreach (PDFDocument *viewer, PDFDocument::documentList())
		if (viewer->autoClose && viewer->getMasterFile() == fi)
			viewer->close();
#endif
}

void Texstudio::fileCloseAll()
{
	bool accept = saveAllFilesForClosing();
	if (accept) {
		closeAllFiles();
	}
}

void Texstudio::fileExit()
{
	if (canCloseNow())
		qApp->quit();
}

bool Texstudio::saveAllFilesForClosing()
{
	return saveFilesForClosing(editors->editors());
}

bool Texstudio::saveFilesForClosing(const QList<LatexEditorView *> &editorList)
{
	LatexEditorView *savedCurrentEditorView = currentEditorView();
	foreach (LatexEditorView *edView, editorList) {
repeatAfterFileSavingFailed:
		if (edView->editor->isContentModified()) {
			editors->setCurrentEditor(edView);
			switch (QMessageBox::warning(this, TEXSTUDIO,
			                             tr("The document \"%1\" contains unsaved work. "
			                                "Do you want to save it before closing?").arg(edView->displayName()),
			                             tr("Save and Close"), tr("Close without Saving"), tr("Cancel"),
			                             0,
			                             2)) {
			case 0:
				fileSave();
				if (currentEditorView()->editor->isContentModified())
					goto repeatAfterFileSavingFailed;
				break;
			case 1:
				break;
			case 2:
			default:
				editors->setCurrentEditor(savedCurrentEditorView);
				return false;
			}
		}
	}
	editors->setCurrentEditor(savedCurrentEditorView);
	return true;
}
/*! \brief close all files
 */
void Texstudio::closeAllFiles()
{
	while (currentEditorView())
		documents.deleteDocument(currentEditorView()->document);
#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *viewer, PDFDocument::documentList())
		viewer->close();
#endif
	documents.setMasterDocument(0);
	updateCaption();
}

bool Texstudio::canCloseNow(bool saveSettings)
{
	if (!saveAllFilesForClosing()) return false;
#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *viewer, PDFDocument::documentList())
		viewer->saveGeometryToConfig();
#endif
	if (saveSettings)
		this->saveSettings();
	closeAllFiles();
	if (userMacroDialog) delete userMacroDialog;
	spellerManager.unloadAll();  //this saves the ignore list
	programStopped = true;
	Guardian::shutdown();
	return true;
}
/*!
 * \brief closeEvent
 * \param e event
 */
void Texstudio::closeEvent(QCloseEvent *e)
{
	if (canCloseNow())  e->accept();
	else e->ignore();
}

void Texstudio::updateUserMacros(bool updateMenu)
{
	if (updateMenu) configManager.updateUserMacroMenu();
	for (int i = 0; i < configManager.completerConfig->userMacros.size(); i++) {
		configManager.completerConfig->userMacros[i].parseTriggerLanguage(m_languages);
	}
}

void Texstudio::centerFileSelector()
{
	if (!fileSelector) return;
	fileSelector.data()->setCentered(centralWidget()->geometry());
}
/*!
 * \brief open file from recent list
 *
 * The filename is determind from the sender-action, where it is encoded in data.
 */
void Texstudio::fileOpenRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	QString fn = action->data().toString();
	if (!QFile::exists(fn)) {
		if (txsConfirmWarning(tr("The file \"%1\" does not exist anymore. Do you want to remove it from the recent file list?").arg(fn))) {
			if (configManager.recentFilesList.removeAll(fn))
				configManager.updateRecentFiles();
			return;
		}
	}
	load(fn);
}

void Texstudio::fileOpenAllRecent()
{
	foreach (const QString &s, configManager.recentFilesList)
		load(s);
}

QRect appendToBottom(QRect r, const QRect &s)
{
	r.setBottom(r.bottom() + s.height());
	return r;
}

void Texstudio::fileRecentList()
{
	if (fileSelector) fileSelector.data()->deleteLater();
	fileSelector = new FileSelector(this, true);

	fileSelector.data()->init(QStringList() << configManager.recentProjectList << configManager.recentFilesList, 0);

	connect(fileSelector.data(), SIGNAL(fileChoosen(QString, int, int, int)), SLOT(fileDocumentOpenFromChoosen(QString, int, int, int)));
	fileSelector.data()->setVisible(true);
	centerFileSelector();
}

void Texstudio::viewDocumentListHidden()
{
	if (fileSelector) fileSelector.data()->deleteLater();
	fileSelector = new FileSelector(this, true);

	QStringList hiddenDocs;
	foreach (LatexDocument *d, documents.hiddenDocuments)
		hiddenDocs << d->getFileName();
	fileSelector.data()->init(hiddenDocs, 0);

	connect(fileSelector.data(), SIGNAL(fileChoosen(QString, int, int, int)), SLOT(fileDocumentOpenFromChoosen(QString, int, int, int)));
	fileSelector.data()->setVisible(true);
	centerFileSelector();
}

void Texstudio::fileDocumentOpenFromChoosen(const QString &doc, int duplicate, int lineNr, int column)
{
	Q_UNUSED(duplicate);
	if (!QFile::exists(doc)) {
		if (txsConfirmWarning(tr("The file \"%1\" does not exist anymore. Do you want to remove it from the recent file list?").arg(doc))) {
			if (configManager.recentFilesList.removeAll(doc) + configManager.recentProjectList.removeAll(doc) > 0)
				configManager.updateRecentFiles();
			return;
		}
	}

	if (!load(doc, duplicate < configManager.recentProjectList.count(doc))) return;
	if (lineNr < 0) return;
	REQUIRE(currentEditor());
	currentEditor()->setCursorPosition(lineNr, column);
}

bool mruEditorViewLessThan(const LatexEditorView *e1, const LatexEditorView *e2)
{
	return e1->lastUsageTime > e2->lastUsageTime;
}

void Texstudio::viewDocumentList()
{
	if (fileSelector) fileSelector.data()->deleteLater();
	fileSelector = new FileSelector(this, false);

	LatexEditorView *curEdView = currentEditorView();
	int curIndex = 0;
	QList<LatexEditorView *> editorList = editors->editors();

	if (configManager.mruDocumentChooser) {
		qSort(editorList.begin(), editorList.end(), mruEditorViewLessThan);
		if (editorList.size() > 1)
			if (editorList.first() == currentEditorView())
				curIndex = 1;
	}

	int i = 0;
	QStringList names;
	foreach (LatexEditorView *edView, editorList) {
		names << edView->displayName();
		if (!configManager.mruDocumentChooser && edView == curEdView) curIndex = i;
		i++;
	}

	fileSelector.data()->init(names, curIndex);
	connect(fileSelector.data(), SIGNAL(fileChoosen(QString, int, int, int)), SLOT(viewDocumentOpenFromChoosen(QString, int, int, int)));
	fileSelector.data()->setVisible(true);
	centerFileSelector();
}

void Texstudio::viewDocumentOpenFromChoosen(const QString &doc, int duplicate, int lineNr, int column)
{
	if (duplicate < 0) return;
	foreach (LatexEditorView *edView, editors->editors()) {
		QString  name = edView->displayName();
		if (name == doc) {
			duplicate -= 1;
			if (duplicate < 0) {
				editors->setCurrentEditor(edView);
				if (lineNr >= 0)
					edView->editor->setCursorPosition(lineNr, column);
				edView->setFocus();
				return;
			}
		}
	}
}

void Texstudio::fileOpenFirstNonOpen()
{
	foreach (const QString &f, configManager.recentFilesList)
		if (!getEditorViewFromFileName(f)) {
			load(f);
			break;
		}
}

void Texstudio::fileOpenRecentProject()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	QString fn = action->data().toString();
	if (!QFile::exists(fn)) {
		if (txsConfirmWarning(tr("The file \"%1\" does not exist anymore. Do you want to remove it from the recent file list?").arg(fn))) {
			if (configManager.recentProjectList.removeAll(fn))
				configManager.updateRecentFiles();
			return;
		}
	}
	load(fn, true);
}

void Texstudio::loadSession(const QString &fileName)
{
	Session s;
	if (!s.load(fileName)) {
		txsCritical(tr("Loading of session failed."));
		return;
	}
	restoreSession(s);
}

void Texstudio::fileLoadSession()
{
	QString openDir = QDir::homePath();
	if (currentEditorView()) {
		LatexDocument *doc = currentEditorView()->document;
		if (doc->getMasterDocument()) {
			openDir = doc->getMasterDocument()->getFileInfo().path();
		} else {
			openDir = doc->getFileInfo().path();
		}
	}
	QString fn = QFileDialog::getOpenFileName(this, tr("Load Session"), openDir, tr("TeXstudio Session") + " (*." + Session::fileExtension() + ")");
	if (fn.isNull()) return;
	loadSession(fn);
	recentSessionList->addFilenameToList(fn);
}

void Texstudio::fileSaveSession()
{
	QString openDir = QDir::homePath();
	if (currentEditorView()) {
		LatexDocument *doc = currentEditorView()->document;
		if (doc->getMasterDocument()) {
			openDir = replaceFileExtension(doc->getMasterDocument()->getFileName(), Session::fileExtension());
		} else {
			openDir = replaceFileExtension(doc->getFileName(), Session::fileExtension());
		}
	}

	QString fn = QFileDialog::getSaveFileName(this, tr("Save Session"), openDir, tr("TeXstudio Session") + " (*." + Session::fileExtension() + ")");
	if (fn.isNull()) return;
	if (!getCurrentSession().save(fn, configManager.sessionStoreRelativePaths)) {
		txsCritical(tr("Saving of session failed."));
		return;
	}
	recentSessionList->addFilenameToList(fn);
}
/*!
 * \brief restore session s
 *
 * closes all files and loads all files given in session s
 * \param s session
 * \param showProgress
 * \param warnMissing give warning if files are missing
 */
void Texstudio::restoreSession(const Session &s, bool showProgress, bool warnMissing)
{
	fileCloseAll();

	cursorHistory->setInsertionEnabled(false);
	QProgressDialog progress(this);
	if (showProgress) {
		progress.setMaximum(s.files().size());
		progress.setCancelButton(0);
		progress.setMinimumDuration(3000);
		progress.setLabel(new QLabel());
	}
	recheckLabels = false; // impede label rechecking on hidden docs

	bookmarks->setBookmarks(s.bookmarks()); // set before loading, so that bookmarks are automatically restored on load

	QStringList missingFiles;
	for (int i = 0; i < s.files().size(); i++) {
		FileInSession f = s.files().at(i);

		if (showProgress) {
			progress.setValue(i);
			progress.setLabelText(QFileInfo(f.fileName).fileName());
		}
		LatexEditorView *edView = load(f.fileName, f.fileName == s.masterFile(), false, false, true);
		if (edView) {
			int line = f.cursorLine;
			int col = f.cursorCol;
			if (line >= edView->document->lineCount()) {
				line = 0;
				col = 0;
			} else {
				if (edView->document->line(line).length() < col) {
					col = 0;
				}
			}
			edView->editor->setCursorPosition(line, col);
			edView->editor->scrollToFirstLine(f.firstLine);
			edView->document->foldLines(f.foldedLines);
		} else {
			missingFiles.append(f.fileName);
		}
	}
	// update ref/labels in one go;
	QList<LatexDocument *> completedDocs;
	foreach (LatexDocument *doc, documents.getDocuments()) {
		doc->recheckRefsLabels();
		if (completedDocs.contains(doc))
			continue;

		doc->updateLtxCommands(true);
		completedDocs << doc->getListOfDocs();
	}
	recheckLabels = true;

	if (showProgress) {
		progress.setValue(progress.maximum());
	}
	activateEditorForFile(s.currentFile());
	cursorHistory->setInsertionEnabled(true);

	if (!s.PDFFile().isEmpty()) {
		runInternalCommand("txs:///view-pdf-internal", QFileInfo(s.PDFFile()), s.PDFEmbedded() ? "--embedded" : "--windowed");
	}
	// update completer
	if (currentEditorView())
		updateCompleter(currentEditorView());

	if (warnMissing && !missingFiles.isEmpty()) {
		txsInformation(tr("The following files could not be loaded:") + "\n" + missingFiles.join("\n"));
	}
}

Session Texstudio::getCurrentSession()
{
	Session s;

	foreach (LatexEditorView *edView, editors->editors()) {
		FileInSession f;
		f.fileName = edView->editor->fileName();
		f.cursorLine = edView->editor->cursor().lineNumber();
		f.cursorCol = edView->editor->cursor().columnNumber();
		f.firstLine = edView->editor->getFirstVisibleLine();
		f.foldedLines = edView->document->foldedLines();
		if (!f.fileName.isEmpty())
			s.addFile(f);
	}
	s.setMasterFile(documents.masterDocument ? documents.masterDocument->getFileName() : "");
	s.setCurrentFile(currentEditorView() ? currentEditor()->fileName() : "");

	s.setBookmarks(bookmarks->getBookmarks());
#ifndef NO_POPPLER_PREVIEW
	if (!PDFDocument::documentList().isEmpty()) {
		PDFDocument *doc = PDFDocument::documentList().at(0);
		s.setPDFEmbedded(doc->embeddedMode);
		s.setPDFFile(doc->fileName());
	}
#endif

	return s;
}

void Texstudio::MarkCurrentFileAsRecent()
{
	configManager.addRecentFile(getCurrentFileName(), documents.masterDocument == currentEditorView()->document);
}

//////////////////////////// EDIT ///////////////////////
void Texstudio::editUndo()
{
	if (!currentEditorView()) return;

	QVariant zw = currentEditor()->property("undoRevision");
	int undoRevision = zw.canConvert<int>() ? zw.toInt() : 0;

	if (currentEditor()->document()->canUndo()) {
		currentEditor()->undo();
		if (undoRevision > 0) {
			undoRevision = -1;
			currentEditor()->setProperty("undoRevision", undoRevision);
		}
	} else {
		if (configManager.svnUndo && (undoRevision >= 0)) {
			svnUndo();
		}
	}
}

void Texstudio::editRedo()
{
	if (!currentEditorView()) return;

	QVariant zw = currentEditor()->property("undoRevision");
	int undoRevision = zw.canConvert<int>() ? zw.toInt() : 0;

	if (currentEditor()->document()->canRedo()) {
		currentEditorView()->editor->redo();
	} else {
		if (configManager.svnUndo && (undoRevision > 0)) {
			svnUndo(true);
		}
	}
}

void Texstudio::editDebugUndoStack()
{
	if (!currentEditor()) return;
	QString history = currentEditor()->document()->debugUndoStack();
	fileNew();
	currentEditor()->document()->setText(history, false);
}

void Texstudio::editCopy()
{
	if ((!currentEditor() || !currentEditor()->hasFocus()) &&
	        outputView->childHasFocus() ) {
		outputView->copy();
		return;
	}
	if (!currentEditorView()) return;
	currentEditorView()->editor->copy();
}

void Texstudio::editPaste()
{
	if (!currentEditorView()) return;

	const QMimeData *d = QApplication::clipboard()->mimeData();
	if (d->hasFormat("application/x-qt-windows-mime;value=\"Star Embed Source (XML)\"") && d->hasFormat("text/plain")) {
		// workaround for LibreOffice (im "application/x-qt-image" has a higher priority for them than "text/plain")
		currentEditorView()->paste();
		return;
	}

	foreach (const QString &format, d->formats()) {
		// formats is a priority order. Use the first (== most suitable) format
		if (format == "text/uri-list") {
			QList<QUrl> uris = d->urls();

			bool onlyLocalImages = true;
			for (int i = 0; i < uris.length(); i++) {
				QFileInfo fi = QFileInfo(uris.at(i).toLocalFile());
				if (!fi.exists() || !InsertGraphics::imageFormats().contains(fi.suffix())) {
					onlyLocalImages = false;
					break;
				}
			}

			if (onlyLocalImages) {
				for (int i = 0; i < uris.length(); i++) {
					quickGraphics(uris.at(i).toLocalFile());
				}
			} else {
				currentEditorView()->paste();
			}
			return;
		} else if (format == "text/plain" || format == "text/html") {
			currentEditorView()->paste();
			return;
		} else if ((format == "application/x-qt-image" || format.startsWith("image/")) && d->hasImage()) {
			editPasteImage(qvariant_cast<QImage>(d->imageData()));
			return;
		}
	}
	currentEditorView()->paste();  // fallback
}

void Texstudio::editPasteImage(QImage image)
{
	static QString filenameSuggestion;  // keep for future calls
	QString rootDir = currentEditorView()->document->getRootDocument()->getFileInfo().absolutePath();
	if (!currentEditorView()) return;
	if (filenameSuggestion.isEmpty()) {
		filenameSuggestion = rootDir + "/screenshot001.png";
	}
	QStringList filters;
	foreach (const QByteArray fmt, QImageWriter::supportedImageFormats()) {
		filters << "*." + fmt;
	}
	QString filter = tr("Image Formats (%1)").arg(filters.join(" "));
	filenameSuggestion = getNonextistentFilename(filenameSuggestion, rootDir);
	QString filename = QFileDialog::getSaveFileName(this, tr("Save Image"), filenameSuggestion, filter, &filter);
	if (filename.isEmpty()) return;
	filenameSuggestion = filename;

	if (!image.save(filename)) {
		txsCritical(tr("Could not save the image file."));
		return;
	}
	quickGraphics(filename);
}

void Texstudio::editPasteLatex()
{
	if (!currentEditorView()) return;
	// manipulate clipboard text
	QClipboard *clipboard = QApplication::clipboard();
	QString originalText = clipboard->text();
	QString newText = textToLatex(originalText);
	//clipboard->setText(newText);
	// insert
	//currentEditorView()->editor->paste();
	QMimeData md;
	md.setText(newText);
	currentEditorView()->editor->insertFromMimeData(&md);
}

void Texstudio::convertToLatex()
{
	if (!currentEditorView()) return;
	// get selection and change it
	QString originalText = currentEditor()->cursor().selectedText();
	QString newText = textToLatex(originalText);
	// insert
	currentEditor()->write(newText);
}

void Texstudio::editDeleteLine()
{
	if (!currentEditorView()) return;
	currentEditorView()->deleteLines(true, true);
}

void Texstudio::editDeleteToEndOfLine()
{
	if (!currentEditorView()) return;
	currentEditorView()->deleteLines(false, true);
}

void Texstudio::editDeleteFromStartOfLine()
{
	if (!currentEditorView()) return;
	currentEditorView()->deleteLines(true, false);
}

void Texstudio::editMoveLineUp()
{
	if (!currentEditorView()) return;
	currentEditorView()->moveLines(-1);
}

void Texstudio::editMoveLineDown()
{
	if (!currentEditorView()) return;
	currentEditorView()->moveLines(1);
}

void Texstudio::editDuplicateLine()
{
	if (!currentEditor()) return;
	QEditor *ed = currentEditor();
	QList<QDocumentCursor> cursors = ed->cursors();
	for (int i = 0; i < cursors.length(); i++)
		cursors[i].setAutoUpdated(false);
	QList<QPair<int, int> > blocks = currentEditorView()->getSelectedLineBlocks();
	for (int i = blocks.size() - 1; i >= 0; i--) {
		QDocumentCursor edit = ed->document()->cursor(blocks[i].first, 0, blocks[i].second);
		QString text = edit.selectedText();
		edit.selectionEnd().insertText("\n" + text);
	}
}

void Texstudio::editAlignMirrors()
{
	if (!currentEditor()) return;
	currentEditorView()->alignMirrors();
}

void Texstudio::editEraseWordCmdEnv()
{
	if (!currentEditorView()) return;
	QDocumentCursor cursor = currentEditorView()->editor->cursor();
	if (cursor.isNull()) return;
	QString line = cursor.line().text();
	QDocumentLineHandle *dlh = cursor.line().handle();
	QString command, value;

	// Hack to fix problem problem reported in bug report 3443336 (see also detailed description there):
	// findContext does not work at beginning of commands. Actually it would need
	// pre-context and post-context otherwise, what context is either ambigous, like in
	// \cmd|\cmd or it cannot work simultaneously at beginning and and (you cannot assign
	// the context for |\cmd and \cmd| even if \cmd is surrounded by spaces. The latter
	// both assignments make sense for editEraseWordCmdEnv().
	//
	// pre-context and post-context may be added when revising the latex parser
	//
	// Prelimiary solution part I:
	// Predictable behaviour on selections: do nothing except in easy cases
	if (cursor.hasSelection()) {
		QRegExp partOfWordOrCmd("\\\\?\\w*");
		if (!partOfWordOrCmd.exactMatch(cursor.selectedText()))
			return;
	}
	// Prelimiary solution part II:
	// If the case |\cmd is encountered, we shift the
	// cursor by one to \|cmd so the regular erase approach works.
	int col = cursor.columnNumber();
	if ((col < line.count())						// not at end of line
	        && (line.at(col) == '\\')					// likely a command/env - check further
	        && (col == 0 || line.at(col - 1).isSpace()))	// cmd is at start or previous char is space: non-ambiguous situation like word|\cmd
		// don't need to finally check for command |\c should be handled like \|c for any c (even space and empty)
	{
		cursor.movePosition(1);
	}

	TokenList tl = dlh->getCookieLocked(QDocumentLine::LEXER_COOKIE).value<TokenList>();
	int tkPos = getTokenAtCol(tl, cursor.columnNumber());
	Token tk;
	if (tkPos > -1)
		tk = tl.at(tkPos);

	switch (tk.type) {

	case Token::command:
		command = tk.getText();
		if (command == "\\begin" || command == "\\end") {
			value = getArg(tl.mid(tkPos + 1), dlh, 0, ArgumentList::Mandatory);
			//remove environment (surrounding)
			currentEditorView()->editor->document()->beginMacro();
			cursor.select(QDocumentCursor::WordOrCommandUnderCursor);
			cursor.removeSelectedText();
			// remove curly brakets as well
			if (cursor.nextChar() == QChar('{')) {
				cursor.deleteChar();
				line = cursor.line().text();
				int col = cursor.columnNumber();
				int i = findClosingBracket(line, col);
				if (i > -1) {
					cursor.movePosition(i - col + 1, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
					cursor.removeSelectedText();
					QDocument *doc = currentEditorView()->editor->document();
					QString searchWord = "\\end{" + value + "}";
					QString inhibitor = "\\begin{" + value + "}";
					bool backward = (command == "\\end");
					int step = 1;
					if (backward) {
						qSwap(searchWord, inhibitor);
						step = -1;
					}
					int startLine = cursor.lineNumber();
					int startCol = cursor.columnNumber();
					int endLine = doc->findLineContaining(searchWord, startLine, Qt::CaseSensitive, backward);
					int inhibitLine = doc->findLineContaining(inhibitor, startLine, Qt::CaseSensitive, backward); // not perfect (same line end/start ...)
					while (inhibitLine > 0 && endLine > 0 && inhibitLine * step < endLine * step) {
						endLine = doc->findLineContaining(searchWord, endLine + step, Qt::CaseSensitive, backward); // not perfect (same line end/start ...)
						inhibitLine = doc->findLineContaining(inhibitor, inhibitLine + step, Qt::CaseSensitive, backward);
					}
					if (endLine > -1) {
						line = doc->line(endLine).text();
						int start = line.indexOf(searchWord);
						cursor.moveTo(endLine, start);
						cursor.movePosition(searchWord.length(), QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
						cursor.removeSelectedText();
						cursor.moveTo(startLine, startCol); // move cursor back to text edit pos
					}
				}
			}

			currentEditorView()->editor->document()->endMacro();
		} else {
			currentEditorView()->editor->document()->beginMacro();
			cursor.select(QDocumentCursor::WordOrCommandUnderCursor);
			cursor.removeSelectedText();
			// remove curly brakets as well
			if (cursor.nextChar() == QChar('{')) {
				cursor.deleteChar();
				line = cursor.line().text();
				int col = cursor.columnNumber();
				int i = findClosingBracket(line, col);
				if (i > -1) {
					cursor.moveTo(cursor.lineNumber(), i);
					cursor.deleteChar();
					cursor.moveTo(cursor.lineNumber(), col);
				}
			}
			currentEditorView()->editor->document()->endMacro();
		}
		break;

	default:
		cursor.select(QDocumentCursor::WordUnderCursor);
		cursor.removeSelectedText();
		break;
	}
	currentEditorView()->editor->setCursor(cursor);
}

void Texstudio::editGotoDefinition(QDocumentCursor c)
{
	if (!currentEditorView())	return;
	if (!c.isValid()) c = currentEditor()->cursor();
	saveCurrentCursorToHistory();
	Token tk = getTokenAtCol(c.line().handle(), c.columnNumber());
	switch (tk.type) {
	case Token::labelRef:
	case Token::labelRefList: {
		LatexEditorView *edView = editorViewForLabel(qobject_cast<LatexDocument *>(c.document()), tk.getText());
		if (!edView) return;
		if (edView != currentEditorView()) {
			editors->setCurrentEditor(edView);
		}
		edView->gotoToLabel(tk.getText());
		break;
	}
	case Token::bibItem: {
		QString bibID = trimLeft(tk.getText());
		// try local \bibitems
		bool found = currentEditorView()->gotoToBibItem(bibID);
		if (found) break;
		// try bib files
		QString bibFile = currentEditorView()->document->findFileFromBibId(bibID);
		LatexEditorView *edView = getEditorViewFromFileName(bibFile);
		if (!edView) {
			if (!load(bibFile)) return;
			edView = currentEditorView();
		}
		int line = edView->document->findLineRegExp("@\\w+{\\s*" + bibID, 0, Qt::CaseSensitive, true, true);
		if (line < 0) {
			line = edView->document->findLineContaining(bibID); // fallback in case the above regexp does not reflect the most general case
			if (line < 0) return;
		}
		int col = edView->document->line(line).text().indexOf(bibID);
		if (col < 0) col = 0;
		gotoLine(line, col, edView);
		break;
	}
	default:; //TODO: Jump to command definition
	}
}

void Texstudio::editHardLineBreak()
{
	if (!currentEditorView()) return;
	UniversalInputDialog dialog;
	dialog.addVariable(&configManager.lastHardWrapColumn, tr("Insert hard line breaks after so many characters:"));
	dialog.addVariable(&configManager.lastHardWrapSmartScopeSelection, tr("Smart scope selecting"));
	dialog.addVariable(&configManager.lastHardWrapJoinLines, tr("Join lines before wrapping"));
	if (dialog.exec() == QDialog::Accepted)
		currentEditorView()->insertHardLineBreaks(configManager.lastHardWrapColumn, configManager.lastHardWrapSmartScopeSelection, configManager.lastHardWrapJoinLines);
}

void Texstudio::editHardLineBreakRepeat()
{
	if (!currentEditorView()) return;
	currentEditorView()->insertHardLineBreaks(configManager.lastHardWrapColumn, configManager.lastHardWrapSmartScopeSelection, configManager.lastHardWrapJoinLines);
}

void Texstudio::editSpell()
{
	if (!currentEditorView()) {
		txsWarning(tr("No document open"));
		return;
	}
	SpellerUtility *su = spellerManager.getSpeller(currentEditorView()->getSpeller());
	if (!su) return; // getSpeller already gives a warning message
	if (su->name() == "<none>") {
		txsWarning(tr("No dictionary available."));
		return;
	}
	if (!spellDlg) spellDlg = new SpellerDialog(this, su);
	spellDlg->setEditorView(currentEditorView());
	spellDlg->startSpelling();
}

void Texstudio::editThesaurus(int line, int col)
{
	if (!ThesaurusDialog::retrieveDatabase()) {
		QMessageBox::warning(this, tr("Error"), tr("Can't load Thesaurus Database"));
		return;
	}
	ThesaurusDialog *thesaurusDialog = new ThesaurusDialog(this);
	QString word;
	if (currentEditorView()) {
		QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
		if (line > -1 && col > -1) {
			m_cursor.moveTo(line, col);
		}
		if (m_cursor.hasSelection()) word = m_cursor.selectedText();
		else {
			m_cursor.select(QDocumentCursor::WordUnderCursor);
			word = m_cursor.selectedText();
		}
		word = latexToPlainWord(word);
		thesaurusDialog->setSearchWord(word);
		if (thesaurusDialog->exec()) {
			QString replace = thesaurusDialog->getReplaceWord();
			m_cursor.document()->clearLanguageMatches();
			m_cursor.insertText(replace);
		}
	}
	delete thesaurusDialog;
}

void Texstudio::editChangeLineEnding()
{
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	currentEditorView()->editor->document()->setLineEnding(QDocument::LineEnding(action->data().toInt()));
	updateCaption();
}

void Texstudio::editSetupEncoding()
{
	if (!currentEditorView()) return;
	EncodingDialog enc(this, currentEditorView()->editor);
	enc.exec();
	updateCaption();
}

void Texstudio::editInsertUnicode()
{
	if (!currentEditorView()) return;
	QDocumentCursor c = currentEditor()->cursor();
	if (!c.isValid()) return;
	int curPoint = 0;
	if (c.hasSelection()) {
		QString sel = c.selectedText();
		if (sel.length() == 1) curPoint = sel[0].unicode();
		else if (sel.length() == 2 && sel.at(0).isHighSurrogate() && sel.at(1).isLowSurrogate()) {
			curPoint = sel.toUcs4().value(0, 0);
		} else c.setAnchorColumnNumber(c.columnNumber());
		currentEditor()->setCursor(c);
	}
	QPoint offset;
	UnicodeInsertion *uid = new UnicodeInsertion (currentEditorView(), curPoint);
	if (!currentEditor()->getPositionBelowCursor(offset, uid->width(), uid->height())) {
		delete uid;
		return;
	}
	connect(uid, SIGNAL(insertCharacter(QString)), currentEditor(), SLOT(insertText(QString)));
	connect(uid, SIGNAL(destroyed()), currentEditor(), SLOT(setFocus()));
	connect(currentEditor(), SIGNAL(cursorPositionChanged()), uid, SLOT(close()));
	connect(currentEditor(), SIGNAL(visibleLinesChanged()), uid, SLOT(close()));
	connect(currentEditor()->document(), SIGNAL(contentsChanged()), uid, SLOT(close()));

	uid->move(currentEditor()->mapTo(uid->parentWidget(), offset));
	this->unicodeInsertionDialog = uid;
	uid->show();
	uid->setFocus();
}

void Texstudio::editIndentSection()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	QDocumentSelection sel = entry->document->sectionSelection(entry);

	// replace list
	QStringList m_replace;
	m_replace << "\\subparagraph" << "\\paragraph" << "\\subsubsection" << "\\subsection" << "\\section" << "\\chapter";
	// replace sections
	QString m_line;
	QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
	for (int l = sel.startLine; l < sel.endLine; l++) {
		currentEditorView()->editor->setCursorPosition(l, 0);
		m_cursor = currentEditorView()->editor->cursor();
		m_line = currentEditorView()->editor->cursor().line().text();
		QString m_old = "";
		foreach (const QString &elem, m_replace) {
			if (m_old != "") m_line.replace(elem, m_old);
			m_old = elem;
		}

		m_cursor.movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
		currentEditor()->setCursor(m_cursor);
		currentEditor()->insertText(m_line);
	}
}

void Texstudio::editUnIndentSection()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	QDocumentSelection sel = entry->document->sectionSelection(entry);

	QStringList m_replace;

	m_replace << "\\chapter" << "\\section" << "\\subsection" << "\\subsubsection" << "\\paragraph" << "\\subparagraph" ;

	// replace sections
	QString m_line;
	QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
	for (int l = sel.startLine; l < sel.endLine; l++) {
		currentEditorView()->editor->setCursorPosition(l, 0);
		m_cursor = currentEditorView()->editor->cursor();
		m_line = currentEditorView()->editor->cursor().line().text();
		QString m_old = "";
		foreach (const QString &elem, m_replace) {
			if (m_old != "") m_line.replace(elem, m_old);
			m_old = elem;
		}

		m_cursor.movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
		currentEditor()->setCursor(m_cursor);
		currentEditor()->insertText(m_line);
	}
}

/*!
 * copy in structure-view selected section from text
 */
void Texstudio::editSectionCopy()
{
	// called by action
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	QDocumentSelection sel = entry->document->sectionSelection(entry);
	editSectionCopy(sel.startLine + 1, sel.endLine);
}

/*!
 * cut in structure-view selected section from text
 */
void Texstudio::editSectionCut()
{
	// called by action
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	QDocumentSelection sel = entry->document->sectionSelection(entry);
	editSectionCut(sel.startLine + 1, sel.endLine);
	//UpdateStructure();
}

void Texstudio::editSectionCopy(int startingLine, int endLine)
{
	if (!currentEditorView()) return;

	currentEditorView()->editor->setCursorPosition(startingLine - 1, 0);
	QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
	//m_cursor.movePosition(1, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.setSilent(true);
	m_cursor.movePosition(endLine - startingLine, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.movePosition(0, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
	currentEditorView()->editor->setCursor(m_cursor);
	currentEditorView()->editor->copy();
}

void Texstudio::editSectionCut(int startingLine, int endLine)
{
	if (!currentEditorView()) return;
	currentEditorView()->editor->setCursorPosition(startingLine - 1, 0);
	QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
	m_cursor.setSilent(true);
	m_cursor.movePosition(endLine - startingLine, QDocumentCursor::NextLine, QDocumentCursor::KeepAnchor);
	m_cursor.movePosition(0, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
	currentEditorView()->editor->setCursor(m_cursor);
	currentEditorView()->editor->cut();
}

void Texstudio::editSectionPasteBefore()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	editSectionPasteBefore(entry->getRealLineNumber());
	//UpdateStructure();
}
/*!
 * \brief paste clipboard after selected section (in structureview)
 */
void Texstudio::editSectionPasteAfter()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || !entry->document->getEditorView()) return;
	editors->setCurrentEditor(entry->document->getEditorView());
	QDocumentSelection sel = entry->document->sectionSelection(entry);
	editSectionPasteAfter(sel.endLine);
	//UpdateStructure();
}

void Texstudio::editSectionPasteAfter(int line)
{
	REQUIRE(currentEditorView());
	if (line >= currentEditorView()->editor->document()->lines()) {
		currentEditorView()->editor->setCursorPosition(line - 1, 0);
		QDocumentCursor c = currentEditorView()->editor->cursor();
		c.movePosition(1, QDocumentCursor::End, QDocumentCursor::MoveAnchor);
		currentEditor()->setCursor(c);
		currentEditor()->insertText("\n");
	} else {
		currentEditor()->setCursorPosition(line, 0);
		currentEditor()->insertText("\n");
		currentEditor()->setCursorPosition(line, 0);
	}
	currentEditorView()->paste();
}

void Texstudio::editSectionPasteBefore(int line)
{
	REQUIRE(currentEditor());
	currentEditor()->setCursorPosition(line, 0);
	currentEditor()->insertText("\n");
	currentEditor()->setCursorPosition(line, 0);
	currentEditorView()->paste();
}

void changeCase(QEditor *editor, QString(*method)(QString))
{
	if (!editor) return;
	QList<QDocumentCursor> cs = editor->cursors();
	bool allEmpty = true;
	foreach (QDocumentCursor c, cs)
		if (!c.selectedText().isEmpty()) {
			allEmpty = false;
			break;
		}
	if (allEmpty) return;

	editor->document()->beginMacro();
	foreach (QDocumentCursor c, editor->cursors())
		c.replaceSelectedText( method(c.selectedText()) );
	editor->document()->endMacro();
}
/*!
 * Helperfunction to convert a string to lower case.
 * \param in input string
 * \result string in lower case
 */
QString txsToLower(QString in)
{
	return in.toLower();
}

/*!
 * Converts the selected text to lower case.
 */
void Texstudio::editTextToLowercase()
{
	changeCase(currentEditor(), &txsToLower);
}
/*!
 * Helperfunction to convert a string to upper case.
 * \param in input string
 * \result string in upper case
 */

QString txsToUpper(QString in)
{
	return in.toUpper();
}

/*!
 * Converts the selected text to upper case.
 */
void Texstudio::editTextToUppercase()
{
	changeCase(currentEditor(), &txsToUpper);
}

/*!
 * Converts the selected text to title case. Small words like a,an etc. are not converted.
 * \param smart: Words containing capital letters are not converted because the are assumed to be acronymes.
 */
void Texstudio::editTextToTitlecase(bool smart)
{
	if (!currentEditorView()) return;
	QDocumentCursor m_cursor = currentEditorView()->editor->cursor();
	QString text = m_cursor.selectedText();
	if (text.isEmpty()) return;
	m_cursor.beginEditBlock();
	// easier to be done in javascript
	scriptengine *eng = new scriptengine();
	eng->setEditorView(currentEditorView());
	QString script =
	    "/* \n" \
	    "	* To Title Case 2.1  http://individed.com/code/to-title-case/ \n" \
	    "	* Copyright © 20082013 David Gouch. Licensed under the MIT License.\n" \
	    "*/ \n" \
	    "toTitleCase = function(text){\n" \
	    "var smallWords = /^(a|an|and|as|at|but|by|en|for|if|in|nor|of|on|or|per|the|to|vs?\\.?|via)$/i;\n" \
	    "return text.replace(/[A-Za-z0-9\\u00C0-\\u00FF]+[^\\s-]*/g, function(match, index, title){\n" \
	    "if (index > 0 && index + match.length !== title.length &&\n" \
	    "  match.search(smallWords) > -1 && title.charAt(index - 2) !== \":\" &&\n" \
	    "  (title.charAt(index + match.length) !== '-' || title.charAt(index - 1) === '-') &&\n" \
	    "  title.charAt(index - 1).search(/[^\\s-]/) < 0) {\n" \
	    "    return match.toLowerCase();\n" \
	    "}\n";
	if (smart) {
		script +=
		    "if (match.substr(1).search(/[A-Z]|\\../) > -1) {\n" \
		    "return match;\n" \
		    "}\n" \
		    "return match.charAt(0).toUpperCase() + match.substr(1);\n";
	} else {
		script +=
		    "return match.charAt(0).toUpperCase() + match.substr(1).toLowerCase();\n";
	}
	script +=
	    "});\n" \
	    "};\n" \
	    "editor.replaceSelectedText(toTitleCase)";
	eng->setScript(script);
	eng->run();
	if (!eng->globalObject) delete eng;
	m_cursor.endEditBlock();
}

void Texstudio::editTextToTitlecaseSmart()
{
	editTextToTitlecase(true);
}

void Texstudio::editFind()
{
#ifndef NO_POPPLER_PREVIEW
	QWidget *w = QApplication::focusWidget();
	while (w && !qobject_cast<PDFDocument *>(w))
		w = w->parentWidget();

	if (qobject_cast<PDFDocument *>(w)) {
		PDFDocument *focusedPdf = qobject_cast<PDFDocument *>(w);
		if (focusedPdf->embeddedMode) {
			focusedPdf->search();
			return;
		}
	}
#endif
	if (!currentEditor()) return;
	currentEditor()->find();
}

/////////////// CONFIG ////////////////////
void Texstudio::readSettings(bool reread)
{
	QuickDocumentDialog::registerOptions(configManager);
	QuickBeamerDialog::registerOptions(configManager);
	buildManager.registerOptions(configManager);
	configManager.registerOption("Files/Default File Filter", &selectedFileFilter);
	configManager.registerOption("PDFSplitter", &pdfSplitterRel, 0.5);

	configManager.buildManager = &buildManager;
	scriptengine::buildManager = &buildManager;
	scriptengine::app = this;
	QSettings *config = configManager.readSettings(reread);
	completionBaseCommandsUpdated = true;

	config->beginGroup("texmaker");

	QRect screen = QApplication::desktop()->availableGeometry();
	int w = config->value("Geometries/MainwindowWidth", screen.width() - 100).toInt();
	int h = config->value("Geometries/MainwindowHeight", screen.height() - 100).toInt() ;
	int x = config->value("Geometries/MainwindowX", screen.x() + 10).toInt();
	int y = config->value("Geometries/MainwindowY", screen.y() + 10).toInt() ;
	int screenNumber = QApplication::desktop()->screenNumber(QPoint(x, y));
	screen = QApplication::desktop()->availableGeometry(screenNumber);
	if (!screen.contains(x, y)) {
		// top left is not on screen
		x = screen.x() + 10;
		y = screen.y() + 10;
		if (x + w > screen.right()) w = screen.width() - 100;
		if (y + h > screen.height()) h = screen.height() - 100;
	}
	resize(w, h);
	move(x, y);
	windowstate = config->value("MainWindowState").toByteArray();
	stateFullScreen = config->value("MainWindowFullssscreenState").toByteArray();
	tobemaximized = config->value("MainWindow/Maximized", false).toBool();
	tobefullscreen = config->value("MainWindow/FullScreen", false).toBool();

	documents.model->setSingleDocMode(config->value("StructureView/SingleDocMode", false).toBool());

	spellerManager.setIgnoreFilePrefix(configManager.configFileNameBase);
	spellerManager.setDictPaths(configManager.parseDirList(configManager.spellDictDir));
	spellerManager.setDefaultSpeller(configManager.spellLanguage);

	ThesaurusDialog::setUserPath(configManager.configFileNameBase);
	ThesaurusDialog::prepareDatabase(configManager.thesaurus_database);

	MapForSymbols = new QVariantMap;
	*MapForSymbols = config->value("Symbols/Quantity").toMap();

	hiddenLeftPanelWidgets = config->value("Symbols/hiddenlists", "").toString();
	symbolFavorites = config->value("Symbols/Favorite IDs", QStringList()).toStringList();

	configManager.editorKeys = QEditor::getEditOperations(false); //this will also initialize the default keys
	configManager.editorAvailableOperations = QEditor::getAvailableOperations();
	if (config->value("Editor/Use Tab for Move to Placeholder", false).toBool()) {
		//import deprecated option
		QEditor::addEditOperation(QEditor::NextPlaceHolder, Qt::ControlModifier, Qt::Key_Tab);
		QEditor::addEditOperation(QEditor::PreviousPlaceHolder, Qt::ShiftModifier | Qt::ControlModifier, Qt::Key_Backtab);
		QEditor::addEditOperation(QEditor::CursorWordLeft, Qt::ControlModifier, Qt::Key_Left);
		QEditor::addEditOperation(QEditor::CursorWordRight, Qt::ControlModifier, Qt::Key_Right);
	};
	// import and remove old key mapping
	{
		config->beginGroup("Editor Key Mapping");
		QStringList sl = config->childKeys();
		if (!sl.empty()) {
			foreach (const QString &key, sl) {
				int k = key.toInt();
				if (k == 0) continue;
				int operationID = config->value(key).toInt();
				QString defaultKey = configManager.editorKeys.key(operationID);
				if (!defaultKey.isNull()) {
					configManager.editorKeys.remove(defaultKey);
				}
				configManager.editorKeys.insert(QKeySequence(k).toString(), config->value(key).toInt());
			}
			QEditor::setEditOperations(configManager.editorKeys);
			config->remove("");
		}
		config->endGroup();
	}
	config->beginGroup("Editor Key Mapping New");
	QStringList sl = config->childKeys();
	QSet<int>manipulatedOps;
	if (!sl.empty()) {
		foreach (const QString &key, sl) {
			if (key.isEmpty()) continue;
			int operationID = config->value(key).toInt();
			if (key.startsWith("#")) {
				// remove predefined key
				QString realKey = key.mid(1);
				if (configManager.editorKeys.value(realKey) == operationID) {
					configManager.editorKeys.remove(realKey);
				}
			} else {
				/*if(!manipulatedOps.contains(operationID)){ // remove predefined keys only once
				    QStringList defaultKeys = configManager.editorKeys.keys(operationID);
				    if (!defaultKeys.isEmpty()) {
				        foreach(const QString elem,defaultKeys){
				            configManager.editorKeys.remove(elem);
				        }
				        manipulatedOps.insert(operationID);
				    }
				}*/
				// replacement of keys needs to add/remove a key explicitely, as otherwise a simple addition can't be saved into .ini
				configManager.editorKeys.insert(key, operationID);
			}
		}
		QEditor::setEditOperations(configManager.editorKeys);
	}
	config->endGroup();
	config->endGroup();

	config->beginGroup("formats");
	m_formats = new QFormatFactory(":/qxs/defaultFormats.qxf", this); //load default formats from resource file
	if (config->contains("data/styleHint/bold")) {
		//rename data/styleHint/* => data/wordRepetition/*
		config->beginGroup("data");
		config->beginGroup("styleHint");
		QStringList temp = config->childKeys();
		config->endGroup();
		foreach (const QString & s, temp) config->setValue("wordRepetition/" + s, config->value("styleHint/" + s));
		config->remove("styleHint");
		config->endGroup();
	}

	m_formats->load(*config, true); //load customized formats
	config->endGroup();

	documents.settingsRead();

	configManager.editorConfig->settingsChanged();
}

void Texstudio::saveSettings(const QString &configName)
{
	bool asProfile = !configName.isEmpty();
	configManager.centralVisible = centralToolBar->isVisible();
	// update completion usage
	LatexCompleterConfig *conf = configManager.completerConfig;
#ifndef NO_POPPLER_PREVIEW
	//pdf viewer embedded open ?
	if (!PDFDocument::documentList().isEmpty()) {
		PDFDocument *doc = PDFDocument::documentList().first();
		if (doc->embeddedMode) {
			QList<int> sz = mainHSplitter->sizes(); // set widths to 50%, eventually restore user setting
			int sum = 0;
			int last = 0;
			foreach (int i, sz) {
				sum += i;
				last = i;
			}
			if (last > 10)
				pdfSplitterRel = 1.0 * last / sum;
		}
	}
#endif

	QSettings *config = configManager.saveSettings(configName);

	config->beginGroup("texmaker");
	QList<int> sizes;
	QList<int>::Iterator it;
	if (!asProfile) {
		if (isFullScreen()) {
			config->setValue("MainWindowState", windowstate);
			config->setValue("MainWindowFullssscreenState", saveState(1));
		} else {
			config->setValue("MainWindowState", saveState(0));
			config->setValue("MainWindowFullssscreenState", stateFullScreen);
		}
		config->setValue("MainWindow/Maximized", isMaximized());
		config->setValue("MainWindow/FullScreen", isFullScreen());

		config->setValue("Geometries/MainwindowWidth", width());

		config->setValue("Geometries/MainwindowHeight", height());
		config->setValue("Geometries/MainwindowX", x());
		config->setValue("Geometries/MainwindowY", y());

		config->setValue("GUI/sidePanelSplitter/state", sidePanelSplitter->saveState());
		config->setValue("centralVSplitterState", centralVSplitter->saveState());
		config->setValue("GUI/outputView/visible", outputView->isVisible());
		config->setValue("GUI/sidePanel/visible", sidePanel->isVisible());

		if (!ConfigManager::dontRestoreSession) { // don't save session when using --no-restore as this is used for single doc handling
			Session s = getCurrentSession();
			s.save(QFileInfo(QDir(configManager.configBaseDir), "lastSession.txss").filePath(), configManager.sessionStoreRelativePaths);
		}
	}


	for (int i = 0; i < struct_level.count(); i++)
		config->setValue("Structure/Structure Level " + QString::number(i + 1), struct_level[i]);

	MapForSymbols->clear();
	foreach (QTableWidgetItem *elem, symbolMostused) {
		int cnt = elem->data(Qt::UserRole).toInt();
		if (cnt < 1) continue;
		QString text = elem->data(Qt::UserRole + 2).toString();
		if (MapForSymbols->value(text).toInt() > cnt) cnt = MapForSymbols->value(text).toInt();
		MapForSymbols->insert(text, cnt);
	}
	config->setValue("Symbols/Quantity", *MapForSymbols);

	config->setValue("Symbols/Favorite IDs", symbolFavorites);

	config->setValue("Symbols/hiddenlists", leftPanel->hiddenWidgets());

	config->setValue("StructureView/SingleDocMode", documents.model->getSingleDocMode());


	QHash<QString, int> keys = QEditor::getEditOperations(true);
	config->remove("Editor/Use Tab for Move to Placeholder");
	config->beginGroup("Editor Key Mapping New");
	if (!keys.empty() || !config->childKeys().empty()) {
		config->remove("");
		QHash<QString, int>::const_iterator i = keys.begin();
		while (i != keys.constEnd()) {
			if (!i.key().isEmpty()) //avoid crash
				config->setValue(i.key(), i.value());
			++i;
		}
	}
	config->endGroup();

	config->endGroup();

	config->beginGroup("formats");
	QFormatFactory defaultFormats(":/qxs/defaultFormats.qxf", this); //load default formats from resource file
	m_formats->save(*config, &defaultFormats);
	config->endGroup();

	// save usageCount in file of its own.
	if (!asProfile) {
		QFile file(configManager.configBaseDir + "wordCount.usage");
		if (file.open(QIODevice::WriteOnly)) {
			QDataStream out(&file);
			out << (quint32)0xA0B0C0D0;  //magic number
			out << (qint32)1; //version
			out.setVersion(QDataStream::Qt_4_0);
			QMap<uint, QPair<int, int> >::const_iterator i = conf->usage.constBegin();
			while (i != conf->usage.constEnd()) {
				QPair<int, int> elem = i.value();
				if (elem.second > 0) {
					out << i.key();
					out << elem.first;
					out << elem.second;
				}
				++i;
			}
		}
	}

	if (asProfile)
		delete config;
}

void Texstudio::restoreDefaultSettings()
{
	if (!txsConfirmWarning("This will reset all settings to their defaults. At the end, TeXstudio will be closed. Please start TeXstudio manually anew afterwards.\n\nDo you want to continue?")) {
		return;
	}
	if (canCloseNow(false)) {
		QFile f(configManager.configFileName);
		if (f.exists()) {
			if (f.open(QFile::WriteOnly)) {
				f.write("\n");  // delete contents of settings file
				f.close();
			} else {
				txsWarning(tr("Unable to write to settings file %1").arg(QDir::toNativeSeparators(f.fileName())));
			}
		}
		qApp->exit(0);
	}
}

////////////////// STRUCTURE ///////////////////
void Texstudio::showStructure()
{
	leftPanel->setCurrentWidget(structureTreeView);
}

/*
 * creates a (hopefully) unique string for a StructureEntry
 */
QString makeTag(StructureEntry *se, QString baseTag)
{
	return QString("%1:::%2**%3**%4**%5").arg(baseTag).arg(se->type).arg(se->title).arg(se->type == StructureEntry::SE_SECTION ? long(se->getLineHandle()) : 0).arg(se->columnNumber);
}

/*
 * adds unique tags for all expanded StructureEntries in structureTreeView to expandedEntryTags
 */
void Texstudio::getExpandedStructureEntries(const QModelIndex &index, QSet<QString> &expandedEntryTags, QString baseTag)
{
	QTreeView *view = structureTreeView;
	QAbstractItemModel *model = view->model();
	if (model->hasChildren(index))	{
		int nRows = model->rowCount(index);
		int nCols = model->columnCount(index);
		for (int row = 0; row < nRows; row++ ) {
			for (int col = 0; col < nCols; col++ ) {
				QModelIndex childIdx = model->index(row, col, index);
				if (!childIdx.isValid())
					continue;
				StructureEntry *se = LatexDocumentsModel::indexToStructureEntry(childIdx);
				QString tag = makeTag(se, baseTag);
				if (view->isExpanded(childIdx)) {
					expandedEntryTags.insert(tag);
				}
				getExpandedStructureEntries(childIdx, expandedEntryTags, tag);
			}
		}
	}
}

/*
 * expands all StructureEntries in structureTreeView whoes unique tags are included in expandedEntryTags
 */
void Texstudio::expandStructureEntries(const QModelIndex index, const QSet<QString> &expandedEntryTags, QString baseTag)
{
	QTreeView *view = structureTreeView;
	QAbstractItemModel *model = view->model();
	if (model->hasChildren(index))	{
		int nRows = model->rowCount(index);
		int nCols = model->columnCount(index);
		for (int row = 0; row < nRows; row++ ) {
			for (int col = 0; col < nCols; col++ ) {
				QModelIndex childIdx = model->index(row, col, index);
				if (!childIdx.isValid())
					continue;
				StructureEntry *se = LatexDocumentsModel::indexToStructureEntry(childIdx);
				QString tag = makeTag(se, baseTag);
				if (expandedEntryTags.contains(tag)) {
					view->setExpanded(childIdx, true);
				}
				expandStructureEntries(childIdx, expandedEntryTags, tag);
			}
		}
	}
}

void Texstudio::updateStructure(bool initial, LatexDocument *doc, bool hidden)
{
	// collect user define tex commands for completer
	// initialize List
	if ((!currentEditorView() || !currentEditorView()->document) && !doc) return;
	if (!doc)
		doc = currentEditorView()->document;
	if (initial) {
		//int len=doc->lineCount();
		doc->patchStructure(0, -1);
		// doc->patchStructure(0,-1,true); // do a second run, if packages are loaded (which might define new commands)
		// admitedly this solution is expensive (though working)
		//TODO: does not working when entering \usepackage in text ... !

		doc->updateMagicCommentScripts();
		configManager.completerConfig->userMacros << doc->localMacros;
		updateUserMacros();
	} else {
		// updateStructure() rebuilds the complete structure model. Therefore, all expansion states in the view are lost
		// to work around this, we save the a tag (unique idetifier) of all expanded entries and restore the expansion state after update
		QSet<QString> expandedEntryTags;
		getExpandedStructureEntries(structureTreeView->rootIndex(), expandedEntryTags);
		structureTreeView->setUpdatesEnabled(false);
		doc->updateStructure();
		expandStructureEntries(structureTreeView->rootIndex(), expandedEntryTags);
		structureTreeView->setUpdatesEnabled(true);
	}

	if (!hidden) {
		updateCompleter(doc->getEditorView());
		cursorPositionChanged();
	}

	//structureTreeView->reset();
}

void Texstudio::clickedOnStructureEntry(const QModelIndex &index)
{
	const StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(index);
	if (!entry) return;
	if (!entry->document) return;

	if (QApplication::mouseButtons() == Qt::RightButton) return; // avoid jumping to line if contextmenu is called

	LatexDocument *doc = entry->document;
	switch (entry->type) {
	case StructureEntry::SE_DOCUMENT_ROOT:
		if (entry->document->getEditorView())
			editors->setCurrentEditor(entry->document->getEditorView());
		else
			load(entry->document->getFileName());
		break;

	case StructureEntry::SE_OVERVIEW:
		break;
	case StructureEntry::SE_MAGICCOMMENT:
		if (entry->valid) {
			QString s = entry->title;
			QString name;
			QString val;
			doc->splitMagicComment(s, name, val);
			if ((name.toLower() == "texroot") || (name.toLower() == "root")) {
				QString fname = doc->findFileName(val);
				load(fname);
				break;
			}
		}
	case StructureEntry::SE_SECTION:
	case StructureEntry::SE_TODO:
	case StructureEntry::SE_LABEL: {
		int lineNr = -1;
		mDontScrollToItem = entry->type != StructureEntry::SE_SECTION;
		LatexEditorView *edView = entry->document->getEditorView();
		QEditor::MoveFlags mflags = QEditor::NavigationToHeader;
		if (!entry->document->getEditorView()) {
			lineNr = entry->getRealLineNumber();
			edView = load(entry->document->getFileName());
			if (!edView) return;
			mflags &= ~QEditor::Animated;
			//entry is now invalid
		} else lineNr = LatexDocumentsModel::indexToStructureEntry(index)->getRealLineNumber();
		gotoLine(lineNr, 0, edView, mflags);
		break;
	}

	case StructureEntry::SE_INCLUDE:
	case StructureEntry::SE_BIBTEX: {
		saveCurrentCursorToHistory();
		QString defaultExt = entry->type == StructureEntry::SE_BIBTEX ? ".bib" : ".tex";
		openExternalFile(entry->title, defaultExt, entry->document);
		break;
	}
	}
}

void Texstudio::editRemovePlaceHolders()
{
	if (!currentEditor()) return;
	for (int i = currentEditor()->placeHolderCount(); i >= 0; i--)
		currentEditor()->removePlaceHolder(i);
	currentEditor()->viewport()->update();
}

void Texstudio::editRemoveCurrentPlaceHolder()
{
	if (!currentEditor()) return;
	currentEditor()->removePlaceHolder(currentEditor()->currentPlaceHolder());
}

//////////TAGS////////////////
void Texstudio::normalCompletion()
{
	if (!currentEditorView())	return;

	QString command;
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QDocumentLineHandle *dlh = c.line().handle();
	//LatexParser::ContextType ctx=view->lp.findContext(word, c.columnNumber(), command, value);
	TokenStack ts = getContext(dlh, c.columnNumber());
	Token tk;
	if (!ts.isEmpty()) {
		tk = ts.top();
		if (tk.type == Token::word && tk.subtype == Token::none && ts.size() > 1) {
			// set brace type
			ts.pop();
			tk = ts.top();
		}
	}

	Token::TokenType type = tk.type;
	if (tk.subtype != Token::none)
		type = tk.subtype;
	if (type == Token::specialArg) {
		int df = int(type - Token::specialArg);
		QString cmd = latexParser.mapSpecialArgs.value(df);
		if (mCompleterNeedsUpdate) updateCompleter();
		completer->setWorkPath(cmd);
        currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_SPECIALOPTION);
	}
	switch (type) {
	case Token::command:
	case Token::commandUnknown:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST);
		break;
	case Token::env:
	case Token::beginEnv:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST);
		break;
	case Token::labelRef:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_REF);
		break;
	case Token::labelRefList:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_REF | LatexCompleter::CF_FORCE_REFLIST);
		break;
	case Token::bibItem:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_CITE);
		break;
	case Token::width:
		if (mCompleterNeedsUpdate) updateCompleter();
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_LENGTH);
		break;
	case Token::imagefile: {
		QString fn = documents.getCompileFileName();
		QFileInfo fi(fn);
		completer->setWorkPath(fi.absolutePath());
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_GRAPHIC);
	}
	break;
	case Token::file: {
		QString fn = documents.getCompileFileName();
		QFileInfo fi(fn);
		completer->setWorkPath(fi.absolutePath());
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_GRAPHIC);
	}
	break;
	case Token::color:
		if (mCompleterNeedsUpdate) updateCompleter();
		completer->setWorkPath("%color");
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_SPECIALOPTION); //TODO: complete support for special opt
		break;
	case Token::keyValArg:
	case Token::keyVal_key:
	case Token::keyVal_val: {
		QString word = c.line().text();
		int col = c.columnNumber();
		command = getCommandFromToken(tk);

		completer->setWorkPath(command);
		if (!completer->existValues()) {
			// no keys found for command
			// command/arg structure ? (yathesis)
			TokenList tl = dlh->getCookieLocked(QDocumentLine::LEXER_COOKIE).value<TokenList>();
			QString subcommand;
			int add = (type == Token::keyVal_val) ? 2 : 1;
			if (tk.type == Token::braces || tk.type == Token::squareBracket)
				add = 0;
			for (int k = tl.indexOf(tk) + 1; k < tl.length(); k++) {
				Token tk_elem = tl.at(k);
				if (tk_elem.level > tk.level - add)
					continue;
				if (tk_elem.level < tk.level - add)
					break;
				if (tk_elem.type == Token::braces) {
					subcommand = word.mid(tk_elem.start + 1, tk_elem.length - 2);
					break;
				}
			}
			if (!subcommand.isEmpty())
				command = command + "/" + subcommand;
			completer->setWorkPath(command);
		}

		bool existValues = completer->existValues();
		// check if c is after keyval
		if (col > tk.start + tk.length) {
			QString interposer = word.mid(tk.start + tk.length, col - tk.start - tk.length);
			if (!interposer.contains(",") && interposer.contains("=")) {
				//assume val for being after key
				command = command + "/" + tk.getText();
				completer->setWorkPath(command);
				existValues = completer->existValues();
			}
		} else {
			if (ts.size() > 1) {
				Token elem = ts.at(ts.size() - 2);
				if (elem.type == Token::keyVal_key && elem.level == tk.level - 1) {
					command = command + "/" + elem.getText();
					completer->setWorkPath(command);
					existValues = completer->existValues();
				}
			}
		}
		completer->setWorkPath(command);
		if (existValues)
			currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_KEYVAL);
	}
	break;
	case Token::beamertheme: {
		QString preambel = "beamertheme";
		currentPackageList.clear();
		foreach (QString elem, latexPackageList) {
			if (elem.startsWith(preambel))
				currentPackageList << elem.mid(preambel.length());
		}
	}
	completer->setPackageList(&currentPackageList);
	currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_PACKAGE);
	break;
	case Token::package:
		completer->setPackageList(&latexPackageList);
		currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_FORCE_PACKAGE);
		break;

	default:
		insertTextCompletion();
	}
}

void Texstudio::insertEnvironmentCompletion()
{
	if (!currentEditorView())	return;
	if (mCompleterNeedsUpdate) updateCompleter();
	QDocumentCursor c = currentEditorView()->editor->cursor();
	if (c.hasSelection()) {
		currentEditor()->cutBuffer = c.selectedText();
		c.removeSelectedText();
	}
	QString eow = getCommonEOW();
	while (c.columnNumber() > 0 && !eow.contains(c.previousChar())) c.movePosition(1, QDocumentCursor::PreviousCharacter);

	static const QString environmentStart = "\\begin{";

	currentEditor()->document()->clearLanguageMatches();
	if (!c.line().text().left(c.columnNumber()).endsWith(environmentStart)) {
		c.insertText(environmentStart);//remaining part is up to the completion engine
	}

	currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST);
}

// tries to complete normal text
// only starts up if already 2 characters have been typed in
void Texstudio::insertTextCompletion()
{
	if (!currentEditorView())    return;
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QString eow = getCommonEOW();

	if (c.columnNumber() == 0 || eow.contains(c.previousChar()))
		return;

	int col = c.columnNumber();
	QString line = c.line().text();
	for (; col > 0 && !eow.contains(line[col - 1]); col-- )
		;

    QString word = line.mid(col, c.columnNumber() - col);
    QSet<QString> words;

    QDocument *doc=currentEditor()->document();

    for(int i=0;i<doc->lineCount();i++){
        QDocumentLineHandle *dlh=doc->line(i).handle();
        TokenList tl = dlh->getCookieLocked(QDocumentLine::LEXER_COOKIE).value<TokenList>();
        QString txt;
        for(int k=0;k<tl.size();k++) {
            Token tk=tl.at(k);
            if(!txt.isEmpty() || (tk.type==Token::word && (tk.subtype==Token::none || tk.subtype==Token::text || tk.subtype==Token::generalArg || tk.subtype==Token::title || tk.subtype==Token::todo))){
                txt+=tk.getText();
                if(txt.startsWith(word)){
                    if(word.length()<txt.length()){
                        words<<txt;
                    }
                    // advance k if tk comprehends several sub-tokens (braces)
                    while(k+1<tl.size() && tl.at(k+1).start<(tk.start+tk.length)){
                        k++;
                    }
                    // add more variants for variable-name like constructions
                    if(k+2<tl.size()){
                        Token tk2=tl.at(k+1);
                        Token tk3=tl.at(k+2);
                        if(tk2.length==1 && tk2.start==tk.start+tk.length && tk2.type==Token::punctuation&&tk3.start==tk2.start+tk2.length){
                            // next token is directly adjacent and of length 1
                            QString txt2=tk2.getText();
                            if(txt2=="_" || txt2=="-"){
                                txt.append(txt2);
                                k++;
                                continue;
                            }
                            if(txt2=="'" && tk3.type==Token::word){ // e.g. don't but not abc''
                                txt.append(txt2);
                                k++;
                                continue;
                            }
                        }
                        // combine abc\_def
                        if(tk2.length==2 && tk2.start==tk.start+tk.length && (tk2.type==Token::command||tk2.type==Token::commandUnknown)&&tk3.start==tk2.start+tk2.length){
                            // next token is directly adjacent and of length 1
                            QString txt2=tk2.getText();
                            if(txt2=="\\_" ){
                                txt.append(txt2);
                                k++;
                                continue;
                            }
                        }
                        // previous was an already appended command, check if argument is present
                        if(tk.type==Token::command){
                            if(tk2.level==tk.level && tk2.subtype!=Token::none){
                                txt.append(tk2.getText());
                                words<<txt;
                                k++;
                            }
                        }
                    }
                }
            }
            txt.clear();
        }

    }
    /*QString my_text = currentEditorView()->editor->text();
	int end = 0;
	int k = 0; // number of occurences of search word.
	QString word = line.mid(col, c.columnNumber() - col);
	//TODO: Boundary needs to specified more exactly
	//TODO: type in text needs to be excluded, if not already present
	//TODO: editor->text() is far too slow
	QSet<QString> words;
	int i;
	while ((i = my_text.indexOf(QRegExp("\\b" + word), end)) > 0) {
		end = my_text.indexOf(QRegExp("\\b"), i + 1);
		if (end > i) {
            bool addVar=false;
            do {
                addVar=false;
                if (word == my_text.mid(i, end - i)) {
                    k = k + 1;
                    if (k == 2) words << my_text.mid(i, end - i);
                } else {
                    QString txt=my_text.mid(i, end - i);
                    if(txt.endsWith("_")){
                        if(my_text.mid(end,1)=="\\"||my_text.mid(end,1)=="{"){
                            // special handling for abc_\cmd{dsfsdf}
                            QRegExp rx("(\\\\[a-zA-Z]+)?(\\{\\w+\\})?"); // better solution would be employing tokens ...
                            int zw=rx.indexIn(my_text,end);
                            if(zw==end){
                                txt.append(rx.cap());
                                words << txt;
                            }
                        }
                    }else{
                        if (!words.contains(txt))
                            words << txt;
                    }
                }
                // add more variants if word boundary is \_ \- or -
                if(my_text.length()>end+1){
                    addVar|=(my_text.mid(end,2)=="\\_");
                    addVar|=(my_text.mid(end,2)=="\\-");
                    if(addVar)
                        end++;
                }
                if(my_text.mid(end,1)=="-")
                    addVar=true;
                if(addVar){
                    end = my_text.indexOf(QRegExp("\\b"), end+2);
                    addVar=(end>i);
                }
            } while(addVar);
		} else {
			if (word == my_text.mid(i, end - i)) {
				k = k + 1;
				if (k == 2) words << my_text.mid(i, end - i);
			} else {
				if (!words.contains(my_text.mid(i, end - i)))
					words << my_text.mid(i, my_text.length() - i);
			}
		}
	}
    */
	completer->setAdditionalWords(words, CT_NORMALTEXT);
	currentEditorView()->complete(LatexCompleter::CF_FORCE_VISIBLE_LIST | LatexCompleter::CF_NORMAL_TEXT);
}

void Texstudio::insertTag(const QString &Entity, int dx, int dy)
{
	if (!currentEditorView()) return;
	int curline, curindex;
	currentEditor()->getCursorPosition(curline, curindex);
	currentEditor()->write(Entity);
	if (dy == 0) currentEditor()->setCursorPosition(curline, curindex + dx);
	else if (dx == 0) currentEditor()->setCursorPosition(curline + dy, 0);
	else currentEditor()->setCursorPosition(curline + dy, curindex + dx);
	currentEditor()->setFocus();
	//	outputView->setMessage("");
	//logViewerTabBar->setCurrentIndex(0);
	//OutputTable->hide();
	//logpresent=false;
}

/*!
  \brief Inserts a citation at the cursor position.

  \a text may be either a complete (also custom) command like \mycite{key} or just the key. In the
  latter case it is expanded to \cite{key}. Key may also be a comma separated list of keys.
  The cursor context is evaluated: If it is within a citation command only the key is inserted at the correct
  position within the existing citation. If not, the whole command is inserted.
*/
void Texstudio::insertCitation(const QString &text)
{
	QString citeCmd, citeKey;

	if (text.length() > 1 && text.at(0) == '\\') {
		LatexParser::ContextType ctx = latexParser.findContext(text, 1, citeCmd, citeKey);
		if (LatexParser::Command != ctx) {
			citeCmd = "";
			citeKey = text;
		}
	} else {
		citeCmd = "";
		citeKey = text;
	}

	if (!currentEditorView()) return;
	QDocumentCursor c = currentEditor()->cursor();
	QString line = c.line().text();
	int cursorCol = c.columnNumber();
	QString command, value;
	LatexParser::ContextType context = latexParser.findContext(line, cursorCol, command, value);

	// Workaround: findContext yields Citation for \cite{..}|\n, but cursorCol is beyond the line,
	// which causes a crash when determining the insertCol later on.
	if (context == LatexParser::Citation && cursorCol == line.length() && cursorCol > 0) cursorCol--;

	// if cursor is directly behind a cite command, isert into that command
	if (context != LatexParser::Citation && cursorCol > 0) {
		LatexParser::ContextType prevContext = LatexParser::Unknown;
		prevContext = latexParser.findContext(line, cursorCol - 1, command, value);
		if (prevContext == LatexParser::Citation) {
			cursorCol--;
			context = prevContext;
		}
	}


	int insertCol = -1;
	if (context == LatexParser::Command && latexParser.possibleCommands["%cite"].contains(command)) {
		insertCol = line.indexOf('{', cursorCol) + 1;
	} else if (context == LatexParser::Citation) {
		if (cursorCol <= 0) return; // should not be possible,
		if (line.at(cursorCol) == '{' || line.at(cursorCol) == ',') {
			insertCol = cursorCol + 1;
		} else if (line.at(cursorCol - 1) == '{' || line.at(cursorCol - 1) == ',') {
			insertCol = cursorCol;
		} else {
			int nextComma = line.indexOf(',', cursorCol);
			int closingBracket = line.indexOf('}', cursorCol);
			if (nextComma >= 0 && (closingBracket == -1 || closingBracket > nextComma)) {
				insertCol = nextComma + 1;
			} else if (closingBracket >= 0) {
				insertCol = closingBracket;
			}
		}
	} else {
		QString tag;
		if (citeCmd.isEmpty())
			tag = citeCmd = "\\cite{" + citeKey + "}";
		else
			tag = text;
		insertTag(tag, tag.length());
		return;
	}

	if (insertCol < 0 || insertCol >= line.length()) return;

	currentEditor()->setCursorPosition(c.lineNumber(), insertCol);
	// now the insertCol is either behind '{', behind ',' or at '}'
	if (insertCol > 0 && line.at(insertCol - 1) == '{') {
		if (line.at(insertCol) == '}') {
			insertTag(citeKey, citeKey.length());
		} else {
			insertTag(citeKey + ",", citeKey.length() + 1);
		}
	} else if (insertCol > 0 && line.at(insertCol - 1) == ',') {
		insertTag(citeKey + ",", citeKey.length() + 1);
	} else {
		insertTag("," + citeKey, citeKey.length() + 1);
	}
}

void Texstudio::insertFormula(const QString &formula)
{
	if (!currentEditorView()) return;

	QString fm = formula;
	QDocumentCursor cur = currentEditorView()->editor->cursor();

	//TODO: Is there a more elegant solution to determine if the cursor is inside a math environment.
	QDocumentLine dl = cur.line();
	int col = cur.columnNumber();
	QList<int> mathFormats = QList<int>() << m_formats->id("numbers") << m_formats->id("math-keyword");
	int mathDelimiter = m_formats->id("math-delimiter");
	mathFormats.removeAll(0); // keep only valid entries in list
	if (mathFormats.contains(dl.getFormatAt(col))) {     // inside math
		fm = fm.mid(1, fm.length() - 2);                 // removes surrounding $...$
	} else if (dl.getFormatAt(col) == mathDelimiter) {   // on delimiter
		while (col > 0 && dl.getFormatAt(col - 1) == mathDelimiter) col--;
		if (mathFormats.contains(dl.getFormatAt(col))) { // was an end-delimiter
			cur.setColumnNumber(col);
			currentEditorView()->editor->setCursor(cur);
			fm = fm.mid(1, fm.length() - 2);             // removes surrounding $...$
		} else {
			//TODO is terhe a better way than hard coding? Since there is no difference in the formats between
			//start and end tags. \[|\] is hard identify without.
			QString editorFormula = dl.text().mid(col);
			if (editorFormula.startsWith("\\[")) {
				col += 2;
				currentEditorView()->editor->setCursor(cur);
				fm = fm.mid(1, fm.length() - 2);         // removes surrounding $...$
			} else if (editorFormula.startsWith("$")) {
				col += 1;
				currentEditorView()->editor->setCursor(cur);
				fm = fm.mid(1, fm.length() - 2);         // removes surrounding $...$
			} else {
				qDebug() << "Unknown math formula tag. Giving up trying to locate formula boundaries.";
			}
		}
	}

	insertTag(fm, fm.length());
}

void Texstudio::insertSymbolPressed(QTableWidgetItem *)
{
	mb = QApplication::mouseButtons();
}

void Texstudio::insertSymbol(QTableWidgetItem *item)
{

	if (mb == Qt::RightButton) return; // avoid jumping to line if contextmenu is called

	QString code_symbol;
	if (item) {
		int cnt = item->data(Qt::UserRole).toInt();
		if (item->data(Qt::UserRole + 1).isValid()) {
			item = item->data(Qt::UserRole + 1).value<QTableWidgetItem *>();
			cnt = item->data(Qt::UserRole).toInt();
		}
		if (configManager.insertUTF && item->data(Qt::UserRole + 4).isValid()) {
			code_symbol = item->data(Qt::UserRole + 4).toString();
		} else {
			code_symbol = item->text();
		}
		item->setData(Qt::UserRole, cnt + 1);
		insertTag(code_symbol, code_symbol.length(), 0);
		setMostUsedSymbols(item);
	}
}

void Texstudio::insertXmlTag(QListWidgetItem *item)
{
	if (!currentEditorView())	return;
	if (item  && !item->font().bold()) {
		QString code = item->data(Qt::UserRole).toString();
		QDocumentCursor c = currentEditorView()->editor->cursor();
		CodeSnippet(code).insertAt(currentEditorView()->editor, &c);
		currentEditorView()->editor->setFocus();
	}
}

void Texstudio::insertXmlTagFromToolButtonAction()
{
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	QToolButton *button = comboToolButtonFromAction(action);
	if (!button) return;
	button->setDefaultAction(action);

	QString tagsID = button->property("tagsID").toString();
	int tagCategorySep = tagsID.indexOf("/", 5);
	XmlTagsListWidget *tagsWidget = findChild<XmlTagsListWidget *>(tagsID.left(tagCategorySep));
	if (!tagsWidget) return;
	QString code = tagsWidget->tagsFromTagTxt(action->text());
	CodeSnippet(code).insert(currentEditorView()->editor);
	currentEditorView()->editor->setFocus();
}

void Texstudio::callToolButtonAction()
{
	QAction *action = qobject_cast<QAction *>(sender());
	QToolButton *button = comboToolButtonFromAction(action);
	REQUIRE(button && button->defaultAction() && button->menu());
	button->setDefaultAction(action);

	QString menuID = button->property("menuID").toString();
	QMenu *menu = configManager.getManagedMenu(menuID);
	if (!menu) return;

	int index = button->menu()->actions().indexOf(action);
	REQUIRE(index >= 0);
	REQUIRE(index < menu->actions().size());
	QList<QAction *> actions = menu->actions();
	for (int i = 0; i < actions.size(); i++) {
		if (actions[i]->isSeparator()) continue;
		if (index == 0) {
			actions[i]->trigger();
			break;
		} else index--;
	}
}

void Texstudio::insertFromAction()
{
	LatexEditorView *edView = currentEditorView();
	if (!edView)	return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)	{
		if (completer->isVisible())
			completer->close();
		execMacro(Macro::fromTypedTag(action->data().toString()), MacroExecContext(), true);
		generateMirror();
		outputView->setMessage(CodeSnippet(action->whatsThis(), false).lines.join("\n"));
	}
}

void Texstudio::insertBib()
{
	if (!currentEditorView())	return;
	//currentEditorView()->editor->viewport()->setFocus();
	QString tag;
	tag = QString("\\bibliography{");
	tag += currentEditor()->fileInfo().completeBaseName();
	tag += QString("}\n");
	insertTag(tag, 0, 1);
	outputView->setMessage(QString("The argument to \\bibliography refers to the bib file (without extension)\n") +
	                       "which should contain your database in BibTeX format.\n" +
	                       "TeXstudio inserts automatically the base name of the TeX file");
}

void Texstudio::quickTabular()
{
	if ( !currentEditorView() )	return;
	QString placeholder;//(0x2022);
	QStringList borderlist, alignlist;
	borderlist << QString("|") << QString("||") << QString("") << QString("@{}");
	alignlist << QString("c") << QString("l") << QString("r") << QString("p{3cm}") << QString(">{\\raggedright\\arraybackslash}p{3cm}") << QString(">{\\centering\\arraybackslash}p{%<3cm%>}") << QString(">{\\raggedleft\\arraybackslash}p{3cm}");
	QString al = "";
	QString vs = "";
	QString el = "";
	QString tag;
	TabDialog *quickDlg = new TabDialog(this, "Tabular");
	QTableWidgetItem *item = new QTableWidgetItem();
	if ( quickDlg->exec() ) {
		int y = quickDlg->ui.spinBoxRows->value();
		int x = quickDlg->ui.spinBoxColumns->value();
		tag = QString("\\begin{tabular}{");
		for ( int j = 0; j < x; j++) {
			tag += borderlist.at(quickDlg->colDataList.at(j).leftborder);
			tag += alignlist.at(quickDlg->colDataList.at(j).alignment);
		}
		tag += borderlist.at(quickDlg->ui.comboBoxEndBorder->currentIndex());
		tag += QString("}\n");
		for ( int i = 0; i < y; i++) {
			if (quickDlg->liDataList.at(i).topborder) tag += QString("\\hline \n");
			if (quickDlg->ui.checkBoxMargin->isChecked()) tag += "\\rule[-1ex]{0pt}{2.5ex} ";
			if (quickDlg->liDataList.at(i).merge && (quickDlg->liDataList.at(i).mergeto > quickDlg->liDataList.at(i).mergefrom)) {
				el = "";
				for ( int j = 0; j < x; j++) {
					item = quickDlg->ui.tableWidget->item(i, j);

					if (j == quickDlg->liDataList.at(i).mergefrom - 1) {
						if (item) el += item->text();
						tag += QString("\\multicolumn{");
						tag += QString::number(quickDlg->liDataList.at(i).mergeto - quickDlg->liDataList.at(i).mergefrom + 1);
						tag += QString("}{");
						if ((j == 0) && (quickDlg->colDataList.at(j).leftborder < 2)) tag += borderlist.at(quickDlg->colDataList.at(j).leftborder);
						if (quickDlg->colDataList.at(j).alignment < 3) tag += alignlist.at(quickDlg->colDataList.at(j).alignment);
						else tag += QString("c");
						if (quickDlg->liDataList.at(i).mergeto == x) tag += borderlist.at(quickDlg->ui.comboBoxEndBorder->currentIndex());
						else tag += borderlist.at(quickDlg->colDataList.at(quickDlg->liDataList.at(i).mergeto).leftborder);
						tag += QString("}{");
					} else if (j == quickDlg->liDataList.at(i).mergeto - 1) {
						if (item) el += item->text();
						if (el.isEmpty()) el = placeholder;
						tag += el + QString("}");
						if (j < x - 1) tag += " & ";
						else tag += QString(" \\\\ \n");
					} else if ((j > quickDlg->liDataList.at(i).mergefrom - 1) && (j < quickDlg->liDataList.at(i).mergeto - 1)) {
						if (item) el += item->text();
					} else {
						if (item) {
							if (item->text().isEmpty()) tag += placeholder;
							else tag += item->text();
						} else tag += placeholder;
						if (j < x - 1) tag += " & ";
						else tag += QString(" \\\\ \n");
					}

				}
			} else {
				for ( int j = 0; j < x - 1; j++) {
					item = quickDlg->ui.tableWidget->item(i, j);
					if (item) {
						if (item->text().isEmpty()) tag += placeholder + QString(" & ");
						else tag += item->text() + QString(" & ");
					} else tag += placeholder + QString(" & ");
				}
				item = quickDlg->ui.tableWidget->item(i, x - 1);
				if (item) {
					if (item->text().isEmpty()) tag += placeholder + QString(" \\\\ \n");
					else tag += item->text() + QString(" \\\\ \n");
				} else tag += placeholder + QString(" \\\\ \n");
			}
		}
		if (quickDlg->ui.checkBoxBorderBottom->isChecked()) tag += QString("\\hline \n\\end{tabular} ");
		else tag += QString("\\end{tabular} ");
		if (tag.contains("arraybackslash")) tag = "% \\usepackage{array} is required\n" + tag;
		insertTag(tag, 0, 0);
	}

}

void Texstudio::quickArray()
{
	if (!currentEditorView())	return;
	//TODO: move this in arraydialog class
	QString al;
	ArrayDialog *arrayDlg = new ArrayDialog(this, "Array");
	if (arrayDlg->exec()) {
		int y = arrayDlg->ui.spinBoxRows->value();
		int x = arrayDlg->ui.spinBoxColumns->value();
		QString env = arrayDlg->ui.comboEnvironment->currentText();
		QString tag = QString("\\begin{") + env + "}";
		if (env == "array") {
			tag += "{";
			if ((arrayDlg->ui.comboAlignment->currentIndex()) == 0) al = QString("c");
			if ((arrayDlg->ui.comboAlignment->currentIndex()) == 1) al = QString("l");
			if ((arrayDlg->ui.comboAlignment->currentIndex()) == 2) al = QString("r");
			for (int j = 0; j < x; j++) {
				tag += al;
			}
			tag += "}";
		}
		tag += QString("\n");
		for (int i = 0; i < y - 1; i++) {
			for (int j = 0; j < x - 1; j++) {
				QTableWidgetItem *item = arrayDlg->ui.tableWidget->item(i, j);
				if (item) tag += item->text() + QString(" & ");
				else tag += QString(" & ");
			}
			QTableWidgetItem *item = arrayDlg->ui.tableWidget->item(i, x - 1);
			if (item) tag += item->text() + QString(" \\\\ \n");
			else tag += QString(" \\\\ \n");
		}
		for (int j = 0; j < x - 1; j++) {
			QTableWidgetItem *item = arrayDlg->ui.tableWidget->item(y - 1, j);
			if (item) tag += item->text() + QString(" & ");
			else tag += QString(" & ");
		}
		QTableWidgetItem *item = arrayDlg->ui.tableWidget->item(y - 1, x - 1);
		if (item) tag += item->text() + QString("\n\\end{") + env + "} ";
		else tag += QString("\n\\end{") + env + "} ";
		insertTag(tag, 0, 0);
	}
}


// returns true if line is inside in the specified environment. In that case start and end lines of the environment are supplied
bool findEnvironmentLines(const QDocument *doc, const QString &env, int line, int &startLine, int &endLine, int scanRange)
{
	QString name, arg;

	startLine = -1;
	for (int l = line; l >= 0; l--) {
		if (scanRange > 0 && line - l > scanRange) break;
		if (findTokenWithArg(doc->line(l).text(), "\\end{", name, arg) && name == env) {
			if (l < line) return false;
		}
		if (findTokenWithArg(doc->line(l).text(), "\\begin{", name, arg) && name == env) {
			startLine = l;
			break;
		}
	}
	if (startLine == -1) return false;

	endLine = -1;
	for (int l = line; l < doc->lineCount(); l++) {
		if (scanRange > 0 && l - line > scanRange) break;
		if (findTokenWithArg(doc->line(l).text(), "\\end{", name, arg) && name == env) {
			endLine = l;
			break;
		}
		if (findTokenWithArg(doc->line(l).text(), "\\begin{", name, arg) && name == env) {
			if (l > line) return false; //second begin without end
		}
	}
	if (endLine == -1) return false;
	return true;
}

void Texstudio::quickGraphics(const QString &graphicsFile)
{
	if (!currentEditorView()) return;

	InsertGraphics *graphicsDlg = new InsertGraphics(this, configManager.insertGraphicsConfig);

	QEditor *editor = currentEditor();

	int startLine, endLine, cursorLine, cursorCol;
	editor->getCursorPosition(cursorLine, cursorCol);
	QDocument *doc = editor->document();

	QDocumentCursor cur = currentEditor()->cursor();
	QDocumentCursor origCur = cur;
	origCur.setAutoUpdated(true);

	bool hasCode = false;
	if (findEnvironmentLines(doc, "figure", cursorLine, startLine, endLine, 20)) {
		cur.moveTo(startLine, 0, QDocumentCursor::MoveAnchor);
		cur.moveTo(endLine + 1, 0, QDocumentCursor::KeepAnchor);
		hasCode = true;
	} else if (findEnvironmentLines(doc, "figure*", cursorLine, startLine, endLine, 20)) {
		cur.moveTo(startLine, 0, QDocumentCursor::MoveAnchor);
		cur.moveTo(endLine + 1, 0, QDocumentCursor::KeepAnchor);
		hasCode = true;
	} else if (findEnvironmentLines(doc, "center", cursorLine, startLine, endLine, 3)) {
		cur.moveTo(startLine, 0, QDocumentCursor::MoveAnchor);
		cur.moveTo(endLine + 1, 0, QDocumentCursor::KeepAnchor);
		hasCode = true;
	} else if (currentEditor()->text(cursorLine).contains("\\includegraphics")) {
		cur.moveTo(cursorLine, 0, QDocumentCursor::MoveAnchor);
		cur.moveTo(cursorLine + 1, 0, QDocumentCursor::KeepAnchor);
		hasCode = true;
	}

	if (hasCode) {
		editor->setCursor(cur);
		graphicsDlg->setCode(cur.selectedText());
	}

	QFileInfo docInfo = currentEditorView()->document->getFileInfo();
	graphicsDlg->setTexFile(docInfo);
	graphicsDlg->setMasterTexFile(currentEditorView()->document->parent->getCompileFileName());
	if (!graphicsFile.isNull()) graphicsDlg->setGraphicsFile(graphicsFile);

	if (graphicsDlg->exec()) {
		editor->insertText(cur, graphicsDlg->getCode());
	} else {
		editor->setCursor(origCur);
	}

	delete graphicsDlg;
}

void Texstudio::quickMath()
{
#ifdef Q_OS_WIN
	connectUnique(MathAssistant::instance(), SIGNAL(formulaReceived(QString)), this, SLOT(insertFormula(QString)));
	MathAssistant::instance()->exec();
#endif
}

void Texstudio::quickTabbing()
{
	if (!currentEditorView()) return;
	TabbingDialog *tabDlg = new TabbingDialog(this, "Tabbing");
	if (tabDlg->exec()) {
		int	x = tabDlg->ui.spinBoxColumns->value();
		int	y = tabDlg->ui.spinBoxRows->value();
		QString s = tabDlg->ui.lineEdit->text();
		QString tag = QString("\\begin{tabbing}\n");
		for (int j = 1; j < x; j++) {
			tag += "\\hspace{" + s + "}\\=";
		}
		tag += "\\kill\n";
		for (int i = 0; i < y - 1; i++) {
			for (int j = 1; j < x; j++) {
				tag += " \\> ";
			}
			tag += "\\\\ \n";
		}
		for (int j = 1; j < x; j++) {
			tag += " \\> ";
		}
		tag += QString("\n\\end{tabbing} ");
		insertTag(tag, 0, 2);
	}
}

void Texstudio::quickLetter()
{
	QString tag = QString("\\documentclass[");
	LetterDialog *ltDlg = new LetterDialog(this, "Letter");
	if (ltDlg->exec()) {
		if (!currentEditorView() ||
		        currentEditorView()->getDocument()->lineCount() > 1 || // first faster than text().isEmpty on large documents
		        !currentEditorView()->getDocument()->text().isEmpty()) {
			fileNew();
			Q_ASSERT(currentEditorView());
		}
		tag += ltDlg->ui.comboBoxPt->currentText() + QString(",");
		tag += ltDlg->ui.comboBoxPaper->currentText() + QString("]{letter}");
		tag += QString("\n");
		if (ltDlg->ui.comboBoxEncoding->currentText() != "NONE") tag += QString("\\usepackage[") + ltDlg->ui.comboBoxEncoding->currentText() + QString("]{inputenc}");
		if (ltDlg->ui.comboBoxEncoding->currentText().startsWith("utf8x")) tag += QString(" \\usepackage{ucs}");
		tag += QString("\n");
		if (ltDlg->ui.checkBox->isChecked()) tag += QString("\\usepackage{amsmath}\n\\usepackage{amsfonts}\n\\usepackage{amssymb}\n");
		tag += "\\address{your name and address} \n";
		tag += "\\signature{your signature} \n";
		tag += "\\begin{document} \n";
		tag += "\\begin{letter}{name and address of the recipient} \n";
		tag += "\\opening{saying hello} \n \n";
		tag += "write your letter here \n \n";
		tag += "\\closing{saying goodbye} \n";
		tag += "%\\cc{Cclist} \n";
		tag += "%\\ps{adding a postscript} \n";
		tag += "%\\encl{list of enclosed material} \n";
		tag += "\\end{letter} \n";
		tag += "\\end{document}";
		if (ltDlg->ui.checkBox->isChecked()) {
			insertTag(tag, 9, 5);
		} else {
			insertTag(tag, 9, 2);
		}
	}
}

void Texstudio::quickDocument()
{
	QuickDocumentDialog *startDlg = new QuickDocumentDialog(this, tr("Quick Start"));
	startDlg->Init();
	if (startDlg->exec()) {
		if (!currentEditorView() ||
		        currentEditorView()->getDocument()->lineCount() > 1 || // first faster than text().isEmpty on large documents
		        !currentEditorView()->getDocument()->text().isEmpty()) {
			fileNew();
			Q_ASSERT(currentEditorView());
		}
		Q_ASSERT(currentEditor());
		currentEditorView()->insertSnippet(startDlg->getNewDocumentText());
		QTextCodec *codec = Encoding::QTextCodecForLatexName(startDlg->document_encoding);
		if (codec && codec != currentEditor()->document()->codec()) {
			currentEditor()->document()->setCodec(codec);
			updateCaption();
		}
	}
	delete startDlg;
}

void Texstudio::quickBeamer()
{
	QuickBeamerDialog *startDlg = new QuickBeamerDialog(this, tr("Quick Beamer Presentation"));
	startDlg->Init();
	if (startDlg->exec()) {
		if (!currentEditorView() ||
		        currentEditorView()->getDocument()->lineCount() > 1 || // first faster than text().isEmpty on large documents
		        !currentEditorView()->getDocument()->text().isEmpty()) {
			fileNew();
			Q_ASSERT(currentEditorView());
		}
		Q_ASSERT(currentEditor());
		currentEditorView()->insertSnippet(startDlg->getNewDocumentText());
		QTextCodec *codec = Encoding::QTextCodecForLatexName(startDlg->document_encoding);
		if (codec && codec != currentEditor()->document()->codec()) {
			currentEditor()->document()->setCodec(codec);
			updateCaption();
		}
	}
	delete startDlg;
}

void Texstudio::insertBibEntryFromAction()
{
	if (!currentEditorView()) return;
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;

	QString insertText = BibTeXDialog::textToInsert(action->data().toString());
	if (!insertText.isEmpty())
		CodeSnippet(insertText, false).insert(currentEditor());
}

void Texstudio::insertBibEntry(const QString &id)
{
	QStringList possibleBibFiles;
	int usedFile = 0;
	if (currentEditor()) {
		if (currentEditor()->fileName().isEmpty())
			possibleBibFiles.prepend(tr("<Current File>"));
		else {
			usedFile = documents.mentionedBibTeXFiles.indexOf(currentEditor()->fileName());
			if (usedFile < 0 && !documents.mentionedBibTeXFiles.empty()) usedFile = 0;
		}
	}
	foreach (const QString &s, documents.mentionedBibTeXFiles)
		possibleBibFiles << QFileInfo(s).fileName();
	BibTeXDialog *bd = new BibTeXDialog(0, possibleBibFiles, usedFile, id);
	if (bd->exec()) {
		usedFile = bd->resultFileId;
		if (usedFile < 0 || usedFile >= possibleBibFiles.count()) fileNew();
		else if (currentEditor()->fileName().isEmpty() && usedFile == 0); //stay in current editor
		else if (QFileInfo(currentEditor()->fileName()) == QFileInfo(possibleBibFiles[usedFile])); //stay in current editor
		else {
			if (currentEditor()->fileName().isEmpty()) usedFile--;
			load(documents.mentionedBibTeXFiles[usedFile]);
			currentEditor()->setCursorPosition(currentEditor()->document()->lines() - 1, 0);
			bd->resultString = "\n" + bd->resultString;
		}

		CodeSnippet(bd->resultString, false).insert(currentEditor());
	}
	delete bd;
}

void Texstudio::setBibTypeFromAction()
{
	QMenu *menu = getManagedMenu("main/bibliography/type");
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	if (menu) {
		menu->setTitle(QString(tr("Type: %1")).arg(act->text()));
	}

	bool isBibtex = (act->data().toString() == "bibtex");
	bibtexEntryActions->setVisible(isBibtex);
	biblatexEntryActions->setVisible(!isBibtex);
	BibTeXDialog::setBibType(isBibtex ? BibTeXDialog::BIBTEX : BibTeXDialog::BIBLATEX);
}

void Texstudio::insertUserTag()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	int id = action->data().toInt();
	execMacro(configManager.completerConfig->userMacros.value(id, Macro()));
}

void Texstudio::execMacro(const Macro &m, const MacroExecContext &context, bool allowWrite)
{
	if (m.type == Macro::Script) {
		runScript(m.script(), context, allowWrite);
	} else {
		if (currentEditorView()) {
			currentEditorView()->insertSnippet(m.snippet());
		}
	}
}

void Texstudio::runScript(const QString &script, const MacroExecContext &context, bool allowWrite)
{
	scriptengine *eng = new scriptengine();
	eng->triggerMatches = context.triggerMatches;
	eng->triggerId = context.triggerId;
	if (currentEditorView()) eng->setEditorView(currentEditorView());

	eng->setScript(script, allowWrite);
	eng->run();
	if (!eng->globalObject) delete eng;
	else QObject::connect(reinterpret_cast<QObject *>(eng->globalObject), SIGNAL(destroyed()), eng, SLOT(deleteLater()));
}

void Texstudio::editMacros()
{
	if (!userMacroDialog)  {
		userMacroDialog = new UserMenuDialog(0, tr("Edit User &Tags"), m_languages);
		foreach (const Macro &m, configManager.completerConfig->userMacros) {
			if (m.name == "TMX:Replace Quote Open" || m.name == "TMX:Replace Quote Close" || m.document)
				continue;
			userMacroDialog->addMacro(m);
		}
		userMacroDialog->init();
		connect(userMacroDialog, SIGNAL(accepted()), SLOT(macroDialogAccepted()));
		connect(userMacroDialog, SIGNAL(rejected()), SLOT(macroDialogRejected()));
		connect(userMacroDialog, SIGNAL(runScript(QString)), SLOT(runScript(QString)));
	}
	userMacroDialog->show();
	userMacroDialog->raise();
	userMacroDialog->setFocus();
}

void Texstudio::macroDialogAccepted()
{
	configManager.completerConfig->userMacros.clear();
	for (int i = 0; i < userMacroDialog->macroCount(); i++) {
		configManager.completerConfig->userMacros << userMacroDialog->getMacro(i);
	}
	for (int i = 0; i < documents.documents.size(); i++)
		configManager.completerConfig->userMacros << documents.documents[i]->localMacros;
	updateUserMacros();
	completer->updateAbbreviations();
	userMacroDialog->deleteLater();
	userMacroDialog = 0;
}

void Texstudio::macroDialogRejected()
{
	userMacroDialog->deleteLater();
	userMacroDialog = 0;
}

void Texstudio::insertRef(const QString &refCmd)
{
	//updateStructure();

	LatexEditorView *edView = currentEditorView();
	QStringList labels;
	if (edView && edView->document) {
		QList<LatexDocument *> docs;
		docs << edView->document->getListOfDocs();
		foreach (const LatexDocument *doc, docs)
			labels << doc->labelItems();
	} else return;
	labels.sort();
	UniversalInputDialog dialog;
	dialog.addVariable(&labels, tr("Labels:"));
	if (dialog.exec() && !labels.isEmpty()) {
		QString tag = refCmd + "{" + labels.first() + "}";
		insertTag(tag, tag.length(), 0);
	} else
		insertTag(refCmd + "{}", refCmd.length() + 1, 0);
}

void Texstudio::insertRef()
{
	insertRef("\\ref");
}

void Texstudio::insertEqRef()
{
	insertRef("\\eqref");
}

void Texstudio::insertPageRef()
{
	insertRef("\\pageref");
}

void Texstudio::createLabelFromAction()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	StructureEntry *entry = qvariant_cast<StructureEntry *>(act->data());
	if (!entry || !entry->document) return;

	// find editor and line nr
	int lineNr = entry->getRealLineNumber();

	mDontScrollToItem = entry->type != StructureEntry::SE_SECTION;
	LatexEditorView *edView = entry->document->getEditorView();
	QEditor::MoveFlags mflags = QEditor::NavigationToHeader;
	if (!edView) {
		edView = load(entry->document->getFileName());
		if (!edView) return;
		mflags &= ~QEditor::Animated;
		//entry is now invalid
	}
	REQUIRE(edView->getDocument());

	if (lineNr < 0) return; //not found. (document was closed)

	// find column position after structure command
	QString lineText = edView->getDocument()->line(lineNr).text();
	int pos = -1;
	for (int i = 0; i < latexParser.structureDepth(); i++) {
		foreach (const QString &cmd, latexParser.possibleCommands[QString("%structure%1").arg(i)]) {
			pos = lineText.indexOf(cmd);
			if (pos >= 0) {
				pos += cmd.length();
				// workaround for starred commands: \section*{Cap}
				// (may have been matched by unstarred version because there is no order in possibleCommands)
				if ((lineText.length() > pos + 1) && lineText.at(pos) == '*') pos++;
				break;
			}
		}
		if (pos >= 0) break;
	}
	if (pos < 0) return; // could not find associated command

	// advance pos behind options, and use title to guess a label
	QList<CommandArgument> args = getCommandOptions(lineText, pos, &pos);
	QString label = "sec:";
	if (args.length() > 0) {
		QString title(args.at(0).value);
		if (!label.contains('\\') && !label.contains('$')) {  // title with these chars are too complicated to extract label
			label += makeLatexLabel(title);
		}
	}

	gotoLine(lineNr, pos, edView, mflags);

	insertTag(QString("\\label{%1}").arg(label), 7);
	QDocumentCursor cur(edView->editor->cursor());
	cur.movePosition(label.length(), QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
	edView->editor->setCursor(cur);
}

void Texstudio::changeTextCodec()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	bool ok;
	int mib = action->data().toInt(&ok);
	if (!ok) return;
	if (!currentEditorView()) return;

	currentEditorView()->editor->setFileCodec(QTextCodec::codecForMib(mib));
	updateCaption();
}

void Texstudio::editorSpellerChanged(const QString &name)
{
	foreach (QAction *act, statusTbLanguage->actions()) {
		if (act->data().toString() == name) {
			act->setChecked(true);
			break;
		}
	}
	if (name == "<default>") {
		statusTbLanguage->setText(spellerManager.defaultSpellerName());
	} else {
		statusTbLanguage->setText(name);
	}
}

void Texstudio::changeEditorSpeller()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;
	if (!currentEditorView()) return;

	if (!currentEditorView()->setSpeller(action->data().toString())) {
		// restore activity of previous action
		foreach (QAction *act, statusTbLanguage->actions()) {
			if (act->data().toString() == currentEditorView()->getSpeller()) {
				act->setChecked(true);
				break;
			}
		}
	}
}

void Texstudio::insertSpellcheckMagicComment()
{
	if (currentEditorView()) {
		QString name = currentEditorView()->getSpeller();
		if (name == "<default>") {
			name = spellerManager.defaultSpellerName();
		}
		currentEditorView()->document->updateMagicComment("spellcheck", name, true);
	}
}

void Texstudio::updateStatusBarEncoding()
{
	if (currentEditorView() && currentEditorView()->editor->getFileCodec()) {
		QTextCodec *codec = currentEditorView()->editor->getFileCodec();
		statusTbEncoding->setText(codec->name() + "  ");
		QStringList aliases;
		foreach (const QByteArray &b, codec->aliases()) {
			aliases << QString(b);
		}
		if (!aliases.isEmpty()) {
			statusTbEncoding->setToolTip(tr("Encoding Aliases: ") + aliases.join(", "));
		} else {
			statusTbEncoding->setToolTip(tr("Encoding"));
		}
	} else {
		statusTbEncoding->setText(tr("Encoding") + "  ");
		statusTbEncoding->setToolTip(tr("Encoding"));
	}
}

void Texstudio::addMagicRoot()
{
	if (currentEditorView()) {
		LatexDocument *doc = currentEditorView()->getDocument();
		if (!doc) return;
		QString name = doc->getRootDocument()->getFileName();
		name = getRelativeFileName(name, doc->getFileName(), true);
		currentEditorView()->document->updateMagicComment("root", name, true);
	}
}

void Texstudio::addMagicCoding()
{
	if (currentEditorView()) {
		QString name = currentEditor()->getFileCodec()->name();
		currentEditorView()->document->updateMagicComment("encoding", name, true);
	}
}

///////////////TOOLS////////////////////
bool Texstudio::runCommand(const QString &commandline, QString *buffer, QTextCodec *codecForBuffer)
{
	fileSaveAll(buildManager.saveFilesBeforeCompiling == BuildManager::SFBC_ALWAYS, buildManager.saveFilesBeforeCompiling == BuildManager::SFBC_ONLY_CURRENT_OR_NAMED);
	if (documents.getTemporaryCompileFileName() == "") {
		if (buildManager.saveFilesBeforeCompiling == BuildManager::SFBC_ONLY_NAMED && currentEditorView()) {
			QString tmpName = buildManager.createTemporaryFileName();
			currentEditor()->saveCopy(tmpName);
			currentEditorView()->document->setTemporaryFileName(tmpName);
		} else {
			QMessageBox::warning(this, tr("Error"), tr("Can't detect the file name.\nYou have to save a document before you can compile it."));
			return false;
		}
	}

	QString finame = documents.getTemporaryCompileFileName();
	if (finame == "") {
		txsWarning(tr("Can't detect the file name"));
		return false;
	}

	int ln = currentEditorView() ? currentEditorView()->editor->cursor().lineNumber() + 1 : 0;

	return buildManager.runCommand(commandline, finame, getCurrentFileName(), ln, buffer, codecForBuffer);
}

void Texstudio::runInternalPdfViewer(const QFileInfo &master, const QString &options)
{
#ifndef NO_POPPLER_PREVIEW
	QStringList ol = BuildManager::splitOptions(options);
	QString pdfFile;
	for (int i = ol.size() - 1; i >= 0; i--) {
		if (!ol[i].startsWith("-")) {
			if (pdfFile.isNull()) pdfFile = dequoteStr(ol[i]);
			ol.removeAt(i);
		} else if (ol[i].startsWith("--")) ol[i] = ol[i].mid(2);
		else ol[i] = ol[i].mid(1);
	}
	bool preserveExisting = ol.contains("preserve-existing");                  //completely ignore all existing internal viewers
	bool preserveExistingEmbedded = ol.contains("preserve-existing-embedded"); //completely ignore all existing embedded internal viewers
	bool preserveExistingWindowed = ol.contains("preserve-existing-windowed"); //completely ignore all existing windowed internal viewers
	bool preserveDuplicates = ol.contains("preserve-duplicates");              //open the document only in one pdf viewer
	bool embedded = ol.contains("embedded");                                   //if a new pdf viewer is opened, use the embedded mode
	bool windowed = ol.contains("windowed");                                   //if a new pdf viewer is opened, use the windowed mode (default)
	bool closeAll = ol.contains("close-all");                                  //close all existing viewers
	bool closeEmbedded = closeAll || ol.contains("close-embedded");            //close all embedded existing viewer
	bool closeWindowed = closeAll || ol.contains("close-windowed");            //close all windowed existing viewer

	bool autoClose;
	if (embedded) autoClose = ! ol.contains("no-auto-close");                  //Don't close the viewer, if the corresponding document is closed
	else autoClose = ol.contains("auto-close");                                //Close the viewer, if the corresponding document is closed

	PDFDocument::DisplayFlags displayPolicy = PDFDocument::FocusWindowed | PDFDocument::Raise;
	if (ol.contains("no-focus")) displayPolicy &= ~PDFDocument::Focus;
	else if (ol.contains("focus")) displayPolicy |= PDFDocument::Focus;
	if (ol.contains("no-foreground")) displayPolicy &= ~PDFDocument::Raise;
	else if (ol.contains("foreground")) displayPolicy |= PDFDocument::Raise;

	if (!(embedded || windowed || closeEmbedded || closeWindowed)) windowed = true; //default

	//embedded/windowed are mutual exclusive
	//no viewer will be opened, if one already exist (unless it was closed by a explicitely given close command)


	QList<PDFDocument *> oldPDFs = PDFDocument::documentList();

	if (preserveExisting) oldPDFs.clear();
	if (preserveExistingWindowed)
		for (int i = oldPDFs.size() - 1; i >= 0; i--)
			if (!oldPDFs[i]->embeddedMode)
				oldPDFs.removeAt(i);
	if (preserveExistingEmbedded)
		for (int i = oldPDFs.size() - 1; i >= 0; i--)
			if (oldPDFs[i]->embeddedMode)
				oldPDFs.removeAt(i);

	//if closing and opening is set, reuse the first document (reuse = optimization, so it does not close a viewer and creates an equal viewer afterwards)
	PDFDocument *reuse = 0;
	if ((embedded || windowed) && (closeEmbedded || closeWindowed) && !oldPDFs.isEmpty() ) {
		for (int i = 0; i < oldPDFs.size(); i++)
			if (oldPDFs[i]->embeddedMode == embedded) {
				reuse = oldPDFs.takeAt(i);
				break;
			}
	}

	//close old
	for (int i = oldPDFs.size() - 1; i >= 0; i--)
		if ( (oldPDFs[i]->embeddedMode && closeEmbedded) ||
		        (!oldPDFs[i]->embeddedMode && closeWindowed) )
			oldPDFs[i]->close(), oldPDFs.removeAt(i);


	//open new
	if (!embedded && !windowed) return;

	if (reuse) oldPDFs.insert(0, reuse);
	if (oldPDFs.isEmpty()) {
		PDFDocument *doc = qobject_cast<PDFDocument *>(newPdfPreviewer(embedded));
		REQUIRE(doc);
		doc->autoClose = autoClose;
		oldPDFs << doc;
	}

	if (pdfFile.isNull()) pdfFile = "?am.pdf";  // no file was explicitly specified in the command
	QString pdfDefFile = BuildManager::parseExtendedCommandLine(pdfFile, master).first();
	QStringList searchPaths = splitPaths(BuildManager::resolvePaths(buildManager.additionalPdfPaths));
	searchPaths.insert(0, master.absolutePath());
	pdfFile = buildManager.findFile(pdfDefFile, searchPaths);
	if (pdfFile == "") pdfFile = pdfDefFile; //use old file name, so pdf viewer shows reasonable error message
	int ln = 0;
	if (currentEditorView()) {
		ln = currentEditorView()->editor->cursor().lineNumber();
		int originalLineNumber = currentEditorView()->document->lineToLineSnapshotLineNumber(currentEditorView()->editor->cursor().line());
		if (originalLineNumber >= 0) ln = originalLineNumber;
	}
	foreach (PDFDocument *viewer, oldPDFs) {
		viewer->loadFile(pdfFile, master, displayPolicy);
		int pg = viewer->syncFromSource(getCurrentFileName(), ln, displayPolicy);
		viewer->fillRenderCache(pg);
        if (viewer->embeddedMode && configManager.viewerEnlarged) {
            sidePanelSplitter->hide();
			viewer->setStateEnlarged(true);
            //centralVSplitter->hide();
		}

		if (preserveDuplicates) break;
	}
#if QT_VERSION>=0x050000 && defined Q_OS_MAC
	if (embedded)
		setMenuBar(configManager.menuParentsBar);
#endif

#else
	txsCritical(tr("You have called the command to open the internal pdf viewer.\nHowever, you are using a version of TeXstudio that was compiled without the internal pdf viewer."));
#endif

}

bool Texstudio::checkProgramPermission(const QString &program, const QString &cmdId, LatexDocument *master)
{
	static const QRegExp txsCmd(QRegExp::escape(BuildManager::TXS_CMD_PREFIX) + "([^/ [{]+))");
	if (txsCmd.exactMatch(program)) return true;
	static QStringList programWhiteList;
	configManager.registerOption("Tools/Program Whitelist", &programWhiteList, QStringList() << "latex" << "pdflatex");
	if (programWhiteList.contains(program)) return true;
	if (buildManager.hasCommandLine(program)) return true;
	if (!master) return false;
	QString id = master->getMagicComment("document-id");
	if (id.contains("=")) return false;
	static QStringList individualProgramWhiteList;
	configManager.registerOption("Tools/Individual Program Whitelist", &individualProgramWhiteList, QStringList());
	if (!id.isEmpty() && individualProgramWhiteList.contains(id + "=" + program)) return true;
	int t = QMessageBox::warning(0, TEXSTUDIO,
	                             tr("The document \"%1\" wants to override the command \"%2\" with \"%3\".\n\n"
	                                "Do you want to allow and run the new, overriding command?\n\n"
	                                "(a) Yes, allow the new command for this document (only if you trust this document)\n"
	                                "(b) Yes, allow the new command to be used for all documents (only if you trust the new command to handle arbitrary documents)\n"
	                                "(c) No, do not use the command \"%3\" and run the default \"%2\" command"
	                               ).arg(master ? master->getFileName() : "").arg(cmdId).arg(program),
	                             tr("(a) allow for this document"),
	                             tr("(b) allow for all documents"),
	                             tr("(c) use the default command"), 0, 2);
	if (t == 2) return false;
	if (t == 1) {
		programWhiteList.append(program);
		return true;
	}
	if (id.isEmpty()) {
		id = QUuid::createUuid().toString();
		master->updateMagicComment("document-id", id, true);
		if (master->getMagicComment("document-id") != id) return false;
	}
	individualProgramWhiteList.append(id + "=" + program);
	return true;
}

void Texstudio::runBibliographyIfNecessary(const QFileInfo &mainFile)
{
	if (!configManager.runLaTeXBibTeXLaTeX) return;
	if (runBibliographyIfNecessaryEntered) return;

	LatexDocument *rootDoc = documents.getRootDocumentForDoc();
	REQUIRE(rootDoc);

	QList<LatexDocument *> docs = rootDoc->getListOfDocs();
	QSet<QString> bibFiles;
	foreach (const LatexDocument *doc, docs)
		foreach (const FileNamePair &bf, doc->mentionedBibTeXFiles())
			bibFiles.insert(bf.absolute);
    if(bibFiles.isEmpty())
        return; // don't try to compile bibtex files if there none
	if (bibFiles == rootDoc->lastCompiledBibTeXFiles) {
		QFileInfo bbl(BuildManager::parseExtendedCommandLine("?am.bbl", documents.getTemporaryCompileFileName()).first());
		if (bbl.exists()) {
			bool bibFilesChanged = false;
			QDateTime bblChanged = bbl.lastModified();
			foreach (const QString &bf, bibFiles) {
				//qDebug() << bf << ": "<<QFileInfo(bf).lastModified()<<" "<<bblChanged;

				if (QFileInfo(bf).exists() && QFileInfo(bf).lastModified() > bblChanged) {
					bibFilesChanged = true;
					break;
				}
			}
			if (!bibFilesChanged) return;
        }
	} else rootDoc->lastCompiledBibTeXFiles = bibFiles;

	runBibliographyIfNecessaryEntered = true;
	buildManager.runCommand(BuildManager::CMD_RECOMPILE_BIBLIOGRAPHY, mainFile);
	runBibliographyIfNecessaryEntered = false;
}

void Texstudio::runInternalCommand(const QString &cmd, const QFileInfo &mainfile, const QString &options)
{
	if (cmd == BuildManager::CMD_VIEW_PDF_INTERNAL || (cmd.startsWith(BuildManager::CMD_VIEW_PDF_INTERNAL) && cmd[BuildManager::CMD_VIEW_PDF_INTERNAL.length()] == ' '))
		runInternalPdfViewer(mainfile, options);
	else if (cmd == BuildManager::CMD_CONDITIONALLY_RECOMPILE_BIBLIOGRAPHY)
		runBibliographyIfNecessary(mainfile);
	else if (cmd == BuildManager::CMD_VIEW_LOG) {
		loadLog();
		viewLog();
	} else txsWarning(tr("Unknown internal command: %1").arg(cmd));
}

void Texstudio::commandLineRequested(const QString &cmdId, QString *result, bool *)
{
	if (!buildManager.m_interpetCommandDefinitionInMagicComment) return;
	LatexDocument *rootDoc = documents.getRootDocumentForDoc();
	if (!rootDoc) return;
	QString magic = rootDoc->getMagicComment("TXS-program:" + cmdId);
	if (!magic.isEmpty()) {
		if (!checkProgramPermission(magic, cmdId, rootDoc)) return;
		*result = magic;
		return;
	}
	QString program = rootDoc->getMagicComment("program");
	if (program.isEmpty()) program = rootDoc->getMagicComment("TS-program");
	if (program.isEmpty()) return;

	if (cmdId == "quick") {
		QString compiler = buildManager.guessCompilerFromProgramMagicComment(program);
		QString viewer = buildManager.guessViewerFromProgramMagicComment(program);
		if (!viewer.isEmpty() && !compiler.isEmpty()) {
			*result = BuildManager::chainCommands(compiler, viewer);
        } else if (checkProgramPermission(program, cmdId, rootDoc)) {  // directly execute whatever program is. Why does quick behave differently from compile ?
			*result = program;
		}
	} else if (cmdId == "compile") {
        QString compiler = buildManager.guessCompilerFromProgramMagicComment(program);
        if(!compiler.isEmpty()){
            *result = compiler;
            // notify used magic comment
            outputView->insertMessageLine(tr("%!TeX program used: %1").arg(program));
        }else{
            // warn about unused magic comment
            outputView->insertMessageLine(tr("%!TeX program not recognized! (%1). Using default.").arg(program));
        }
	} else if (cmdId == "view") {
        QString viewer = buildManager.guessViewerFromProgramMagicComment(program);
        if(!viewer.isEmpty()){
            *result = viewer;
        }
	}
}

void Texstudio::beginRunningCommand(const QString &commandMain, bool latex, bool pdf, bool async)
{
	if (pdf) {
		runningPDFCommands++;
		if (async && pdf) runningPDFAsyncCommands++;
#ifndef NO_POPPLER_PREVIEW
		PDFDocument::isCompiling = true;
		PDFDocument::isMaybeCompiling |= runningPDFAsyncCommands > 0;
#endif

		if (configManager.autoCheckinAfterSaveLevel > 0) {
			QFileInfo fi(documents.getTemporaryCompileFileName());
			fi.setFile(fi.path() + "/" + fi.baseName() + ".pdf");
			if (fi.exists() && !fi.isWritable()) {
				//pdf not writeable, needs locking ?
				svnLock(fi.filePath());
			}
		}
	}
    //outputView->resetMessagesAndLog(!configManager.showMessagesWhenCompiling);
	if (latex) {
		foreach (LatexEditorView *edView, editors->editors()) {
			edView->document->saveLineSnapshot();
		}
    }
	statusLabelProcess->setText(QString(" %1 ").arg(buildManager.getCommandInfo(commandMain).displayName));
}

void Texstudio::connectSubCommand(ProcessX *p, bool showStdoutLocally)
{
	connect(p, SIGNAL(standardErrorRead(QString)), outputView, SLOT(insertMessageLine(QString)));
	if (p->showStdout()) {
		p->setShowStdout((configManager.showStdoutOption == 2) || (showStdoutLocally && configManager.showStdoutOption == 1));
		connect(p, SIGNAL(standardOutputRead(QString)), outputView, SLOT(insertMessageLine(QString)));
	}
}

void Texstudio::beginRunningSubCommand(ProcessX *p, const QString &commandMain, const QString &subCommand, const RunCommandFlags &flags)
{
	if (commandMain != subCommand)
		statusLabelProcess->setText(QString(" %1: %2 ").arg(buildManager.getCommandInfo(commandMain).displayName).arg(buildManager.getCommandInfo(subCommand).displayName));
	if (flags & RCF_COMPILES_TEX)
		clearLogEntriesInEditors();
	//outputView->resetMessages();
	connectSubCommand(p, (RCF_SHOW_STDOUT & flags));
}

void Texstudio::endRunningSubCommand(ProcessX *p, const QString &commandMain, const QString &subCommand, const RunCommandFlags &flags)
{
	if (p->exitCode() && (flags & RCF_COMPILES_TEX) && !logExists()) {
		if (!QFileInfo(QFileInfo(documents.getTemporaryCompileFileName()).absolutePath()).isWritable())
			txsWarning(tr("You cannot compile the document in a non writable directory."));
		else
			txsWarning(tr("Could not start %1.").arg( buildManager.getCommandInfo(commandMain).displayName + ":" + buildManager.getCommandInfo(subCommand).displayName + ":\n" + p->getCommandLine()));
	}
	if ((flags & RCF_CHANGE_PDF)  && !(flags & RCF_WAITFORFINISHED) && (runningPDFAsyncCommands > 0)) {
		runningPDFAsyncCommands--;
#ifndef NO_POPPLER_PREVIEW
		if (runningPDFAsyncCommands <= 0) PDFDocument::isMaybeCompiling = false;
#endif
	}

}

void Texstudio::endRunningCommand(const QString &commandMain, bool latex, bool pdf, bool async)
{
	Q_UNUSED(commandMain)
	Q_UNUSED(async);
	if (pdf) {
		runningPDFCommands--;
#ifndef NO_POPPLER_PREVIEW
		if (runningPDFCommands <= 0)
			PDFDocument::isCompiling = false;
#endif
	}
	statusLabelProcess->setText(QString(" %1 ").arg(tr("Ready")));
	if (latex) emit infoAfterTypeset();
}

void Texstudio::processNotification(const QString &message)
{
	if (message.startsWith(tr("Error:")))
		outputView->showPage(outputView->MESSAGES_PAGE);
	outputView->insertMessageLine(message + "\n");
}

void Texstudio::clearLogs(){
    outputView->resetMessagesAndLog(!configManager.showMessagesWhenCompiling);
}

void Texstudio::openTerminal()
{
	QString workdir;
	if (currentEditor())
		workdir = currentEditor()->fileInfo().absolutePath();
	else
		workdir = getUserDocumentFolder();
	QString command = getTerminalCommand();
	if (command.isEmpty()) {
		txsCritical("Unable to detect a terminal application.");
		return;
	}
	QStringList args;
	args = command.split(' ');
	command = args.takeFirst();
	QProcess proc;
	// maybe some visual feedback here ?
	proc.startDetached(command, args, workdir);
}

void Texstudio::commandFromAction()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	checkShortcutChangeNotification(act);
	runCommand(act->data().toString());
}

void Texstudio::checkShortcutChangeNotification(QAction *act)
{
	bool showDialog = configManager.getOption("NotifyShortcutChange", true).toBool();
	if (!showDialog) return;
	if (act->data().toString() == BuildManager::CMD_QUICK && act->shortcuts().contains(Qt::Key_F1)) {
		QDialog *dialog = new QDialog();
		QVBoxLayout *layout = new QVBoxLayout();
		dialog->setLayout(layout);

		QTextEdit *te = new QTextEdit();
		te->setReadOnly(true);
		te->setFocusPolicy(Qt::NoFocus);
		te->setText(tr("<h4>Change of Default Shortcuts</h4>"
		               "<p>Over the time, the shortcuts for the main tools have become somewhat fragmented. Additionally, they partly overlapped with standard keys. In particular, F1, F3, F10, F11 and F12 have reserved meanings on some systems.</p>"
		               "<p>We've decided to set this right in favor of more a consistent layout:</p>"
		               "<ul>"
		               "<li>The shortcut for <code>Build & View</code> will move from F1 to F5.</li>"
		               "<li>The shortcut for <code>Bibliograpy</code> will move from F11 to F8.</li>"
		               "<li>The shortcut for <code>Glossary</code> will move from F10 to F9."
		               "<li>The tool <code>Index</code> won't have a default shortcut anymore (formerly F12) because it's not called very often.</li>"
		               "</ul>"
		               "<p>We are sorry, that you have to relearn the most used shortcut for <code>Build & View</code>. For a transition period, both F1 and F5 will work. "
		               "In the end, collecting the most important tools in the central block F5-F8 will increase usability. As usual, you can still fully customize the shortcuts in the options.</p>"
		              ));
		layout->addWidget(te);
		QLabel *image = new QLabel();
		image->setPixmap(QPixmap(":/images-ng/shortcut_change.svg"));
		image->setFocusPolicy(Qt::NoFocus);
		layout->addWidget(image);
		layout->setAlignment(image, Qt::AlignHCenter);
		QCheckBox *cbNoShowAgain = new QCheckBox(tr("Do not show this message again."));
		configManager.getOption("NotifyShortcutChange", true).toBool();
		layout->addWidget(cbNoShowAgain);
		QPushButton *okButton = new QPushButton(tr("OK"));
		okButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
		okButton->setDefault(true);
		layout->addWidget(okButton);
		layout->setAlignment(okButton, Qt::AlignHCenter);
		connect(okButton, SIGNAL(clicked()), dialog, SLOT(close()));
		dialog->resize(440, 480);
		dialog->setModal(true);
		dialog->show();
		while (dialog->isVisible()) {
			QApplication::processEvents();
		}
		if (cbNoShowAgain->isChecked()) {
			configManager.setOption("NotifyShortcutChange", false);
		}
		delete dialog;
	}
}

void Texstudio::cleanAll()
{
	CleanDialog cleanDlg(this);
	if (cleanDlg.checkClean(documents)) {
		cleanDlg.exec();
	} else {
		txsInformation(tr("No open project or tex file to clean."));
	}
}

void Texstudio::webPublish()
{
	if (!currentEditorView()) {
		txsWarning(tr("No document open"));
		return;
	}
	if (!currentEditorView()->editor->getFileCodec()) return;
	fileSave();
	QString finame = documents.getCompileFileName();

	WebPublishDialog *ttwpDlg = new WebPublishDialog(this, configManager.webPublishDialogConfig, &buildManager,
	        currentEditorView()->editor->getFileCodec());
	ttwpDlg->ui.inputfileEdit->setText(finame);
	ttwpDlg->exec();
	delete ttwpDlg;
}

void Texstudio::webPublishSource()
{
	if (!currentEditor()) return;
	QDocumentCursor cur = currentEditor()->cursor();
	QString	html = currentEditor()->document()->exportAsHtml(cur.hasSelection() ? cur : QDocumentCursor(), true);
	fileNew(getCurrentFileName() + ".html");
	currentEditor()->insertText(html);
	/*QLabel* htmll = new QLabel(html, this);
	htmll->show();
	htmll->resize(300,300);*/
}

void Texstudio::analyseText()
{
	if (!currentEditorView()) {
		txsWarning(tr("No document open"));
		return;
	}
	if (!textAnalysisDlg) {
		textAnalysisDlg = new TextAnalysisDialog(this, tr("Text Analysis"));
		connect(textAnalysisDlg, SIGNAL(destroyed()), this, SLOT(analyseTextFormDestroyed()));
	}
	if (!textAnalysisDlg) return;
	textAnalysisDlg->setEditor(currentEditorView()->editor);//->document(), currentEditorView()->editor->cursor());
	textAnalysisDlg->init();
	textAnalysisDlg->interpretStructureTree(currentEditorView()->document->baseStructure);

	textAnalysisDlg->show();
	textAnalysisDlg->raise(); //not really necessary, since the dlg is always on top
	textAnalysisDlg->activateWindow();
}

void Texstudio::analyseTextFormDestroyed()
{
	textAnalysisDlg = 0;
}

void Texstudio::generateRandomText()
{
	if (!currentEditorView()) {
		txsWarning(tr("The random text generator constructs new texts from existing words, so you have to open some text files"));
		return;
	}

	QStringList allLines;
	foreach (LatexEditorView *edView, editors->editors())
		allLines << edView->editor->document()->textLines();
	RandomTextGenerator generator(this, allLines);
	generator.exec();
}

//////////////// MESSAGES - LOG FILE///////////////////////
bool Texstudio::logExists()
{
	QString finame = documents.getTemporaryCompileFileName();
	if (finame == "")
		return false;
	QString logFileName = buildManager.findFile(getAbsoluteFilePath(documents.getLogFileName()), splitPaths(BuildManager::resolvePaths(buildManager.additionalLogPaths)));
	QFileInfo fic(logFileName);
	if (fic.exists() && fic.isReadable()) return true;
	else return false;
}

bool Texstudio::loadLog()
{
	outputView->getLogWidget()->resetLog();
	if (!documents.getCurrentDocument()) return false;
	QString compileFileName = documents.getTemporaryCompileFileName();
	if (compileFileName == "") {
		QMessageBox::warning(this, tr("Error"), tr("File must be saved and compiling before you can view the log"));
		return false;
	}
	QString logFileName = buildManager.findFile(getAbsoluteFilePath(documents.getLogFileName()), splitPaths(BuildManager::resolvePaths(buildManager.additionalLogPaths)));
	QTextCodec * codec = QTextCodec::codecForName(configManager.logFileEncoding.toLatin1());
	return outputView->getLogWidget()->loadLogFile(logFileName, compileFileName, codec ? codec : documents.getCurrentDocument()->codec() );
}

void Texstudio::showLog()
{
	outputView->showPage(outputView->LOG_PAGE);
}

//shows the log (even if it is empty)
void Texstudio::viewLog()
{
	showLog();
	setLogMarksVisible(true);
	if (configManager.goToErrorWhenDisplayingLog && hasLatexErrors()) {
		int errorMarkID = outputView->getLogWidget()->getLogModel()->markID(LT_ERROR);
		if (!gotoMark(false, errorMarkID)) {
			gotoMark(true, errorMarkID);
		}
	}
}

void Texstudio::viewLogOrReRun(LatexCompileResult *result)
{
	loadLog();
	REQUIRE(result);
	if (hasLatexErrors()) {
		onCompileError();
		*result = LCR_ERROR;
	} else {
		*result = LCR_NORMAL;
		if (outputView->getLogWidget()->getLogModel()->existsReRunWarning())
			*result = LCR_RERUN;
		else if (configManager.runLaTeXBibTeXLaTeX) {
			//run bibtex if citation is unknown to bibtex but contained in an included bib file
			QStringList missingCitations = outputView->getLogWidget()->getLogModel()->getMissingCitations();
			bool runBibTeX = false;
			foreach (const QString &s, missingCitations) {
				for (int i = 0; i < documents.mentionedBibTeXFiles.count(); i++) {
					if (!documents.bibTeXFiles.contains(documents.mentionedBibTeXFiles[i])) continue;
					BibTeXFileInfo &bibTex = documents.bibTeXFiles[documents.mentionedBibTeXFiles[i]];
					if (bibTex.ids.contains(s)) {
						runBibTeX = true;
						break;
					}
				}
				if (runBibTeX) break;
			}
			if (runBibTeX)
				*result = LCR_RERUN_WITH_BIBLIOGRAPHY;
		}
	}
}

////////////////////////// ERRORS /////////////////////////////

void Texstudio::onCompileError()
{
	if (configManager.getOption("Tools/ShowLogInCaseOfCompileError").toBool()) {
		viewLog();
	} else {
		setLogMarksVisible(true);
	}
}

// changes visibilita of log markers in all editors
void Texstudio::setLogMarksVisible(bool visible)
{
	foreach (LatexEditorView *edView, editors->editors()) {
		edView->setLogMarksVisible(visible);
	}
	QAction *act = getManagedAction("main/tools/logmarkers");
	if (act) act->setChecked(visible);
}

// removes the log entries from all editors
void Texstudio::clearLogEntriesInEditors()
{
	foreach (LatexEditorView *edView, editors->editors()) {
		edView->clearLogMarks();
	}
}

// adds the current log entries to all editors
void Texstudio::updateLogEntriesInEditors()
{
	clearLogEntriesInEditors();
	LatexLogModel *logModel = outputView->getLogWidget()->getLogModel();
	QHash<QString, LatexEditorView *> tempFilenames; //temporary maps the filenames (as they appear in this log!) to the editor
	int errorMarkID = QLineMarksInfoCenter::instance()->markTypeId("error");
	int warningMarkID = QLineMarksInfoCenter::instance()->markTypeId("warning");
	int badboxMarkID = QLineMarksInfoCenter::instance()->markTypeId("badbox");

	for (int i = logModel->count() - 1; i >= 0; i--) {
		if (logModel->at(i).oldline != -1) {
			LatexEditorView *edView;
			if (tempFilenames.contains(logModel->at(i).file)) edView = tempFilenames.value(logModel->at(i).file);
			else {
				edView = getEditorViewFromFileName(logModel->at(i).file, true);
				tempFilenames[logModel->at(i).file] = edView;
			}
			if (edView) {
				int markID;
				switch (logModel->at(i).type) {
				case LT_ERROR:
					markID = errorMarkID;
					break;
				case LT_WARNING:
					markID = warningMarkID;
					break;
				case LT_BADBOX:
					markID = badboxMarkID;
					break;
				default:
					markID = -1;
				}
				edView->addLogEntry(i, logModel->at(i).oldline - 1, markID);
			}
		}
	}
}

bool Texstudio::hasLatexErrors()
{
	return outputView->getLogWidget()->getLogModel()->found(LT_ERROR);
}

bool Texstudio::gotoNearLogEntry(int lt, bool backward, QString notFoundMessage)
{
	if (!outputView->getLogWidget()->logPresent()) {
		loadLog();
	}
	if (outputView->getLogWidget()->logPresent()) {
		if (outputView->getLogWidget()->getLogModel()->found((LogType) lt)) {
			showLog();
			setLogMarksVisible(true);
			return gotoMark(backward, outputView->getLogWidget()->getLogModel()->markID((LogType) lt));
		} else {
			txsInformation(notFoundMessage);
		}
	}
	return false;
}

void Texstudio::clearMarkers()
{
	setLogMarksVisible(false);
}
//////////////// HELP /////////////////
void Texstudio::latexHelp()
{
	QString latexHelp = findResourceFile("latex2e.html");
	if (latexHelp == "")
		QMessageBox::warning(this, tr("Error"), tr("File not found"));
	else if (!QDesktopServices::openUrl("file:///" + latexHelp))
		QMessageBox::warning(this, tr("Error"), tr("Could not open browser"));
}

void Texstudio::userManualHelp()
{
	QString latexHelp = findResourceFile("usermanual_en.html");
	if (latexHelp == "")
		QMessageBox::warning(this, tr("Error"), tr("File not found"));
	else if (!QDesktopServices::openUrl("file:///" + latexHelp))
		QMessageBox::warning(this, tr("Error"), tr("Could not open browser"));
}

void Texstudio::texdocHelp()
{
	QString selection;
	QStringList packages;
	if (currentEditorView()) {
		selection = currentEditorView()->editor->cursor().selectedText();
		foreach (const QString &key, currentEditorView()->document->parent->cachedPackages.keys()) {
			if (currentEditorView()->document->parent->cachedPackages[key].completionWords.isEmpty())
				// remove empty packages which probably do not exist
				continue;
			packages << LatexPackage::keyToPackageName(key);
		}

		packages.removeDuplicates();
		packages.removeAll("latex-209");
		packages.removeAll("latex-dev");
		packages.removeAll("latex-l2tabu");
		packages.removeAll("latex-document");
		packages.removeAll("latex-mathsymbols");
		packages.removeAll("tex");
	}

	Help::instance()->execTexdocDialog(packages, selection);
}

void Texstudio::helpAbout()
{
	// The focus will return to the parent. Therefore we have to provide the correct caller (may be a viewer window).
	QWidget *parentWindow = windowForObject(sender(), this);
	AboutDialog *abDlg = new AboutDialog(parentWindow);
	abDlg->exec();
	delete abDlg;
}
////////////// OPTIONS //////////////////////////////////////

void Texstudio::generalOptions()
{
	QMap<QString, QVariant> oldCustomEnvironments = configManager.customEnvironments;
	bool oldModernStyle = modernStyle;
	bool oldSystemTheme = useSystemTheme;
	int oldReplaceQuotes = configManager.replaceQuotes;
	autosaveTimer.stop();
	m_formats->modified = false;
	bool realtimeChecking = configManager.editorConfig->realtimeChecking;
	bool inlineSpellChecking = configManager.editorConfig->inlineSpellChecking;
	bool inlineCitationChecking = configManager.editorConfig->inlineCitationChecking;
	bool inlineReferenceChecking = configManager.editorConfig->inlineReferenceChecking;
	bool inlineSyntaxChecking = configManager.editorConfig->inlineSyntaxChecking;
	QString additionalBibPaths = configManager.additionalBibPaths;
	QStringList loadFiles = configManager.completerConfig->getLoadedFiles();

    // init pdf shortcuts if pdfviewer is not open
#ifndef NO_POPPLER_PREVIEW
    PDFDocument *pdfviewerWindow=NULL;
    if(PDFDocument::documentList().isEmpty()){
        pdfviewerWindow = new PDFDocument(configManager.pdfDocumentConfig, false);
        pdfviewerWindow->hide();
    }
#endif


#if QT_VERSION<0x050000
	if (configManager.possibleMenuSlots.isEmpty()) {
		for (int i = 0; i < staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append(staticMetaObject.method(i).signature());
		for (int i = 0; i < QEditor::staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append("editor:" + QString(QEditor::staticMetaObject.method(i).signature()));
		for (int i = 0; i < LatexEditorView::staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append("editorView:" + QString(LatexEditorView::staticMetaObject.method(i).signature()));
		configManager.possibleMenuSlots = configManager.possibleMenuSlots.filter(QRegExp("^[^*]+$"));
	}
#else
	if (configManager.possibleMenuSlots.isEmpty()) {
		for (int i = 0; i < staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append(staticMetaObject.method(i).methodSignature());
		for (int i = 0; i < QEditor::staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append("editor:" + QString(QEditor::staticMetaObject.method(i).methodSignature()));
		for (int i = 0; i < LatexEditorView::staticMetaObject.methodCount(); i++) configManager.possibleMenuSlots.append("editorView:" + QString(LatexEditorView::staticMetaObject.method(i).methodSignature()));
		configManager.possibleMenuSlots = configManager.possibleMenuSlots.filter(QRegExp("^[^*]+$"));
	}
#endif
	// GUI scaling
	connect(&configManager, SIGNAL(iconSizeChanged(int)), this, SLOT(changeIconSize(int)));
	connect(&configManager, SIGNAL(secondaryIconSizeChanged(int)), this, SLOT(changeSecondaryIconSize(int)));
	connect(&configManager, SIGNAL(symbolGridIconSizeChanged(int)), this, SLOT(changeSymbolGridIconSize(int)));

	// The focus will return to the parent. Therefore we have to provide the correct caller (may be a viewer window).
	QWidget *parentWindow = windowForObject(sender(), this);

	if (configManager.execConfigDialog(parentWindow)) {
		QApplication::setOverrideCursor(Qt::WaitCursor);

		configManager.editorConfig->settingsChanged();

		spellerManager.setDictPaths(configManager.parseDirList(configManager.spellDictDir));
		spellerManager.setDefaultSpeller(configManager.spellLanguage);

		GrammarCheck::staticMetaObject.invokeMethod(grammarCheck, "init", Qt::QueuedConnection, Q_ARG(LatexParser, latexParser), Q_ARG(GrammarCheckerConfig, *configManager.grammarCheckerConfig));

		if (configManager.autoDetectEncodingFromLatex || configManager.autoDetectEncodingFromChars) QDocument::setDefaultCodec(0);
		else QDocument::setDefaultCodec(configManager.newFileEncoding);
		QDocument::removeGuessEncodingCallback(&ConfigManager::getDefaultEncoding);
		QDocument::removeGuessEncodingCallback(&Encoding::guessEncoding);
		if (configManager.autoDetectEncodingFromLatex)
			QDocument::addGuessEncodingCallback(&Encoding::guessEncoding);
		if (configManager.autoDetectEncodingFromChars)
			QDocument::addGuessEncodingCallback(&ConfigManager::getDefaultEncoding);


		ThesaurusDialog::prepareDatabase(configManager.thesaurus_database);
		if (additionalBibPaths != configManager.additionalBibPaths) documents.updateBibFiles(true);

		// update syntaxChecking with alls docs
		foreach (LatexDocument *doc, documents.getDocuments()) {
			doc->enableSyntaxCheck(configManager.editorConfig->inlineSyntaxChecking);
		}

		//update highlighting ???
		bool updateHighlighting = (inlineSpellChecking != configManager.editorConfig->inlineSpellChecking);
		updateHighlighting |= (inlineCitationChecking != configManager.editorConfig->inlineCitationChecking);
		updateHighlighting |= (inlineReferenceChecking != configManager.editorConfig->inlineReferenceChecking);
		updateHighlighting |= (inlineSyntaxChecking != configManager.editorConfig->inlineSyntaxChecking);
		updateHighlighting |= (realtimeChecking != configManager.editorConfig->realtimeChecking);
		updateHighlighting |= (additionalBibPaths != configManager.additionalBibPaths);
		// check for change in load completion files
		QStringList newLoadedFiles = configManager.completerConfig->getLoadedFiles();
		foreach (const QString &elem, newLoadedFiles) {
			if (loadFiles.removeAll(elem) == 0)
				updateHighlighting = true;
			if (updateHighlighting)
				break;
		}
		if (!loadFiles.isEmpty())
			updateHighlighting = true;
		buildManager.clearPreviewPreambleCache();//possible changed latex command / preview behaviour

		if (currentEditorView()) {
			foreach (LatexEditorView *edView, editors->editors()) {
				edView->updateSettings();
				if (updateHighlighting) {
					edView->clearOverlays(); // for disabled syntax check
					if (configManager.editorConfig->realtimeChecking) {
						edView->document->updateLtxCommands();
						edView->documentContentChanged(0, edView->document->lines());
						edView->document->updateCompletionFiles(false, true);
					} else {
						edView->clearOverlays();
					}
				}

			}
			if (m_formats->modified)
				QDocument::setBaseFont(QDocument::baseFont(), true);
			updateCaption();

			if (documents.indentIncludesInStructure != configManager.indentIncludesInStructure ||
			        documents.showCommentedElementsInStructure != configManager.showCommentedElementsInStructure ||
			        documents.markStructureElementsBeyondEnd != configManager.markStructureElementsBeyondEnd ||
			        documents.markStructureElementsInAppendix != configManager.markStructureElementsInAppendix) {
				documents.indentIncludesInStructure = configManager.indentIncludesInStructure;
				documents.showCommentedElementsInStructure = configManager.showCommentedElementsInStructure;
				documents.markStructureElementsBeyondEnd = configManager.markStructureElementsBeyondEnd;
				documents.markStructureElementsInAppendix = configManager.markStructureElementsInAppendix;
				foreach (LatexDocument *doc, documents.documents)
					updateStructure(false, doc);
			}
		}
		if (oldReplaceQuotes != configManager.replaceQuotes)
			updateUserMacros();
		// scale GUI
		changeIconSize(configManager.guiToolbarIconSize);
		changeSecondaryIconSize(configManager.guiSecondaryToolbarIconSize);
		changeSymbolGridIconSize(configManager.guiSymbolGridIconSize, false);
		//custom toolbar
		setupToolBars();
		// custom evironments
		bool customEnvironmentChanged = configManager.customEnvironments != oldCustomEnvironments;
		if (customEnvironmentChanged) {
			updateTexQNFA();
		}
		//completion
		completionBaseCommandsUpdated = true;
		completerNeedsUpdate();
		completer->setConfig(configManager.completerConfig);
		//update changed line mark colors
		QList<QLineMarkType> &marks = QLineMarksInfoCenter::instance()->markTypes();
		for (int i = 0; i < marks.size(); i++)
			if (m_formats->format("line:" + marks[i].id).background.isValid())
				marks[i].color = m_formats->format("line:" + marks[i].id).background;
			else
				marks[i].color = Qt::transparent;
		// update all docuemnts views as spellcheck may be different
		QEditor::setEditOperations(configManager.editorKeys, false); // true -> false, otherwise edit operation can't be removed, e.g. tab for indentSelection
		foreach (LatexEditorView *edView, editors->editors()) {
			QEditor *ed = edView->editor;
			ed->document()->markFormatCacheDirty();
			ed->update();
		}
		if (oldModernStyle != modernStyle || oldSystemTheme != useSystemTheme) {
			iconCache.clear();
			setupMenus();
			setupDockWidgets();
		}
		updateUserToolMenu();
		QApplication::restoreOverrideCursor();
	}
	if (configManager.autosaveEveryMinutes > 0) {
		autosaveTimer.start(configManager.autosaveEveryMinutes * 1000 * 60);
	}
#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *viewer, PDFDocument::documentList()) {
		viewer->reloadSettings();
	}
    if(pdfviewerWindow){
        pdfviewerWindow->close();
        delete pdfviewerWindow;
    }
#endif
}

void Texstudio::executeCommandLine(const QStringList &args, bool realCmdLine)
{
	// parse command line
	QStringList filesToLoad;
	bool hasExplicitRoot = false;

	int line = -1;
	int col = 0;
	QString cite;
#ifndef NO_POPPLER_PREVIEW
	int page = -1;
	bool pdfViewerOnly = false;
#endif
	for (int i = 0; i < args.size(); ++i) {
		if (args[i] == "") continue;
		if (args[i][0] != '-')  filesToLoad << args[i];
		//-form is for backward compatibility
		if (args[i] == "--root" || args[i] == "--master") hasExplicitRoot = true;
		if (args[i] == "--line" && i + 1 < args.size()) {
			QStringList lineCol = args[++i].split(":");
			line = lineCol.at(0).toInt() - 1;
			if (lineCol.count() >= 2) {
				col = lineCol.at(1).toInt();
				if ((col) < 0) col = 0;
			}
		}
		if (args[i] == "--insert-cite" && i + 1 < args.size()) {
			cite = args[++i];
		}
#ifndef NO_POPPLER_PREVIEW
		if (args[i] == "--pdf-viewer-only") pdfViewerOnly = true;
		if (args[i] == "--page") page = args[++i].toInt() - 1;
#endif
	}

#ifndef NO_POPPLER_PREVIEW
	if (pdfViewerOnly) {
		if (PDFDocument::documentList().isEmpty())
			newPdfPreviewer();
		foreach (PDFDocument *viewer, PDFDocument::documentList()) {
			if (!filesToLoad.isEmpty()) viewer->loadFile(filesToLoad.first(), QFileInfo());
			connect(viewer, SIGNAL(destroyed()), SLOT(deleteLater()));
			viewer->show();
			viewer->setFocus();
			if (page != -1) viewer->goToPage(page);
		}
		hide();
		return;
	}
#endif

	// execute command line
	foreach (const QString &fileToLoad, filesToLoad) {
		QFileInfo ftl(fileToLoad);
		if (fileToLoad != "") {
			if (ftl.exists()) {
				if (ftl.suffix() == Session::fileExtension()) {
					loadSession(ftl.filePath());
				} else {
					load(fileToLoad, hasExplicitRoot);
				}
			} else if (ftl.absoluteDir().exists()) {
				fileNew(ftl.absoluteFilePath());
				if (hasExplicitRoot) {
					setExplicitRootDocument(currentEditorView()->getDocument());
				}
				//return ;
			}
		}
	}

	if (line != -1) {
		gotoLine(line, col, 0, QEditor::KeepSurrounding | QEditor::ExpandFold);
		QTimer::singleShot(500, currentEditor(), SLOT(ensureCursorVisible())); //reshow cursor in case the windows size changes
	}

	if (!cite.isNull()) {
		insertCitation(cite);
	}

#ifndef QT_NO_DEBUG
	//execute test after command line is known
	if (realCmdLine) { //only at start
		executeTests(args);

		if (args.contains("--update-translations")) {
			generateAddtionalTranslations();
		}
	}
#endif

	if (realCmdLine) Guardian::summon();
}

void Texstudio::hideSplash()
{
	if (splashscreen) splashscreen->hide();
}

void Texstudio::executeTests(const QStringList &args)
{
	QFileInfo myself(QCoreApplication::applicationFilePath());
	if (args.contains("--disable-tests")) return;
#if !defined(QT_NO_DEBUG) && !defined(NO_TESTS)
	bool allTests = args.contains("--execute-all-tests")
	                //execute all tests once a week or if command paramter is set
	                || (configManager.debugLastFullTestRun.daysTo(myself.lastModified()) > 6);
	if (args.contains("--execute-tests") || myself.lastModified() != configManager.debugLastFileModification || allTests) {
		fileNew();
		if (!currentEditorView() || !currentEditorView()->editor)
			QMessageBox::critical(0, "wtf?", "test failed", QMessageBox::Ok);
		if (allTests) configManager.debugLastFullTestRun = myself.lastModified();

		TestManager testManager;
		connect(&testManager, SIGNAL(newMessage(QString)), this, SLOT(showTestProgress(QString)));
		QString result = testManager.execute(allTests ? TestManager::TL_ALL : TestManager::TL_FAST, currentEditorView(), currentEditorView()->codeeditor, currentEditorView()->editor, &buildManager);
		m_languages->setLanguageFromName(currentEditorView()->editor, "TXS Test Results");
		currentEditorView()->editor->setText(result, false);
		if (result.startsWith("*** THERE SEEM TO BE FAILED TESTS! ***")) {
			QSearchReplacePanel *searchpanel = qobject_cast<QSearchReplacePanel *>(currentEditorView()->codeeditor->panels("Search")[0]);
			if (searchpanel) {
				searchpanel->find("FAIL!", false, false, false, false, true);
			}
		}
		configManager.debugLastFileModification = QFileInfo(QCoreApplication::applicationFilePath()).lastModified();
	}
#endif
}

void Texstudio::showTestProgress(const QString &message)
{
	outputView->insertMessageLine(message);
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
}

void Texstudio::generateAddtionalTranslations()
{
	QStringList translations;
	translations << "/******************************************************************************";
	translations << " * Do not manually edit this file. It is automatically generated by a call to";
	translations << " * texstudio --update-translations";
	translations << " * This generates some additional translations which lupdate doesn't find";
	translations << " * (e.g. from uiconfig.xml, color names, qnfa format names) ";
	translations << " ******************************************************************************/";

	translations << "#undef UNDEFINED";
	translations << "#ifdef UNDEFINED";
	translations << "static const char* translations[] = {";

	QRegExp commandOnly("\\\\['`^\"~=.^]?[a-zA-Z]*(\\{\\})* *"); //latex command
	//copy menu item text
	QFile xmlFile(":/uiconfig.xml");
	xmlFile.open(QIODevice::ReadOnly);
	QDomDocument xml;
	xml.setContent(&xmlFile);

	CodeSnippet::debugDisableAutoTranslate = true;
	QStringList tagNames = QStringList() << "menu" << "insert" << "action";
	foreach (const QString &tag, tagNames) {
		QDomNodeList nodes = xml.elementsByTagName(tag);
		for(int i = 0; i < nodes.count(); i++)
		{
			QDomNode current = nodes.at(i);
			QDomNamedNodeMap attribs = current.attributes();
			QString text = attribs.namedItem("text").nodeValue();
			if (!text.isEmpty() && !commandOnly.exactMatch(text))
				translations << "QT_TRANSLATE_NOOP(\"ConfigManager\", \"" + text.replace("\\", "\\\\").replace("\"", "\\\"") + "\"), ";
			QString insert = attribs.namedItem("insert").nodeValue();
			if (!insert.isEmpty()) {
				CodeSnippet cs(insert, false);
				for (int i = 0; i < cs.placeHolders.size(); i++)
					for (int j = 0; j < cs.placeHolders[i].size(); j++)
						if (cs.placeHolders[i][j].flags & CodeSnippetPlaceHolder::Translatable)
							translations << "QT_TRANSLATE_NOOP(\"CodeSnippet_PlaceHolder\", \"" + cs.lines[i].mid(cs.placeHolders[i][j].offset, cs.placeHolders[i][j].length) + "\"), ";
			}
		}
	}
	CodeSnippet::debugDisableAutoTranslate = false;
	//copy
	QFile xmlFile2(":/qxs/defaultFormats.qxf");
	xmlFile2.open(QIODevice::ReadOnly);
	xml.setContent(&xmlFile2);
	QDomNodeList formats = xml.documentElement().elementsByTagName("format");
	for (int i = 0; i < formats.size(); i++)
		translations << "QT_TRANSLATE_NOOP(\"QFormatConfig\", \"" + formats.at(i).attributes().namedItem("id").nodeValue() + "\"), ";
	translations << "QT_TRANSLATE_NOOP(\"QFormatConfig\", \"normal\"),";
	for (int i = 0; i < configManager.managedToolBars.size(); i++)
		translations << "QT_TRANSLATE_NOOP(\"Texstudio\",\"" + configManager.managedToolBars[i].name + "\"),";

	foreach (const QString &s, m_languages->languages())
		translations << "QT_TRANSLATE_NOOP(\"Texstudio\", \"" + s + "\", \"Format name of language definition \"), ";

	translations << "\"\"};";
	translations << "#endif\n\n";

	QFile translationFile("additionaltranslations.cpp");
	if (translationFile.open(QIODevice::WriteOnly)) {
		translationFile.write(translations.join("\n").toLatin1());
		translationFile.close();
	}
}

void Texstudio::onOtherInstanceMessage(const QString &msg)   // Added slot for messages to the single instance
{
	show();
	activateWindow();
	executeCommandLine(msg.split("#!#"), false);
}

void Texstudio::setAutomaticRootDetection()
{
	documents.setMasterDocument(0);
}

void Texstudio::setExplicitRootDocument(LatexDocument *doc)
{
	if (!doc) {
		setAutomaticRootDetection();
		return;
	}
	if (doc->getFileName().isEmpty() && doc->getEditorView()) {
		editors->setCurrentEditor(doc->getEditorView());
		fileSave();
	}
	if (doc->getFileName().isEmpty()) {
		txsWarning(tr("You have to save the file before it can be defined as root document."));
		return;
	}
	documents.setMasterDocument(doc);
}

void Texstudio::setCurrentDocAsExplicitRoot()
{
	if (currentEditorView()) {
		setExplicitRootDocument(currentEditorView()->document);
	}
}

////////////////// VIEW ////////////////
void Texstudio::gotoNextDocument()
{
	// TODO check: can we have managed action connecting to the Editors slot directly? Then we could remove this slot
	editors->activateNextEditor();
}

void Texstudio::gotoPrevDocument()
{
	// TODO check: can we have managed action connecting to the Editors slot directly? Then we could remove this slot
	editors->activatePreviousEditor();
}

void Texstudio::gotoOpenDocument()
{
	QAction *act = qobject_cast<QAction *>(sender());
	REQUIRE(act);
	editors->setCurrentEditor(act->data().value<LatexEditorView *>());
}

/*!
 * Update the document menu. If only the name of the current file changed, use localChange = true
 * for a faster update.
 * Note: localChange is a low-cost variant which is basically there because updateCaption() calls this
 * far too often even when it's not necessary. The calling logic (in particular updateCaption and its
 * uses should be refactored).
 */
void Texstudio::updateOpenDocumentMenu(bool localChange)
{
	if (localChange) {
		LatexEditorView *edView = currentEditorView();
		QMenu *menu = configManager.getManagedMenu("main/view/documents");
		if (!menu) return;
		foreach (QAction *act, menu->actions()) {
			if (edView == act->data().value<LatexEditorView *>()) {
				act->setText(edView->displayNameForUI());
				//qDebug() << "local SUCCESS" << act->text() << edView->displayName();
				return;
			}
		}
		//qDebug() << "local updateOpenDocumentMenu failed. Falling back to complete update.";
		// if there was no editor for a local change, fall back to a complete update of the menu
	}
	QStringList names;
	QList<QVariant> data;
	foreach (LatexEditorView *edView, editors->editors()) {
		names << edView->displayNameForUI();
		data << QVariant::fromValue<LatexEditorView *>(edView);
	}
	//qDebug() << "complete" << names;
	configManager.updateListMenu("main/view/documents", names, "doc", false, SLOT(gotoOpenDocument()), 0, true, 0, data);
}

void Texstudio::onEditorsReordered()
{
	// we currently reorder the documents so that their order matches the order of editors
	// this is purely conventional now (structure view inherits the order of the documents.)
	// There is no technical necessity to align the order of editors and documents. We could drop
	// this behavior in the future
	QList<LatexDocument *> docs;
	foreach (const LatexEditorView *edView, editors->editors()) {
		docs.append(edView->getDocument());
	}
	documents.reorder(docs);
}

void Texstudio::focusEditor()
{
	raise();
	activateWindow();
	if (currentEditorView())
		currentEditorView()->setFocus();
}

void Texstudio::focusViewer()
{
#ifndef NO_POPPLER_PREVIEW
	QList<PDFDocument *> viewers = PDFDocument::documentList();
	if (viewers.isEmpty()) return;

	if (viewers.count() > 1 && currentEditorView()) {
		// try: PDF for current file
		QFileInfo currentFile = currentEditorView()->getDocument()->getFileInfo();
		foreach (PDFDocument *viewer, viewers) {
			if (viewer->getMasterFile() == currentFile) {
				viewer->focus();
				return;
			}
		}
		// try: PDF for master file
		LatexDocument *rootDoc = documents.getRootDocumentForDoc(0);
		if (rootDoc) {
			QFileInfo masterFile = rootDoc->getFileInfo();
			foreach (PDFDocument *viewer, viewers) {
				if (viewer->getMasterFile() == masterFile) {
					viewer->focus();
					return;
				}
			}
		}
	}
	// fall back to first
	viewers.at(0)->focus();
#endif
}

void Texstudio::viewCloseSomething()
{
	if (fileSelector) {
		fileSelector.data()->deleteLater();
		return;
	}
	if (buildManager.stopBuildAction()->isEnabled()) {
		buildManager.stopBuildAction()->trigger();
		return;
	}
	if (unicodeInsertionDialog) {
		unicodeInsertionDialog->close();
		return;
	}
	if (completer && completer->isVisible() && completer->close())
		return;

#ifndef NO_POPPLER_PREVIEW
	QWidget *w = QApplication::focusWidget();
	while (w && !qobject_cast<PDFDocument *>(w))
		w = w->parentWidget();

	if (qobject_cast<PDFDocument *>(w)) {
		PDFDocument *focusedPdf = qobject_cast<PDFDocument *>(w);
		if (focusedPdf->embeddedMode) {
			bool pdfClosed = focusedPdf->closeSomething();
			if (pdfClosed) {
				focusEditor();
			} else {
				focusedPdf->widget()->setFocus();
			}
			return;
		}

	}
#endif

	if (textAnalysisDlg) {
		textAnalysisDlg->close();
		return;
	}
	if (currentEditorView() && currentEditorView()->closeSomething())
		return;
	if (outputView->isVisible() && configManager.useEscForClosingLog) {
		outputView->hide();
		return;
	}
#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *doc, PDFDocument::documentList())
		if (doc->embeddedMode) {
			doc->close();
			return;
		}
#endif
	if (windowState() == Qt::WindowFullScreen && !configManager.disableEscForClosingFullscreen) {
		stateFullScreen = saveState(1);
		setWindowState(Qt::WindowNoState);
		restoreState(windowstate, 0);
		fullscreenModeAction->setChecked(false);
		return;
	}
	QTime ct = QTime::currentTime();
	if (ct.second() % 5 != 0) return;
	for (int i = 2; i < 63; i++) if (ct.minute() != i && ct.minute() % i  == 0) return;
	txsInformation("<html><head></head><body><img src=':/images/egg.png'></body></html>");
}

void Texstudio::setFullScreenMode()
{
	if (!fullscreenModeAction->isChecked()) {
		stateFullScreen = saveState(1);
        if(tobemaximized){
            showMaximized();
        }else{
            showNormal();
        }
		restoreState(windowstate, 0);
#if QT_VERSION < 0x040701
		setUnifiedTitleAndToolBarOnMac(true);
#endif
	} else {
		windowstate = saveState(0);
#if QT_VERSION < 0x040701
		setUnifiedTitleAndToolBarOnMac(false); //prevent crash, see https://bugreports.qt-project.org/browse/QTBUG-16274?page=com.atlassian.jira.plugin.system.issuetabpanels:all-tabpanel
#endif
        tobemaximized=isMaximized();
		showFullScreen();
		restoreState(stateFullScreen, 1);
	}
}

void Texstudio::viewSetHighlighting(QAction *act)
{
	if (!currentEditor()) return;
	if (!m_languages->setLanguageFromName(currentEditor(), act->data().toString())) return;
	currentEditorView()->clearOverlays();
	configManager.recentFileHighlightLanguage.insert(getCurrentFileName(), act->data().toString());
	if (configManager.recentFileHighlightLanguage.size() > configManager.recentFilesList.size()) {
		QMap<QString, QString> recentFileHighlightLanguageNew;
		foreach (QString fn, configManager.recentFilesList)
			if (configManager.recentFileHighlightLanguage.contains(fn))
				recentFileHighlightLanguageNew.insert(fn, configManager.recentFileHighlightLanguage.value(fn));
		configManager.recentFileHighlightLanguage = recentFileHighlightLanguageNew;
	}
	// TODO: Check if reCheckSyntax is really necessary. Setting the language emits (among others) contentsChange(0, lines)
	currentEditorView()->document->reCheckSyntax();
}

void Texstudio::showHighlightingMenu()
{
	// check active item just before showing the menu. So we don't have to keep track of the languages, e.g. when switching editors
	if (!currentEditor()) return;
	QLanguageDefinition *ld = currentEditor()->languageDefinition();
	if (ld) {
		foreach (QAction *act, highlightLanguageActions->actions()) {
			if (act->data().toString() == ld->language()) {
				act->setChecked(true);
				break;
			}
		}
	}
}

void Texstudio::viewCollapseBlock()
{
	if (!currentEditorView()) return;
	currentEditorView()->foldBlockAt(false, currentEditorView()->editor->cursor().lineNumber());
}

void Texstudio::viewExpandBlock()
{
	if (!currentEditorView()) return;
	currentEditorView()->foldBlockAt(true, currentEditorView()->editor->cursor().lineNumber());
}

void Texstudio::pdfClosed()
{
#ifndef NO_POPPLER_PREVIEW
	PDFDocument *from = qobject_cast<PDFDocument *>(sender());
	if (from) {
		if (from->embeddedMode) {
			shrinkEmbeddedPDFViewer(true);
			QList<int> sz = mainHSplitter->sizes(); // set widths to 50%, eventually restore user setting
			int sum = 0;
			int last = 0;
			foreach (int i, sz) {
				sum += i;
				last = i;
			}
			if (sum > 0)
				pdfSplitterRel = 1.0 * last / sum;

		}
	}
	//QTimer::singleShot(100, this, SLOT(restoreMacMenuBar()));
#endif
}

void Texstudio::restoreMacMenuBar()
{
#if QT_VERSION<0x050000 && defined Q_OS_MAC
	//workaround to restore mac menubar
	menuBar()->setNativeMenuBar(false);
	menuBar()->setNativeMenuBar(true);
#endif
}

QObject *Texstudio::newPdfPreviewer(bool embedded)
{
#ifndef NO_POPPLER_PREVIEW
	PDFDocument *pdfviewerWindow = new PDFDocument(configManager.pdfDocumentConfig, embedded);
	pdfviewerWindow->setToolbarIconSize(pdfviewerWindow->embeddedMode ? configManager.guiSecondaryToolbarIconSize : configManager.guiToolbarIconSize);
	if (embedded) {
		mainHSplitter->addWidget(pdfviewerWindow);
		QList<int> sz = mainHSplitter->sizes(); // set widths to 50%, eventually restore user setting
		int sum = 0;
		foreach (int i, sz) {
			sum += i;
		}
		sz.clear();
		if (pdfSplitterRel < 0.1 || pdfSplitterRel > 0.9) //sanity check
			pdfSplitterRel = 0.5;
		sz << sum - qRound(pdfSplitterRel * sum);
		sz << qRound(pdfSplitterRel * sum);
		mainHSplitter->setSizes(sz);
	}
	connect(pdfviewerWindow, SIGNAL(triggeredAbout()), SLOT(helpAbout()));
	connect(pdfviewerWindow, SIGNAL(triggeredEnlarge()), SLOT(enlargeEmbeddedPDFViewer()));
	connect(pdfviewerWindow, SIGNAL(triggeredShrink()), SLOT(shrinkEmbeddedPDFViewer()));
	connect(pdfviewerWindow, SIGNAL(triggeredManual()), SLOT(userManualHelp()));
	connect(pdfviewerWindow, SIGNAL(documentClosed()), SLOT(pdfClosed()));
	connect(pdfviewerWindow, SIGNAL(triggeredQuit()), SLOT(fileExit()));
	connect(pdfviewerWindow, SIGNAL(triggeredConfigure()), SLOT(generalOptions()));
	connect(pdfviewerWindow, SIGNAL(syncSource(const QString &, int, bool, QString)), SLOT(syncFromViewer(const QString &, int, bool, QString)));
	connect(pdfviewerWindow, SIGNAL(focusEditor()), SLOT(focusEditor()));
	connect(pdfviewerWindow, SIGNAL(runCommand(const QString &, const QFileInfo &, const QFileInfo &, int)), &buildManager, SLOT(runCommand(const QString &, const QFileInfo &, const QFileInfo &, int)));
	connect(pdfviewerWindow, SIGNAL(triggeredClone()), SLOT(newPdfPreviewer()));

	PDFDocument *from = qobject_cast<PDFDocument *>(sender());
	if (from) {
		pdfviewerWindow->loadFile(from->fileName(), from->getMasterFile(), PDFDocument::Raise | PDFDocument::Focus);
		pdfviewerWindow->goToPage(from->widget()->getPageIndex());
	}//load file before enabling sync or it will jump to the first page

	foreach (PDFDocument *doc, PDFDocument::documentList()) {
		if (doc == pdfviewerWindow) continue;
		connect(doc, SIGNAL(syncView(QString, QFileInfo, int)), pdfviewerWindow, SLOT(syncFromView(QString, QFileInfo, int)));
		connect(pdfviewerWindow, SIGNAL(syncView(QString, QFileInfo, int)), doc, SLOT(syncFromView(QString, QFileInfo, int)));
	}
	return pdfviewerWindow;
#else
	return 0;
#endif
}

void Texstudio::masterDocumentChanged(LatexDocument *doc)
{
	Q_UNUSED(doc);
	Q_ASSERT(documents.singleMode() == !documents.masterDocument);
	if (documents.singleMode()) {
		outputView->resetMessagesAndLog();
	} else {
		configManager.addRecentFile(documents.masterDocument->getFileName(), true);
		editors->moveEditor(doc->getEditorView(), Editors::GroupFront);
	}

	updateMasterDocumentCaption();
	completerNeedsUpdate();
}

void Texstudio::aboutToDeleteDocument(LatexDocument *doc)
{
	emit infoFileClosed();
	editors->removeEditor(doc->getEditorView());
	for (int i = configManager.completerConfig->userMacros.size() - 1; i >= 0; i--)
		if (configManager.completerConfig->userMacros[i].document == doc)
			configManager.completerConfig->userMacros.removeAt(i);
}

//*********************************
void Texstudio::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/uri-list")) event->acceptProposedAction();
}

void Texstudio::dropEvent(QDropEvent *event)
{
	QList<QUrl> uris = event->mimeData()->urls();

	QStringList imageFormats = InsertGraphics::imageFormats();
	saveCurrentCursorToHistory();

	bool alreadyMovedCursor = false;
	for (int i = 0; i < uris.length(); i++) {
		QFileInfo fi = QFileInfo(uris.at(i).toLocalFile());
		if (imageFormats.contains(fi.suffix().toLower()) && currentEditor()) {
			if (!alreadyMovedCursor) {
				QPoint p = currentEditor()->mapToContents(currentEditor()->mapToFrame(currentEditor()->mapFrom(this, event->pos())));
				QDocumentCursor cur = currentEditor()->cursorForPosition(p);
				cur.beginEditBlock();
				if (!cur.atLineStart()) {
					if (!cur.movePosition(1, QDocumentCursor::NextBlock, QDocumentCursor::MoveAnchor)) {
						cur.movePosition(1, QDocumentCursor::EndOfBlock, QDocumentCursor::MoveAnchor);
						cur.insertLine();
					}
				}
				currentEditor()->setCursor(cur);
				cur.endEditBlock();
				alreadyMovedCursor = true;
			}
			quickGraphics(uris.at(i).toLocalFile());
		} else if (fi.suffix() == Session::fileExtension()) {
			loadSession(fi.filePath());
		} else
			load(fi.filePath());
	}
	event->acceptProposedAction();
	raise();
}

void Texstudio::changeEvent(QEvent *e)
{
	switch (e->type()) {
	case QEvent::LanguageChange:
		if (configManager.lastLanguage == configManager.language) return; //don't update if config not changed
		//QMessageBox::information(0,"rt","retranslate",0);
		if (!splashscreen) {
			setupMenus();
			setupDockWidgets();
			updateCaption();
			updateMasterDocumentCaption();
		}
		break;
	default:
		break;
	}
}

void Texstudio::resizeEvent(QResizeEvent *e)
{
	centerFileSelector();
	QMainWindow::resizeEvent(e);
}

#if (QT_VERSION > 0x050000) && (QT_VERSION <= 0x050700) && (defined(Q_OS_MAC))
// workaround for qt/osx not handling all possible shortcuts esp. alt+key/esc
bool Texstudio::eventFilter(QObject *obj, QEvent *event)
{
    if (obj->objectName() == "ConfigDialogWindow" || obj->objectName() == "ShortcutComboBox" || obj->objectName() == "QPushButton" || obj->objectName().startsWith("QMessageBox"))
		return false; // don't handle keys from shortcutcombo (config)
	if (event->type() == QEvent::KeyPress) {
		//qDebug()<<obj->objectName();
		//qDebug()<<obj->metaObject()->className();
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		QString modifier = QString::null;

		if (keyEvent->modifiers() & Qt::ShiftModifier)
			modifier += "Shift+";
		if (keyEvent->modifiers() & Qt::ControlModifier)
			modifier += "Ctrl+";
		if (keyEvent->modifiers() & Qt::AltModifier)
			modifier += "Alt+";
		if (keyEvent->modifiers() & Qt::MetaModifier)
			modifier += "Meta+";

		QString key = QKeySequence(keyEvent->key()).toString();

		QKeySequence result(modifier + key);

		if ((keyEvent->key() == 0) || ((keyEvent->modifiers() == 0) && (key != "Esc")) || (keyEvent->modifiers()&Qt::MetaModifier) || (keyEvent->modifiers() & Qt::ControlModifier) || (keyEvent->modifiers() & Qt::KeypadModifier))
			return false; // no need to handle these

		if (configManager.specialShortcuts.contains(result)) {
            QList<QAction *>acts = configManager.specialShortcuts.values(result);
            foreach(QAction *act,acts){
                if (act && act->parent()->children().contains(obj)){
                    act->trigger();
                    return true;
                }
            }
		}
		return false;
	}
	return false;
}
#endif

//***********************************
void Texstudio::setMostUsedSymbols(QTableWidgetItem *item)
{

	bool changed = false;
	int index = symbolMostused.indexOf(item);
	if (index < 0) {
		//check whether it was loaded as mosteUsed ...
		for (int i = 0; i < symbolMostused.count(); i++) {
			QTableWidgetItem *elem = symbolMostused.at(i);
			if (elem->data(Qt::UserRole + 2) == item->data(Qt::UserRole + 2)) {
				index = i;
				item->setData(Qt::UserRole + 1, QVariant::fromValue(elem));
				int cnt = elem->data(Qt::UserRole).toInt();
				elem->setData(Qt::UserRole, cnt + 1);
				elem->setData(Qt::UserRole + 3, QVariant::fromValue(item)); //reference to original item for removing later
				item = elem;
				break;
			}
		}
		if (index < 0) {
			QTableWidgetItem *elem = item->clone();
			item->setData(Qt::UserRole + 1, QVariant::fromValue(elem));
			elem->setData(Qt::UserRole + 3, QVariant::fromValue(item)); //reference to original item for removing later
			symbolMostused.append(elem);
			if (symbolMostused.size() <= 12)
				changed = true;
		}
	}
	if (index > -1) {
		symbolMostused.removeAt(index);
		while (index > 0 && symbolMostused[index - 1]->data(Qt::UserRole).toInt() < item->data(Qt::UserRole).toInt()) {
			index--;
		}
		symbolMostused.insert(index, item);
		changed = true;
	}
	if (changed) MostUsedSymbolWidget->SetUserPage(symbolMostused);
}

void Texstudio::updateCompleter(LatexEditorView *edView)
{
	CodeSnippetList words;

	if (configManager.parseBibTeX) documents.updateBibFiles();

	if (!edView)
		edView = currentEditorView();

	QList<LatexDocument *> docs;
	LatexParser ltxCommands = LatexParser::getInstance();

	if (edView && edView->document) {
		// determine from which docs data needs to be collected
		docs = edView->document->getListOfDocs();

		// collect user commands and references
		foreach (LatexDocument *doc, docs) {
			words.unite(doc->userCommandList());
			words.unite(doc->additionalCommandsList());

			ltxCommands.append(doc->ltxCommands);
		}
	}

	// collect user commands and references
	QSet<QString> collected_labels;
	foreach (const LatexDocument *doc, docs) {
		collected_labels.unite(doc->labelItems().toSet());
		foreach (const QString &refCommand, latexParser.possibleCommands["%ref"]) {
			QString temp = refCommand + "{%1}";
			foreach (const QString &l, doc->labelItems())
				words.insert(temp.arg(l));
		}
	}
	if (configManager.parseBibTeX) {
		QSet<QString> bibIds;

		QStringList collected_mentionedBibTeXFiles;
		foreach (const LatexDocument *doc, docs) {
			collected_mentionedBibTeXFiles << doc->listOfMentionedBibTeXFiles();
		}

		for (int i = 0; i < collected_mentionedBibTeXFiles.count(); i++) {
			if (!documents.bibTeXFiles.contains(collected_mentionedBibTeXFiles[i])) {
				qDebug("BibTeX-File %s not loaded", collected_mentionedBibTeXFiles[i].toLatin1().constData());
				continue; //wtf?s
			}
			BibTeXFileInfo &bibTex = documents.bibTeXFiles[collected_mentionedBibTeXFiles[i]];

			// add citation to completer for direct citation completion
			bibIds.unite(bibTex.ids);
		}
		//handle bibitem definitions
		foreach (const LatexDocument *doc, docs) {
			bibIds.unite(doc->bibItems().toSet());
		}
		//automatic use of cite commands
		QStringList citationCommands;
		foreach (const QString &citeCommand, latexParser.possibleCommands["%cite"]) {
			QString temp = '@' + citeCommand + "{@}";
			citationCommands.append(temp);
			//words.insert(temp);
			/*foreach (const QString &value, bibIds)
			    words.insert(temp.arg(value));*/
		}
		foreach (QString citeCommand, latexParser.possibleCommands["%citeExtended"]) {
			QString temp = '@' + citeCommand.replace("%<bibid%>", "@");
			citationCommands.append(temp);
			//temp=citeCommand.replace("%<bibid%>","@");
			//words.insert(temp);
		}
		completer->setAdditionalWords(citationCommands.toSet(), CT_CITATIONCOMMANDS);
		completer->setAdditionalWords(bibIds, CT_CITATIONS);
	}

	completer->setAdditionalWords(collected_labels, CT_LABELS);

	completionBaseCommandsUpdated = false;


	completer->setAdditionalWords(words, CT_COMMANDS);

	// add keyval completion, add special lists
	foreach (const QString &elem, ltxCommands.possibleCommands.keys()) {
		if (elem.startsWith("key%")) {
			QString name = elem.mid(4);
			if (name.endsWith("#c"))
				name.chop(2);
			if (!name.isEmpty()) {
				completer->setKeyValWords(name, ltxCommands.possibleCommands[elem]);
			}
		}
		if (elem.startsWith("%") && latexParser.mapSpecialArgs.values().contains(elem)) {
			completer->setKeyValWords(elem, ltxCommands.possibleCommands[elem]);
		}
	}
	// add context completion
	LatexCompleterConfig *config = completer->getConfig();
	if (config) {
		foreach (const QString &elem, config->specialCompletionKeys) {
			completer->setContextWords(ltxCommands.possibleCommands[elem], elem);
		}
	}


	if (edView) edView->viewActivated();

	GrammarCheck::staticMetaObject.invokeMethod(grammarCheck, "init", Qt::QueuedConnection, Q_ARG(LatexParser, latexParser), Q_ARG(GrammarCheckerConfig, *configManager.grammarCheckerConfig));

	updateHighlighting();

	mCompleterNeedsUpdate = false;
}

void Texstudio::outputPageChanged(const QString &id)
{
	if (id == outputView->LOG_PAGE && !outputView->getLogWidget()->logPresent()) {
		if (!loadLog())
			return;
		if (hasLatexErrors())
			viewLog();
	}
}

void Texstudio::jumpToSearchResult(QDocument *doc, int lineNumber, const SearchQuery *query)
{
	REQUIRE(qobject_cast<LatexDocument *>(doc));
	if (currentEditor() && currentEditor()->document() == doc && currentEditor()->cursor().lineNumber() == lineNumber) {
		QDocumentCursor c = currentEditor()->cursor();
		int col = c.columnNumber();
		col = query->getNextSearchResultColumn(c.line().text() , col + 1);
		gotoLine(lineNumber, col);
	} else {
		gotoLine(lineNumber, doc->getFileName().size() ? doc->getFileName() : qobject_cast<LatexDocument *>(doc)->getTemporaryFileName());
		int col = query->getNextSearchResultColumn(currentEditor()->document()->line(lineNumber).text(), 0);
		gotoLine(lineNumber, col);
		outputView->showPage(outputView->SEARCH_RESULT_PAGE);
	}
	QDocumentCursor highlight = currentEditor()->cursor();
	highlight.movePosition(query->searchExpression().length(), QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
	currentEditorView()->temporaryHighlight(highlight);
}

void Texstudio::gotoLine(int line, int col, LatexEditorView *edView, QEditor::MoveFlags mflags, bool setFocus)
{
	bool changeCurrentEditor = (edView != currentEditorView());
	if (!edView)
		edView = currentEditorView(); // default

	if (!edView || line < 0) return;

	saveCurrentCursorToHistory();

	if (changeCurrentEditor) {
		if (editors->containsEditor(edView)) {
			editors->setCurrentEditor(edView);
			mflags &= ~QEditor::Animated;
		} else {
			load(edView->getDocument()->getFileName());
		}
	}
	edView->editor->setCursorPosition(line, col, false);
	edView->editor->ensureCursorVisible(mflags);
	if (setFocus) {
		edView->editor->setFocus();
	}
}

bool Texstudio::gotoLine(int line, const QString &fileName)
{
	LatexEditorView *edView = getEditorViewFromFileName(fileName, true);
	QEditor::MoveFlags mflags = QEditor::Navigation;
	if (!edView) {
		if (!load(fileName)) return false;
		mflags &= ~QEditor::Animated;
	}
	gotoLine(line, 0, edView, mflags);
	return true;
}

void Texstudio::gotoLogEntryEditorOnly(int logEntryNumber)
{
	if (logEntryNumber < 0 || logEntryNumber >= outputView->getLogWidget()->getLogModel()->count()) return;
	LatexLogEntry entry = outputView->getLogWidget()->getLogModel()->at(logEntryNumber);
	QString fileName = entry.file;
	if (!activateEditorForFile(fileName, true))
		if (!load(fileName)) return;
	if (currentEditorView()->logEntryToLine.isEmpty()) {
		updateLogEntriesInEditors();
	}
	if (configManager.showLogMarkersWhenClickingLogEntry) {
		setLogMarksVisible(true);
	}
	//get line
	QDocumentLineHandle *dlh = currentEditorView()->logEntryToLine.value(logEntryNumber, 0);
	if (!dlh) return;
	//goto
	gotoLine(currentEditor()->document()->indexOf(dlh));
	QDocumentCursor c = getLogEntryContextCursor(dlh, entry);
	if (c.isValid()) {
		currentEditorView()->editor->setCursor(c, false);
	}
}

/*!
 * Returns a cursor marking the part of the line which the log entry is referring to.
 * This assumes that the cursor was already set to the correct line before calling the function.
 */
QDocumentCursor Texstudio::getLogEntryContextCursor(const QDocumentLineHandle *dlh, const LatexLogEntry &entry)
{
	QRegExp rxUndefinedControlSequence("^Undefined\\ control\\ sequence.*(\\\\\\w+)$");
	QRegExp rxEnvironmentUndefined("^Environment (\\w+) undefined\\.");
	QRegExp rxReferenceMissing("^Reference `(\\w+)' on page (\\d+) undefined");
	QRegExp rxCitationMissing("^Citation `(\\w+)' on page (\\d+) undefined");
	if (entry.message.indexOf(rxUndefinedControlSequence) == 0) {
		QString cmd = rxUndefinedControlSequence.cap(1);
		int startCol = dlh->text().indexOf(cmd);
		if (startCol >= 0) {
			QDocumentCursor cursor = currentEditorView()->editor->cursor();
			cursor.selectColumns(startCol, startCol + cmd.length());
			return cursor;
		}
	} else if (entry.message.indexOf(rxEnvironmentUndefined) == 0) {
		QString env = rxEnvironmentUndefined.cap(1);
		int startCol = dlh->text().indexOf("\\begin{" + env + "}");
		if (startCol >= 0) {
			startCol += 7;  // length of \begin{
			QDocumentCursor cursor = currentEditorView()->editor->cursor();
			cursor.selectColumns(startCol, startCol + env.length());
			return cursor;
		}
	} else if (entry.message.indexOf(rxReferenceMissing) == 0) {
		int fid = currentEditorView()->document->getFormatId("referenceMissing");
		foreach (const QFormatRange &fmtRange, dlh->getOverlays(fid)) {
			if (dlh->text().mid(fmtRange.offset, fmtRange.length) == rxReferenceMissing.cap(1)) {
				QDocumentCursor cursor = currentEditorView()->editor->cursor();
				cursor.selectColumns(fmtRange.offset, fmtRange.offset + fmtRange.length);
				return cursor;
			}
		}
	} else if (entry.message.indexOf(rxCitationMissing) == 0) {
		int fid = currentEditorView()->document->getFormatId("citationMissing");
		foreach (const QFormatRange &fmtRange, dlh->getOverlays(fid)) {
			if (dlh->text().mid(fmtRange.offset, fmtRange.length) == rxCitationMissing.cap(1)) {
				QDocumentCursor cursor = currentEditorView()->editor->cursor();
				cursor.selectColumns(fmtRange.offset, fmtRange.offset + fmtRange.length);
				return cursor;
			}
		}
	} else {
		// error messages that are followed by the context; e.g. Too many }'s. \textit{}}
		QStringList messageStarts = QStringList() << "Too many }'s. " << "Missing $ inserted. ";
		foreach (const QString &messageStart, messageStarts) {
			if (entry.message.startsWith(messageStart)) {
				QString context = entry.message.mid(messageStart.length());
				int startCol = dlh->text().indexOf(context);
				if (startCol >= 0) {
					QDocumentCursor cursor = currentEditorView()->editor->cursor();
					cursor.setColumnNumber(startCol += context.length());
					return cursor;
				}
			}
		}
	}
	return QDocumentCursor();
}

bool Texstudio::gotoLogEntryAt(int newLineNumber)
{
	//goto line
	if (newLineNumber < 0) return false;
	gotoLine(newLineNumber);
	//find error number
	QDocumentLineHandle *lh = currentEditorView()->editor->document()->line(newLineNumber).handle();
	int logEntryNumber = currentEditorView()->lineToLogEntries.value(lh, -1);
	if (logEntryNumber == -1) return false;
	//goto log entry
	outputView->selectLogEntry(logEntryNumber);

	QPoint p = currentEditorView()->editor->mapToGlobal(currentEditorView()->editor->mapFromContents(currentEditorView()->editor->cursor().documentPosition()));
	//  p.ry()+=2*currentEditorView()->editor->document()->fontMetrics().lineSpacing();

	REQUIRE_RET(outputView->getLogWidget()->getLogModel(), true);
	QList<int> errors = currentEditorView()->lineToLogEntries.values(lh);
	QString msg = outputView->getLogWidget()->getLogModel()->htmlErrorTable(errors);

	QToolTip::showText(p, msg, 0);
	LatexEditorView::hideTooltipWhenLeavingLine = newLineNumber;
	return true;
}

bool Texstudio::gotoMark(bool backward, int id)
{
	if (!currentEditorView()) return false;
	if (backward)
		return gotoLogEntryAt(currentEditorView()->editor->document()->findPreviousMark(id, qMax(0, currentEditorView()->editor->cursor().lineNumber() - 1), 0));
	else
		return gotoLogEntryAt(currentEditorView()->editor->document()->findNextMark(id, currentEditorView()->editor->cursor().lineNumber() + 1));
}

QList<int> Texstudio::findOccurencesApproximate(QString line, const QString &guessedWord)
{
	QList<int> columns;

	// exact match
	columns = indicesOf(line, guessedWord);
	if (columns.isEmpty()) columns = indicesOf(line, guessedWord, Qt::CaseSensitive);

	QString changedWord = guessedWord;
	if (columns.isEmpty()) {
		//search again and ignore useless characters

		QString regex;
		for (int i = 0; i < changedWord.size(); i++)
			if (changedWord[i].category() == QChar::Other_Control || changedWord[i].category() == QChar::Other_Format)
				changedWord[i] = '\1';
		foreach (const QString &x, changedWord.split('\1', QString::SkipEmptyParts))
			if (regex.isEmpty()) regex += QRegExp::escape(x);
			else regex += ".{0,2}" + QRegExp::escape(x);
		QRegExp rx = QRegExp(regex);
		columns = indicesOf(line, rx);
		if (columns.isEmpty()) {
			rx.setCaseSensitivity(Qt::CaseSensitive);
			columns = indicesOf(line, rx);
		}
	}
	if (columns.isEmpty()) {
		//search again and allow additional whitespace
		QString regex;
		foreach (const QString &x , changedWord.split(" ", QString::SkipEmptyParts))
			if (regex.isEmpty()) regex = QRegExp::escape(x);
			else regex += "\\s+" + QRegExp::escape(x);
		QRegExp rx = QRegExp(regex);
		columns = indicesOf(line, rx);
		if (columns.isEmpty()) {
			rx.setCaseSensitivity(Qt::CaseSensitive);
			columns = indicesOf(line, rx);
		}
	}
	if (columns.isEmpty()) {
		int bestMatch = -1, bestScore = 0;
		for (int i = 0; i < line.size() - guessedWord.size(); i++) {
			int score = 0;
			for (int c = i, s = 0; c < line.size() && s < guessedWord.size(); c++, s++) {
				QChar C = line[c], S = guessedWord[s];
				if (C == S) score += 5; //perfect match
				else if (C.toLower() == S.toLower()) score += 2; //ok match
				else if (C.isSpace()) s--; //skip spaces
				else if (S.isSpace()) c--; //skip spaces
				else if (S.category() == QChar::Other_Control || S.category() == QChar::Other_Format) {
					for (s++; s < guessedWord.size() && (guessedWord[s].category() == QChar::Other_Control || guessedWord[s].category() == QChar::Other_Format); s++); //skip nonsense
					if (s >= guessedWord.size()) continue;
					if (guessedWord[s] == C) {
						score += 5;
						continue;
					}
					if (c + 1 < line.size() && guessedWord[s] == line[c + 1]) {
						score += 5;
						c++;
						continue;
					}
					//also skip next character after that nonsense
				}
			}
			if (score > bestScore) bestScore = score, bestMatch = i;
		}
		if (bestScore > guessedWord.size() * 5 / 3) columns.append(bestMatch); //accept if 0.33 similarity
	}
	return columns;
}

void Texstudio::syncFromViewer(const QString &fileName, int line, bool activate, const QString &guessedWord)
{
	if (!activateEditorForFile(fileName, true, activate)) {
		QWidget *w = focusWidget();
		bool success = load(fileName);
		if (!activate)
			w->setFocus();  // restore focus
		if (!success) return;
	}
	shrinkEmbeddedPDFViewer();

	QDocumentLine l = currentEditorView()->document->lineFromLineSnapshot(line);
	if (l.isValid()) {
		int originalLineNumber = currentEditorView()->document->indexOf(l, line);
		if (originalLineNumber >= 0) line = originalLineNumber;
	}

	gotoLine(line, 0, 0, QEditor::Navigation, activate);
	Q_ASSERT(currentEditor());

	// guessedWord may appear multiple times -> we highlight them all
	QList<int> columns = findOccurencesApproximate(currentEditor()->cursor().line().text(), guessedWord);

	if (columns.isEmpty() || guessedWord.isEmpty()) {
		// highlight complete line
		QDocumentCursor highlight(currentEditor()->document(), line, 0);
		highlight.movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::KeepAnchor);
		currentEditorView()->temporaryHighlight(highlight);
	} else {
		// highlight all found positions
		QDocumentCursor highlight(currentEditor()->document(), line, 0);
		foreach (int col, columns) {
			int cursorCol = col + guessedWord.length() / 2;
			highlight.setColumnNumber(cursorCol);
			highlight.movePosition(1, QDocumentCursor::StartOfWord, QDocumentCursor::MoveAnchor);
			highlight.movePosition(1, QDocumentCursor::EndOfWord, QDocumentCursor::KeepAnchor);
			if (!highlight.hasSelection()) { // fallback, if we are not at a word
				highlight.setColumnNumber(cursorCol);
				highlight.movePosition(1, QDocumentCursor::PreviousCharacter, QDocumentCursor::MoveAnchor);
				highlight.movePosition(1, QDocumentCursor::NextCharacter, QDocumentCursor::KeepAnchor);
			}
			currentEditor()->setCursorPosition(currentEditor()->cursor().lineNumber(), cursorCol, false);
			currentEditor()->ensureCursorVisible(QEditor::KeepSurrounding | QEditor::ExpandFold);
			currentEditorView()->temporaryHighlight(highlight);
		}
	}

	if (activate) {
		raise();
		show();
		activateWindow();
		if (isMinimized()) showNormal();
	}

}

void Texstudio::goBack()
{
	QDocumentCursor currentCur;
	if (currentEditorView()) currentCur = currentEditorView()->editor->cursor();
	setGlobalCursor(cursorHistory->back(currentCur));
}

void Texstudio::goForward()
{
	QDocumentCursor currentCur;
	if (currentEditorView()) currentCur = currentEditorView()->editor->cursor();
	setGlobalCursor(cursorHistory->forward(currentCur));
}

void Texstudio::setGlobalCursor(const QDocumentCursor &c)
{
	if (c.isValid()) {
		LatexDocument *doc = qobject_cast<LatexDocument *>(c.document());
		if (doc && doc->getEditorView()) {
			LatexEditorView *edView = doc->getEditorView();
			QEditor::MoveFlags mflags = QEditor::KeepSurrounding | QEditor::ExpandFold;
			if (edView == currentEditorView()) mflags |= QEditor::Animated;
			editors->setCurrentEditor(edView);
			edView->editor->setFocus();
			edView->editor->setCursor(c, false);
			edView->editor->ensureCursorVisible(mflags);
		}
	}
}

void Texstudio::fuzzBackForward()
{
#ifdef NOT_DEFINED__FUZZER_NEEDED_ONLY_FOR_DEBUGGING_RANDOM_CRASH_OF_CURSOR_HISTORY
	int rep = random() % (1 + cursorHistory->count());
	for (int j = 0; j < rep; j++) goBack();
	rep = random() % (1 + cursorHistory->count());
	for (int j = 0; j < rep; j++) goForward();
#endif
}

void Texstudio::fuzzCursorHistory()
{
#ifdef NOT_DEFINED__FUZZER_NEEDED_ONLY_FOR_DEBUGGING_RANDOM_CRASH_OF_CURSOR_HISTORY
	QString fillText;
	for (int i = 0; i < 100; i++)
		fillText += "\n" + QString("foobar abc xyz").repeated(random() % 100);
	for (int i = 0; i < 100; i++) {
		if (!documents.documents.isEmpty()) {
			if (random() % 1000 < 500) documents.deleteDocument(documents.documents[random() % documents.documents.length()]);
			fuzzBackForward();
		}
		if (!documents.documents.isEmpty()) {
			QApplication::processEvents();
			if (random() % 1000 < 500) documents.deleteDocument(documents.documents[random() % documents.documents.length()]);
			fuzzBackForward();
		}
		fileNew();
		currentEditor()->setText(fillText);
		QApplication::processEvents();
		int rep = random() % 100;
		for (int j = 0; j < rep; j++) {
			EditorView->setCurrentIndex(EditorView->count());
			int l =  random() % currentEditor()->document()->lineCount();
			int c = random() % (currentEditor()->document()->line(l).length() + 100);
			currentEditor()->setCursor(currentEditor()->document()->cursor(l, c, random() % 100, random() % 100));
			saveCurrentCursorToHistory();
			fuzzBackForward();
		}
	}
#endif
}

void Texstudio::saveCurrentCursorToHistory()
{
	saveEditorCursorToHistory(currentEditorView());
}

void Texstudio::saveEditorCursorToHistory(LatexEditorView *edView)
{
	if (!edView) return;
	cursorHistory->insertPos(edView->editor->cursor());
}

void Texstudio::StructureContextMenu(const QPoint &point)
{
	//StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	contextIndex = structureTreeView->indexAt(point);
	contextEntry = LatexDocumentsModel::indexToStructureEntry(contextIndex);
	if (!contextEntry) return;
	if (contextEntry->type == StructureEntry::SE_DOCUMENT_ROOT) {
		QMenu menu;
		if (contextEntry->document != documents.masterDocument) {
			menu.addAction(tr("Close document"), this, SLOT(structureContextMenuCloseDocument()));
			menu.addAction(tr("Set as explicit root document"), this, SLOT(structureContextMenuSwitchMasterDocument()));
			menu.addAction(tr("Open all related documents"), this, SLOT(structureContextMenuOpenAllRelatedDocuments()));
			menu.addAction(tr("Close all related documents"), this, SLOT(structureContextMenuCloseAllRelatedDocuments()));
		} else
			menu.addAction(tr("Remove explicit root document role"), this, SLOT(structureContextMenuSwitchMasterDocument()));
		if (documents.model->getSingleDocMode()) {
			menu.addAction(tr("Show all open documents in this tree"), this, SLOT(latexModelViewMode()));
		} else {
			menu.addAction(tr("Show only current document in this tree"), this, SLOT(latexModelViewMode()));
		}
		menu.addSeparator();
		menu.addAction(tr("Move document to &front"), this, SLOT(moveDocumentToFront()));
		menu.addAction(tr("Move document to &end"), this, SLOT(moveDocumentToEnd()));
		menu.addSeparator();
		menu.addAction(tr("Expand Subitems"), this, SLOT(structureContextMenuExpandSubitems()));
		menu.addAction(tr("Collapse Subitems"), this, SLOT(structureContextMenuCollapseSubitems()));
		menu.addAction(tr("Expand all documents"), this, SLOT(structureContextMenuExpandAllDocuments()));
		menu.addAction(tr("Collapse all documents"), this, SLOT(structureContextMenuCollapseAllDocuments()));
		menu.addSeparator();
		menu.addAction(msgGraphicalShellAction(), this, SLOT(structureContextMenuShowInGraphicalShell()));
		menu.exec(structureTreeView->mapToGlobal(point));
		return;
	}
	if (!contextEntry->parent) return;
	if (contextEntry->type == StructureEntry::SE_LABEL) {
		QMenu menu;
		menu.addAction(tr("Insert"), this, SLOT(editPasteRef()))->setData(contextEntry->title);
		menu.addAction(tr("Insert as %1").arg("\\ref{...}"), this, SLOT(editPasteRef()))->setData(QString("\\ref{%1}").arg(contextEntry->title));
		menu.addAction(tr("Insert as %1").arg("\\pageref{...}"), this, SLOT(editPasteRef()))->setData(QString("\\pageref{%1}").arg(contextEntry->title));
		menu.addSeparator();
		QAction *act = menu.addAction(tr("Find Usages"), this, SLOT(findLabelUsages()));
		act->setData(contextEntry->title);
		act->setProperty("doc", QVariant::fromValue<LatexDocument *>(contextEntry->document));
		menu.exec(structureTreeView->mapToGlobal(point));
		return;
	}
	if (contextEntry->type == StructureEntry::SE_SECTION) {
		QMenu menu(this);

		StructureEntry *labelEntry = LatexDocumentsModel::labelForStructureEntry(contextEntry);
		if (labelEntry) {
			menu.addAction(tr("Insert Label"), this, SLOT(editPasteRef()))->setData(labelEntry->title);
			foreach (QString refCmd, configManager.referenceCommandsInContextMenu.split(",")) {
				refCmd = refCmd.trimmed();
				if (!refCmd.startsWith('\\')) continue;
				menu.addAction(QString(tr("Insert %1 to Label", "autoreplaced, e.g.: Insert \\ref to Label").arg(refCmd)), this, SLOT(editPasteRef()))->setData(QString("%1{%2}").arg(refCmd).arg(labelEntry->title));
			}
			menu.addSeparator();
		} else {
			menu.addAction(tr("Create Label"), this, SLOT(createLabelFromAction()))->setData(QVariant::fromValue(contextEntry));
			menu.addSeparator();
		}

		menu.addAction(tr("Copy"), this, SLOT(editSectionCopy()));
		menu.addAction(tr("Cut"), this, SLOT(editSectionCut()));
		menu.addAction(tr("Paste Before"), this, SLOT(editSectionPasteBefore()));
		menu.addAction(tr("Paste After"), this, SLOT(editSectionPasteAfter()));
		menu.addSeparator();
		menu.addAction(tr("Indent Section"), this, SLOT(editIndentSection()));
		menu.addAction(tr("Unindent Section"), this, SLOT(editUnIndentSection()));
		if (!contextEntry->children.isEmpty()) {
			menu.addSeparator();
			menu.addAction(tr("Expand Subitems"), this, SLOT(structureContextMenuExpandSubitems()));
			menu.addAction(tr("Collapse Subitems"), this, SLOT(structureContextMenuCollapseSubitems()));
		}
		menu.exec(structureTreeView->mapToGlobal(point));
		return;
	}
	if (contextEntry->type == StructureEntry::SE_MAGICCOMMENT) {
		QMenu menu;
		menu.addAction(LatexEditorView::tr("Go to Definition"), this, SLOT(moveCursorTodlh()))->setData(QVariant::fromValue(contextEntry));
		menu.exec(structureTreeView->mapToGlobal(point));
		return;
	}
	if (contextEntry->type == StructureEntry::SE_INCLUDE) {
		QMenu menu;
		menu.addAction(LatexEditorView::tr("Open Document"), this, SLOT(openExternalFile()))->setData(QVariant::fromValue(contextEntry));
		menu.addAction(LatexEditorView::tr("Go to Definition"), this, SLOT(moveCursorTodlh()))->setData(QVariant::fromValue(contextEntry));
		menu.exec(structureTreeView->mapToGlobal(point));
		return;
	}

}

void Texstudio::openExternalFile()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	StructureEntry *entry = qvariant_cast<StructureEntry *>(act->data());
	if (entry) {
		openExternalFile(entry->title);
	}
}

void Texstudio::structureContextMenuCloseDocument()
{
	if (!contextEntry)
		return;
	LatexDocument *document = contextEntry->document;
	contextEntry = 0;
	if (!document) return;
	if (document->getEditorView()) editors->requestCloseEditor(document->getEditorView());
	else if (document == documents.masterDocument) structureContextMenuSwitchMasterDocument();
}

void Texstudio::structureContextMenuSwitchMasterDocument()
{
	if (!contextEntry)
		return;
	LatexDocument *document = contextEntry->document;
	contextEntry = 0;
	if (!document) return;
	if (document == documents.masterDocument) setAutomaticRootDetection();
	else setExplicitRootDocument(document);
}

void Texstudio::structureContextMenuOpenAllRelatedDocuments()
{
	if (!contextEntry)
		return;
	LatexDocument *document = contextEntry->document;
	contextEntry = 0;
	if (!document) return;

	QSet<QString> checkedFiles, filesToCheck;
	filesToCheck.insert(document->getFileName());

	while (!filesToCheck.isEmpty()) {
		QString f = *filesToCheck.begin();
		filesToCheck.erase(filesToCheck.begin());
		if (checkedFiles.contains(f)) continue;
		checkedFiles.insert(f);
		document = documents.findDocument(f);
		if (!document) {
			LatexEditorView *lev = load(f);
			document = lev ? lev->document : 0;
		}
		if (!document) continue;
		foreach (const QString &fn, document->includedFilesAndParent()) {
			QString t = document->findFileName(fn);
			if (!t.isEmpty()) filesToCheck.insert(t);
		}
	}
}

void Texstudio::structureContextMenuCloseAllRelatedDocuments()
{
	if (!contextEntry)
		return;
	LatexDocument *document = contextEntry->document;
	contextEntry = 0;
	if (!document) return;

	QList<LatexDocument *> l = document->getListOfDocs();
	QList<LatexEditorView *> viewsToClose;
	foreach (LatexDocument *d, l)
		if (d->getEditorView())
			viewsToClose << d->getEditorView();
	if (!saveFilesForClosing(viewsToClose)) return;
	foreach (LatexDocument *d, l) {
		if (documents.documents.contains(d))
			documents.deleteDocument(d); //this might hide the document
		if (documents.hiddenDocuments.contains(d))
			documents.deleteDocument(d, d->isHidden(), d->isHidden());
	}
}

void Texstudio::structureContextMenuExpandSubitems()
{
	if (!contextEntry)
		return;
	setSubtreeExpanded(structureTreeView, contextIndex, true);
	contextEntry = 0;
}

void Texstudio::structureContextMenuCollapseSubitems()
{
	if (!contextEntry)
		return;
	setSubtreeExpanded(structureTreeView, contextIndex, false);
	contextEntry = 0;
}

void Texstudio::structureContextMenuExpandAllDocuments()
{
	setSubtreeExpanded(structureTreeView, structureTreeView->rootIndex(), true);
}

void Texstudio::structureContextMenuCollapseAllDocuments()
{
	setSubtreeExpanded(structureTreeView, structureTreeView->rootIndex(), false);
}

void Texstudio::structureContextMenuShowInGraphicalShell()
{
	if (!contextEntry)
		return;
	LatexDocument *document = contextEntry->document;
	contextEntry = 0;
	if (!document) return;
	showInGraphicalShell(this, document->getFileName());
}

void Texstudio::editPasteRef()
{
	if (!currentEditorView()) return;

	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;

	currentEditor()->write(action->data().toString());
	currentEditorView()->setFocus();
}

void Texstudio::previewLatex()
{
	if (!currentEditorView()) return;
	// get selection
	QDocumentCursor c = currentEditorView()->editor->cursor();
	QDocumentCursor previewc;
	if (c.hasSelection()) {
		previewc = c; //X o riginalText = c.selectedText();
	} else {
		// math context
		QSet<int> mathFormats = QSet<int>() << m_formats->id("numbers") << m_formats->id("math-keyword") << m_formats->id("align-ampersand");
		QSet<int> lineEndFormats = QSet<int>() << m_formats->id("keyword") /* newline char */ << m_formats->id("comment");
		mathFormats.remove(0); // keep only valid entries in list
		lineEndFormats.remove(0);
		previewc = currentEditorView()->findFormatsBegin(c, mathFormats, lineEndFormats);
		previewc = currentEditorView()->parenthizedTextSelection(previewc);
	}
	if (!previewc.hasSelection()) {
		// special handling for cusor in the middle of \[ or \]
		if (c.previousChar() == '\\' and (c.nextChar() == '[' || c.nextChar() == ']')) {
			c.movePosition(1, QDocumentCursor::PreviousCharacter);
			previewc = currentEditorView()->parenthizedTextSelection(c);
		}
	}
	if (!previewc.hasSelection()) {
		// in environment delimiter (\begin{env} or \end{env})
		QString command;
		Token tk = getTokenAtCol(c.line().handle(), c.columnNumber());
		if (tk.type != Token::none)
			command = tk.getText();
		if (tk.type == Token::env || tk.type == Token::beginEnv ) {
			c.setColumnNumber(tk.start);
			previewc = currentEditorView()->parenthizedTextSelection(c);
		}
		if (tk.type == Token::command && (command == "\\begin" || command == "\\end")) {
			c.setColumnNumber(tk.start + tk.length + 1);
			previewc = currentEditorView()->parenthizedTextSelection(c);
		}
	}
	if (!previewc.hasSelection()) {
		// already at parenthesis
		previewc = currentEditorView()->parenthizedTextSelection(currentEditorView()->editor->cursor());
	}
	if (!previewc.hasSelection()) return;

	showPreview(previewc, true);

}

void Texstudio::previewAvailable(const QString &imageFile, const PreviewSource &source)
{
	QPixmap pixmap;
	int devPixelRatio = 1;
#if QT_VERSION >= 0x050000
	devPixelRatio = devicePixelRatio();
#endif
	float scale = configManager.segmentPreviewScalePercent / 100.;
	float min = 0.2;
	float max = 100;
	scale = qMax(min, qMin(max, scale)) * devPixelRatio;
	bool fromPDF = false;

#ifndef NO_POPPLER_PREVIEW
	fromPDF = imageFile.toLower().endsWith(".pdf");
	if (fromPDF) {
		// special treatment for pdf files (embedded pdf mode)
		if (configManager.previewMode == ConfigManager::PM_EMBEDDED) {
			runInternalCommand("txs:///view-pdf-internal", QFileInfo(imageFile), "--embedded");
			if (currentEditorView())
				currentEditorView()->setFocus();
			return;
		} else {
			Poppler::Document *document = Poppler::Document::load(imageFile);
			if (!document)
				return;
			Poppler::Page *page = document->page(0);
			if (!page) {
				delete document;
				return;
			}
			document->setRenderHint(Poppler::Document::Antialiasing);
			document->setRenderHint(Poppler::Document::TextAntialiasing);
			float c = 1.25;  // empirical correction factor because pdf images are smaller than dvipng images. TODO: is logicalDpiX correct?
			pixmap = QPixmap::fromImage(page->renderToImage(logicalDpiX() * scale * c, logicalDpiY() * scale * c));
			delete page;
			delete document;
		}
	}
#endif
	if (!fromPDF) {
		pixmap.load(imageFile);
		if (scale < 0.99 || 1.01 < scale) {
			// TODO: this does scale the pixmaps, but it would be better to render higher resolution images directly in the compilation process.
			pixmap = pixmap.scaledToWidth(pixmap.width() * scale, Qt::SmoothTransformation);
		}
	}
#if QT_VERSION >= 0x050000
	if (devPixelRatio != 1) {
		pixmap.setDevicePixelRatio(devPixelRatio);
	}
#endif

	if (configManager.previewMode == ConfigManager::PM_BOTH ||
	        configManager.previewMode == ConfigManager::PM_PANEL ||
	        (configManager.previewMode == ConfigManager::PM_TOOLTIP_AS_FALLBACK && outputView->isPreviewPanelVisible())) {
		outputView->showPage(outputView->PREVIEW_PAGE);
		outputView->previewLatex(pixmap);
	}
	if (configManager.previewMode == ConfigManager::PM_BOTH ||
	        configManager.previewMode == ConfigManager::PM_TOOLTIP ||
            //source.atCursor || // respect preview setting
	        (configManager.previewMode == ConfigManager::PM_TOOLTIP_AS_FALLBACK && !outputView->isPreviewPanelVisible()) ||
            (source.fromLine < 0 && !source.atCursor)) { // completer preview
		QPoint p;
		if (source.atCursor)
			p = currentEditorView()->getHoverPosistion();
		else
			p = currentEditorView()->editor->mapToGlobal(currentEditorView()->editor->mapFromContents(currentEditorView()->editor->cursor().documentPosition()));

		QRect screen = QApplication::desktop()->screenGeometry();
		int w = pixmap.width();
		if (w > screen.width()) w = screen.width() - 2;
		if (!fromPDF) {
			QToolTip::showText(p, QString("<img src=\"" + imageFile + "\" width=%1 />").arg(w / devPixelRatio), 0);
		} else {
			QString text;
#if QT_VERSION >= 0x040700
			text = getImageAsText(pixmap, w);
#else
			QString tempPath = QDir::tempPath() + QDir::separator() + "." + QDir::separator();
			pixmap.save(tempPath + "txs_preview.png", "PNG");
			buildManager.addPreviewFileName(tempPath + "txs_preview.png");
			text = QString("<img src=\"" + tempPath + "txs_preview.png\" width=%1 />").arg(w);
#endif
			if (completerPreview) {
				completerPreview = false;
				completer->showTooltip(text);
			} else {
				QToolTip::showText(p, text, 0);
			}
		}
		LatexEditorView::hideTooltipWhenLeavingLine = currentEditorView()->editor->cursor().lineNumber();
	}
	if (configManager.previewMode == ConfigManager::PM_INLINE && source.fromLine >= 0) {
		QDocument *doc = currentEditor()->document();
		doc->setForceLineWrapCalculation(true);
		int toLine = qBound(0, source.toLine, doc->lines() - 1);
		for (int l = source.fromLine; l <= toLine; l++ )
			if (doc->line(l).getCookie(QDocumentLine::PICTURE_COOKIE).isValid()) {
				doc->line(l).removeCookie(QDocumentLine::PICTURE_COOKIE);
				doc->line(l).removeCookie(QDocumentLine::PICTURE_COOKIE_DRAWING_POS);
				doc->line(l).setFlag(QDocumentLine::LayoutDirty);
				if (l != toLine) //must not adjust line toLine here, or will recalculate the document height without preview and scroll away if the preview is very height
					doc->adjustWidth(l);
			}
		doc->line(toLine).setCookie(QDocumentLine::PICTURE_COOKIE, QVariant::fromValue<QPixmap>(pixmap));
		doc->line(toLine).setFlag(QDocumentLine::LayoutDirty);
		doc->adjustWidth(toLine);
	}
}

void Texstudio::clearPreview()
{
	QEditor *edit = currentEditor();
	if (!edit) return;

	int startLine = 0;
	int endLine = 0;

	QAction *act = qobject_cast<QAction *>(sender());
	if (act && act->data().isValid()) {
		// inline preview context menu supplies the calling point in doc coordinates as data
		startLine = edit->document()->indexOf(edit->lineAtPosition(act->data().toPoint()));
		// slight performance penalty for use of lineNumber(), which is not stictly necessary because
		// we convert it back to a QDocumentLine, but easier to handle together with the other cases
		endLine = startLine;
		act->setData(QVariant());
	} else if (edit->cursor().hasSelection()) {
		startLine = edit->cursor().selectionStart().lineNumber();
		endLine = edit->cursor().selectionEnd().lineNumber();
	} else {
		startLine = edit->cursor().lineNumber();
		endLine = startLine;
	}

	for (int i = startLine; i <= endLine; i++) {
		edit->document()->line(i).removeCookie(QDocumentLine::PICTURE_COOKIE);
		edit->document()->line(i).removeCookie(QDocumentLine::PICTURE_COOKIE_DRAWING_POS);
		edit->document()->adjustWidth(i);
		for (int j = currentEditorView()->autoPreviewCursor.size() - 1; j >= 0; j--)
			if (currentEditorView()->autoPreviewCursor[j].selectionStart().lineNumber() <= i &&
			        currentEditorView()->autoPreviewCursor[j].selectionEnd().lineNumber() >= i) {
				// remove mark
				int sid = edit->document()->getFormatId("previewSelection");
				if (!sid) return;
				updateEmphasizedRegion(currentEditorView()->autoPreviewCursor[j], -sid);
				currentEditorView()->autoPreviewCursor.removeAt(j);
			}

	}
}

void Texstudio::showImgPreview(const QString &fname)
{
	completerPreview = (sender() == completer); // completer needs signal as answer
	QString imageName = fname;
	QFileInfo fi(fname);
	QString suffix;
	QStringList suffixList;
	suffixList << "jpg" << "jpeg" << "png" << "pdf";
	if (fi.exists()) {
		if (!suffixList.contains(fi.suffix()))
			return;
		suffix = fi.suffix();
	}

	if (suffix.isEmpty()) {
		foreach (QString elem, suffixList) {
			imageName = fname + elem;
			fi.setFile(imageName);
			if (fi.exists()) {
				suffix = elem;
				break;
			}
		}
	}

	suffixList.clear();
	suffixList << "jpg" << "jpeg" << "png";
	if (suffixList.contains(suffix)) {
		QPoint p;
		//if(previewEquation)
		p = currentEditorView()->getHoverPosistion();
		//else
		//    p=currentEditorView()->editor->mapToGlobal(currentEditorView()->editor->mapFromContents(currentEditorView()->editor->cursor().documentPosition()));
		QRect screen = QApplication::desktop()->screenGeometry();
		QPixmap img(imageName);
		int w = qMin(img.width(), configManager.editorConfig->maxImageTooltipWidth);
		w = qMin(w, screen.width() - 8);
		QString text = QString("<img src=\"" + imageName + "\" width=%1 />").arg(w);
		if (completerPreview) {
			completerPreview = false;
			emit imgPreview(text);
		} else {
			QToolTip::showText(p, text, 0);
			LatexEditorView::hideTooltipWhenLeavingLine = currentEditorView()->editor->cursor().lineNumber();
		}
	}
#ifndef NO_POPPLER_PREVIEW
	if (suffix == "pdf") {
		//render pdf preview
		PDFRenderManager *renderManager = new PDFRenderManager(this, 1);
		PDFRenderManager::Error error = PDFRenderManager::NoError;
		QSharedPointer<Poppler::Document> document = renderManager->loadDocument(imageName, error, "");
		if (error == PDFRenderManager::NoError) {
			renderManager->renderToImage(0, this, "showImgPreviewFinished", 20, 20, -1, -1, -1, -1, false, true);
		} else {
			delete renderManager;
		}
	}
#endif
}

void Texstudio::showImgPreviewFinished(const QPixmap &pm, int page)
{
	if (!currentEditorView()) return;
	Q_UNUSED(page);
	QPoint p;
	//if(previewEquation)
	p = currentEditorView()->getHoverPosistion();
	//else
	//    p=currentEditorView()->editor->mapToGlobal(currentEditorView()->editor->mapFromContents(currentEditorView()->editor->cursor().documentPosition()));
	QRect screen = QApplication::desktop()->screenGeometry();
	int w = pm.width();
	if (w > screen.width()) w = screen.width() - 2;
	QString text;
#if QT_VERSION >= 0x040700
	text = getImageAsText(pm, w);
#else
	QString tempPath = QDir::tempPath() + QDir::separator() + "." + QDir::separator();
	pm.save(tempPath + "txs_preview.png", "PNG");
	buildManager.addPreviewFileName(tempPath + "txs_preview.png");
	text = QString("<img src=\"" + tempPath + "txs_preview.png\" width=%1 />").arg(w);
#endif
	if (completerPreview) {
		emit imgPreview(text);
	} else {
		QToolTip::showText(p, text, 0);
		LatexEditorView::hideTooltipWhenLeavingLine = currentEditorView()->editor->cursor().lineNumber();
	}
#ifndef NO_POPPLER_PREVIEW
	PDFRenderManager *renderManager = qobject_cast<PDFRenderManager *>(sender());
	delete renderManager;
#endif
}

void Texstudio::showPreview(const QString &text)
{
	completerPreview = (sender() == completer); // completer needs signal as answer
	LatexEditorView *edView = getEditorViewFromFileName(documents.getCompileFileName()); //todo: temporary compi
	if (!edView)
		edView = currentEditorView();
	if (!edView) return;
	int m_endingLine = edView->editor->document()->findLineContaining("\\begin{document}", 0, Qt::CaseSensitive);
	if (m_endingLine < 0) return; // can't create header
	QStringList header;
	for (int l = 0; l < m_endingLine; l++)
		header << edView->editor->document()->line(l).text();
	if (buildManager.dvi2pngMode == BuildManager::DPM_EMBEDDED_PDF) {
		header << "\\usepackage[active,tightpage]{preview}"
		       << "\\usepackage{varwidth}"
		       << "\\AtBeginDocument{\\begin{preview}\\begin{varwidth}{\\linewidth}}"
		       << "\\AtEndDocument{\\end{varwidth}\\end{preview}}";
	}
	header << "\\pagestyle{empty}";// << "\\begin{document}";
	buildManager.preview(header.join("\n"), PreviewSource(text, -1, -1, true), documents.getCompileFileName(), edView->editor->document()->codec());
}

void Texstudio::showPreview(const QDocumentCursor &previewc)
{
	if (previewQueueOwner != currentEditorView())
		previewQueue.clear();
	previewQueueOwner = currentEditorView();
	previewQueue.insert(previewc.lineNumber());

	// mark region which is previewed, or update
	int sid = previewc.document()->getFormatId("previewSelection");
	if (sid)
		updateEmphasizedRegion(previewc, sid);

	QTimer::singleShot(qMax(40, configManager.autoPreviewDelay), this, SLOT(showPreviewQueue())); //slow down or it could create thousands of images
}

void Texstudio::showPreview(const QDocumentCursor &previewc, bool addToList)
{
	REQUIRE(currentEditor());
	REQUIRE(previewc.document() == currentEditor()->document());

	QString originalText = previewc.selectedText();
	if (originalText == "") return;
	// get document definitions
	//preliminary code ...
	const LatexDocument *rootDoc = documents.getRootDocumentForDoc();
	if (!rootDoc) return;
	QStringList header = makePreviewHeader(rootDoc);
	if (header.isEmpty()) return;
	PreviewSource ps(originalText, previewc.selectionStart().lineNumber(), previewc.selectionEnd().lineNumber(), false);
	buildManager.preview(header.join("\n"), ps,  documents.getCompileFileName(), rootDoc->codec());

	if (!addToList)
		return;

	if (configManager.autoPreview == ConfigManager::AP_PREVIOUSLY) {
		QList<QDocumentCursor> &clist = currentEditorView()->autoPreviewCursor;
		int sid = previewc.document()->getFormatId("previewSelection");
		for (int i = clist.size() - 1; i >= 0; i--)
			if (clist[i].anchorLineNumber() <= ps.toLine &&
			        clist[i].lineNumber()   >= ps.fromLine) {
				if (sid > 0)
					updateEmphasizedRegion(clist[i], -sid);
				clist.removeAt(i);
			}

		QDocumentCursor ss = previewc.selectionStart();
		QDocumentCursor se = previewc.selectionEnd();
		QDocumentCursor c(ss, se);
		c.setAutoUpdated(true);
		currentEditorView()->autoPreviewCursor.insert(0, c);
		// mark region
		if (sid)
			updateEmphasizedRegion(c, sid);
	}
}

QStringList Texstudio::makePreviewHeader(const LatexDocument *rootDoc)
{
	LatexEditorView *edView = rootDoc->getEditorView();
	if (!edView) QStringList();
	int m_endingLine = edView->editor->document()->findLineContaining("\\begin{document}", 0, Qt::CaseSensitive);
	if (m_endingLine < 0) return QStringList(); // can't create header
	QStringList header;
	for (int l = 0; l < m_endingLine; l++) {
		const QString &line = edView->editor->document()->line(l).text();
		int start = line.indexOf("\\input{");
		if (start < 0) {
			header << line;
		} else {
			// rewrite input to absolute paths
			QString newLine(line);
			start += 7;  // behind curly brace of \\input{
			int end = newLine.indexOf('}', start);
			if (end >= 0) {
				QString filename(newLine.mid(start, end - start));
				QString absPath = documents.getAbsoluteFilePath(filename);
#ifdef Q_OS_WIN
				absPath.replace('\\', '/');  // make sure the path argumment to \input uses '/' as dir separator
#endif
				newLine.replace(start, end - start, absPath);
			}
			header << newLine;
		}
	}
	if ((buildManager.dvi2pngMode == BuildManager::DPM_EMBEDDED_PDF) && configManager.previewMode != ConfigManager::PM_EMBEDDED) {
		header << "\\usepackage[active,tightpage]{preview}"
		       << "\\usepackage{varwidth}"
		       << "\\AtBeginDocument{\\begin{preview}\\begin{varwidth}{\\linewidth}}"
		       << "\\AtEndDocument{\\end{varwidth}\\end{preview}}";
	}
	header << "\\pagestyle{empty}";// << "\\begin{document}";
	return header;
}

/*!
 * Add a format overlay to the provided selection. Existing overlays of the format will be deleted
 * from all lines that are touched by the selection.
 *
 * \param c: a QDocumentCursor with a selection
 * \param sid: formatScheme id
 */
void Texstudio::updateEmphasizedRegion(QDocumentCursor c, int sid)
{
	QDocument *doc = c.document();
	QDocumentCursor ss = c.selectionStart();
	QDocumentCursor se = c.selectionEnd();
	for (int i = ss.anchorLineNumber(); i <= se.anchorLineNumber(); i++) {
		int begin = i == ss.anchorLineNumber() ? ss.anchorColumnNumber() : 0;
		int end = i == se.anchorLineNumber() ? se.anchorColumnNumber() : doc->line(i).length();
		if (sid > 0) {
			doc->line(i).clearOverlays(sid);
			doc->line(i).addOverlay(QFormatRange(begin, end - begin, sid));
		} else {
			// remove overlay if sid <0 (removes -sid)
			doc->line(i).clearOverlays(-sid);
		}
	}
}

void Texstudio::showPreviewQueue()
{
	if (previewQueueOwner != currentEditorView()) {
		previewQueue.clear();
		return;
	}
	if (configManager.autoPreview == ConfigManager::AP_NEVER) {
		// remove marks
		int sid = previewQueueOwner->document->getFormatId("previewSelection");
		if (sid > 0) {
			foreach (const QDocumentCursor &c, previewQueueOwner->autoPreviewCursor) {
				updateEmphasizedRegion(c, -sid);
			}
		}
		previewQueueOwner->autoPreviewCursor.clear();
		previewQueue.clear();
		return;
	}
	foreach (const int line, previewQueue)
		foreach (const QDocumentCursor &c, previewQueueOwner->autoPreviewCursor)
			if (c.lineNumber() == line)
				showPreview(c, false);
	previewQueue.clear();
}

void Texstudio::editInsertRefToNextLabel(const QString &refCmd, bool backward)
{
	if (!currentEditorView()) return;
	QDocumentCursor c = currentEditor()->cursor();
	int l = c.lineNumber();
	int m = currentEditorView()->editor->document()->findLineContaining("\\label", l, Qt::CaseSensitive, backward);
	if (!backward && m < l) return;
	if (m < 0) return;
	QDocumentLine dLine = currentEditor()->document()->line(m);
	QString mLine = dLine.text();
	int col = mLine.indexOf("\\label");
	if (col < 0) return;
	QString cmd;
	ArgumentList args;
	if (findCommandWithArgs(mLine, cmd, args, 0, col) >= 0) {
		if (cmd != "\\label" || args.count(ArgumentList::Mandatory) < 1) return;
		currentEditor()->write(refCmd + "{" + args.argContent(0, ArgumentList::Mandatory) + "}");
	}
}

void Texstudio::editInsertRefToPrevLabel(const QString &refCmd)
{
	editInsertRefToNextLabel(refCmd, true);
}

void Texstudio::symbolGridContextMenu(QWidget *widget, const QPoint &point)
{
	if (!widget->property("isSymbolGrid").toBool()) return;
	if (widget == MostUsedSymbolWidget) {
		QMenu menu;
		menu.addAction(tr("Add to favorites"), this, SLOT(symbolAddFavorite()));
		menu.addAction(tr("Remove"), this, SLOT(mostUsedSymbolsTriggered()));
		menu.addAction(tr("Remove all"), this, SLOT(mostUsedSymbolsTriggered()));
		menu.exec(point);
	} else if (widget == FavoriteSymbolWidget) {
		QMenu menu;
		menu.addAction(tr("Remove from favorites"), this, SLOT(symbolRemoveFavorite()));
		menu.addAction(tr("Remove all favorites"), this, SLOT(symbolRemoveAllFavorites()));
		menu.exec(point);
	} else {
		QMenu menu;
		menu.addAction(tr("Add to favorites"), this, SLOT(symbolAddFavorite()));
		menu.exec(point);
	}

}

void Texstudio::symbolAddFavorite()
{
	SymbolGridWidget *grid = qobject_cast<SymbolGridWidget *>(leftPanel->currentWidget());
	if (!grid) return;
	QString symbol = grid->getCurrentSymbol();
	if (symbol.isEmpty()) return;
	if (!symbolFavorites.contains(symbol)) symbolFavorites.append(symbol);
	FavoriteSymbolWidget->loadSymbols(symbolFavorites);
}

void Texstudio::symbolRemoveFavorite()
{
	QString symbol = FavoriteSymbolWidget->getCurrentSymbol();
	if (symbol.isEmpty()) return;
	symbolFavorites.removeAll(symbol);
	FavoriteSymbolWidget->loadSymbols(symbolFavorites);
}

void Texstudio::symbolRemoveAllFavorites()
{
	symbolFavorites.clear();
	FavoriteSymbolWidget->loadSymbols(symbolFavorites);
}

void Texstudio::mostUsedSymbolsTriggered(bool direct)
{
	QAction *action = 0;
	QTableWidgetItem *item = 0;
	if (!direct) {
		action = qobject_cast<QAction *>(sender());
		item = MostUsedSymbolWidget->currentItem();
	}
	if (direct || action->text() == tr("Remove")) {
		if (!direct) {
			if (item->data(Qt::UserRole + 3).isValid()) {
				QTableWidgetItem *elem = item->data(Qt::UserRole + 3).value<QTableWidgetItem *>();
				elem->setData(Qt::UserRole + 1, QVariant());
			}
			symbolMostused.removeAll(item);
			delete item;
		} else {
			symbolMostused.clear();
			foreach (QTableWidgetItem *elem, MostUsedSymbolWidget->findItems("*", Qt::MatchWildcard)) {
				if (!elem) continue;
				int cnt = elem->data(Qt::UserRole).toInt();
				if (symbolMostused.isEmpty()) {
					symbolMostused.append(elem);
				} else {
					int index = 0;
					while (index < symbolMostused.size() && symbolMostused[index]->data(Qt::UserRole).toInt() > cnt) {
						index++;
					}
					symbolMostused.insert(index, elem);
				}
			}
		}
	} else {
		for (int i = 0; symbolMostused.count(); i++) {
			QTableWidgetItem *item = symbolMostused.at(i);
			QTableWidgetItem *elem = item->data(Qt::UserRole + 3).value<QTableWidgetItem *>();
			elem->setData(Qt::UserRole + 1, QVariant());
			delete item;
			symbolMostused.clear();
		}
	}
	// Update Most Used Symbols Widget
	MostUsedSymbolWidget->SetUserPage(symbolMostused);
}

void Texstudio::editFindGlobal()
{
	if (!currentEditor()) return;
	QEditor *e = currentEditor();
	if (!findDlg)
		findDlg = new findGlobalDialog(this);
	if (e->cursor().hasSelection()) {
		findDlg->setSearchWord(e->cursor().selectedText());
	}
	if (findDlg->exec()) {
		QList<LatexDocument *> docs;
		LatexDocument *doc = currentEditorView()->document;
		switch (findDlg->getSearchScope()) {
		case 1:
			docs << doc;
			break;
		case 0:
			docs << documents.getDocuments();
			break;
		case 2:
			docs << doc->getListOfDocs();
			break;
		default:
			break;
		}
		//updateFindGlobal(docs,findDlg->getSearchWord(),findDlg->getReplaceWord(),findDlg->isCase(),findDlg->isWords(),findDlg->isRegExp());
	}
}

void Texstudio::runSearch(SearchQuery *query)
{
	if (!currentEditorView() || !query) return;
	QString searchText = currentEditorView()->getSearchText();
	query->setExpression(searchText);
	SearchResultWidget *srw = outputView->getSearchResultWidget();
	if (srw) {
		srw->updateSearchExpr(searchText);
	}
	query->run(currentEditorView()->document);
}

void Texstudio::findLabelUsages()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action) return;

	QString labelText = action->data().toString();
	LatexDocument *doc = action->property("doc").value<LatexDocument *>();

	findLabelUsages(doc, labelText);
}

void Texstudio::findLabelUsages(LatexDocument *contextDoc, const QString &labelText)
{
	if (!contextDoc) return;
	LabelSearchQuery *query = new LabelSearchQuery(labelText);
	searchResultWidget()->setQuery(query);
	query->run(contextDoc);
	outputView->showPage(outputView->SEARCH_RESULT_PAGE);
}

SearchResultWidget *Texstudio::searchResultWidget()
{
	return outputView->getSearchResultWidget();
}

// show current cursor position in structure view
void Texstudio::cursorPositionChanged()
{
	LatexEditorView *view = currentEditorView();
	if (!view) return;
	int i = view->editor->cursor().lineNumber();

	view->checkRTLLTRLanguageSwitching();

	// search line in structure
	if (currentLine == i) return;
	currentLine = i;

	LatexDocumentsModel *model = qobject_cast<LatexDocumentsModel *>(structureTreeView->model());
	if (!model) return; //shouldn't happen

	StructureEntry *newSection = currentEditorView()->document->findSectionForLine(currentLine);

	model->setHighlightedEntry(newSection);
	if (!mDontScrollToItem)
		structureTreeView->scrollTo(model->highlightedEntry());
	syncPDFViewer(currentEditor()->cursor(), false);
}

void Texstudio::syncPDFViewer(QDocumentCursor cur, bool inForeground)
{
#ifndef NO_POPPLER_PREVIEW
	if (PDFDocument::documentList().isEmpty() && inForeground) {
		// open new viewer, if none exists
		QAction *viewAct = getManagedAction("main/tools/view");
		if (viewAct) viewAct->trigger();
		return;
	}

	LatexDocument *doc = qobject_cast<LatexDocument *>(cur.document());
	if (!doc) doc = documents.currentDocument;
	if (doc) {
		QString filename = doc->getFileNameOrTemporaryFileName();
		if (!filename.isEmpty()) {
			int lineNumber = cur.isValid() ? cur.lineNumber() : currentLine;
			int originalLineNumber = doc->lineToLineSnapshotLineNumber(cur.line());
			if (originalLineNumber >= 0) lineNumber = originalLineNumber;
			PDFDocument::DisplayFlags displayPolicy = PDFDocument::NoDisplayFlags;
			if (inForeground) displayPolicy = PDFDocument::Raise | PDFDocument::Focus;
			foreach (PDFDocument *viewer, PDFDocument::documentList()) {
				if (inForeground || viewer->followCursor()) {
					viewer->syncFromSource(filename, lineNumber, displayPolicy);
				}
			}
		}
	}
#endif
}

void Texstudio::fileCheckin(QString filename)
{
	if (!currentEditorView()) return;
	QString fn = filename.isEmpty() ? currentEditor()->fileName() : filename;
	UniversalInputDialog dialog;
	QString text;
	dialog.addTextEdit(&text, tr("commit comment:"));
	bool wholeDirectory;
	dialog.addVariable(&wholeDirectory, tr("check in whole directory ?"));
	if (dialog.exec() == QDialog::Accepted) {
		fileSave(true);
		if (wholeDirectory) {
			fn = QFileInfo(fn).absolutePath();
		}
		//checkin(fn,text);
		if (svnadd(fn)) {
			checkin(fn, text, configManager.svnKeywordSubstitution);
		} else {
			//create simple repository
			svncreateRep(fn);
			svnadd(fn);
			checkin(fn, text, configManager.svnKeywordSubstitution);
		}
	}
}
/*!
 * \brief lock pdf file
 *
 * Determines pdf filename by using the current text file name and substitutes its extension to 'pdf'
 * \param filename
 */
void Texstudio::fileLockPdf(QString filename)
{
	if (!currentEditorView()) return;
	QString finame = filename;
	if (finame.isEmpty())
		finame = documents.getTemporaryCompileFileName();
	QFileInfo fi(finame);
	QString basename = fi.baseName();
	QString path = fi.path();
	fi.setFile(path + "/" + basename + ".pdf");
	if (!fi.isWritable()) {
		svnLock(fi.filePath());
	}
}
/*!
 * \brief check-in pdf file
 *
 * Determines pdf filename by using the current text file name and substitutes its extension to 'pdf'
 * If the file is not under version management, it tries to add the file.
 * \param filename
 */
void Texstudio::fileCheckinPdf(QString filename)
{
	if (!currentEditorView()) return;
	QString finame = filename;
	if (finame.isEmpty())
		finame = documents.getTemporaryCompileFileName();
	QFileInfo fi(finame);
	QString basename = fi.baseName();
	QString path = fi.path();
	QString fn = path + "/" + basename + ".pdf";
	SVNSTATUS status = svnStatus(fn);
	if (status == SVN_CheckedIn) return;
	if (status == SVN_Unmanaged)
		svnadd(fn);
	fileCheckin(fn);
}
/*!
 * \brief svn update file
 * \param filename
 */
void Texstudio::fileUpdate(QString filename)
{
	if (!currentEditorView()) return;
	QString fn = filename.isEmpty() ? currentEditor()->fileName() : filename;
	if (fn.isEmpty()) return;
	QString cmd = BuildManager::CMD_SVN + " up --non-interactive \"" + fn + "\"";
	statusLabelProcess->setText(QString(" svn update "));
	QString buffer;
	runCommand(cmd, &buffer);
	outputView->insertMessageLine(buffer);
}
/*!
 * \brief svn update work directory
 *
 * Uses the directory of the current file as cwd.
 * \param filename
 */
void Texstudio::fileUpdateCWD(QString filename)
{
	if (!currentEditorView()) return;
	QString fn = filename.isEmpty() ? currentEditor()->fileName() : filename;
	if (fn.isEmpty()) return;
	QString cmd = BuildManager::CMD_SVN;
	fn = QFileInfo(fn).path();
	cmd += " up --non-interactive  \"" + fn + "\"";
	statusLabelProcess->setText(QString(" svn update "));
	QString buffer;
	runCommand(cmd, &buffer);
	outputView->insertMessageLine(buffer);
}

void Texstudio::checkinAfterSave(QString filename, int checkIn)
{
	if (checkIn > 1) { // special treatment for save
		// 2: checkin
		// 1: don't check in
		checkin(filename);
		if (configManager.svnUndo) currentEditor()->document()->clearUndo();
	}
	if (checkIn == 0) { // from fileSaveAs
		if (configManager.autoCheckinAfterSaveLevel > 1) {
			if (svnadd(filename)) {
				checkin(filename, "txs auto checkin", configManager.svnKeywordSubstitution);
			} else {
				//create simple repository
				svncreateRep(filename);
				svnadd(filename);
				checkin(filename, "txs auto checkin", configManager.svnKeywordSubstitution);
			}
			// set SVN Properties if desired
			if (configManager.svnKeywordSubstitution) {
				QString cmd = BuildManager::CMD_SVN + " propset svn:keywords \"Date Author HeadURL Revision\" \"" + filename + "\"";
				statusLabelProcess->setText(QString(" svn propset svn:keywords "));
				runCommand(cmd, 0);
			}
		}
	}
}

void Texstudio::checkin(QString fn, QString text, bool blocking)
{
	Q_UNUSED(blocking)
	QString cmd = BuildManager::CMD_SVN;
	cmd += " ci -m \"" + text + "\" \"" + fn + ("\"");
	statusLabelProcess->setText(QString(" svn check in "));
	//TODO: blocking
	QString buffer;
	runCommand(cmd, &buffer);
	LatexEditorView *edView = getEditorViewFromFileName(fn);
	if (edView)
		edView->editor->setProperty("undoRevision", 0);
}

bool Texstudio::svnadd(QString fn, int stage)
{
	QString path = QFileInfo(fn).absolutePath();
	if (!QFile::exists(path + "/.svn")) {
		if (stage < configManager.svnSearchPathDepth) {
			if (stage > 0) {
				QDir dr(path);
				dr.cdUp();
				path = dr.absolutePath();
			}
			if (svnadd(path, stage + 1)) {
				checkin(path);
			} else
				return false;
		} else {
			return false;
		}
	}
	QString cmd = BuildManager::CMD_SVN;
	cmd += " add \"" + fn + ("\"");
	statusLabelProcess->setText(QString(" svn add "));
	QString buffer;
	runCommand(cmd, &buffer);
	return true;
}

void Texstudio::svnLock(QString fn)
{
	QString cmd = BuildManager::CMD_SVN;
	cmd += " lock \"" + fn + ("\"");
	statusLabelProcess->setText(QString(" svn lock "));
	QString buffer;
	runCommand(cmd, &buffer);
}

void Texstudio::svncreateRep(QString fn)
{
	QString cmd = BuildManager::CMD_SVN;
	QString admin = BuildManager::CMD_SVNADMIN;
	QString path = QFileInfo(fn).absolutePath();
	admin += " create \"" + path + "/repo\"";
	statusLabelProcess->setText(QString(" svn create repo "));
	QString buffer;
	runCommand(admin, &buffer);
	QString scmd = cmd + " mkdir \"file:///" + path + "/repo/trunk\" -m\"txs auto generate\"";
	runCommand(scmd, &buffer);
	scmd = cmd + " mkdir \"file:///" + path + "/repo/branches\" -m\"txs auto generate\"";
	runCommand(scmd, &buffer);
	scmd = cmd + " mkdir \"file:///" + path + "/repo/tags\" -m\"txs auto generate\"";
	runCommand(scmd, &buffer);
	statusLabelProcess->setText(QString(" svn checkout repo"));
	cmd += " co \"file:///" + path + "/repo/trunk\" \"" + path + "\"";
	runCommand(cmd, &buffer);
}

void Texstudio::svnUndo(bool redo)
{
	QString cmd_svn = BuildManager::CMD_SVN;
	QString fn = currentEditor()->fileName();
	// get revisions of current file
	QString cmd = cmd_svn + " log \"" + fn + "\"";
	QString buffer;
	runCommand(cmd, &buffer);
	QStringList revisions = buffer.split("\n", QString::SkipEmptyParts);
	buffer.clear();
	QMutableStringListIterator i(revisions);
	bool keep = false;
	QString elem;
	while (i.hasNext()) {
		elem = i.next();
		if (!keep) i.remove();
		keep = elem.contains(QRegExp("-{60,}"));
	}

	QVariant zw = currentEditor()->property("undoRevision");
	int undoRevision = zw.canConvert<int>() ? zw.toInt() : 0;

	int l = revisions.size();
	if (undoRevision >= l - 1) return;
	if (!redo) undoRevision++;

	if (redo) changeToRevision(revisions.at(undoRevision - 1), revisions.at(undoRevision));
	else changeToRevision(revisions.at(undoRevision), revisions.at(undoRevision - 1));

	currentEditor()->document()->clearUndo();
	if (redo) undoRevision--;
	currentEditor()->setProperty("undoRevision", undoRevision);
}

void Texstudio::svnPatch(QEditor *ed, QString diff)
{
	QStringList lines;
	//for(int i=0;i<diff.length();i++)
	//   qDebug()<<diff[i];
	if (diff.contains("\r\n")) {
		lines = diff.split("\r\n");
	} else {
		if (diff.contains("\n")) {
			lines = diff.split("\n");
		} else {
			lines = diff.split("\r");
		}
	}
	for (int i = 0; i < lines.length(); i++) {
		//workaround for svn bug
		// at times it shows text@@ pos insted of text \n @@ ...
		if (lines[i].contains("@@")) {
			int p = lines[i].indexOf("@@");
			lines[i] = lines[i].mid(p);
		}
	}
	for (int i = 0; i < 3; i++) lines.removeFirst();
	if (!lines.first().contains("@@")) {
		lines.removeFirst();
	}

	QRegExp rx("@@ -(\\d+),(\\d+)\\s*\\+(\\d+),(\\d+)");
	int cur_line;
	bool atDocEnd = false;
	QDocumentCursor c = ed->cursor();
	foreach (const QString &elem, lines) {
		QChar ch = ' ';
		if (!elem.isEmpty()) {
			ch = elem.at(0);
		}
		if (ch == '@') {
			if (rx.indexIn(elem) > -1) {
				cur_line = rx.cap(3).toInt();
				c.moveTo(cur_line - 1, 0);
			} else {
				qDebug() << "Bug";
			}
		} else {
			if (ch == '-') {
				atDocEnd = (c.lineNumber() == ed->document()->lineCount() - 1);
				if (c.line().text() != elem.mid(1))
					qDebug() << "del:" << c.line().text() << elem;
				c.eraseLine();
				if (atDocEnd) c.deletePreviousChar();
			} else {
				if (ch == '+') {
					atDocEnd = (c.lineNumber() == ed->document()->lineCount() - 1);
					if (atDocEnd) {
						c.movePosition(1, QDocumentCursor::EndOfLine, QDocumentCursor::MoveAnchor);
						c.insertLine();
					}
					c.insertText(elem.mid(1));
					// if line contains \r, no further line break needed
					if (!atDocEnd) {
						c.insertText("\n");
					}
				} else {
					atDocEnd = (c.lineNumber() == ed->document()->lineCount() - 1);
					int limit = 5;
					if (!atDocEnd) {
						while ((c.line().text() != elem.mid(1)) && (limit > 0) && (c.lineNumber() < ed->document()->lineCount() - 1)) {
							qDebug() << c.line().text() << c.lineNumber() << "<>" << elem.mid(1);
							c.movePosition(1, QDocumentCursor::NextLine, QDocumentCursor::MoveAnchor);
							c.movePosition(1, QDocumentCursor::StartOfLine, QDocumentCursor::MoveAnchor);
							limit--;
							if (limit == 0) {
								qDebug() << "failed";
							}
						}
						atDocEnd = (c.lineNumber() == ed->document()->lineCount() - 1);
						if (!atDocEnd) {
							c.movePosition(1, QDocumentCursor::NextLine, QDocumentCursor::MoveAnchor);
							c.movePosition(1, QDocumentCursor::StartOfLine, QDocumentCursor::MoveAnchor);
						}
					}
				}
			}
		}
	}
}

void Texstudio::showOldRevisions()
{
	// check if a dialog is already open
	if (svndlg) return;
	//needs to save first if modified
	if (!currentEditor())
		return;

	if (currentEditor()->fileName() == "" || !currentEditor()->fileInfo().exists())
		return;

	if (currentEditor()->isContentModified()) {
		removeDiffMarkers();// clean document from diff markers first
		currentEditor()->save();
		//currentEditorView()->editor->setModified(false);
		MarkCurrentFileAsRecent();
		checkin(currentEditor()->fileName(), "txs auto checkin", true);
	}
	updateCaption();

	QStringList log = svnLog();
	if (log.size() < 1) return;

	svndlg = new QDialog(this);
	QVBoxLayout *lay = new QVBoxLayout(svndlg);
	QLabel *label = new QLabel(tr("Attention: dialog is automatically closed if the text is manually edited!"), svndlg);
	lay->addWidget(label);
	cmbLog = new QComboBox(svndlg);
	cmbLog->insertItems(0, log);
	lay->addWidget(cmbLog);
	connect(svndlg, SIGNAL(finished(int)), this, SLOT(svnDialogClosed()));
	connect(cmbLog, SIGNAL(currentIndexChanged(QString)), this, SLOT(changeToRevision(QString)));
	connect(currentEditor(), SIGNAL(textEdited(QKeyEvent *)), svndlg, SLOT(close()));
	currentEditor()->setProperty("Revision", log.first());
	svndlg->setAttribute(Qt::WA_DeleteOnClose, true);
	svndlg->show();
}

void Texstudio::svnDialogClosed()
{
	if (cmbLog->currentIndex() == 0) currentEditor()->document()->setClean();
	svndlg = 0;
}

SVNSTATUS Texstudio::svnStatus(QString filename)
{
	QString cmd = BuildManager::CMD_SVN;
	cmd += " st \"" + filename + ("\"");
	statusLabelProcess->setText(QString(" svn status "));
	QString buffer;
	runCommand(cmd, &buffer);
	if (buffer.isEmpty()) return SVN_CheckedIn;
	if (buffer.startsWith("?")) return SVN_Unmanaged;
	if (buffer.startsWith("M")) return SVN_Modified;
	if (buffer.startsWith("C")) return SVN_InConflict;
	if (buffer.startsWith("L")) return SVN_Locked;
	return SVN_Unknown;
}

void Texstudio::changeToRevision(QString rev, QString old_rev)
{
	QString cmd_svn = BuildManager::CMD_SVN;
	QString fn = "\"" + currentEditor()->fileName() + "\"";
	// get diff
	QRegExp rx("^[r](\\d+) \\|");
	QString old_revision;
	if (old_rev.isEmpty()) {
		disconnect(currentEditor(), SIGNAL(contentModified(bool)), svndlg, SLOT(close()));
		QVariant zw = currentEditor()->property("Revision");
		Q_ASSERT(zw.isValid());
		old_revision = zw.toString();
	} else {
		old_revision = old_rev;
	}
	if (rx.indexIn(old_revision) > -1) {
		old_revision = rx.cap(1);
	} else return;
	QString new_revision = rev;
	if (rx.indexIn(new_revision) > -1) {
		new_revision = rx.cap(1);
	} else return;
	QString cmd = cmd_svn + " diff -r " + old_revision + ":" + new_revision + " " + fn;
	QString buffer;
	runCommand(cmd, &buffer, currentEditor()->getFileCodec());
	// patch
	svnPatch(currentEditor(), buffer);
	currentEditor()->setProperty("Revision", rev);
}

QStringList Texstudio::svnLog()
{
	QString cmd_svn = BuildManager::CMD_SVN;
	QString fn = "\"" + currentEditor()->fileName() + "\"";
	// get revisions of current file
	QString cmd = cmd_svn + " log " + fn;
	QString buffer;
	runCommand(cmd, &buffer);
	QStringList revisions = buffer.split("\n", QString::SkipEmptyParts);
	buffer.clear();
	QMutableStringListIterator i(revisions);
	bool keep = false;
	QString elem;
	while (i.hasNext()) {
		elem = i.next();
		if (!keep) i.remove();
		keep = elem.contains(QRegExp("-{60,}"));
	}
	return revisions;
}

bool Texstudio::generateMirror(bool setCur)
{
	if (!currentEditorView()) return false;
	QDocumentCursor cursor = currentEditorView()->editor->cursor();
	QDocumentCursor oldCursor = cursor;
	QString line = cursor.line().text();
	QString command, value;
	Token tk = getTokenAtCol(cursor.line().handle(), cursor.columnNumber());

	if (tk.type == Token::env || tk.type == Token::beginEnv) {
		if (tk.length > 0) {
			value = tk.getText();
			command = getCommandFromToken(tk);
			//int l=cursor.lineNumber();
			if (currentEditor()->currentPlaceHolder() != -1 &&
			        currentEditor()->getPlaceHolder(currentEditor()->currentPlaceHolder()).cursor.isWithinSelection(cursor))
				currentEditor()->removePlaceHolder(currentEditor()->currentPlaceHolder()); //remove currentplaceholder to prevent nesting
			//move cursor to env name
			int pos = tk.start;
			cursor.selectColumns(pos, pos + tk.length);

			LatexDocument *doc = currentEditorView()->document;

			PlaceHolder ph;
			ph.cursor = cursor;
			ph.autoRemove = true;
			ph.autoRemoveIfLeft = true;
			// remove curly brakets as well
			QString searchWord = "\\end{" + value + "}";
			QString inhibitor = "\\begin{" + value + "}";
			bool backward = (command == "\\end");
			int step = 1;
			if (backward) {
				qSwap(searchWord, inhibitor);
				step = -1;
			}
			int ln = cursor.lineNumber();
			int col = cursor.columnNumber();
			bool finished = false;
			int nested = 0;
			int colSearch = 0;
			int colInhibit = 0;
			if (step < 0)
				line = line.left(col);
			while (!finished) {
				if (step > 0) {
					//forward search
					colSearch = line.indexOf(searchWord, col);
					colInhibit = line.indexOf(inhibitor, col);
				} else {
					//backward search
					colSearch = line.lastIndexOf(searchWord);
					colInhibit = line.lastIndexOf(inhibitor);
				}
				if (colSearch < 0 && colInhibit < 0) {
					ln += step;
					if (doc->lines() <= ln || ln < 0)
						break;
					line = doc->line(ln).text();
					col = 0;
				}
				if (colSearch >= 0 && colInhibit >= 0) {
					if (colSearch * step < colInhibit * step) {
						if (nested == 0) {
							finished = true;
						}
						nested--;
						col = colSearch + 1;
						if (step < 0)
							line = line.left(colSearch);
					} else {
						if (step > 0) {
							col = colInhibit + 1;
						} else {
							line = line.left(colInhibit);
						}
						nested++;
					}
				} else {
					if (colSearch >= 0) {
						nested--;
						if (nested < 0)
							finished = true;
						if (step > 0) {
							col = colSearch + 1;
						} else {
							line = line.left(colSearch);
						}
					}
					if (colInhibit >= 0) {
						nested++;
						if (step > 0) {
							col = colInhibit + 1;
						} else {
							line = line.left(colInhibit);
						}
					}
				}
			}
			if (finished) {
				line = doc->line(ln).text();
				int start = colSearch;
				int offset = searchWord.indexOf("{");
				ph.mirrors << currentEditor()->document()->cursor(ln, start + offset + 1, ln, start + searchWord.length() - 1);
			}

			currentEditor()->addPlaceHolder(ph);
			currentEditor()->setPlaceHolder(currentEditor()->placeHolderCount() - 1);
			if (setCur)
				currentEditorView()->editor->setCursor(oldCursor);
			return true;
		}
	}
	return false;
}

void Texstudio::generateBracketInverterMirror()
{
	if (!currentEditor()) return;
	REQUIRE(currentEditor()->document() && currentEditor()->document()->languageDefinition());
	QDocumentCursor orig, to;
	currentEditor()->cursor().getMatchingPair(orig, to, false);
	if (!orig.isValid() && !to.isValid()) return;  // no matching pair found

	PlaceHolder ph;
	ph.cursor = orig.selectionStart();
	ph.mirrors << to.selectionStart();
	ph.length = orig.selectedText().length();
	ph.affector = BracketInvertAffector::instance();
	currentEditor()->addPlaceHolder(ph);
	currentEditor()->setPlaceHolder(currentEditor()->placeHolderCount() - 1);
}

void Texstudio::jumpToBracket()
{
	if (!currentEditor()) return;
	REQUIRE(sender() && currentEditor()->document() && currentEditor()->document()->languageDefinition());
	QDocumentCursor orig, to;
	const QDocumentCursor se = currentEditor()->cursor().selectionEnd();
	se.getMatchingPair(orig, to, false);
	if (!orig.isValid() && !to.isValid()) return;  // no matching pair found
	if (orig.selectionEnd() == se) currentEditor()->setCursor(to.selectionStart());
	else currentEditor()->setCursor(to.selectionEnd());
}

void Texstudio::selectBracket()
{
	if (!currentEditor()) return;
	REQUIRE(sender() && currentEditor()->document());
	if (!currentEditor()->document()->languageDefinition()) return;

	QDocumentCursor cursor = currentEditor()->cursor();
	QString type = sender()->property("type").toString();
	if (type == "inner") {
		cursor.select(QDocumentCursor::ParenthesesInner);
	} else if (type == "outer") {
		cursor.select(QDocumentCursor::ParenthesesOuter);
	} else if (type == "command") {
		Token tk = getTokenAtCol(cursor.line().handle(), cursor.columnNumber());
		if (tk.type == Token::command) {
			cursor.setColumnNumber(tk.start + tk.length);
			cursor.select(QDocumentCursor::ParenthesesOuter);
			cursor.setAnchorColumnNumber(tk.start);
		} else {
			cursor.select(QDocumentCursor::ParenthesesOuter);
			if (cursor.anchorColumnNumber() > 0) {
				tk = getTokenAtCol(cursor.line().handle(), cursor.anchorColumnNumber() - 1);
				if (tk.type == Token::command) {
					cursor.setAnchorColumnNumber(tk.start);
				}
			}
		}
	} else if (type == "line") {
		QDocumentCursor orig, to;
		cursor.getMatchingPair(orig, to, true);
		if (!orig.isValid() && !to.isValid()) return;  // no matching pair found

		if (to < orig) to.setColumnNumber(0);
		else to.setColumnNumber(to.line().length());

		QDocumentCursor::sort(orig, to);
		if (orig.hasSelection()) orig = orig.selectionStart();
		if (to.hasSelection()) to = to.selectionEnd();

		cursor.select(orig.lineNumber(), orig.columnNumber(), to.lineNumber(), to.columnNumber());
	} else {
		qWarning("Unhandled selectBracket() type");
	}
	currentEditor()->setCursor(cursor);
}

void Texstudio::findMissingBracket()
{
	if (!currentEditor()) return;
	REQUIRE(currentEditor()->document() && currentEditor()->document()->languageDefinition());
	QDocumentCursor c = currentEditor()->languageDefinition()->getNextMismatch(currentEditor()->cursor());
	if (c.isValid()) currentEditor()->setCursor(c);
}

void Texstudio::openExternalFile(QString name, const QString &defaultExt, LatexDocument *doc)
{
	if (!doc) {
		if (!currentEditor()) return;
		doc = qobject_cast<LatexDocument *>(currentEditor()->document());
	}
	if (!doc) return;
	name.remove('"');  // ignore quotes (http://sourceforge.net/p/texstudio/bugs/1366/)
	QStringList curPaths;
	if (documents.masterDocument)
		curPaths << ensureTrailingDirSeparator(documents.masterDocument->getFileInfo().absolutePath());
	if (doc->getMasterDocument())
		curPaths << ensureTrailingDirSeparator(doc->getRootDocument()->getFileInfo().absolutePath());
	curPaths << ensureTrailingDirSeparator(doc->getFileInfo().absolutePath());
	if (defaultExt == "bib") {
		curPaths << configManager.additionalBibPaths.split(getPathListSeparator());
	}
	bool loaded = false;
	for (int i = 0; i < curPaths.count(); i++) {
		const QString &curPath = ensureTrailingDirSeparator(curPaths.value(i));
		if ((loaded = load(getAbsoluteFilePath(curPath + name, defaultExt))))
			break;
		if ((loaded = load(getAbsoluteFilePath(curPath + name, ""))))
			break;
		if ((loaded = load(getAbsoluteFilePath(name, defaultExt))))
			break;
	}

	if (!loaded) {
		Q_ASSERT(curPaths.count() > 0);
		QFileInfo fi(getAbsoluteFilePath(curPaths[0] + name, defaultExt));
		if (fi.exists()) {
			txsCritical(tr("Unable to open file \"%1\".").arg(fi.fileName()));
		} else {
			if (txsConfirmWarning(tr("The file \"%1\" does not exist.\nDo you want to create it?").arg(fi.fileName()))) {
				int lineNr = -1;
				if (currentEditor()) {
					lineNr = currentEditor()->cursor().lineNumber();
				}
				if (!fi.absoluteDir().exists())
					fi.absoluteDir().mkpath(".");
				fileNew(fi.absoluteFilePath());
				qDebug() << doc->getFileName() << lineNr;
				doc->patchStructure(lineNr, 1);
			}
		}
	}
}

void Texstudio::cursorHovered()
{
	if (completer->isVisible()) return;
	generateMirror(true);
}

void Texstudio::saveProfile()
{
	QString currentDir = configManager.configBaseDir;
	QString fname = QFileDialog::getSaveFileName(this, tr("Save Profile"), currentDir, tr("TXS Profile", "filter") + "(*.txsprofile);;" + tr("All files") + " (*)");
	QFileInfo info(fname);
	if (info.suffix().isEmpty())
		fname += ".txsprofile";
	saveSettings(fname);
}

void Texstudio::loadProfile()
{
	QString currentDir = configManager.configBaseDir;
	QString fname = QFileDialog::getOpenFileName(this, tr("Load Profile"), currentDir, tr("TXS Profile", "filter") + "(*.txsprofile *.tmxprofile);;" + tr("All files") + " (*)"); //*.tmxprofile for compatibility - may be removed later
	if (fname.isNull())
		return;
	if (QFileInfo(fname).isReadable()) {
		bool macro = false;
		bool userCommand = false;
		saveSettings();
		QSettings *profile = new QSettings(fname, QSettings::IniFormat);
		QSettings *config = configManager.newQSettings();
		if (profile && config) {
			QStringList keys = profile->allKeys();
			foreach (const QString &key, keys) {
				//special treatment for macros/usercommands (list maybe shorter than before)
				if (key.startsWith("texmaker/Macros")) {
					if (!macro) { //remove old values
						config->beginGroup("texmaker");
						config->remove("Macros");
						config->endGroup();
						configManager.completerConfig->userMacros.clear();
					}
					QStringList ls = profile->value(key).toStringList();
					if (!ls.isEmpty()) {
						configManager.completerConfig->userMacros.append(Macro(ls));
						macro = true;
					}
					continue;
				}
				if (key == "texmaker/Tools/User Order") {
					// logic assumes that the user command name is exclusive
					QStringList order = config->value(key).toStringList() << profile->value(key).toStringList();
					config->setValue(key, order);
					userCommand = true;
					QString nameKey("texmaker/Tools/Display Names");
					QStringList displayNames = config->value(nameKey).toStringList() << profile->value(nameKey).toStringList();
					config->setValue(nameKey, displayNames);
					continue;
				}
				if (key == "texmaker/Tools/Display Names") {
					continue;  // handled above
				}
				config->setValue(key, profile->value(key));
			}
		}
		delete profile;
		delete config;
		readSettings(true);
		if (macro)
			updateUserMacros();
		if (userCommand)
			updateUserToolMenu();
	} else txsWarning(tr("Failed to read profile file %1.").arg(fname));
}

void Texstudio::addRowCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	int cols = LatexTables::getNumberOfColumns(cur);
	if (cols < 1) return;
	LatexTables::addRow(cur, cols);
}

void Texstudio::addColumnCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	int col = LatexTables::getColumn(cur) + 1;
	if (col < 1) return;
	if (col == 1 && cur.atLineStart()) col = 0;
	LatexTables::addColumn(currentEditorView()->document, currentEditorView()->editor->cursor().lineNumber(), col);
}

void Texstudio::removeColumnCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	// check if cursor has selection
	int numberOfColumns = 1;
	int col = LatexTables::getColumn(cur);
	if (cur.hasSelection()) {
		// if selection span within one row, romove all touched columns
		QDocumentCursor c2(cur.document(), cur.anchorLineNumber(), cur.anchorColumnNumber());
		if (!LatexTables::inTableEnv(c2)) return;
		QString res = cur.selectedText();
		if (res.contains("\\\\")) return;
		int col2 = LatexTables::getColumn(c2);
		numberOfColumns = abs(col - col2) + 1;
		if (col2 < col) col = col2;
	}
	int ln = cur.lineNumber();
	for (int i = 0; i < numberOfColumns; i++) {
		LatexTables::removeColumn(currentEditorView()->document, ln, col, 0);
	}
}

void Texstudio::removeRowCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	LatexTables::removeRow(cur);
}

void Texstudio::cutColumnCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	// check if cursor has selection
	int numberOfColumns = 1;
	int col = LatexTables::getColumn(cur);
	if (cur.hasSelection()) {
		// if selection span within one row, romove all touched columns
		QDocumentCursor c2(cur.document(), cur.anchorLineNumber(), cur.anchorColumnNumber());
		if (!LatexTables::inTableEnv(c2)) return;
		QString res = cur.selectedText();
		if (res.contains("\\\\")) return;
		int col2 = LatexTables::getColumn(c2);
		numberOfColumns = abs(col - col2) + 1;
		if (col2 < col) col = col2;
	}
	int ln = cur.lineNumber();
	m_columnCutBuffer.clear();
	QStringList lst;
	for (int i = 0; i < numberOfColumns; i++) {
		lst.clear();
		LatexTables::removeColumn(currentEditorView()->document, ln, col, &lst);
		if (m_columnCutBuffer.isEmpty()) {
			m_columnCutBuffer = lst;
		} else {
			for (int i = 0; i < m_columnCutBuffer.size(); i++) {
				QString add = "&";
				if (!lst.isEmpty()) add += lst.takeFirst();
				m_columnCutBuffer[i] += add;
			}
		}
	}

}

void Texstudio::pasteColumnCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	int col = LatexTables::getColumn(cur) + 1;
	if (col == 1 && cur.atLineStart()) col = 0;
	LatexTables::addColumn(currentEditorView()->document, currentEditorView()->editor->cursor().lineNumber(), col, &m_columnCutBuffer);
}

void Texstudio::addHLineCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	LatexTables::addHLine(cur);
}

void Texstudio::remHLineCB()
{
	if (!currentEditorView()) return;
	QDocumentCursor cur = currentEditorView()->editor->cursor();
	if (!LatexTables::inTableEnv(cur)) return;
	LatexTables::addHLine(cur, -1, true);
}

void Texstudio::findWordRepetions()
{
	if (!currentEditorView()) return;
	if (configManager.editorConfig && !configManager.editorConfig->inlineSpellChecking) {
		QMessageBox::information(this, tr("Problem"), tr("Finding word repetitions only works with activated online spell checking !"), QMessageBox::Ok);
		return;
	}
	QDialog *dlg = new QDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);
	dlg->setWindowTitle(tr("Find Word Repetitions"));
	QGridLayout *layout = new QGridLayout;
	layout->setColumnStretch(1, 1);
	layout->setColumnStretch(0, 1);
	QComboBox *cb = new QComboBox(dlg);
	cb->addItems(QStringList() << "spellingMistake" << "wordRepetition" << "wordRepetitionLongRange" << "badWord" << "grammarMistake" << "grammarMistakeSpecial1" << "grammarMistakeSpecial2" << "grammarMistakeSpecial3" << "grammarMistakeSpecial4");
	cb->setCurrentIndex(1);
	cb->setObjectName("kind");
	cb->setEditable(true); //so people can search for other things as well
	QPushButton *btNext = new QPushButton(tr("&Find Next"), dlg);
	btNext->setObjectName("next");
	QPushButton *btPrev = new QPushButton(tr("&Find Previous"), dlg);
	btPrev->setObjectName("prev");
	QPushButton *btClose = new QPushButton(tr("&Close"), dlg);
	btClose->setObjectName("close");
	layout->addWidget(cb, 0, 0);
	layout->addWidget(btNext, 0, 1);
	layout->addWidget(btPrev, 0, 2);
	layout->addWidget(btClose, 0, 3);
	dlg->setLayout(layout);
	connect(btNext, SIGNAL(clicked()), this, SLOT(findNextWordRepetion()));
	connect(btPrev, SIGNAL(clicked()), this, SLOT(findNextWordRepetion()));
	connect(btClose, SIGNAL(clicked()), dlg, SLOT(close()));
	dlg->setModal(false);
	dlg->show();
	dlg->raise();

}

void Texstudio::findNextWordRepetion()
{
	QPushButton *mButton = qobject_cast<QPushButton *>(sender());
	bool backward = mButton->objectName() == "prev";
	if (!currentEditorView()) return;
	typedef QFormatRange (QDocumentLine::*OverlaySearch) (int, int, int) const;
	OverlaySearch overlaySearch = backward ? &QDocumentLine::getLastOverlay : &QDocumentLine::getFirstOverlay;
	QComboBox *kind = mButton->parent()->findChild<QComboBox *>("kind");
	int overlayType = currentEditorView()->document->getFormatId(kind ? kind->currentText() : "wordRepetition");
	QDocumentCursor cur = currentEditor()->cursor();
	if (cur.hasSelection()) {
		if (backward) cur = cur.selectionStart();
		else cur = cur.selectionEnd();
	}
	int lineNr = cur.lineNumber();
	QDocumentLine line = cur.line();
	int fx = backward ? 0 : (cur.endColumnNumber() + 1), tx = backward ? cur.startColumnNumber() - 1 : line.length();
	while (line.isValid()) {
		if (line.hasOverlay(overlayType)) {
			QFormatRange range = (line.*overlaySearch)(fx, tx, overlayType);
			if (range.length > 0) {
				currentEditor()->setCursor(currentEditor()->document()->cursor(lineNr, range.offset, lineNr, range.offset + range.length));
				return;
			}
		}
		if (backward)
			lineNr--;
		else
			lineNr++;
		line = currentEditor()->document()->line(lineNr);
		fx = 0;
		tx = line.length();
	}
	txsInformation(backward ? tr("Reached beginning of text.") : tr("Reached end of text."));
}

void Texstudio::latexModelViewMode()
{
	bool mode = documents.model->getSingleDocMode();
	documents.model->setSingleDocMode(!mode);
}

void Texstudio::moveDocumentToFront()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || entry->type != StructureEntry::SE_DOCUMENT_ROOT) return;
	editors->moveEditor(entry->document->getEditorView(), Editors::AbsoluteFront);
}

void Texstudio::moveDocumentToEnd()
{
	StructureEntry *entry = LatexDocumentsModel::indexToStructureEntry(structureTreeView->currentIndex());
	if (!entry || entry->type != StructureEntry::SE_DOCUMENT_ROOT) return;
	editors->moveEditor(entry->document->getEditorView(), Editors::AbsoluteEnd);
}

void Texstudio::importPackage(QString name)
{
	if (!latexStyleParser) {
		QString cmd_latex = buildManager.getCommandInfo(BuildManager::CMD_LATEX).commandLine;
		QString baseDir;
		if (!QFileInfo(cmd_latex).isRelative())
			baseDir = QFileInfo(cmd_latex).absolutePath() + "/";
		latexStyleParser = new LatexStyleParser(this, configManager.configBaseDir, baseDir + "kpsewhich");
		connect(latexStyleParser, SIGNAL(scanCompleted(QString)), this, SLOT(packageScanCompleted(QString)));
		connect(latexStyleParser, SIGNAL(finished()), this, SLOT(packageParserFinished()));
		latexStyleParser->setAlias(latexParser.packageAliases);
		latexStyleParser->start();
		QTimer::singleShot(30000, this, SLOT(stopPackageParser()));
	}
	QString dirName;
	if (name.contains("/")) {
		int i = name.indexOf("/");
		dirName = "/" + name.mid(i + 1);
		name = name.left(i);
	}
	name.chop(4);
	name.append(".sty");
	latexStyleParser->addFile(name + dirName);
	name.chop(4);
	name.append(".cls"); // try also cls
	latexStyleParser->addFile(name + dirName);
}

void Texstudio::packageScanCompleted(QString name)
{
	QStringList lst = name.split('#');
	QString baseName = name;
	if (lst.size() > 1) {
		baseName = lst.first();
		name = lst.last();
	}
	foreach (LatexDocument *doc, documents.documents) {
		if (doc->containsPackage(baseName)) {
			documents.cachedPackages.remove(name + ".cwl"); // TODO: check is this still correct if keys are complex?
			doc->updateCompletionFiles(false);
		}
	}
}

void Texstudio::stopPackageParser()
{
	if (latexStyleParser)
		latexStyleParser->stop();
}

void Texstudio::packageParserFinished()
{
	delete latexStyleParser;
	latexStyleParser = 0;
}

void Texstudio::readinAllPackageNames()
{
	if (!packageListReader) {
		// preliminarily use cached packages
		QFileInfo cacheFileInfo = QFileInfo(QDir(configManager.configBaseDir), "packageCache.dat");
		if (cacheFileInfo.exists()) {
			QSet<QString> cachedPackages = PackageScanner::readPackageList(cacheFileInfo.absoluteFilePath());
			packageListReadCompleted(cachedPackages);
		}
		if (configManager.scanInstalledLatexPackages) {
			// start reading actually installed packages
			QString cmd_latex = buildManager.getCommandInfo(BuildManager::CMD_LATEX).commandLine;
			QString baseDir;
			if (!QFileInfo(cmd_latex).isRelative())
				baseDir = QFileInfo(cmd_latex).absolutePath() + "/";
#ifdef Q_OS_WIN
			bool isMiktex = baseDir.contains("miktex", Qt::CaseInsensitive)
			                || (!baseDir.contains("texlive", Qt::CaseInsensitive) && execCommand(baseDir + "latex.exe --version").contains("miktex", Qt::CaseInsensitive));
			if (isMiktex)
				packageListReader = new MiktexPackageScanner(quotePath(baseDir + "mpm.exe"), configManager.configBaseDir, this);
			else
				packageListReader = new KpathSeaParser(quotePath(baseDir + "kpsewhich"), this); // TeXlive on windows uses kpsewhich
#else
			packageListReader = new KpathSeaParser(quotePath(baseDir + "kpsewhich"), this);
#endif
			connect(packageListReader, SIGNAL(scanCompleted(QSet<QString>)), this, SLOT(packageListReadCompleted(QSet<QString>)));
			packageListReader->start();
		}
	}
}

void Texstudio::packageListReadCompleted(QSet<QString> packages)
{
	latexPackageList = packages;
	if (qobject_cast<PackageScanner *>(sender())) {
		PackageScanner::savePackageList(packages, QFileInfo(QDir(configManager.configBaseDir), "packageCache.dat").absoluteFilePath());
		packageListReader->wait();
		delete packageListReader;
		packageListReader = 0;
	}
	foreach (LatexDocument *doc, documents.getDocuments()) {
		LatexEditorView *edView = doc->getEditorView();
		if (edView)
			edView->updatePackageFormats();
	}
}

QString Texstudio::clipboardText(const QClipboard::Mode &mode) const
{
	return QApplication::clipboard()->text(mode);
}

void Texstudio::setClipboardText(const QString &text, const QClipboard::Mode &mode)
{
	QApplication::clipboard()->setText(text, mode);
}

int Texstudio::getVersion() const
{
	return TXSVERSION_NUMERIC;
}

/*!
 * This function is mainly intended for use in scripting
 * \a shortcut: textual representation of the keysequence, e.g. simulateKeyPress("Shift+Up")
 */
void Texstudio::simulateKeyPress(const QString &shortcut)
{
	QKeySequence seq = QKeySequence::fromString(shortcut, QKeySequence::PortableText);
	if (seq.count() > 0) {
		int key = seq[0] & ~Qt::KeyboardModifierMask;
		Qt::KeyboardModifiers modifiers = static_cast<Qt::KeyboardModifiers>(seq[0]) & Qt::KeyboardModifierMask;
		// TODO: we could additionally provide the text for the KeyEvent (necessary for actually typing characters
		QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, key, modifiers);
		QApplication::postEvent(QApplication::focusWidget(), event);
		event = new QKeyEvent(QEvent::KeyRelease, key, modifiers);
		QApplication::postEvent(QApplication::focusWidget(), event);
	}
}

void Texstudio::updateTexQNFA()
{
	updateTexLikeQNFA("(La)TeX", "tex.qnfa");
	updateTexLikeQNFA("Sweave", "sweave.qnfa");
	updateTexLikeQNFA("Pweave", "pweave.qnfa");
	updateUserMacros(false); //update macro triggers for languages
}

/*!
 * \brief Extends the given Language with dynamic information
 * \param languageName - the language name as specified in the language attribute of the <QNFA> tag.
 * \param filename - the filename for the language. This is just the filename without a path.
 * the file is searched for in the user language directory and as a fallback in the builtin language files.
 */
void Texstudio::updateTexLikeQNFA(QString languageName, QString filename)
{
	QLanguageFactory::LangData m_lang = m_languages->languageData(languageName);

	QFile f(configManager.configBaseDir + "languages/" + filename);
	if (!f.exists()) {
		f.setFileName(findResourceFile("qxs/" + filename));
	}
	QDomDocument doc;
	doc.setContent(&f);

	for (QMap<QString, QVariant>::const_iterator i = configManager.customEnvironments.constBegin(); i != configManager.customEnvironments.constEnd(); ++i) {
		QString mode = configManager.enviromentModes.value(i.value().toInt(), "verbatim");
		addEnvironmentToDom(doc, i.key(), mode);
	}

	//detected math envs
	for (QMap<QString, QString>::const_iterator i = detectedEnvironmentsForHighlighting.constBegin(); i != detectedEnvironmentsForHighlighting.constEnd(); ++i) {
		QString envMode = i.value() == "verbatim" ? "verbatim" :  "numbers";
		QString env = i.key();
		if (env.contains('*')) {
			env.replace("*", "\\*");
		}
		addEnvironmentToDom(doc, env, envMode, envMode != "verbatim");
	}
	// structure commands
	addStructureCommandsToDom(doc, latexParser.possibleCommands);

	QLanguageDefinition *oldLangDef = 0, *newLangDef = 0;
	oldLangDef = m_lang.d;
	Q_ASSERT(oldLangDef);

	// update editors using the language
	QNFADefinition::load(doc, &m_lang, m_formats);
	m_languages->addLanguage(m_lang);

	newLangDef = m_lang.d;
	Q_ASSERT(oldLangDef != newLangDef);

	if (editors) {
		documents.enablePatch(false);
        foreach (LatexDocument *doc, documents.getDocuments()) {
            LatexEditorView *edView=doc->getEditorView();
            if(edView){
                QEditor *ed = edView->editor;
                if (ed->languageDefinition() == oldLangDef) {
                    ed->setLanguageDefinition(newLangDef);
                    ed->highlight();
                }
            }
		}
		documents.enablePatch(true);
	}
}

/// Updates the highlighting of environments specified via environmentAliases
void Texstudio::updateHighlighting()
{

	QStringList envList;
	envList << "math" << "verbatim";
	bool updateNecessary = false;
	QMultiHash<QString, QString>::const_iterator it = latexParser.environmentAliases.constBegin();
	while (it != latexParser.environmentAliases.constEnd()) {
		if (envList.contains(it.value())) {
			if (!detectedEnvironmentsForHighlighting.contains(it.key())) {
				detectedEnvironmentsForHighlighting.insert(it.key(), it.value());
				updateNecessary = true;
			}
		}
		++it;
	}
	foreach (LatexDocument *doc, documents.getDocuments()) {
		QMultiHash<QString, QString>::const_iterator it = doc->ltxCommands.environmentAliases.constBegin();
		while (it != doc->ltxCommands.environmentAliases.constEnd()) {
			if (envList.contains(it.value())) {
				if (!detectedEnvironmentsForHighlighting.contains(it.key())) {
					detectedEnvironmentsForHighlighting.insert(it.key(), it.value());
					updateNecessary = true;
				}
			}
			++it;
		}
	}
	if (!updateNecessary)
		return;

	updateTexQNFA();
}

void Texstudio::toggleGrammar(int type)
{
	QAction *a = qobject_cast<QAction *>(sender());
	REQUIRE(a);
	LatexEditorView::setGrammarOverlayDisabled(type, !a->isChecked());
	//a->setChecked(!a->isChecked());
	for (int i = 0; i < documents.documents.size(); i++)
		if (documents.documents[i]->getEditorView())
			documents.documents[i]->getEditorView()->updateGrammarOverlays();
}

void Texstudio::fileDiff()
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	//remove old markers
	removeDiffMarkers();

	QString currentDir = QDir::homePath();
	if (!configManager.lastDocument.isEmpty()) {
		QFileInfo fi(configManager.lastDocument);
		if (fi.exists() && fi.isReadable()) {
			currentDir = fi.absolutePath();
		}
	}
	QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Files"), currentDir, tr("LaTeX Files (*.tex);;All Files (*)"),  &selectedFileFilter);
	if (files.isEmpty())
		return;
	//
	LatexDocument *doc2 = diffLoadDocHidden(files.first());
	doc2->setObjectName("diffObejct");
	doc2->setParent(doc);
	diffDocs(doc, doc2);
	//delete doc2;

	// show changes (by calling LatexEditorView::documentContentChanged)
	LatexEditorView *edView = currentEditorView();
	edView->documentContentChanged(0, edView->document->lines());
}

void Texstudio::jumpNextDiff()
{
	QEditor *m_edit = currentEditor();
	if (!m_edit)
		return;
	QDocumentCursor c = m_edit->cursor();
	if (c.hasSelection()) {
		int l, col;
		c.endBoundary(l, col);
		c.moveTo(l, col);
	}
	LatexDocument *doc = documents.currentDocument;

	//search for next diff
	int lineNr = c.lineNumber();
	QVariant var = doc->line(lineNr).getCookie(QDocumentLine::DIFF_LIST_COOCKIE);
	if (var.isValid()) {
		DiffList lineData = var.value<DiffList>();
		for (int j = 0; j < lineData.size(); j++) {
			DiffOp op = lineData.at(j);
			if (op.start + op.length > c.columnNumber()) {
				c.moveTo(lineNr, op.start);
				c.moveTo(lineNr, op.start + op.length, QDocumentCursor::KeepAnchor);
				m_edit->setCursor(c);
				return;
			}
		}
	}
	lineNr++;


	for (; lineNr < doc->lineCount(); lineNr++) {
		var = doc->line(lineNr).getCookie(QDocumentLine::DIFF_LIST_COOCKIE);
		if (var.isValid()) {
			break;
		}
	}
	if (var.isValid()) {
		DiffList lineData = var.value<DiffList>();
		DiffOp op = lineData.first();
		c.moveTo(lineNr, op.start);
		c.moveTo(lineNr, op.start + op.length, QDocumentCursor::KeepAnchor);
		m_edit->setCursor(c);
	}
}

void Texstudio::jumpPrevDiff()
{
	QEditor *m_edit = currentEditor();
	if (!m_edit)
		return;
	QDocumentCursor c = m_edit->cursor();
	if (c.hasSelection()) {
		int l, col;
		c.beginBoundary(l, col);
		c.moveTo(l, col);
	}
	LatexDocument *doc = documents.currentDocument;

	//search for next diff
	int lineNr = c.lineNumber();
	QVariant var = doc->line(lineNr).getCookie(QDocumentLine::DIFF_LIST_COOCKIE);
	if (var.isValid()) {
		DiffList lineData = var.value<DiffList>();
		for (int j = lineData.size() - 1; j >= 0; j--) {
			DiffOp op = lineData.at(j);
			if (op.start < c.columnNumber()) {
				c.moveTo(lineNr, op.start);
				c.moveTo(lineNr, op.start + op.length, QDocumentCursor::KeepAnchor);
				m_edit->setCursor(c);
				return;
			}
		}
	}
	lineNr--;


	for (; lineNr >= 0; lineNr--) {
		var = doc->line(lineNr).getCookie(QDocumentLine::DIFF_LIST_COOCKIE);
		if (var.isValid()) {
			break;
		}
	}
	if (var.isValid()) {
		DiffList lineData = var.value<DiffList>();
		DiffOp op = lineData.last();
		c.moveTo(lineNr, op.start);
		c.moveTo(lineNr, op.start + op.length, QDocumentCursor::KeepAnchor);
		m_edit->setCursor(c);
	}
}

void Texstudio::removeDiffMarkers(bool theirs)
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	diffRemoveMarkers(doc, theirs);
	QList<QObject *>lst = doc->children();
	foreach (QObject *o, lst)
		delete o;

	LatexEditorView *edView = currentEditorView();
	edView->documentContentChanged(0, edView->document->lines());

}

void Texstudio::editChangeDiff(QPoint pt)
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	bool theirs = (pt.x() < 0);
	int ln = pt.x();
	if (ln < 0)
		ln = -ln - 1;
	int col = pt.y();

	diffChange(doc, ln, col, theirs);
	LatexEditorView *edView = currentEditorView();
	edView->documentContentChanged(0, edView->document->lines());
}

void Texstudio::fileDiffMerge()
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	diffMerge(doc);

	LatexEditorView *edView = currentEditorView();
	edView->documentContentChanged(0, edView->document->lines());
}

LatexDocument *Texstudio::diffLoadDocHidden(QString f)
{
	QString f_real = f;
#ifdef Q_OS_WIN32
	QRegExp regcheck("/([a-zA-Z]:[/\\\\].*)");
	if (regcheck.exactMatch(f)) f_real = regcheck.cap(1);
#endif

	if (!QFile::exists(f_real)) return 0;

	LatexDocument *doc = new LatexDocument(this);
	//LatexEditorView *edit = new LatexEditorView(0,configManager.editorConfig,doc);

	//edit->document=doc;
	//edit->document->setEditorView(edit);

	QFile file(f_real);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, tr("Error"), tr("You do not have read permission to this file."));
		return 0;
	}
	file.close();

	//if (edit->editor->fileInfo().suffix().toLower() != "tex")
	//	m_languages->setLanguage(edit->editor, f_real);

	//QTime time;
	//time.start();

	//edit->editor->load(f_real,QDocument::defaultCodec());
	doc->load(f_real, QDocument::defaultCodec());

	//qDebug() << "Load time: " << time.elapsed();
	//edit->editor->document()->setLineEndingDirect(edit->editor->document()->originalLineEnding());

	//edit->document->setEditorView(edit); //update file name (if document didn't exist)


	return doc;
}

void Texstudio::fileDiff3()
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	//remove old markers
	removeDiffMarkers();

	QString currentDir = QDir::homePath();
	if (!configManager.lastDocument.isEmpty()) {
		QFileInfo fi(configManager.lastDocument);
		if (fi.exists() && fi.isReadable()) {
			currentDir = fi.absolutePath();
		}
	}
	QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Compare File"), currentDir, tr("LaTeX Files (*.tex);;All Files (*)"),  &selectedFileFilter);
	if (files.isEmpty())
		return;
	QStringList basefiles = QFileDialog::getOpenFileNames(this, tr("Open Base File"), currentDir, tr("LaTeX Files (*.tex);;All Files (*)"),  &selectedFileFilter);
	if (basefiles.isEmpty())
		return;
	showDiff3(files.first(), basefiles.first());
}

void Texstudio::showDiff3(const QString file1, const QString file2)
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	LatexDocument *doc2 = diffLoadDocHidden(file1);
	doc2->setObjectName("diffObejct");
	doc2->setParent(doc);
	LatexDocument *docBase = diffLoadDocHidden(file2);
	docBase->setObjectName("baseObejct");
	docBase->setParent(doc);
	diffDocs(doc2, docBase, true);
	diffDocs(doc, doc2);

	// show changes (by calling LatexEditorView::documentContentChanged)
	LatexEditorView *edView = currentEditorView();
	edView->documentContentChanged(0, edView->document->lines());
}

void Texstudio::declareConflictResolved()
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return;

	QString fn = doc->getFileName();
	QString cmd = BuildManager::CMD_SVN;
	cmd += " resolve --accept working \"" + fn + ("\"");
	statusLabelProcess->setText(QString(" svn resolve conflict "));
	QString buffer;
	runCommand(cmd, &buffer);
	checkin(fn, "txs: commit after resolve");
}
/*!
 * \brief mark svn conflict of current file resolved
 */
void Texstudio::fileInConflict()
{
	QEditor *mEditor = qobject_cast<QEditor *>(sender());
	REQUIRE(mEditor);
	if (!QFileInfo(mEditor->fileName()).exists())  //create new qfileinfo to avoid caching
		return;
	int ret = QMessageBox::warning(this,
	                               tr("Conflict!"),
	                               tr(
	                                   "%1\nhas been modified by another application.\n"
	                                   "Press \"OK\" to show differences\n"
	                                   "Press \"Cancel\"to do nothing.\n"
	                               ).arg(mEditor->fileName()),
	                               QMessageBox::Ok
	                               |
	                               QMessageBox::Cancel
	                              );
	if (ret == QMessageBox::Ok) {
		//remove old markers
		removeDiffMarkers();

		if (!checkSVNConflicted(false)) {

			LatexDocument *doc2 = diffLoadDocHidden(mEditor->fileName());
			if (!doc2)
				return;
			LatexDocument *doc = qobject_cast<LatexDocument *>(mEditor->document());
			doc2->setObjectName("diffObejct");
			doc2->setParent(doc);
			diffDocs(doc, doc2);
			//delete doc2;

			// show changes (by calling LatexEditorView::documentContentChanged)
			LatexEditorView *edView = doc->getEditorView();
			if (edView)
				edView->documentContentChanged(0, edView->document->lines());
		}
	}
}

bool Texstudio::checkSVNConflicted(bool substituteContents)
{
	LatexDocument *doc = documents.currentDocument;
	if (!doc)
		return false;

	QString fn = doc->getFileName();
	QFileInfo qf(fn + ".mine");
	if (qf.exists()) {
		SVNSTATUS status = svnStatus(fn);
		if (status == SVN_InConflict) {
			int ret = QMessageBox::warning(this,
			                               tr("SVN Conflict!"),
			                               tr(
			                                   "%1is conflicted with repository.\n"
			                                   "Press \"OK\" to show differences instead of the generated source by subversion\n"
			                                   "Press \"Cancel\"to do nothing.\n"
			                               ).arg(doc->getFileName()),
			                               QMessageBox::Ok
			                               |
			                               QMessageBox::Cancel
			                              );
			if (ret == QMessageBox::Ok) {
				QString path = qf.absolutePath();
				QDir dir(path);
				dir.setSorting(QDir::Name);
				qf.setFile(fn);
				QString filter = qf.fileName() + ".r*";
				QFileInfoList list = dir.entryInfoList(QStringList(filter));
				QStringList lst;
				foreach (QFileInfo elem, list) {
					lst << elem.absoluteFilePath();
				}
				if (lst.count() != 2)
					return false;
				if (substituteContents) {
					QTextCodec *codec = doc->codec();
					doc->load(fn + ".mine", codec);
				}
				QString baseFile = lst.takeFirst();
				QString compFile = lst.takeFirst();
				showDiff3(compFile, baseFile);
				return true;
			}
		}
	}
	return false;
}


QThread *killAtCrashedThread = 0;
QThread *lastCrashedThread = 0;

void recover()
{
	Texstudio::recoverFromCrash();
}

void Texstudio::recoverFromCrash()
{
	bool wasLoop;
	QString backtraceFilename;
	QString name = getLastCrashInformation(wasLoop);
	if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
		QThread *t = QThread::currentThread();
		lastCrashedThread = t;
		if (qobject_cast<SafeThread *>(t)) qobject_cast<SafeThread *>(t)->crashed = true;
		QTimer::singleShot(0, txsInstance, SLOT(threadCrashed()));
		while (!programStopped) {
			ThreadBreaker::sleep(1);
			if (t &&  t == killAtCrashedThread) {
				name += QString(" forced kill in %1").arg((long int)t, sizeof(long int) * 2, 16, QChar('0'));
				name += QString(" (TXS-Version %1 %2 )").arg(TEXSTUDIO_HG_REVISION).arg(COMPILED_DEBUG_OR_RELEASE);
				backtraceFilename = print_backtrace(name);
				exit(1);
			}
		};
		ThreadBreaker::forceTerminate();
		return;
	}

	static int nestedCrashes = 0;

	nestedCrashes++;

	if (nestedCrashes > 5) {
		qFatal("Forced kill after recovering failed after: %s\n", qPrintable(name));
		exit(1);
	}

	fprintf(stderr, "crashed with signal %s\n", qPrintable(name));

	if (nestedCrashes <= 2) {
		backtraceFilename = print_backtrace(name + QString(" (TXS-Version %1 %2 )").arg(TEXSTUDIO_HG_REVISION).arg(COMPILED_DEBUG_OR_RELEASE));
	}

	//hide editor views in case the error occured during drawing
	foreach (LatexEditorView *edView, txsInstance->editors->editors())
		edView->hide();

	//save recover information
	foreach (LatexEditorView *edView, txsInstance->editors->editors()) {
		QEditor *ed = edView ? edView->editor : 0;
		if (ed && ed->isContentModified() && !ed->fileName().isEmpty())
			ed->saveEmergencyBackup(ed->fileName() + ".recover.bak~");
	}



	QMessageBox *mb = new QMessageBox();  //Don't use the standard methods like ::critical, because they load an icon, which will cause a crash again with gtk. ; mb must be on the heap, or continuing a paused loop can crash
	mb->setWindowTitle(tr("TeXstudio Emergency"));
	QString backtraceMsg;
	if (QFileInfo(backtraceFilename).exists()) {
		qDebug() << backtraceFilename;
		backtraceMsg = tr("A backtrace was written to\n%1\nPlease provide this file if you send a bug report.\n\n").arg(QDir::toNativeSeparators(backtraceFilename));
	}
	if (!wasLoop) {
		mb->setText(tr( "TeXstudio has CRASHED due to a %1.\n\n%2Do you want to keep TeXstudio running? This may cause data corruption.").arg(name).arg(backtraceMsg));
		mb->setDefaultButton(mb->addButton(tr("Yes, try to recover"), QMessageBox::AcceptRole));
		mb->addButton(tr("No, kill the program"), QMessageBox::RejectRole); //can't use destructiverole, it always becomes rejectrole
	} else {
		mb->setText(tr( "TeXstudio has been paused due to a possible endless loop.\n\n%1Do you want to keep the program running? This may cause data corruption.").arg(backtraceMsg));
		mb->setDefaultButton(mb->addButton(tr("Yes, stop the loop and try to recover"), QMessageBox::AcceptRole));
		mb->addButton(tr("Yes, continue the loop"), QMessageBox::RejectRole);
		mb->addButton(tr("No, kill the program"), QMessageBox::DestructiveRole);
	}

	//show the dialog (non modal, because on Windows showing the dialog modal here, permanently disables all other windows)
	mb->setWindowFlags(Qt::WindowStaysOnTopHint);
	mb->setWindowModality(Qt::NonModal);
	mb->setModal(false);
	mb->show();
	QApplication::processEvents(QEventLoop::AllEvents);
	mb->setFocus(); //without it, raise doesn't work. If it is in the loop (outside time checking if), the buttons can't be clicked on (windows)
	QTime t;
	t.start();
	while (mb->isVisible()) {
		QApplication::processEvents(QEventLoop::AllEvents);
		if (t.elapsed() > 1000 ) {
			mb->raise(); //sometimes the box is not visible behind the main window (windows)
			t.restart();
		}
		ThreadBreaker::msleep(1);
	}

	//print edit history
	int i = 0;
	foreach (LatexEditorView *edView, txsInstance->editors->editors()) {
		Q_ASSERT(edView);
		if (!edView) continue;

		QFile tf(QDir::tempPath() + QString("/texstudio_undostack%1%2.txt").arg(i++).arg(edView->editor->fileInfo().completeBaseName()));
		if (tf.open(QFile::WriteOnly)) {
			tf.write(edView->editor->document()->debugUndoStack(100).toLocal8Bit());
			tf.close();
		};
	}


	//fprintf(stderr, "result: %i, accept: %i, yes: %i, reject: %i, dest: %i\n",mb->result(),QMessageBox::AcceptRole,QMessageBox::YesRole,QMessageBox::RejectRole,QMessageBox::DestructiveRole);
	if (mb->result() == QMessageBox::DestructiveRole || (!wasLoop && mb->result() == QMessageBox::RejectRole)) {
		qFatal("Killed on user request after error: %s\n", qPrintable(name));
		exit(1);
	}
	if (wasLoop && mb->result() == QMessageBox::RejectRole) {
		delete mb;
		Guardian::continueEndlessLoop();
		while (1) ;
	}

	//restore editor views
	foreach (LatexEditorView *edView, txsInstance->editors->editors())
		edView->show();

	if (!programStopped)
		QApplication::processEvents(QEventLoop::AllEvents);

	nestedCrashes = 0; //assume it was successfully recovered if it gets this far

	while (!programStopped) {
		QApplication::processEvents(QEventLoop::AllEvents);
		ThreadBreaker::msleep(1);
	}
	name = "Normal close after " + name;
	print_backtrace(name);
	exit(0);
}

void Texstudio::threadCrashed()
{
	bool wasLoop;
	QString signal = getLastCrashInformation(wasLoop);
	QThread *thread = lastCrashedThread;

	QString threadName = "<unknown>";
	QString threadId = QString("%1").arg((long)(thread), sizeof(long int) * 2, 16, QChar('0'));
	if (qobject_cast<QThread *>(static_cast<QObject *>(thread)))
		threadName = QString("%1 %2").arg(threadId).arg(qobject_cast<QThread *>(thread)->objectName());

	fprintf(stderr, "crashed with signal %s in thread %s\n", qPrintable(signal), qPrintable(threadName));

	int btn = QMessageBox::warning(this, tr("TeXstudio Emergency"),
	                               tr("TeXstudio has CRASHED due to a %1 in thread %2.\nThe thread has been stopped.\nDo you want to keep TeXstudio running? This may cause data corruption.").arg(signal).arg(threadId),
	                               tr("Yes"), tr("No, kill the program"));
	if (btn) {
		killAtCrashedThread = thread;
		ThreadBreaker::sleep(10);
		QMessageBox::warning(this, tr("TeXstudio Emergency"), tr("I tried to die, but nothing happened."));
	}
}

void Texstudio::iamalive()
{
	Guardian::calm();
}

void Texstudio::slowOperationStarted()
{
	Guardian::instance()->slowOperationStarted();
}

void Texstudio::slowOperationEnded()
{
	Guardian::instance()->slowOperationEnded();
}
/*!
 * \brief check current latex install as it is visible from txs
 */
void Texstudio::checkLatexInstall()
{

	QString result;
	// run pdflatex
	statusLabelProcess->setText(QString("check pdflatex"));
	QString buffer;
    // create result editor here in order to avoid empty editor
    fileNew(QFileInfo(QDir::temp(), tr("System Report") + ".txt").absoluteFilePath());
    m_languages->setLanguageFromName(currentEditor(), "Plain text");

	CommandInfo cmdInfo = buildManager.getCommandInfo(BuildManager::CMD_PDFLATEX);
	QString cmd = cmdInfo.getBaseName();
	int index = cmdInfo.commandLine.indexOf(cmd);
	if (index > -1)
		cmd = cmdInfo.commandLine.left(index) + cmd;
	// where is pdflatex located
#ifdef Q_OS_WIN
	runCommand("where " + cmd, &buffer);
	result = "where pdflatex: " + buffer + "\n\n";
#else
	runCommand("which " + cmd, &buffer);
	result = "which pdflatex: " + buffer + "\n\n";
#endif
	buffer.clear();
	cmd += " -version";
	// run pdflatex
	runCommand(cmd, &buffer);
	result += "PDFLATEX: " + cmd + "\n";
	result += buffer;
	result += "\n\n";
	// check env
	result += "Environment variables:\n";
	buffer.clear();
#ifdef Q_OS_WIN
	// TODO: it's not guaranteed that a process started with runCommand has the same environment
	// currently that should be the case, as long as we don't start to change the environment in
	// runCommand before execution.
	// The equivalent to printenv on windows would be set. However this is not a program, but a
	// command directly built into cmd.com, so we cannot directly use runCommand("set");
	buffer = QProcessEnvironment::systemEnvironment().toStringList().join("\n");
#else
	runCommand("printenv", &buffer);
#endif
	result += buffer + "\n";

	result += "\nTeXstudio:\n";
	result += "Path        : " + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\n";
	result += "Program call: " + QCoreApplication::arguments().join(" ") + "\n";
	result += "Setting file: " + QDir::toNativeSeparators(configManager.configFileName) + "\n";

	result += "\nCommand configuration in TeXstudio:\n";
	const CommandMapping &cmds = buildManager.getAllCommands();
	foreach (const CommandInfo &ci, cmds)
		result += QString("    %1 (%2)%3: %4\n").arg(ci.displayName).arg(ci.id).arg(ci.rerunCompiler ? " (r)" : "").arg(ci.commandLine);

	result += "\nAdditional Search Paths:\n";
	result += "    Command: " + buildManager.additionalSearchPaths + "\n";
	result += "    Log: " + buildManager.additionalLogPaths + "\n";
	result += "    Pdf: " + buildManager.additionalPdfPaths + "\n";

    //fileNew(QFileInfo(QDir::temp(), tr("System Report") + ".txt").absoluteFilePath());
    //m_languages->setLanguageFromName(currentEditor(), "Plain text");
	currentEditorView()->editor->setText(result, false);
}
/*!
 * \brief display which cwls are loaded.
 *
 * This function is for debugging.
 */
void Texstudio::checkCWLs()
{
	bool newFile = currentEditor();
	if (!newFile) fileNew();

	QList<LatexDocument *> docs = currentEditorView()->document->getListOfDocs();
	QStringList res;
	QSet<QString> cwls;
	// collect user commands and references
	foreach (LatexDocument *doc, docs) {
		const QSet<QString> &cwl = doc->getCWLFiles();
		cwls.unite(cwl);
		res << doc->getFileName() + ": " + QStringList(cwl.toList()).join(", ");
		QList<CodeSnippet> users = doc->userCommandList();
		if (!users.isEmpty()) {
			QString line = QString("\t%1 user commands: ").arg(users.size());
			foreach (const CodeSnippet & cs, users) line += (line.isEmpty() ? "" : "; ") + cs.word;
			res << line;
		}
	}
	cwls.unite(configManager.completerConfig->getLoadedFiles().toSet());
	res << "global: " << configManager.completerConfig->getLoadedFiles().join(", ");

	res << "" << "";

	foreach (QString s, cwls) {
		res << QString("------------------- Package %1: ---------------------").arg(s);
		LatexPackage package;
		if (!documents.cachedPackages.contains(s)) {
			res << "Package not cached (normal for global packages)";
			package = loadCwlFile(s);
		} else package = documents.cachedPackages.value(s);

		res << "\tpossible commands";
		foreach (const QString &key, package.possibleCommands.keys())
			res << QString("\t\t%1: %2").arg(key).arg(QStringList(package.possibleCommands.value(key).toList()).join(", "));
		res << "\tspecial def commands";
		foreach (const QString &key, package.specialDefCommands.keys())
			res << QString("\t\t%1: %2").arg(key).arg(package.specialDefCommands.value(key));
		res << "\tspecial treatment commands";
		foreach (const QString &key, package.specialDefCommands.keys()) {
			QString line = QString("\t\t%1: ").arg(key);
			foreach (const QPairQStringInt &pair, package.specialTreatmentCommands.value(key))
				line += QString("%1 (%2)").arg(pair.first).arg(pair.second) + ", ";
			line.chop(2);
			res << line;
		}
		res << QString("\toption Commands: %1").arg(QStringList(package.optionCommands.toList()).join(", "));
		QString line = QString("\tkinds: ");
		foreach (const QString &key, package.commandDescriptions.keys()) {
			const CommandDescription &cmd = package.commandDescriptions.value(key);
			line += key + "(" + cmd.toDebugString() + "), ";
		}
		line.chop(2);
		res << line;
		line = QString("\tall commands: ");
		foreach (const CodeSnippet & cs, package.completionWords) line += (line.isEmpty() ? "" : "; ") + cs.word;
		res << line;
		res << "" << "";
	}

	if (newFile) fileNew();
	m_languages->setLanguageFromName(currentEditor(), "Plain text");
	currentEditorView()->editor->setText(res.join("\n"), false);

}
/*!
 * \brief load document hidden
 *
 * when parsing a document, child-documents can be loaded automatically.
 * They are loaded here into the hidden state.
 * \param filename
 */
void Texstudio::addDocToLoad(QString filename)
{
	//qDebug()<<"fname:"<<filename;
	if (filename.isEmpty())
		return;
	load(filename, false, true, recheckLabels);
}

void Texstudio::moveCursorTodlh()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (!act) return;
	StructureEntry *entry = qvariant_cast<StructureEntry *>(act->data());
	LatexDocument *doc = entry->document;
	QDocumentLineHandle *dlh = entry->getLineHandle();
	LatexEditorView *edView = doc->getEditorView();
	if (edView) {
		int lineNr = -1;
		if ((lineNr = doc->indexOf(dlh)) >= 0) {
			gotoLine(lineNr, 0, edView);
		}
	}
}
/*!
 * \brief open pdf documentation of latex packages in the internal viewer
 * \param package package name
 * \param command latex command to search within that documentation
 */
void Texstudio::openInternalDocViewer(QString package, const QString command)
{
#ifndef NO_POPPLER_PREVIEW
	runInternalCommand("txs:///view-pdf-internal", QFileInfo(package), "--embedded");
	QList<PDFDocument *> pdfs = PDFDocument::documentList();
	if (pdfs.count() > 0) {
		pdfs[0]->raise();
		PDFDocument *pdf = pdfs.first();
		pdf->goToPage(0);
		pdf->doFindDialog(command);
		if (!command.isEmpty())
			pdf->search(command, false, false, false, false);
	}
#endif
}
/*!
 * \brief close a latex environment
 *
 * If the cursor is after a \begin{env} which is not closed by the end of the document, \end{env} is inserted.
 *
 * This function uses information which is generated by the syntax checker.
 * As the syntaxchecker works asynchronously, a small delay between typing and functioning of this action is mandatory though that delay is probably too small for any user to notice.
 */
void Texstudio::closeEnvironment()
{
	LatexEditorView *edView = currentEditorView();
	if (!edView)
		return;
	QEditor *m_edit = currentEditor();
	if (!m_edit)
		return;
	QDocumentCursor cursor = m_edit->cursor();

	// handle current line -- based on text parsing of current line
	// the below method is not exact and will fail on certain edge cases
	// for the time being this is good enough. An alternative approach may use the token system:
	//   QDocumentLineHandle *dlh = edView->document->line(cursor.lineNumber()).handle();
	//   TokenList tl = dlh->getCookie(QDocumentLine::LEXER_COOKIE).value<TokenList>();
	if (cursor.columnNumber() > 0) {
		QString text = cursor.line().text();
		QRegExp rxBegin = QRegExp("\\\\begin\\{([^}]+)\\}");
		int beginCol = text.lastIndexOf(rxBegin, cursor.columnNumber()-1);
		while (beginCol >= 0) {
			QDocumentCursor from, to;
			QDocumentCursor c = cursor.clone(false);
			c.setColumnNumber(beginCol);
			c.getMatchingPair(from, to);
			QString endText = "\\end{" + rxBegin.cap(1) + "}";
			if (!to.isValid() || to.selectedText() != endText) {
				cursor.insertText(endText);
				return;
			}
			beginCol -= 6;
			if (beginCol < 0) break;
			beginCol = text.lastIndexOf(rxBegin, beginCol);
		}
	}

	// handle previous lines -- based on StackEnvironment / syntax checker
	// This only works on a succeding line of the \begin{env} statement, not in the same line.
	// Therefore the above separate handling of the current line.
	StackEnvironment env;
	LatexDocument *doc = edView->document;
	doc->getEnv(cursor.lineNumber(), env);
	int lineCount = doc->lineCount();
	if (lineCount < 1)
		return;
	StackEnvironment env_end;
	QDocumentLineHandle *dlh = edView->document->line(lineCount - 1).handle();
	QVariant envVar = dlh->getCookie(QDocumentLine::STACK_ENVIRONMENT_COOKIE);
	if (envVar.isValid())
		env_end = envVar.value<StackEnvironment>();
	else
		return;

	if (env.count() > 0 && env_end.count() > 0) {
		Environment mostRecentEnv = env.pop();
		while (!env_end.isEmpty()) {
			Environment e = env_end.pop();
			if (env_end.isEmpty()) // last env is for internal use only
				break;
			if (e == mostRecentEnv) { // found, now close it
				QString txt;
				if (e.origName.isEmpty()) {
					txt = "\\end{" + e.name + "}";
				} else {
					txt = e.origName;
					int i = latexParser.mathStartCommands.indexOf(txt);
					txt = latexParser.mathStopCommands.value(i);
				}
				m_edit->insertText(txt);
				break;
			}
		}
	}
}




/*!
 * \brief make embedded viewer larger so that it covers the text edit
 * If the viewer is not embedded, no action is performed.
 */
void Texstudio::enlargeEmbeddedPDFViewer()
{
#ifndef NO_POPPLER_PREVIEW
	QList<PDFDocument *> oldPDFs = PDFDocument::documentList();
	if (oldPDFs.isEmpty())
		return;
	PDFDocument *viewer = oldPDFs.first();
	if (!viewer->embeddedMode)
		return;
	sidePanelSplitter->hide();
	configManager.viewerEnlarged = true;
	viewer->setStateEnlarged(true);
#endif
}
/*!
 * \brief set size of embedded viewer back to previous value
 * \param preserveConfig note change in config
 */
void Texstudio::shrinkEmbeddedPDFViewer(bool preserveConfig)
{
#ifndef NO_POPPLER_PREVIEW
	sidePanelSplitter->show();
	if (!preserveConfig)
		configManager.viewerEnlarged = false;
	QList<PDFDocument *> oldPDFs = PDFDocument::documentList();
	if (oldPDFs.isEmpty())
		return;
	PDFDocument *viewer = oldPDFs.first();
	if (!viewer->embeddedMode)
		return;
	viewer->setStateEnlarged(false);
#endif
}

void Texstudio::showStatusbar()
{
	QAction *act = qobject_cast<QAction *>(sender());
	if (act) {
		statusBar()->setVisible(act->isChecked());
		configManager.setOption("View/ShowStatusbar", act->isChecked());
	}
}
/*!
 * \brief open extended search in bottom panel
 *
 * This is called from the editor search panel by pressing '+'.
 */
void Texstudio::showExtendedSearch()
{
	LatexEditorView *edView = currentEditorView();
	if (!edView) return;

	bool isWord = edView->getSearchIsWords();
	bool isCase = edView->getSearchIsCase();
	bool isReg = edView->getSearchIsRegExp();
	SearchQuery *query = new SearchQuery(edView->getSearchText(), edView->getReplaceText(), isCase, isWord, isReg);
	query->setScope(searchResultWidget()->searchScope());
	searchResultWidget()->setQuery(query);
	outputView->showPage(outputView->SEARCH_RESULT_PAGE);
	runSearch(query);
}
/*!
 * \brief change icon size of toolbars
 *
 * This is intended to have larger symbols on high-resolution screens.
 * The change is instantly performed from the config dialog as visual feed-back.
 * \param value size in points
 */
void Texstudio::changeIconSize(int value)
{
	setIconSize(QSize(value, value));
#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *pdfviewer, PDFDocument::documentList()) {
		if (!pdfviewer->embeddedMode) pdfviewer->setToolbarIconSize(value);
	}
#endif
}
/*!
 * \brief change icon size for central tool-bar
 *
 * This is intended to have larger symbols on high-resolution screens.
 * The change is instantly performed from the config dialog as visual feed-back.
 * \param value size in points
 */
void Texstudio::changeSecondaryIconSize(int value)
{
	centralToolBar->setIconSize(QSize(value, value));
	leftPanel->setToolbarIconSize(value);

	foreach (QObject *c, statusBar()->children()) {
		QAbstractButton *bt = qobject_cast<QAbstractButton *>(c);
		if (bt) {
			bt->setIconSize(QSize(value, value));
		}
	}

#ifndef NO_POPPLER_PREVIEW
	foreach (PDFDocument *pdfviewer, PDFDocument::documentList()) {
		if (pdfviewer->embeddedMode) pdfviewer->setToolbarIconSize(value);
	}
#endif
}
/*!
 * \brief change symbol grid icon size
 *
 * This is intended to have larger symbols on high-resolution screens.
 * The change is instantly performed from the config dialog as visual feed-back.
 * \param value size in points
 * \param changePanel change to a symbolgrid in sidepanel in order to make the change directly visible
 */
void Texstudio::changeSymbolGridIconSize(int value, bool changePanel)
{
	if (changePanel && !qobject_cast<SymbolGridWidget *>(leftPanel->currentWidget())) {
		// no symbols visible - make them visible for the life-updates
		leftPanel->setCurrentWidget(leftPanel->widget("greek"));
	}
	QList<QWidget *> lstOfWidgets = leftPanel->getWidgets();
	foreach (QWidget *wdg, lstOfWidgets) {
		SymbolGridWidget *list = qobject_cast<SymbolGridWidget *>(wdg);
		if (list) {
			list->setSymbolSize(value);
		}
	}
}

/*! @} */
