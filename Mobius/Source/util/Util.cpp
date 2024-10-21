/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Yet another collection of utilities.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
// for stat() on Mac but oddly not required on Windows
// should be using Juce anyway
#include <sys/stat.h>

// Scale functions want to Trace
#include "Trace.h"

#include "Util.h"

//////////////////////////////////////////////////////////////////////
//
// String utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * This little bit of folderol is because isspace(*ptr) will throw
 * a fucking microsoft assertion if ptr was "char *" and the character
 * is corrupted or Unicode or something with the high bit set.  It does
 * sign extension to make it negative since the isspace argument is an "int".
 * And this fails a >-1 < 255 debug assertion.
 *
 * Saw this parsing some old Mobius script files that got corrupted along the way.
 * Unclear if this is because the debug runtime was being used, because some builds
 * didn't have this problem.
 *
 * Whatever the reason, do this stupid cast so it doesn't sign extend and returns
 * a goddamn false so we can get on with our lives without the application halting.
 */
bool IsSpace(int ch)
{
    return isspace(static_cast<unsigned char>(ch)) != 0;
}

bool IsPrint(int ch)
{
    return isprint(static_cast<unsigned char>(ch)) != 0;
}

/**
 * Copy a string to a point.
 */
char* CopyString(const char* src, int len)
{
	char* copy = NULL;
	if (src != NULL && len > 0) {
		int srclen = (int)strlen(src);
		if (len <= srclen) {
			copy = new char[len + 1];
			if (copy != NULL) {
				strncpy(copy, src, len);
				copy[len] = 0;
			}
		}
	}
	return copy;
}

/**
 * Copy one string to a buffer with care.
 * The max argument is assumed to be the maximum number
 * of char elements in the dest array *including* the nul terminator.
 *
 * TODO: Some of the applications of this would like us to favor
 * the end rather than the start.
 */
void CopyString(const char* src, char* dest, int max)
{
	if (dest != nullptr && max > 0) {
		if (src == nullptr) 
		  strcpy(dest, "");
		else {
			int len = (int)strlen(src);
			int avail = max - 1;
			if (avail > len)
			  strcpy(dest, src);
			else {
				strncpy(dest, src, avail);
				dest[avail] = 0;
			}
		}
	}
}

/**
 * Copy one string to a buffer with care.
 * This one has limits on both the source and destination arrays,
 * typically used when tokenizing strings like CSVs without modifying them
 * to insert zero terminators.
 * Is there a library function for this??
 */
void CopyString(const char* src, int srcchars, char* dest, int max)
{
	if (dest != nullptr && max > 0) {
		if (src == nullptr)  {
            strcpy(dest, "");
        }
		else {
			int len = srcchars;
			int avail = max - 1;
			if (avail > len) {
                strncpy(dest, src, srcchars);
                dest[srcchars] = 0;
            }
			else {
                strncpy(dest, src, avail);
                dest[avail] = 0;
            }
		}
	}
}

/**
 * CopyString
 *
 * Return a copy of a string allocated with new.
 * Returns nullptr if nullptr is passed.
 * 
 * strdup() doesn't handle nullptr input pointers, and the result is supposed
 * to be freed with free() not the delete operator.  Many compilers don't
 * make a distinction between free() and delete, but that is not guarenteed.
 */
char* CopyString(const char *src)
{
	char *copy = nullptr;
	if (src) {
		copy = new char[strlen(src) + 1];
		if (copy)
		  strcpy(copy, src);
	}
	return copy;
}

void AppendString(const char* src, char* dest, int max)
{
    if (src != NULL) {
        int current = (int)strlen(dest);
        int neu = (int)strlen(src);
        int avail = max - 1;
        if (avail > current + neu)
          strcat(dest, src);
    }
}

int LastIndexOf(const char* str, const char* substr)
{
	int index = -1;

	// not a very smart search
	if (str != NULL && substr != NULL) {
		int len = (int)strlen(str);
		int sublen = (int)strlen(substr);
		int psn = len - sublen;
		if (psn >= 0) {
			while (psn >= 0 && strncmp(&str[psn], substr, sublen))
			  psn--;
			index = psn;
		}
	}
	return index;
}

/**
 * Case insensitive string comparison.
 * Return true if the strings are equal.
 */
