#ifndef SEARCHQUERY_H
#define SEARCHQUERY_H

#include "mostQtHeaders.h"
#include "searchresultmodel.h"
#include "qdocument.h"

class LatexDocument;


class SearchQuery : public QObject {
	Q_OBJECT
public:
	enum SearchFlag
	{
		NoFlags				= 0x0000,
		SearchParams		= 0x00FF,
		IsCaseSensitive		= 0x0001,
		IsWord				= 0x0002,
		IsRegExp			= 0x0004,
		
		SearchCapabilities	= 0xFF00,
		ScopeChangeAllowed	= 0x0100,
		ReplaceAllowed		= 0x0200,
		SearchAgainAllowed	= 0x0400,
	};
	Q_DECLARE_FLAGS(SearchFlags, SearchFlag)
	enum Scope {
		CurrentDocumentScope,
		ProjectScope,
		GlobalScope,
	};

	SearchQuery(QString expr, QString replaceText, SearchFlags f);
	SearchQuery(QString expr, QString replaceText, bool isCaseSensitive, bool isWord, bool isRegExp);

	bool flag(SearchFlag f) const;
	QString type() { return mType; }

	QString searchExpression() const { return mModel->searchExpression(); }
	SearchResultModel * model() const { return mModel; }
	int getNextSearchResultColumn(QString text, int col) const;

	void setScope(Scope sc) { mScope = sc; }
	Scope scope() { return mScope; }
	
signals:
	
public slots:
	virtual void run(LatexDocument *doc);
	void addDocSearchResult(QDocument *doc, QList<QDocumentLineHandle *> search);
	void setReplacementText(QString text);
	
protected:
	void setFlag(SearchFlag f, bool b=true);
	QString mType;
	Scope mScope;
	SearchResultModel *mModel;
	SearchFlags searchFlags;
	
private:

};
Q_DECLARE_OPERATORS_FOR_FLAGS(SearchQuery::SearchFlags)


class LabelSearchQuery : public SearchQuery {
	Q_OBJECT
public:
	LabelSearchQuery(QString label);
	virtual void run(LatexDocument *doc);
};


#endif // SEARCHQUERY_H
