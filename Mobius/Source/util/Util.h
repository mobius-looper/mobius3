/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Yet another collection of utilities.
 * 
 * This is a mixture of things pulled from several sources that are
 * potentially of general use and not specific to Mobius.  A few like ERR_BASE
 * are though and need thought.
 *
 * Things like string handling grew out of the early days before STL was
 * widespared and should eventually be replaced with std: classes.
 *
 * Most things in here should be considered temporary until better or more
 * modern ways are found.
 * 
 */

#pragma once

//////////////////////////////////////////////////////////////////////
//
// Simple memory allocation tracker
//
//////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////
//
// Pointer Arithmetic
//
// Surely there is something better by now
//
//////////////////////////////////////////////////////////////////////

/**
 * PTRDIFF
 *
 * The usual macro for warning-free pointer differencing.
 * Note that this always returns a difference in bytes/chars so it suitable
 * only for string manipulation.
 *
 * Historially downcast this to an int before ptrdiff_t was introduced.
 * It would be better to start using that.  For our purposes this
 * should be safe since we're never dealing with differences that large.
 */

#define PTRDIFF(start, end) (int)((char*)end - (char*)start)

//////////////////////////////////////////////////////////////////////
//
// String utilities
//
// Almost all of this can be replaced with std::string
//
//////////////////////////////////////////////////////////////////////

void CopyString(const char* src, char* dest, int max);
char* CopyString(const char *src);
char* CopyString(const char* src, int len);
void CopyString(const char* src, int srcchars, char* dest, int max);
void AppendString(const char* src, char* dest, int max);

bool StringEqual(const char* s1, const char* s2);
bool StringEqualNoCase(const char* s1, const char* s2);
bool StringEqualNoCase(const char* s1, const char* s2, int max);
int LastIndexOf(const char* str, const char* substr);

bool StartsWith(const char* str, const char* prefix);
bool StartsWithNoCase(const char* str, const char* prefix);
bool EndsWith(const char* str, const char* suffix);
bool EndsWithNoCase(const char* str, const char* suffix);

void GetLeafName(const char* path, char* buffer, bool extension);

#define MAX_NUMBER_TOKEN 128
int ParseNumberString(const char* src, int* numbers, int max);

bool IsInteger(const char* str);
int ToInt(const char* str);

int Random(int low, int high);

// sigh, Random() already used on Mac in Quickdraw.h
float RandomFloat();

int IndexOf(const char* str, const char* substr);
int IndexOf(const char* str, const char* substr, int start);

// started using in BindingResolver, probably don't need all of them
int ScaleValueIn(float value, int min, int max);
float ScaleValueOut(int value, int min, int max);
int Scale128ValueIn(int value, int min, int max);
int ScaleValue(int value, int inmin, int inmax, int outmin, int outmax);

char* TrimString(char* src);

//////////////////////////////////////////////////////////////////////
//
// File Utilities
//
// Used to have a lot more of these, but if you need any start using
// Juce
//
//////////////////////////////////////////////////////////////////////

bool IsAbsolute(const char* path);
bool IsFile(const char *name);


//////////////////////////////////////////////////////////////////////
//
// Exceptions
//
// I forget the origin of this, it seems to be a object that can be
// included with an exception to contain more information about what
// happened rather than just a code.
//
// See if the library has something for this by now
//
//////////////////////////////////////////////////////////////////////

/**
 * ERR_BASE_*
 *
 * Base numbers for ranges of error codes used by Mobius modules.
 */

#define ERR_BASE			20000

#define ERR_BASE_GENERAL	ERR_BASE 
#define ERR_BASE_XMLP		ERR_BASE + 100

#define ERR_MEMORY 			ERR_BASE_GENERAL + 1
#define ERR_GENERIC			ERR_BASE_GENERAL + 2

/**
 * A convenient exception class containing a message and/or error code.
 */
class AppException {

  public:
	
	AppException(AppException &src);

	AppException(const char *msg, bool nocopy = false);

	AppException(int c, const char *msg = 0, bool nocopy = false);

	~AppException(void);

	int getCode(void) {
		return mCode;
	}

	const char *getMessage(void) {
		return mMessage;
	}

	char *stealMessage(void) {
		char *s = mMessage;
		mMessage = nullptr;
		return s;
	}

	// for debugging convience, senes a message to the 
	// console and debug stream
	void print(void);

  private:

	int 	mCode;
	char 	*mMessage;

};





