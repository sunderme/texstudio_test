/****************************************************************************
**
** Copyright (C) 2006-2009 fullmetalcoder <fullmetalcoder@hotmail.fr>
**
** This file is part of the Edyuk project <http://edyuk.org>
** 
** This file may be used under the terms of the GNU General Public License
** version 3 as published by the Free Software Foundation and appearing in the
** file GPL.txt included in the packaging of this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef _QSEARCH_REPLACE_PANEL_H_
#define _QSEARCH_REPLACE_PANEL_H_

#include "mostQtHeaders.h"

#include "qpanel.h"

/*!
	\file qsearchreplacepanel.h
	\brief Definition of the QSearchReplacePanel class.
	
	\see QSearchReplacePanel
*/


class QDocumentCursor;
class QDocumentLine;
class QDocumentSearch;

class QCE_EXPORT QSearchReplacePanel : public QPanel //, private Ui::SearchReplace
{
	Q_OBJECT
	
	public:
		Q_PANEL(QSearchReplacePanel, "Search Replace Panel")
		
		QSearchReplacePanel(QWidget *p = 0);
		virtual ~QSearchReplacePanel();
		
		virtual QString type() const;
		bool isReplaceModeActive() const;
		QDocumentCursor getSearchScope() const;
		QDocumentSearch * search(){
			return m_search;
		}

		void setUseLineForSearch(bool b);
		bool getUseLineForSearch() const;
		void setSearchOnlyInSelection(bool b);
		bool getSearchOnlyInSelection() const;
		
		
	public slots:
		void display(int mode, bool replace);
		
		void findNext();
		void findReplace(bool backward, bool replace=false, bool replaceAll=false, bool countOnly = false);
		void find(QString text, bool backward, bool highlight, bool regex, bool word, bool caseSensitive);
		void find(QString text, bool backward, bool highlight, bool regex, bool word, bool caseSensitive, bool fromCursor, bool selection);
		void selectAllMatches();
		void setOptions(int searchOptions, bool cursor, bool selection);
		
	signals:
		void onClose();
	protected:
		virtual bool forward(QMouseEvent *e);
		virtual void editorChange(QEditor *e);
		
		virtual bool eventFilter(QObject *o, QEvent *e);
		
		virtual void hideEvent(QHideEvent *e);
		virtual void closeEvent(QCloseEvent *e);

		virtual void resizeEvent(QResizeEvent *e);

	private slots:
		void cFind_textEdited(const QString& text);
		void cReplace_textEdited(const QString& text);
		
		void on_cbReplace_toggled(bool on);
		
		void on_cbCase_toggled(bool on);
		void on_cbWords_toggled(bool on);
		void on_cbRegExp_toggled(bool on);
		void on_cbCursor_toggled(bool on);
		void on_cbHighlight_toggled(bool on);
		void on_cbSelection_toggled(bool on);
		void on_cbEscapeSeq_toggled(bool on);
		void on_cbPrompt_toggled(bool on);
		

		void on_bNext_clicked();
		void on_bPrevious_clicked();
		void on_bCount_clicked();

		void on_bReplaceNext_clicked();
		void on_bReplacePrevious_clicked();
		void on_bReplaceAll_clicked();
		
		void cursorPositionChanged();

	private:
		void init();
		void updateSearchOptions(bool replace, bool replaceAll);
		void on_cFind_returnPressed(bool backward);
		void on_cReplace_returnPressed(bool backward);
		QDocumentSearch *m_search;
		bool m_lastDirection;
		int minimum_width;

	protected:
		QStringList getHistory(bool findHistory = true);
		void setHistory(const QStringList& newHistory, bool findHistory = true);
		void rememberLastSearch(QStringList& history, const QString& str, bool incremental);
		//protected to give unit tests access
		QToolButton *bClose;
		QComboBox *cFind;
		QToolButton *bNext, *bPrevious, *bCount;
		QGridLayout *gridLayout1;
		QCheckBox *cbCase;
		QCheckBox *cbWords;
		QCheckBox *cbRegExp;
		QCheckBox *cbHighlight;
		QCheckBox *cbCursor;
		QCheckBox *cbSelection;
		QToolButton *bReplaceAll;
		QCheckBox *cbPrompt;
		QCheckBox *cbReplaceAll;
		QCheckBox *cbEscapeSeq;
		QCheckBox *cbReplace;
		QComboBox *cReplace;
		QToolButton *bReplaceNext;
		QToolButton *bReplacePrevious;

		
		bool useLineForSearch, searchOnlyInSelection;
};

#endif // _QSEARCH_REPLACE_PANEL_H_
