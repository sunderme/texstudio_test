#include "scriptengine.h"
#include "filechooser.h"
#include "smallUsefulFunctions.h"
#include "qdocumentsearch.h"
#include "scriptobject.h"
#include "buildmanager.h"
#include "texmaker.h"
Q_DECLARE_METATYPE(QDocument*);

BuildManager* scriptengine::buildManager = 0;
Texmaker* scriptengine::app = 0;

//copied from trolltech mailing list
template <typename Tp> QScriptValue qScriptValueFromQObject(QScriptEngine *engine, Tp const &qobject)
{
	return engine->newQObject(qobject);
}

template <typename Tp> void qScriptValueToQObject(const QScriptValue &value, Tp &qobject) {
	qobject = qobject_cast<Tp>(value.toQObject());
}

template <typename Tp> int qScriptRegisterQObjectMetaType( QScriptEngine *engine, const QScriptValue &prototype = QScriptValue(), Tp * /* dummy */ = 0)
{
    return qScriptRegisterMetaType<Tp>(engine, qScriptValueFromQObject, qScriptValueToQObject, prototype);
}
//


QScriptValue qScriptValueFromDocumentCursor(QScriptEngine *engine, QDocumentCursor const &cursor)
{
	return engine->newQObject(new QDocumentCursor(cursor), QScriptEngine::ScriptOwnership);
}
void qScriptValueToDocumentCursor(const QScriptValue &value, QDocumentCursor &qobject) {
	Q_ASSERT(value.toQObject());
	qobject = *qobject_cast<QDocumentCursor*>(value.toQObject());
}
/**QScriptValue qScriptValueFromKeySequence(QScriptEngine *engine, QKeySequence const &ks)
{
	return engine->newVariant(ks);
}
void qScriptValueToKeySequence(const QScriptValue &value, QKeySequence &ks) {
	if (value.isString()) ks = QKeySequence(value.toString());
	else if (value.isNumber()) ks = QKeySequence(value.toInt32());
	else if (value.isVariant()) ks = value.toVariant().value<QKeySequence>();
	else ks = QKeySequence();
}*/

Q_DECLARE_METATYPE(ProcessX*);
Q_DECLARE_METATYPE(QAction*);
Q_DECLARE_METATYPE(QMenu*);
Q_DECLARE_METATYPE(SubScriptObject*);

scriptengine::scriptengine(QObject *parent) : QObject(parent),globalObject(0), m_editor(0)
{
	engine=new QScriptEngine(this);
	qScriptRegisterQObjectMetaType<QDocument*>(engine);
	qScriptRegisterMetaType<QDocumentCursor>(engine, qScriptValueFromDocumentCursor, qScriptValueToDocumentCursor, QScriptValue());
	qScriptRegisterQObjectMetaType<ProcessX*>(engine);
	qScriptRegisterQObjectMetaType<SubScriptObject*>(engine);
	qScriptRegisterQObjectMetaType<Texmaker*>(engine);
	qScriptRegisterQObjectMetaType<QAction*>(engine);
	qScriptRegisterQObjectMetaType<QMenu*>(engine);
//	qScriptRegisterMetaType<QKeySequence>(engine, qScriptValueFromKeySequence, qScriptValueToKeySequence, QScriptValue());
	//qScriptRegisterQObjectMetaType<QUILoader*>(engine);
//	engine->setDefaultPrototype(qMetaTypeId<QDocument*>(), QScriptValue());
	//engine->setDefaultPrototype(qMetaTypeId<QDocumentCursor>(), QScriptValue());
}

scriptengine::~scriptengine(){
	delete engine;
	//don't delete globalObject, it has either been destroyed in run, or by another script
}

void scriptengine::setScript(const QString& script){
	m_script=script;
}
void scriptengine::setEditor(QEditor *editor){
	m_editor = editor;
}

#define SCRIPT_REQUIRE(cond, message) if (!(cond)) { context->throwError(scriptengine::tr(message)); return engine->undefinedValue(); }

QDocumentCursor cursorFromValue(const QScriptValue& value){
	QDocumentCursor * c = qobject_cast<QDocumentCursor*> (value.toQObject());
	if (!c) {
		if (value.engine() && value.engine()->currentContext()) value.engine()->currentContext()->throwError(scriptengine::tr("Expected cursor object"));
		return QDocumentCursor();
	}
	return *c;
}