bool StringEqualNoCase(const char* s1, const char* s2)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL) {
		int len = (int)strlen(s1);
		int len2 = (int)strlen(s2);
		if (len == len2) {
			equal = true;
			for (int i = 0 ; i < len ; i++) {
				char ch = (char)tolower(s1[i]);
				char ch2 = (char)tolower(s2[i]);
				if (ch != ch2) {
					equal = false;
					break;
				}
			}
		}

	}
	return equal;
}

/**
 * String comparison handling nulls.
 */
bool StringEqual(const char* s1, const char* s2)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL)
	  equal = !strcmp(s1, s2);

	return equal;
}

bool StringEqualNoCase(const char* s1, const char* s2, int max)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL) {
		int len = (int)strlen(s1);
		int len2 = (int)strlen(s2);
        if (len >= max && len2 >= max) {
			equal = true;
			for (int i = 0 ; i < max ; i++) {
				char ch = (char)tolower(s1[i]);
				char ch2 = (char)tolower(s2[i]);
				if (ch != ch2) {
					equal = false;
					break;
				}
			}
		}

	}
	return equal;
}

/**
 * Given a string of numbers, either whitespace or comma delimited, 
 * parse it and build an array of ints.  Return the number of ints
 * parsed.
 */
int ParseNumberString(const char* src, int* numbers, int max)
{
	char buffer[MAX_NUMBER_TOKEN + 1];
	const char* ptr = src;
	int parsed = 0;

	if (src != NULL) {
		while (*ptr != 0 && parsed < max) {
			// skip whitespace
			while (*ptr != 0 && IsSpace(*ptr) && !(*ptr == ',')) ptr++;

			// isolate the number
			char* psn = buffer;
			for (int i = 0 ; *ptr != 0 && i < MAX_NUMBER_TOKEN ; i++) {
				if (IsSpace(*ptr) || *ptr == ',') {
					ptr++;
					break;
				}
				else
				  *psn++ = *ptr++;
			}
			*psn = 0;

			if (strlen(buffer) > 0) {
				int ival = atoi(buffer);
				if (numbers != NULL)
				  numbers[parsed] = ival;
				parsed++;
			}
		}
	}

	return parsed;
}

bool StartsWith(const char* str, const char* prefix)
{
	bool startsWith = false;
	if (str != NULL && prefix != NULL)
      startsWith = !strncmp(str, prefix, strlen(prefix));
    return startsWith;
}

bool StartsWithNoCase(const char* str, const char* prefix)
{
	bool startsWith = false;
	if (str != NULL && prefix != NULL)
      startsWith = StringEqualNoCase(str, prefix, (int)strlen(prefix));
	return startsWith;
}

bool EndsWith(const char* str, const char* suffix)
{
	bool endsWith = false;
	if (str != NULL && suffix != NULL) {
		int len1 = (int)strlen(str);
		int len2 = (int)strlen(suffix);
		if (len1 > len2)
		  endsWith = !strcmp(suffix, &str[len1 - len2]);
	}
	return endsWith;
}

bool EndsWithNoCase(const char* str, const char* suffix)
{
	bool endsWith = false;
	if (str != NULL && suffix != NULL) {
		int len1 = (int)strlen(str);
		int len2 = (int)strlen(suffix);
		if (len1 > len2)
		  endsWith = StringEqualNoCase(suffix, &str[len1 - len2]);
	}
	return endsWith;
}

/**
 * Necessary because atoi() doesn't accept NULL arguments.
 */
int ToInt(const char* str)
{
	int value = 0;
	if (str != NULL)
	  value = atoi(str);
	return value;
}

/**
 * Return true if the string looks like a signed integer.
 */
bool IsInteger(const char* str)
{
    bool is = false;
    if (str != NULL) {
        int max = (int)strlen(str);
        if (max > 0) {
            is = true;
            for (int i = 0 ; i < max && is ; i++) {
                char ch = str[i];
                if (!isdigit(ch) && (i > 0 || ch != '-'))
                  is = false;
            }
        }
    }
    return is;
}

