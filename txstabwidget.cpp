#include "txstabwidget.h"
#include "latexeditorview.h"
#include "smallUsefulFunctions.h"

TxsTabWidget::TxsTabWidget(QWidget *parent) :
	QTabWidget(parent)
{
	setFocusPolicy(Qt::ClickFocus);
	setContextMenuPolicy(Qt::PreventContextMenu);

	ChangeAwareTabBar *tb = new ChangeAwareTabBar();
	tb->setContextMenuPolicy(Qt::CustomContextMenu);
	tb->setUsesScrollButtons(true);
	connect(tb, SIGNAL(customContextMenuRequested(QPoint)), this, SIGNAL(tabBarContextMenuRequested(QPoint)));
	connect(tb, SIGNAL(currentTabAboutToChange(int, int)), this, SLOT(currentTabAboutToChange(int, int)));
	connect(tb, SIGNAL(currentTabLeftClicked()), this, SIGNAL(activationRequest()));
	connect(tb, SIGNAL(middleMouseButtonPressed(int)), this, SLOT(closeTab(int)));
	setTabBar(tb);

	if (hasAtLeastQt(4, 5)) {
		setDocumentMode(true);
		const QTabBar *tb = tabBar();
		connect(tb, SIGNAL(tabMoved(int, int)), this, SIGNAL(tabMoved(int, int)));
		connect(this, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
		setProperty("tabsClosable", true);
		setProperty("movable", true);
	}
	connect(this, SIGNAL(currentChanged(int)), this, SIGNAL(currentEditorChanged()));
}

void TxsTabWidget::moveTab(int from, int to)
{
	int cur = currentIndex();
	QString text = tabText(from);
	QWidget *wdg = widget(from);
	removeTab(from);
	insertTab(to, wdg, text);
	if (cur == from) setCurrentIndex(to);
	else if (from < to && cur >= from && cur < to)
		setCurrentIndex(cur - 1);
	else if (to > from && cur >= from && cur < to)
		setCurrentIndex(cur + 1);
}

QList<LatexEditorView *> TxsTabWidget::editors() const
{
	QList<LatexEditorView *> list;
	for (int i = 0; i < count(); i++) {
		LatexEditorView *edView = qobject_cast<LatexEditorView *>(widget(i));
		Q_ASSERT(edView); // there should only be editors as tabs
		list.append(edView);
	}
	return list;
}

bool TxsTabWidget::containsEditor(LatexEditorView *edView) const
{
	if (!edView) return false;
	return (indexOf(edView) >= 0);
}

LatexEditorView *TxsTabWidget::currentEditorView() const
{
	return qobject_cast<LatexEditorView *>(currentWidget());
}

void TxsTabWidget::setCurrentEditor(LatexEditorView *edView)
{
	if (currentWidget() == edView)
		return;

	if (indexOf(edView) < 0) {
		// catch calls in which editor is not a member tab.
		// TODO: such calls are deprecated as bad practice. We should avoid them in the long run. For the moment the fallback to do nothing is ok.
		qDebug() << "Warning (deprecated call): TxsTabWidget::setCurrentEditor: editor not member of TxsTabWidget";
#ifndef QT_NO_DEBUG
		txsWarning("Warning (deprecated call): TxsTabWidget::setCurrentEditor: editor not member of TxsTabWidget");
#endif
		return;
	}
	setCurrentWidget(edView);
}

LatexEditorView *TxsTabWidget::editorAt(QPoint p) {
	int index = tabBar()->tabAt(p);
	if (index < 0) return 0;
	return qobject_cast<LatexEditorView *>(widget(index));
}

void TxsTabWidget::setActive(bool active) {
	// somehow mark the widget (or selected tab visually as active)
	// we currently use bold but due to a Qt bug we're required to increase
	// the tab width all tabs in the active widget.
	// see https://bugreports.qt.io/browse/QTBUG-6905
	if (active) {
		setStyleSheet("QTabBar {font-weight: bold;} QTabBar::tab:!selected {font-weight: normal;}");
	} else {
		setStyleSheet(QString());
	}
}

bool TxsTabWidget::isEmpty() const {
	return (count() == 0);
}

bool TxsTabWidget::currentEditorViewIsFirst() const {
	return (currentIndex() == 0);
}

bool TxsTabWidget::currentEditorViewIsLast() const {
	return (currentIndex() >= count()-1);
}

void TxsTabWidget::gotoNextDocument()
{
	if (count() <= 1) return;
	int cPage = currentIndex() + 1;
	if (cPage >= count()) setCurrentIndex(0);
	else setCurrentIndex(cPage);
}

void TxsTabWidget::gotoPrevDocument()
{
	if (count() <= 1) return;
	int cPage = currentIndex() - 1;
	if (cPage < 0) setCurrentIndex(count() - 1);
	else setCurrentIndex(cPage);
}

void TxsTabWidget::gotoFirstDocument() {
	if (count() <= 1) return;
	setCurrentIndex(0);
}

void TxsTabWidget::gotoLastDocument() {
	if (count() <= 1) return;
	setCurrentIndex(count()-1);
}

void TxsTabWidget::currentTabAboutToChange(int from, int to)
{
	LatexEditorView *edFrom = qobject_cast<LatexEditorView *>(widget(from));
	LatexEditorView *edTo = qobject_cast<LatexEditorView *>(widget(to));
	REQUIRE(edFrom);
	REQUIRE(edTo);
	emit editorAboutToChangeByTabClick(edFrom, edTo);
}

void TxsTabWidget::closeTab(int i)
{
	int cur = currentIndex();
	int total = count();
	setCurrentIndex(i);
	emit closeCurrentEditorRequest();
	if (cur > i) cur--; //removing moves to left
	if (total != count() && cur != i) //if user clicks cancel stay in clicked editor
		setCurrentIndex(cur);
}

void TxsTabWidget::closeTab(LatexEditorView *edView)
{
	int i = indexOf(edView);
	if (i >= 0) closeTab(i);
}

void TxsTabWidget::removeEditor(LatexEditorView *edView)
{
	int i = indexOf(edView);
	if (i >= 0)
		removeTab(i);
}

void TxsTabWidget::insertEditor(LatexEditorView *edView, int pos, bool asCurrent)
{
	Q_ASSERT(edView);
	insertTab(pos, edView, "?bug?");
	if (asCurrent) setCurrentEditor(edView);
}

void ChangeAwareTabBar::mousePressEvent(QMouseEvent *event)
{
	int current = currentIndex();
	int toIndex = tabAt(event->pos());
	if (event->button() == Qt::LeftButton) {
		if (toIndex >= 0) {
			emit currentTabAboutToChange(current, toIndex);
		}
	}
#if QT_VERSION>=0x040700
	else if (event->button() == Qt::MiddleButton) {
		int tabNr = tabAt(event->pos());
		if (tabNr >= 0) {
			emit middleMouseButtonPressed(tabNr);
		}
	}
#endif
	QTabBar::mousePressEvent(event);
	if (event->button() == Qt::LeftButton && current == toIndex) {
		emit currentTabLeftClicked();
	}
}
