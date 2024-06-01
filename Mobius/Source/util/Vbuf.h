/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Yet another dynamic array with chunky resizing and pooling, because
 * the world just needed another.
 *
 * To be replaced with std:: at some point
 *
 */

#ifndef VBUF_H
#define VBUF_H

/****************************************************************************
 * VBUF_INITIAL_SIZE
 *
 * Description: 
 * 
 * Initialize size of the array.
 * In practice use, this should be large enough for the largest
 * blob of pcdata in your XML to reduce growths.
 ****************************************************************************/

#define VBUF_DEFAULT_SIZE 8192
#define VBUF_GROW_SIZE 8192

/****************************************************************************
 * Vbuf
 *
 * Description: 
 * 
 * A class implementing your basic dynamic character array.  Intended for
 * strings, but can put binary stuff in there too.
 * 
 ****************************************************************************/

class Vbuf {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors & accessors
	//
	//////////////////////////////////////////////////////////////////////

	// create a new one
	Vbuf(int initial = 0);

	~Vbuf(void);

	// allocate from pool
	static Vbuf *create(int initial = 0);

	// return to the pool
	void free(void);

	// flush the pool
	static void flushPool(void);

	// initialization for subclasses
	void init(int initial = 0);

	int getSize(void);
	int getLast(void);

	const char *getBuffer(void);
	const char *getString(void);
	char *copyString(void);
	char *stealString(void);

    void clear(void);
	void add(const char *text);
	void add(const char *data, int size);
	void addChar(const char v);
	void add(int v);
	void addXmlAttribute(const char *value);
	void addSqlString(const char *value);
	void prepend(const char *text);

	//////////////////////////////////////////////////////////////////////
	//
	// protected
	//
	//////////////////////////////////////////////////////////////////////

  protected:

	Vbuf		*mNext;			// for pool
	char 		*mBuffer;		// internal buffer we're maintaining
	char		*mEnd;			// one byte past the end of the buffer
	char 		*mPtr;			// current position within the buffer
	int 		mGrow;			// minimum amount to grow

	void grow(int size);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
