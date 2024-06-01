/*
 * Container of variably typed values
 * Factored out of the Expr expression evaluator
 * eventually start using std:: for this or maybe Juce
 */

#include <stdio.h>
#include <stdlib.h>
//#include <memory.h>
#include <string.h>
#include <ctype.h>

#include "../util/Util.h"
#include "../util/Vbuf.h"
#include "../util/Trace.h"
#include "../util/List.h"

#include "ExValue.h"

//////////////////////////////////////////////////////////////////////
//
// ExValueList
//
//////////////////////////////////////////////////////////////////////


ExValueList::ExValueList()
{
    mOwner = nullptr;
}

/**
 * For some shitty C++ reason we have to define a destructor or else
 * our overloaded deleteElement method won't be called.
 */
ExValueList::~ExValueList()
{
    reset();
}

void ExValueList::deleteElement(void* o)
{
	delete (ExValue *)o;
}

/**
 * This is called whenever an element is added to the list
 * with add() or set().  We don't actually copy but this provides
 * a hook where we can make sure ownership of embedded lists is taken.
 */
void* ExValueList::copyElement(void* src)
{
    if (src != nullptr) {
        ExValue* srcv = (ExValue*)src;
        if (srcv->getType() == EX_LIST) {
            ExValueList* childlist = srcv->getList();   
            if (childlist->getOwner() != srcv) {
                printf("WARNING: Transfering ownership of list within list\n");
                fflush(stdout);
            }
            childlist->setOwner(srcv);
        }
    }

	return src;
}

ExValue* ExValueList::getValue(int index)
{
    return (ExValue*)get(index);
}

void ExValueList::setOwner(void* owner)
{
    mOwner = owner;
}

void* ExValueList::getOwner()
{
    return mOwner;
}

ExValueList* ExValueList::copy()
{
    ExValueList* neu = new ExValueList();

    for (int i = 0 ; i < size() ; i++) {
        ExValue* srcvalue = getValue(i);
        if (srcvalue != nullptr) {
            ExValue* newvalue = new ExValue();
            neu->add(newvalue);
            if (srcvalue->getType() != EX_LIST)
              newvalue->set(srcvalue);
            else {
                ExValueList* srclist = srcvalue->getList();
                if (srclist != nullptr)
                  newvalue->setOwnedList(srclist->copy());
            }
        }
    }
    return neu;
}

/****************************************************************************
 *                                                                          *
 *   								VALUE                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * We don't have an explicit nullptr right now.  
 * The default value is the empty string
 */
ExValue::ExValue()
{
	mType = EX_STRING;
	mInt = 0;
	mFloat = 0.0;
	mBool = false;
	mList = nullptr;
	strcpy(mString, "");
}

ExValue::~ExValue()
{
	// this will delete elements too
    releaseList();
}

void ExValue::releaseList()
{
    if (mList != nullptr) {
        if (mList->getOwner() == this)
          delete mList;
        mList = nullptr;
    }
}

ExType ExValue::getType()
{
	return mType;
}

void ExValue::setType(ExType t)
{
	mType = t;
}

char* ExValue::getBuffer()
{
	return mString;
}

int ExValue::getBufferMax()
{
	return EX_MAX_STRING;
}

void ExValue::setNull()
{
	mType = EX_STRING;
    mInt = 0;   
    mFloat = 0.0;
    mBool = false;
	strcpy(mString, "");
    releaseList();
}

bool ExValue::isNull()
{
	return ((mType == EX_STRING) && (strlen(mString) == 0));
}

int ExValue::getInt()
{
	int ival = mInt;

	switch (mType) {
		case EX_FLOAT: {
			ival = (int)mFloat;
		}
		break;

		case EX_BOOL: {
			ival = ((mBool) ? 1 : 0);
		}
		break;
 
		case EX_STRING: { 
			// if empty, it won't set this
            ival = 0;
			sscanf(mString, "%d", &ival);
		}
		break;
 
		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != nullptr)
			  ival = el->getInt();
			else
			  ival = 0;
		}
		break;

		case EX_INT: {
			// xcode whines if we don't have this
		}
		break;
	}

	return ival; 
}

void ExValue::setInt(int i)
{
	mType = EX_INT;
	mInt = i;
    releaseList();
}

long ExValue::getLong()
{
	// !! need to have a true long internal value
	return (long)getInt();
}

void ExValue::setLong(long i)
{
	// !! need to have a true long internal value
	mType = EX_INT;
	mInt = (int)i;
    releaseList();
}

float ExValue::getFloat()
{
	float fval = mFloat;

	switch (mType) {

		case EX_INT: {
			fval = (float)mInt;
		}
		break;

		case EX_BOOL: {
			fval = ((mBool) ? 1.0f : 0.0f);
		}
		break;

		case EX_STRING: {
			sscanf(mString, "%f", &fval);
		}
		break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != nullptr)
			  fval = el->getFloat();
			else
			  fval = 0.0f;
		}
		break;

		case EX_FLOAT: {
			// xcode whines if we don't have this
		}
		break;
	}

	return fval; 
}

