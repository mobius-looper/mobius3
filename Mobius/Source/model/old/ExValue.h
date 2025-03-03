/*
 * Container of variably typed values
 * Factored out of the Expr expression evaluator
 * eventually start using std:: for this or maybe Juce
 */

#pragma once

#include "../../util/List.h"

/****************************************************************************
 *                                                                          *
 *   								VALUE                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * The maximum length of a string value returned by an expression node.
 * This can be used for paths so it needs to be healthy.
 * Originally this was 2K but I started using them embedded in Action
 * and that was way too large.  Paths are only used for testing, so just
 * be sure to test with short paths.
 */
#define EX_MAX_STRING 128

/**
 * An enumeration of the types of values we may hold in an ExValue.
 */
typedef enum {

    EX_INT,
	EX_FLOAT,
	EX_BOOL,
	EX_STRING,
	EX_LIST

} ExType;

/**
 * Expressions generate values.
 * String values have an upper bound so we don't have to deal with
 * dynamic allocation during evaluation.
 */
class ExValue {

  public:
	
	ExValue();
	~ExValue();

	ExType getType();
	void setType(ExType t);
	void coerce(ExType newtype);
	
	void setNull();
	bool isNull();

	int getInt();
	void setInt(int i);

	long getLong();
	void setLong(long i);
	
	float getFloat();
	void setFloat(float f);

	bool getBool();
	void setBool(bool b);

	const char* getString();
	void getString(char *buffer, int max);
	void setString(const char* src);
    void addString(const char* src);

	class ExValueList* getList();
	class ExValueList* takeList();
	void setList(class ExValueList* l);
	void setOwnedList(class ExValueList* l);

	char* getBuffer();
	int getBufferMax();

	int compare(ExValue* other);
    void set(ExValue* src, bool owned);
	void set(ExValue* other);
	void setOwned(ExValue* other);

	void toString(class Vbuf* b);
	void dump();

  private:
    
    void releaseList();
	void copyList(class ExValueList* src, class ExValueList* dest);
	ExValue* getList(int index);

	int compareInt(ExValue *other);
	int compareFloat(ExValue *other);
	int compareBool(ExValue *other);
	int compareString(ExValue *other);

	ExType mType;
	int mInt;
	float mFloat;
	bool mBool;
	char mString[EX_MAX_STRING + 4];
	class ExValueList* mList;
};

//////////////////////////////////////////////////////////////////////
//
// ExValueList
//
//////////////////////////////////////////////////////////////////////

/**
 * A list of ExValues.
 *
 * These are a little weird because we don't have formal support
 * in the scripting languge for "pass by value" or "pass by reference".
 * Most things are pass by value, each ExValue has it's own char buffer
 * for strings.  But lists are more complicated, we generally want to 
 * use pass by reference so the reciever can modify the list.
 *
 * I don't like reference counting so we're going to try a marginally
 * more stable notion of a list "owner".  
 * 
 * The rules for referencing ExValueList in an ExValue
 *
 *   - setting a list in an ExValue sets the reference it does not copy
 * 
 *   - returning a list in a caller supplied ExValue returns a reference
 *     to the list, not a copy
 *
 *   - when an ExValue contains an ExValueList and the ExValue is deleted,
 *     the ExValueList is deleted only if the owner pointer in the
 *     ExValueList is equal to the ExValue address
 *
 *   - to transfer a list from one ExValue to another you can either
 *     use the copy() method or use takeList() that returns the list
 *     clears the owner pointer, and removes the reference from the 
 *     original ExValue
 * 
 * The rules for ExValueList elements are:
 *
 *   - deleting the list deletes the ExValues in it
 *   - adding or setting an ExValue takes ownership of the ExValue
 *   - if an ExValue in a list contains an ExValueList, the contained
 *     list is deleted when the parent list is deleted, 
 *     this means the contained ExValueList will have it's owner set
 *
 */
class ExValueList : public List {

  public:

	ExValueList();
	~ExValueList();

    // List overloads
    void deleteElement(void* o);
    void* copyElement(void* src);

    ExValue* getValue(int i);
    void* getOwner();
    void setOwner(void* v);

    ExValueList* copy();

  protected:

    void* mOwner;

};