void GetLeafName(const char* path, char* buffer, bool extension)
{
	int last = (int)strlen(path) - 1;
	int dot = -1;
	int psn = last;

	while (psn > 0 && path[psn] != '/' && path[psn] != '\\') {
		if (path[psn] == '.')
		  dot = psn;
		psn--;
	}

	if (psn < 0) {
		// looked like a simple file name, no change
		psn = 0;
	}
	else
	  psn++;

	if (!extension && dot > 0)
	  last = dot - 1;

	int len = last - psn + 1;

	strncpy(buffer, &path[psn], len);
	buffer[len] = 0;
}

int IndexOf(const char* str, const char* substr)
{
    return IndexOf(str, substr, 0);
}

int IndexOf(const char* str, const char* substr, int start)
{
    // !! start unreferenced, must not have ever worked?
    (void)start;
    
	int index = -1;

	// not a very smart search
	if (str != NULL && substr != NULL) {
		int len = (int)strlen(str);
		int sublen = (int)strlen(substr);
        int max = len - sublen;
        if (sublen > 0 && max >= 0) {
            for (int i = 0 ; i <= max ; i++) {
                if (strncmp(&str[i], substr, sublen) == 0) {
                    index = i;
                    break;
                }
            }
        }
	}
	return index;
}

//////////////////////////////////////////////////////////////////////
//
// File utilities
//
//////////////////////////////////////////////////////////////////////

bool IsFile(const char *name)
{
	bool exists = false;
	struct stat sb;
	
	if (stat(name, &sb) == 0) {
		if (sb.st_mode & S_IFREG)
		  exists = true;
	}
	return exists;
}

/**
 * Return true if the path is an absolute path.
 * Not sure how accurate this was, but it got the job done.
 * Juce is far better at this sort of thing.
 */
bool IsAbsolute(const char* path)
{
    bool absolute = false;
    if (path != NULL) {
        int len = (int)strlen(path);
        if (len > 0) {
            absolute = (path[0] == '/' || 
                        path[0] == '\\' ||
						// actually this is meaningless, it's a shell thing
                        //path[0] == '~' ||
                        (len > 2 && path[1] == ':'));
        }
    }
    return absolute;
}

//////////////////////////////////////////////////////////////////////
//
// Exceptions
//
// This was originally used by the XML parser, not sure how much it
// has grown since then.
//
// Note that since this does dynamic allocation it is unsuitable for
// use in the audio interrupt.  Need to replace this with something better
// from the standard library.
//
//////////////////////////////////////////////////////////////////////

AppException::AppException(const char *msg, bool nocopy)
{
	AppException(ERR_GENERIC, msg, nocopy);
}

AppException::AppException(int c, const char *msg, bool nocopy)
{
	mCode = c;
	if (nocopy)
	  mMessage = (char *)msg;
	else if (msg == nullptr) 
	  mMessage = nullptr;
	else {
		// if this throws, then I guess its more important 
		mMessage = new char[strlen(msg) + 1];
		strcpy(mMessage, msg);
	}	
}

/** 
 * copy constructor, important for this exceptions since
 * we want to pass ownership of the string
 */
AppException::AppException(AppException &src) 
{
	mCode = src.mCode;
	mMessage = src.mMessage;
	src.mMessage = nullptr;
}

AppException::~AppException(void)
{
	delete mMessage;
}

void AppException::print(void)
{
	if (mMessage)
	  printf("ERROR %d : %s\n", mCode, mMessage);
	else
	  printf("ERROR %d\n", mCode);
}

//////////////////////////////////////////////////////

static bool RandomSeeded = false;

/**
 * Generate a random number between the two values, inclusive.
 */
int Random(int min, int max)
{
	// !! potential csect issues here
	if (!RandomSeeded) {
		// passing 1 "reinitializes the generator", passing any other number
		// "sets the generator to a random starting point"
		// Unclear how the seed affects the starting point, probably should
		// be based on something, maybe pass in the layer size?
		srand(2);
		RandomSeeded = true;
	}

	int range = max - min + 1;
	int value = (rand() % range) + min;

	return value;
}

float RandomFloat()
{
	if (!RandomSeeded) {
		srand(2);
		RandomSeeded = true;
	}

	return (float)rand() / (float)RAND_MAX;
}

