/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * A very simple XML object model.  Used with the XML mini parser
 * for fast instantiation of XML streams into C++ objects.
 * 
 * This is conceptually similar to DOM, but is simpler and less functional,
 * which can be a good or bad thing depending on your point of view.  
 * 
 */

#ifndef XOM_H
#define XOM_H

// might as well start making it easier for Juce apps
// till we rewrite this
#include <JuceHeader.h>

/****************************************************************************
 *                                                                          *
 *   							 XML OBJECTS                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * ERR_XOM
 *
 * Description: 
 * 
 * Xom parser error codes, extend those of XmlMiniParser.
 ****************************************************************************/

#define ERR_XOM_BASE 100

#define ERR_XOM_UNBALANCED_TAGS 	ERR_XOM_BASE + 0
#define ERR_XOM_DANGLING_TAGS 		ERR_XOM_BASE + 1

/****************************************************************************
 * XmlProperty
 *
 * Description: 
 * 
 * These are objects that can be hanging off any node in an XML tree. 
 * They allow for the attachment of arbitrary "properties" or "metadata" 
 * to nodes that aren't considered part of the XML source.  They are 
 * similar in behavior to XML attributes (represented by the 
 * XmlAttribute class) except that any node may have them, not just elements.
 * 
 * NOTE: After the addition of the "attachment" to nodes and attributes, 
 * the original use of properties is now gone.  Still, it might be useful
 * to keep around.
 *
 ****************************************************************************/