void ExValue::setFloat(float f)
{
	mType = EX_FLOAT;
	mFloat = f;
    releaseList();
}

bool ExValue::getBool()
{
	bool bval = mBool;

	switch (mType) {

		case EX_INT: {
			bval = (mInt != 0);
		}
		break;

		case EX_FLOAT: {
			bval = (mFloat != 0.0);
		}
		break;

		case EX_STRING: {
			bval = (StringEqualNoCase(mString, "true") ||
					StringEqualNoCase(mString, "yes") ||
					StringEqualNoCase(mString, "on") ||
					StringEqualNoCase(mString, "1"));
		}
		break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != nullptr)
			  bval = el->getBool();
			else
			  bval = false;
		}
		break;

		case EX_BOOL: {
			// xcode whines if we don't have this
		}
		break;
	}

	return bval; 
}

void ExValue::setBool(bool b)
{
	mType = EX_BOOL;
	mBool = b;
    releaseList();
}

const char* ExValue::getString() 
{
	switch (mType) {

		case EX_INT: {
			snprintf(mString, sizeof(mString), "%d", mInt);
		}
		break;

		case EX_FLOAT: {
			snprintf(mString, sizeof(mString), "%f", mFloat);
		}
		break;

		case EX_BOOL: {
			if (mBool)
			  strcpy(mString, "true");
			else
			  strcpy(mString, "false");
        }
        break;

		case EX_LIST: {
			ExValue* el = getList(0);
			if (el != nullptr)
			  el->getString(mString, EX_MAX_STRING);
			else
			  strcpy(mString, "");
			break;
		}
		break;

		case EX_STRING: {
			// xcode whines if we don't have this
		}
		break;
	}

	return mString;
}

/**
 * Render the value as a string, but do not change
 * the underlying type.
 */
void ExValue::getString(char* buffer, int max)
{
	switch (mType) {

		case EX_INT: {
			snprintf(buffer, max, "%d", mInt);
		}
		break;

		case EX_FLOAT: {
			snprintf(buffer, max, "%f", mFloat);
		}
		break;

		case EX_BOOL: {
			if (mBool)
			  strcpy(buffer, "true");
			else
			  strcpy(buffer, "false");
        }
        break;

        case EX_STRING: {
            CopyString(mString, buffer, max);
		}
        break;

		case EX_LIST: {
            // in theory we should do all of them, just do the first
            // for debugging
			ExValue* el = getList(0);
			if (el != nullptr)
			  el->getString(buffer, max);
			else
			  strcpy(buffer, "");
			break;
		}
		break;
	}
}

void ExValue::setString(const char* src)
{
	mType = EX_STRING;
	// coerce may call us with our own buffer
	if (src != mString)
	  CopyString(src, mString, EX_MAX_STRING);
    releaseList();
}

void ExValue::addString(const char* src)
{
    if (mType != EX_STRING)
      setString(src);

    else if (src != mString) 
      AppendString(src, mString, EX_MAX_STRING);
}

ExValueList* ExValue::getList() 
{
	ExValueList* list = nullptr;
	ExValue* first = nullptr;

	switch (mType) {

		case EX_INT: {
			first = new ExValue();
			first->setInt(mInt);
		}
		break;

		case EX_FLOAT: {
			first = new ExValue();
			first->setFloat(mFloat);
		}
		break;

		case EX_BOOL: {
			first = new ExValue();
			first->setBool(mBool);
		}
		break;

		case EX_STRING: {
			first = new ExValue();
			first->setString(mString);
		}
		break;

		case EX_LIST: {
			list = mList;
		}
		break;
	}

	if (list == nullptr && first != nullptr) {
		// it promotes so we can keep track of it
		list = new ExValueList();
		list->add(first);
        list->setOwner(this);
		mList = list;
		mType = EX_LIST;
	}

	return list;
}

ExValueList* ExValue::takeList()
{
    ExValueList* list = getList();
    if (list != nullptr) {
        if (list->getOwner() != this) {
            // we weren't the owner, I guess this could happen
            // with intermediate ExValues but it's worrisome that
            // there will be a dangling reference somewhere
            printf("WARNING: takeList with someone else's list\n");
            fflush(stdout);
        }
        list->setOwner(nullptr);
        mList = nullptr;
        mType = EX_STRING;
    }
    return list;
}

void ExValue::setList(ExValueList* src)
{
    if (src == nullptr) {
        setNull();
    }
    else {
        // if we had a list already free it
        releaseList();
        mType = EX_LIST;
        mList = src;
    }
}

void ExValue::setOwnedList(ExValueList* src)
{
    if (src != nullptr) {
        if (src->getOwner() != nullptr) {
            printf("WARNING: setOwnedList called with already owned list\n");
            fflush(stdout);
        }
        src->setOwner(this);
    }
    setList(src);
}