/****************************************************************************
 *                                                                          *
 *                                  SCALING                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Convert a floating point nubmer from 0.0 to 1.0 into an integer
 * within the specified range.
 *
 * This can be used to scale both OSC arguments and VST host parameter
 * values into scaled integers for Mobius parameters and controls.
 *
 * VstMobius notes
 * 
 * On the way in values are quantized to the beginning of their chunk
 * like ScaleValueOut, but when the value reaches 1.0 we'll be
 * at the beginning of the chunk beyond the last one so we have
 * to limit it.
 *
 * HACK: For parameters that have chunk sizes that are repeating
 * fractions we have to be careful about rounding down.  Example
 * trackCount=6 so selectedTrack has a chunk size of .16666667
 * Track 3 (base 0) scales out to .5 since the begging of the chunk
 * is exactly in the middle of the range.  When we try to apply
 * that value here, .5 / .16666667 results in 2.99999 which rounds
 * down to 2 instead of 3.
 *
 * There are proably several ways to handle this, could add a little
 * extra to ScaleParameterOut to be sure we'll cross the boundary
 * when scaling back.  Here we'll check to see if the beginning of
 * the chunk after the one we calculate here is equal to the
 * starting value and if so bump to the next chunk.
 * 
 */
int ScaleValueIn(float value, int min, int max)
{
    int ivalue = 0;
    int range = max - min + 1;

    if (range > 0) {
        float chunk = (1.0f / (float)range);
        ivalue = (int)(value / chunk);
        
        // check round down
        float next = (ivalue + 1) * chunk;
        if (next <= value)
          ivalue++;

        // add in min and constraint range
        ivalue += min;
        if (ivalue > max) 
          ivalue = max; // must be at 1.0
    }

    return ivalue;
}

/**
 * Scale a value within a range to a float from 0.0 to 1.0.
 * VstMobius notes
 * 
 * On the way out, the float values will be quantized
 * to the beginning of their "chunk".  This makes zero
 * align with the left edge, but makes the max value slightly
 * less than the right edge.
 */
float ScaleValueOut(int value, int min, int max)
{
    float fvalue = 0.0;

    int range = max - min + 1;
    float chunk = 1.0f / (float)range;

    int base = value - min;
    fvalue = chunk * base;

    return fvalue;
}

/**
 * Scale an integer from 0 to 127 into a smaller numeric range.
 */
int Scale128ValueIn(int value, int min, int max)
{
    int scaled = 0;
    
    if (value < 0 || value > 127) {
        Trace(1, "Invalid value at Scale128ValueIn %ld\n", (long)value);
    }
    else if (min == 0 && max == 127) {
        // don't round it
        scaled = value;
    }
    else {
        int range = max - min + 1;
        if (range > 0) {
            float chunk = 128.0f / (float)range;
            scaled = (int)((float)value / chunk);

            // check round down
            float next = (scaled + 1) * chunk;
            if (next <= value)
              scaled++;

            // add in min and constraint range
            scaled += min;
            if (scaled > max) 
              scaled = max;
        }
    }

    return scaled;
}

/**
 * Scale a value from one range to another.
 */
int ScaleValue(int value, int inmin, int inmax, int outmin, int outmax)
{
    int scaled = 0;
    
    if (value < inmin || value > inmax) {
        Trace(1, "ScaleValue out of range %ld\n", (long)value);
    }
    else if (inmin == outmin && inmax == outmax) {
        // don't round it
        scaled = value;
    }
    else {
        int inrange = inmax - inmin;
        int outrange = outmax - outmin;
        
        if (inrange == 0 || outrange == 0) {
            // shouldn't see this on outrange but
            // some Mobius states can be empty
            // avoid div by zero
        }
        else {
            float fraction = (float)value / (float)inrange;
            scaled = outmin + (int)(fraction * (float)outrange);
        }
    }

    return scaled;
}

/**
 * Trim leading and trailing whitespace from a string buffer.
 * Note that this MUST NOT be a static string, we modify the 
 * buffer when trimming on the right.
 */
char* TrimString(char* src)
{
	char* start = src;

	if (start != NULL) {
		// skip preceeding whitespace
		while (*start && IsSpace(*start)) start++;

		// remove trailing whitespace
		int last = (int)strlen(start) - 1;
		while (last >= 0 && IsSpace(start[last])) {
			start[last] = '\0';
			last--;
		}
	}

	return start;
}