class XmlProperty {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty *getNext(void) {
		return mNext;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getValue(void) {
		return mValue;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty(void) {
		mNext		= nullptr;
		mName		= nullptr;
		mValue		= nullptr;
	}

	XmlProperty *copy(void);

	~XmlProperty(void);

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setValue(char *v) {
		delete mValue;
		mValue = v;
	}

	void setNext(XmlProperty *n) {
		mNext = n;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlProperty 	*mNext;
	char 			*mName;
	char 			*mValue;

};
	
/****************************************************************************
 * XmlNode
 *
 * Description: 
 * 
 * The base class for most XML objects.
 * Defines the basic tree node interface and implementation.
 *  
 * Unlike more complex Composites, we don't try to push implementations
 * of things like child maintenance down into the non-leaf subclasses.  
 * Could do that if necessary.
 * 
 * Note that deleting a node deletes all the right siblings as well.
 * 
 * All nodes have a class code so we can do type checks quickly.
 * Various isFoo methods provided for safe downcasting.
 * 
 * We're optimizing for construction speed, vs. memory usage.
 * For this reason, we keep a tail pointer on the child list so 
 * we can append quickly.  This is used only by the XomParser.
 * 
 * All nodes may have a list of XmlProperty objects that allows the 
 * attachment of user defined state to each node.
 * 
 * All nodes also have a void* "attachment" that can be used in controlled
 * conditions to associate user defined objects with nodes.  
 * 
 ****************************************************************************/

class XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	// 
	// class codes
	//
	//////////////////////////////////////////////////////////////////////

	typedef enum {

		XML_UNKNOWN,
		XML_DOCUMENT,
		XML_DOCTYPE,
		XML_ELEMENT,
		XML_PI,
		XML_COMMENT,
		XML_MSECT,
		XML_PCDATA,
		XML_ENTREF

	} XML_CLASS;

	//////////////////////////////////////////////////////////////////////
	// 
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XML_CLASS getClass(void) {
		return mClass;
	}
	
	int isCLass(XML_CLASS c) {
		return mClass == c;
	}

	XmlNode *getParent(void) {
		return mParent;
	}

	XmlNode *getChildren(void) {
		return mChildren;
	}

	XmlNode *getNext(void) {
		return mNext;
	}

	// getChildElement and getNextElement are convenience methods
	// that skip over non-element nodes like comments, pi's etc.

	class XmlElement *getChildElement(void);
	class XmlElement *getNextElement(void);

	// debug only
	void dump(int level = 0);

	void *getAttachment(void) {
		return mAttachment;
	}

	void setAttachment(void *a) {
		mAttachment = a;
	}

	//////////////////////////////////////////////////////////////////////
	// 
	// downcasting/typechecking 
	//
	//////////////////////////////////////////////////////////////////////
 
	virtual class XmlDocument *isDocument(void) {
		return nullptr;
	}

	virtual class XmlDoctype *isDoctype(void) {
		return nullptr;
	}

	virtual class XmlElement* isElement(void) {
		return nullptr;
	}

	virtual class XmlPi *isPi(void) {
		return nullptr;
	}
	
	virtual class XmlComment *isComment(void) {
		return nullptr;
	}

	virtual class XmlMsect *isMsect(void) {
		return nullptr;
	}

	virtual class XmlPcdata *isPcdata(void) {
		return nullptr;
	}

	virtual class XmlEntref *isEntref(void) {
		return nullptr;
	}

	//////////////////////////////////////////////////////////////////////
	// 
	// properties
	//
	//////////////////////////////////////////////////////////////////////

	XmlProperty *getProperties(void) {
		return mProperties;
	}

	void setProperties(XmlProperty *props) {
		delete mProperties;
		mProperties = props;
	}

	const char *getProperty(const char *name);
	void setProperty(const char *name, const char *value);
	XmlProperty *getPropertyObject(const char *name);

	//////////////////////////////////////////////////////////////////////
	// 
	// convenience utilities
	//
	//////////////////////////////////////////////////////////////////////

	XmlElement *findElement(const char *name);
	XmlElement *findElement(const char *elname, const char *attname,
									  const char *attval);
	const char *getElementContent(const char *name);

	//////////////////////////////////////////////////////////////////////
	// 
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	// normally you don't create one of these, create a subclass

	XmlNode(void) {
		mParent 	= nullptr;
		mChildren 	= nullptr;
		mLastChild	= nullptr;
		mNext 		= nullptr;
		mProperties	= nullptr;
		mAttachment	= nullptr;
	}

	virtual ~XmlNode(void);

	// this performs a deep copy, I decided not to define this
	// as a copy constructor in case we wanted to use that for shallow copy
	virtual XmlNode *copy(void) = 0;

	void setParent(XmlNode *p) {
		mParent = p;
	}

	void setNext(XmlNode *n) {
		mNext = n;
	}

	void setChildren(XmlNode *c);
	void addChild(XmlNode *c);
	void deleteChild(XmlNode *c);
	XmlNode *stealChildren(void);

	// visitor interface

	virtual void visit(class XmlVisitor *v) = 0;

	// convenient interface to the xml writer visitor

	char *serialize(int indent = 0);

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XML_CLASS 	mClass;
	XmlNode 	*mNext;
	XmlNode 	*mParent;
	XmlNode 	*mChildren;
	XmlNode		*mLastChild;
	XmlProperty	*mProperties;
	void 		*mAttachment;

};

/****************************************************************************
 * XmlDoctype
 *
 * Description: 
 * 
 * Encapsultes the contents of a <!DOCTYPE statement.
 * These are XmlNode's since they can have children (the internal subset),
 * but they're not currently on the child list of the XmlDocument,
 * since its usually invonvenient to deal with them there.
 ****************************************************************************/

class XmlDoctype : public XmlNode 
{

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlDoctype *isDoctype(void) {
		return this;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getPubid(void) {
		return mPubid;
	}

	const char *getSysid(void) {
		return mSysid;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlDoctype(void) {
		mClass	= XML_DOCTYPE;
		mName	= nullptr;	
		mPubid	= nullptr;
		mSysid	= nullptr;
	}

	XmlNode *copy(void);

	~XmlDoctype(void) {
		delete mName;
		delete mPubid;
		delete mSysid;
	}

	void setName(char *name) {
		delete mName;
		mName = name;
	}

	void setPubid(char *pubid) {
		delete mPubid;
		mPubid = pubid;
	}

	void setSysid(char *sysid) {
		delete mSysid;
		mSysid = sysid;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char *mName;
	char *mPubid;
	char *mSysid;

};

/****************************************************************************
 * XmlDocument
 *
 * Description: 
 * 
 * Class that forms the root of an XML document in memory.
 * We keep the preamble PI's and the DOCTYPE statement out of the child 
 * list since that's usually the what you want.  The preamble normally
 * consists of only processing instructions, but we let it be
 * an XmlNode list in case there are comments.
 * 
 ****************************************************************************/

class XmlDocument : public XmlNode 
{

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlDocument *isDocument(void) {
		return this;
	}

	XmlNode *getPreamble(void) {
		return mPreamble;
	}

	XmlDoctype *getDoctype(void) {
		return mDoctype;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlDocument(void) {
		mClass		= XML_DOCUMENT;
		mPreamble	= nullptr;
		mDoctype	= nullptr;
	}

	XmlNode *copy(void);

	~XmlDocument(void) {
		delete mPreamble;
		delete mDoctype;
	}

	void setPreamble(XmlNode *n) {

		delete mPreamble;
		mPreamble = n;

		for (n = mPreamble ; n != nullptr ; n = n->getNext())
		  n->setParent(this);
	}

	void setDoctype(XmlDoctype *d) {
		delete mDoctype;
		mDoctype = d;
		if (d != nullptr)
		  d->setParent(this);
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlNode			*mPreamble;
	XmlDoctype		*mDoctype;

};

/****************************************************************************
 * XmlAttribute
 *
 * Description: 
 * 
 * Class representing an element attribute.
 * Note that unlike DOM these aren't XmlNodes.  They could be, but
 * since they're not tree structured, its a bit of a waste of space.
 * I personally don't find treating attributes as nodes a useful thing.
 * 
 * Added an attachment pointer like XmlNode.  Should have properties
 * here too?
 * 
 ****************************************************************************/

class XmlAttribute {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	XmlAttribute *getNext(void) {
		return mNext;
	}

	const char *getName(void) {
		return mName;
	}

	const char *getValue(void) {
		return mValue;
	}

	void *getAttachment(void) {
		return mAttachment;
	}

	void setAttachment(void *a) {
		mAttachment = a;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlAttribute(void) {
		mNext		= nullptr;
		mName		= nullptr;
		mValue		= nullptr;
		mAttachment	= 0;
	}

	~XmlAttribute(void);

	XmlAttribute *copy(void);

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setValue(char *v) {
		delete mValue;
		mValue = v;
	}

	void setNext(XmlAttribute *n) {
		mNext = n;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	XmlAttribute 	*mNext;
	char 			*mName;
	char 			*mValue;
	void			*mAttachment;

};
	
/****************************************************************************
 * XmlElement
 *
 * Description: 
 * 
 * Class representing an XML element.
 * Attributes are kept on a special list.
 ****************************************************************************/

class XmlElement : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	class XmlElement *isElement(void) {
		return this;
	}

	int isEmpty(void) {
		return mEmpty;
	}

	const char *getName(void) {
		return mName;
	}
	
	bool isName(const char *name);

	XmlAttribute *getAttributes(void) {
		return mAttributes;
	}

	const char *getAttribute(const char *name);
	int getIntAttribute(const char *name, int dflt);
	int getIntAttribute(const char *name);
	bool getBoolAttribute(const char *name);
	void setAttribute(const char *name, const char *value);
	void setAttributeInt(const char *name, int value);
	XmlAttribute *getAttributeObject(const char *name);

	const char *getContent(void);
	XmlElement *findNextElement(const char *name);

	void visit(class XmlVisitor *v);

    // shorthand I always want
    int getInt(const char* name) {
        return getIntAttribute(name);
    }

    const char* getString(const char* name) {
        return getAttribute(name);
    }

    bool getBool(const char* name) {
        return getBoolAttribute(name);
    }

    // gradual Juce migration
    juce::String getJString(const char* name) {
        return juce::String(getAttribute(name));
    }
    
	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlElement(void) {
		mClass			= XML_ELEMENT;
		mName			= nullptr;
		mAttributes		= nullptr;
		mLastAttribute	= nullptr;
		mEmpty			= 0;
	}

	XmlNode *copy(void);

	~XmlElement(void) {
		delete mName;
		delete mAttributes;
	}

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setAttributes(XmlAttribute *a) {
		delete mAttributes;
		mAttributes = a;
		mLastAttribute = a;
	}

	void addAttribute(XmlAttribute *a) {
		if (a != nullptr) {
			if (mAttributes == nullptr)
			  mAttributes = a;
			else
		  mLastAttribute->setNext(a);
			mLastAttribute = a;
		}
	}

	void setEmpty(int e) {
		mEmpty = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mName;
	XmlAttribute	*mAttributes;
	XmlAttribute 	*mLastAttribute;
	int 			mEmpty;

};

/****************************************************************************
 * XmlPi
 *
 * Description: 
 * 
 * Class representing an XML processing instruction.
 ****************************************************************************/

class XmlPi : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlPi *isPi(void) {
		return this;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlPi(void) {
		mClass		= XML_PI;
		mText		= nullptr;
	}

	XmlNode *copy(void);

	~XmlPi(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlComment
 *
 * Description: 
 * 
 * Class representing an XML comment.
 ****************************************************************************/

class XmlComment : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlComment *isComment(void) {
		return this;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlComment(void) {
		mClass		= XML_COMMENT;
		mText		= nullptr;
	}

	XmlNode *copy(void);

	~XmlComment(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlMsect
 *
 * Description: 
 * 
 * Class representing an XML marked section.
 * Three types, CDATA, INCLUDE, IGNORE.
 ****************************************************************************/

class XmlMsect : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	typedef enum {

		MS_IGNORE,
		MS_INCLUDE,
		MS_CDATA

	} MSECT_TYPE;

	MSECT_TYPE getType(void) {
		return mType;
	}

	const char *getText(void) {
		return mText;
	}

	const char *getEntity(void) {
		return mEntity;
	}

	XmlMsect *isMsect(void) {
		return this;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////
	
	XmlMsect(void) {
		mClass		= XML_MSECT;
		mType		= MS_CDATA;
		mText		= nullptr;
		mEntity		= nullptr;
	}

	XmlNode *copy(void);

	~XmlMsect(void) {
		delete mText;
		delete mEntity;
	}

	void setType(MSECT_TYPE t) {
		mType = t;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	void setEntity(char *e) {
		delete mEntity;
		mEntity = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;
	char			*mEntity; 	// when using parameter keywords
	MSECT_TYPE		mType;

};

/****************************************************************************
 * XmlPcdata
 *
 * Description: 
 * 
 * Class representing a string of pcdata.  
 ****************************************************************************/

class XmlPcdata : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getText(void) {
		return mText;
	}

	XmlPcdata *isPcdata(void) {
		return this;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlPcdata(void) {
		mClass		= XML_PCDATA;
		mText		= nullptr;
	}

	XmlNode *copy(void);

	~XmlPcdata(void) {
		delete mText;
	}

	void setText(char *t) {
		delete mText;
		mText = t;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mText;

};

/****************************************************************************
 * XmlEntref
 *
 * Description: 
 * 
 * Class representing an XML entity reference.
 * If had a representation for <!ENTITY statements in the doctype, 
 * we could try to resolve the name to an object.
 ****************************************************************************/

class XmlEntref : public XmlNode {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	const char *getName(void) {
		return mName;
	}

	XmlEntref *isEntref(void) {
		return this;
	}

	int isParameter(void) {
		return mParameter;
	}

	void visit(class XmlVisitor *v);

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	XmlEntref(void) {
		mClass		= XML_ENTREF;
		mName		= nullptr;
		mParameter	= 0;
	}

	XmlNode *copy(void);

	~XmlEntref(void) {
		delete mName;
	}

	void setName(char *n) {
		delete mName;
		mName = n;
	}

	void setParameter(int p) {
		mParameter = p;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	char 			*mName;
	int				mParameter;

};

/****************************************************************************
 *                                                                          *
 *   							  VISITORS                                  *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * XmlVisitor
 *
 * Description: 
 * 
 * Base class of a visitor for XmlNode composites.
 * 
 ****************************************************************************/

class XmlVisitor {

  public:
	
	// XmlNode hierachy

	virtual void visitDocument(XmlDocument *){}
	virtual void visitDoctype(XmlDoctype *){}
	virtual void visitElement(XmlElement *){}
	virtual void visitPi(XmlPi *){}
	virtual void visitComment(XmlComment *){}
	virtual void visitMsect(XmlMsect *){}
	virtual void visitPcdata(XmlPcdata *){}
	virtual void visitEntref(XmlEntref *){}

	// Not part of the XmlNode hierarchy, but some iterators may
	// be able to visit them.

	virtual void visitAttribute(XmlAttribute *){}
	virtual void visitProperty(XmlProperty *){}


  protected:

	XmlVisitor(void){}
	virtual ~XmlVisitor(void){}

};

/****************************************************************************
 * XmlWriter
 *
 * Description: 
 * 
 * An XmlVisitor that renders the XmlNode tree as a string of XML text.
 * Iteration is done by the visitor itself right now, might want
 * an iterator someday.
 * 
 ****************************************************************************/

class XmlWriter : private XmlVisitor {

  public:

	XmlWriter(void);
	~XmlWriter(void);

	char *exec(XmlNode *node);

	void setIndent(int i) {
		mIndent = i;
	}

  private:

	class Vbuf *mBuf;		// buffer we're accumulating
	int mIndent;

	//
	// visitor implementations
	//

	void visitDocument(XmlDocument *obj);
	void visitDoctype(XmlDoctype *obj);
	void visitElement(XmlElement *obj);
	void visitPi(XmlPi *obj);
	void visitComment(XmlComment *obj);
	void visitMsect(XmlMsect *obj);
	void visitPcdata(XmlPcdata *obj);
	void visitEntref(XmlEntref *obj);
	void visitAttribute(XmlAttribute *obj);
	void visitProperty(XmlProperty *obj);

	void indent(void);
};

/****************************************************************************
 * XmlCopier
 *
 * Description: 
 * 
 * An XmlVisitor that copes an XmlNode tree.
 * 
 ****************************************************************************/

class XmlCopier : private XmlVisitor {

  public:

	XmlCopier(void);
	~XmlCopier(void);

	char *exec(XmlNode *node);

  private:

	XmlNode	*mParent;		// parent node of the copy

	//
	// visitor implementations
	//

	void visitDocument(XmlDocument *obj);
	void visitDoctype(XmlDoctype *obj);
	void visitElement(XmlElement *obj);
	void visitPi(XmlPi *obj);
	void visitComment(XmlComment *obj);
	void visitMsect(XmlMsect *obj);
	void visitPcdata(XmlPcdata *obj);
	void visitEntref(XmlEntref *obj);
	void visitAttribute(XmlAttribute *obj);
	void visitProperty(XmlProperty *obj);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