void ExValue::set(ExValue* src, bool owned)
{
    setNull();
    if (src != nullptr) {
		ExType otype = src->getType();
		switch (otype) {
			case EX_INT:
				setInt(src->getInt());
				break;

			case EX_FLOAT:
				setFloat(src->getFloat());
				break;

			case EX_BOOL:
				setBool(src->getBool());
				break;

			case EX_STRING:
				setString(src->getString());
				break;

			case EX_LIST:
                if (owned)
                  setOwnedList(src->takeList());
                else
                  setList(src->getList());
				break;
		}
	}
}

/**
 * By default we do not transfer ownership of lists, same as
 * calling setList()
 */
void ExValue::set(ExValue* src)
{
    set(src, false);
}

void ExValue::setOwned(ExValue* src)
{
    set(src, true);
}

/**
 * Coerce a value to a specific type.
 */
void ExValue::coerce(ExType newtype)
{
	if (mType != newtype) {
		switch (newtype) {
			case EX_INT:
				setInt(getInt());
				break;

			case EX_FLOAT:
				setFloat(getFloat());
				break;

			case EX_BOOL:
				setBool(getBool());
				break;

			case EX_STRING:
				setString(getString());
				break;

			case EX_LIST:
				// this coerces and leaves it as a list
				getList();
				break;
		}
	}
}

/**
 * Compare two values, return 1 if they are equal, zero if inequal.
 *
 * If either side is a bool, the other is coerced to bool.
 *
 * If either side is float and the other integer, the other is coerced
 * to float.
 *
 * If either side is a string, and the other not, the string is coerced
 * to the type of the other.
 *
 * Lists aren't comparing right now, don't see a use case.
 */
int ExValue::compare(ExValue* other)
{
	int retval = 0;
	
	if (other == nullptr) {
		// assume we are always larger than nothing, though
		// if we have the empty string, could consider both sides "null"?
		retval = 1;
	}
	else {
		ExType otype = other->getType();

		if (mType == EX_BOOL || otype == EX_BOOL) {
			// always a boolean 
			retval = compareBool(other);
		}
		else {
			switch (mType) {
				case EX_INT: {
					switch (otype) {
						case EX_INT:
							retval = compareInt(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareInt(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
				}
				break;
				case EX_FLOAT: {
					switch (otype) {
						case EX_INT:
							retval = compareFloat(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareFloat(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
				}
				break;
				case EX_STRING: {
					switch (otype) {
						case EX_INT:
							retval = compareInt(other);
							break;
						case EX_FLOAT:
							retval = compareFloat(other);
							break;
						case EX_STRING:
							retval = compareString(other);
							break;
						case EX_BOOL:
						case EX_LIST:
							// not handled
							break;
					}
					break;
				}
				default:
					break;
			}
		}
	}

	return retval;
}

int ExValue::compareInt(ExValue *other)
{
	int retval = 0;
	int myval = getInt();
	int oval = other->getInt();
	
	if (myval > oval)
	  retval = 1;
	else if (myval < oval)
	  retval = -1;

	return retval;
}

int ExValue::compareFloat(ExValue *other)
{
	int retval = 0;
	float myval = getFloat();
	float oval = other->getFloat();
	
	if (myval > oval)
	  retval = 1;
	else if (myval < oval)
	  retval = -1;

	return retval;
}

int ExValue::compareBool(ExValue *other)
{
	int retval = 0;
	bool myval = getBool();
	bool oval = other->getBool();
	
	if (myval && !oval)
	  retval = 1;
	else if (!myval && oval)
	  retval = -1;

	return retval;
}

int ExValue::compareString(ExValue *other)
{
	int retval = 0;
	const char* myval = getString();
	const char* oval = other->getString();

	if (myval == nullptr) {
		if (oval != nullptr)
		  retval = -1;
	}
	else if (oval == nullptr)
	  retval = 1;
	else
	  retval = strcmp(myval, oval);

	return retval;
}

void ExValue::toString(Vbuf* b)
{
    if (mType == EX_LIST) {
        // this is different than getString which is inconsistent and
        // I don't like, think more about toString and getString
        if (mList == nullptr)
          b->add("null");
        else {
            b->add("[");    
            for (int i = 0 ; i < mList->size() ; i++) {
                ExValue* el = mList->getValue(i);
                if (i > 0) b->add(",");
                if (el == nullptr)
                  b->add("null");
                else
                  el->toString(b);
            }
            b->add("]");
        }
    }
    else if (mType == EX_STRING && strlen(mString) == 0) {
        b->add("null");
    }
    else {
        switch (mType) {
            case EX_INT: b->add("i("); break;
            case EX_FLOAT: b->add("f("); break;
            case EX_BOOL: b->add("b("); break;
            case EX_STRING: b->add("s("); break;
            default: b->add("?("); break;
        }

        b->add(getString());
        b->add(")");
    }
}

void ExValue::dump()
{
	Vbuf* b = new Vbuf();
	toString(b);
	printf("%s\n", b->getString());
	delete b;
}

//
// List Maintenance
//

/**
 * This is used in the atomic value methods to return the first
 * list element.  Could expose these for general use but I think
 * it's better to make callers get the list directly.
 */
ExValue* ExValue::getList(int index)
{
	ExValue* value = nullptr;
	if (mList != nullptr)
	  value = mList->getValue(index);
	return value;
}