QScriptValue searchReplaceFunction(QScriptContext *context, QScriptEngine *engine, bool replace){
	QEditor *editor = qobject_cast<QEditor*>(context->thisObject().toQObject());
	//read arguments
	SCRIPT_REQUIRE(editor, "invalid object");
	SCRIPT_REQUIRE(!replace || context->argumentCount()>=2, "at least two arguments are required");
	SCRIPT_REQUIRE(context->argumentCount()>=1, "at least one argument is required");
	SCRIPT_REQUIRE(context->argumentCount()<=4, "too many arguments");
	SCRIPT_REQUIRE(context->argument(0).isString()||context->argument(0).isRegExp(), "first argument must be a string or regexp");
	QDocumentSearch::Options flags = QDocumentSearch::Silent;
	bool global = false, caseInsensitive = false;
	QString searchFor;
	if (context->argument(0).isRegExp()) {
		flags |= QDocumentSearch::RegExp;
		QRegExp r = context->argument(0).toRegExp();
		searchFor = r.pattern();
		caseInsensitive = r.caseSensitivity() == Qt::CaseInsensitive;
		Q_ASSERT(caseInsensitive == context->argument(0).property("ignoreCase").toBool()); //check assumption about javascript core
		global = context->argument(0).property("global").toBool();
	} else searchFor = context->argument(0).toString();
	QScriptValue handler;
	QDocumentCursor scope = editor->document()->cursor(0,0,editor->document()->lineCount(),0);
	int handlerCount = 0;
	for (int i=1; i<context->argumentCount();i++)
		if (context->argument(i).isString() || context->argument(i).isFunction()) handlerCount++;
	SCRIPT_REQUIRE(handlerCount <= (replace?2:1), "too many string or function arguments");
	for (int i=1; i<context->argumentCount();i++) {
		QScriptValue a = context->argument(i);
		if (a.isFunction()) {
			SCRIPT_REQUIRE(!handler.isValid(), "Multiple callbacks");
			handler = a;
		} else if (a.isString()) {
			if (!replace || handlerCount > 1) {
				QString s = a.toString().toLower();
				global = s.contains("g");
				caseInsensitive = s.contains("i");
				if (s.contains("w")) flags |= QDocumentSearch::WholeWords;
			} else {
				SCRIPT_REQUIRE(!handler.isValid(), "Multiple callbacks");
				handler = a;
			}
			handlerCount--;
		} else if (a.isNumber()) flags |= QDocumentSearch::Options((int)a.toNumber());
		else if (a.isObject()) scope = cursorFromValue(a);
		else SCRIPT_REQUIRE(false, "Invalid argument");
	}
	SCRIPT_REQUIRE(handler.isValid() || !replace, "No callback given");
	if (!caseInsensitive) flags |= QDocumentSearch::CaseSensitive;

	//search/replace
	QDocumentSearch search(editor, searchFor, flags);
	search.setScope(scope);
	if (replace && handler.isString()) {
		search.setReplaceText(handler.toString());
		search.setOption(QDocumentSearch::Replace,true);
		return search.next(false, global, false, false);
	}
	if (!handler.isValid())
		return search.next(false,global,true,false);
	int count=0;
	while (search.next(false, false, true, false) && search.cursor().isValid()) {
		count++;
		QDocumentCursor temp = search.cursor();
		QScriptValue cb = handler.call(QScriptValue(), QScriptValueList() << engine->newQObject(&temp));
		if (replace && cb.isValid()){
			QDocumentCursor tmp = search.cursor();
			tmp.replaceSelectedText(cb.toString());
			search.setCursor(tmp.selectionEnd());
		}
		if (!global) break;
	}
	return count;
}

QScriptValue searchFunction(QScriptContext *context, QScriptEngine *engine){
	return searchReplaceFunction(context, engine, false);
}
QScriptValue replaceFunction(QScriptContext *context, QScriptEngine *engine){
	return searchReplaceFunction(context, engine, true);
}

#define Q_SCRIPT_DECLARE_QMETAOBJECT_WITH_ENGINE(T, _Arg1) \
	/* Copied from qt's Q_SCRIPT_DECLARE_QMETAOBJECT */ \
template<> inline QScriptValue qscriptQMetaObjectConstructor<T>(QScriptContext *ctx, QScriptEngine *eng, T *) \
{ \
    _Arg1 arg1 = qscriptvalue_cast<_Arg1> (ctx->argument(0)); \
    T* t = new T(eng, arg1); \
    if (ctx->isCalledAsConstructor()) \
        return eng->newQObject(ctx->thisObject(), t, QScriptEngine::AutoOwnership); \
    QScriptValue o = eng->newQObject(t, QScriptEngine::AutoOwnership); \
    o.setPrototype(ctx->callee().property(QString::fromLatin1("prototype"))); \
    return o; \
}

Q_SCRIPT_DECLARE_QMETAOBJECT_WITH_ENGINE(UniversalInputDialogScript, QWidget*)

void scriptengine::run(){
	if (globalObject) delete globalObject;
	globalObject = new ScriptObject(m_script,buildManager,app);
	QScriptValue globalValue = engine->newQObject(globalObject);
	globalValue.setPrototype(engine->globalObject());
	engine->setGlobalObject(globalValue);
	
	QDocumentCursor c;
	QScriptValue cursorValue;
	if (m_editor) {
		QScriptValue editorValue = engine->newQObject(m_editor);
		editorValue.setProperty("search", engine->newFunction(&searchFunction), QScriptValue::ReadOnly|QScriptValue::Undeletable);
		editorValue.setProperty("replace", engine->newFunction(&replaceFunction), QScriptValue::ReadOnly|QScriptValue::Undeletable);
		engine->globalObject().setProperty("editor", editorValue);
		
		c=m_editor->cursor();
		c.setAutoUpdated(true); //auto updated so the editor text insert functions actually move the cursor		
		cursorValue = engine->newQObject(&c);
		engine->globalObject().setProperty("cursor", cursorValue);
	}
	
	QScriptValue qsMetaObject = engine->newQMetaObject(&QDocumentCursor::staticMetaObject);
	engine->globalObject().setProperty("cursorEnums", qsMetaObject);

	QScriptValue uidClass = engine->scriptValueFromQMetaObject<UniversalInputDialogScript>();
       engine->globalObject().setProperty("UniversalInputDialog", uidClass);
	
	FileChooser flchooser(0,scriptengine::tr("File Chooser"));
	engine->globalObject().setProperty("fileChooser", engine->newQObject(&flchooser));

	engine->evaluate(m_script);

	if(engine->hasUncaughtException()){
		QString error = QString(tr("Uncaught exception at line %1: %2\n")).arg(engine->uncaughtExceptionLineNumber()).arg(engine->uncaughtException().toString());
		error += "\n"+QString(tr("Backtrace %1")).arg(engine->uncaughtExceptionBacktrace().join(", "));
		qDebug() << error;
		QMessageBox::critical(0, tr("Script-Error"), error);
	}
	
	if (m_editor) {
		if (engine->globalObject().property("cursor").strictlyEquals(cursorValue)) m_editor->setCursor(c);
		else m_editor->setCursor(cursorFromValue(engine->globalObject().property("cursor")));
	}

	if (!globalObject->backgroundScript) {
		delete globalObject;
		globalObject = 0;
	}
}


UniversalInputDialogScript::UniversalInputDialogScript(QScriptEngine* engine, QWidget* parent):UniversalInputDialog(parent), engine(engine){
	
}
UniversalInputDialogScript::~UniversalInputDialogScript(){
	for (int i=0;i<properties.size();i++) properties[i].deallocate();
}

QScriptValue UniversalInputDialogScript::add(const QScriptValue& def, const QScriptValue& description, const QScriptValue& id){
	QWidget* w = 0;
	if (def.isArray()) {
		QStringList options;
		QScriptValueIterator it(def);
		while (it.hasNext()) {
		    it.next();
		    if (it.flags() & QScriptValue::SkipInEnumeration)
			 continue;
		    if (it.value().isString() || it.value().isNumber()) options << it.value().toString();
		    else engine->currentContext()->throwError("Invalid default value in array (must be string or number): "+it.value().toString());
		}
		w = addComboBox(ManagedProperty::fromValue(options), description.toString());
	} else if (def.isBool()) {
		w = addCheckBox(ManagedProperty::fromValue(def.toBool()), description.toString());
	} else if (def.isNumber()) {
		w = addDoubleSpinBox(ManagedProperty::fromValue(def.toNumber()), description.toString());
	} else if (def.isString()) {
		w = addLineEdit(ManagedProperty::fromValue(def.toString()), description.toString());
	} else {	
		
		engine->currentContext()->throwError(tr("Invalid default value: %1").arg(def.toString()));
		return QScriptValue();
	}
	if (id.isValid()) properties.last().name = id.toString();
	return engine->newQObject(w);
}

QScriptValue UniversalInputDialogScript::exec(){
	if (!UniversalInputDialog::exec()) return QScriptValue();
	return getAll();
}

QScriptValue UniversalInputDialogScript::getAll(){
	QScriptValue res = engine->newArray(properties.size());
	for (int i=0;i<properties.size();i++) {
		res.setProperty(i, engine->newVariant(properties[i].valueToQVariant()));
		if (!properties[i].name.isEmpty())
			res.setProperty(properties[i].name, engine->newVariant(properties[i].valueToQVariant()));
	}
	return res;
}

QScriptValue UniversalInputDialogScript::get(const QScriptValue& id){
	if (id.isNumber()) {
		int i = id.toInt32();
		if (i < 0 || i > properties.size()) return QScriptValue();
		return engine->newVariant(properties[i].valueToQVariant());
	}
	if (id.isString()) {
		QString sid = id.toString();
		foreach (const ManagedProperty& mp, properties)
			if (mp.name == sid) 
				return engine->newVariant(mp.valueToQVariant());
		return QScriptValue();
	}
	engine->currentContext()->throwError(tr("Unkown variable %1").arg(id.toString()));
	return QScriptValue();
}
