/**
 * Simple utility function to print the sizes of fundamental types.
 * Used only during initial porting.
 *
 * https://en.cppreference.com/w/cpp/language/types
 *
 * On 64-bit systems, there are two models in wide acceptance:
 *
 * LLP64 or 4/4/8
 *
 *    int and long are 32, pointer is 64
 *    used by Win32 with compilation target x64
 *
 * LP64 or 4/8/8
 *
 *    int is 32, long and pointer are 64
 *    used by Unix-like and macOS
 *
 * Integer types can have modifiersK
 *    short - target type optimized for space and has at least 16
 *    long  - target type will have at least 64
 *
 * Reading this to mean that "long" is always 32 in LLP64
 * and "long int" is at least 32, maybe 64. "long long" is at least 64.
 *
 * For my purposes, int is almost always enough for calculations and indexing.
 * Code dealing with buffer indexing (frame offsets) often used long, though
 * this was not necessary since Win32 and macOS both used ILP32 4/4/4
 *
 * There was no code that required 64 bit integers or floats.
 *
 * Pointers are the main issue.  There were a small number of cases that
 * assumed you could cast a pointer to an int to service as a unique identifier.
 * These will break.
 *
 * The annoying one is 16 bit, which I think was only used by WaveFile.
 * The C++ standard says "short int" is "at least 16, but all four common
 * data models use 16 bits so "short int" should be okay for the few cases
 * where it is needed.
 *
 * Assuming we're only targeting Win32, Win32x64, and macOS the following
 * types have stable sizes
 *
 *      short, short int        16
 *      int                     32
 *      long long               64
 *
 * Just "long" is unstable, it is 32 on Windows and 64 on Mac
 * Pointers should be always assumed 64
 *
 * sizeof(bool) is implementation defined and may not be 1
 *
 * char is unclear, standard says sizeof(char) == 1 but also says
 * "which can be most efficiently processed".
 *
 * I don't think I do much char array processing and when I do it doesn't
 * matter how wide it is.  Only that an int is big enough for a char
 * and if you cast an int to a char the expectation is that it is limited to
 * 8 bits.
 *
 * float and double are less clear in the document.  It says float is
 * "single precision floating-point type" and "Usually IEEE-754 binary32".
 * And double is "Usually IEEE-754 binary64".
 *
 * For Windows and Mac, I think we can assume float is 32 bits and double is 64
 * but this should be verified.
 *
 * I have always dealt with float with an expectation of 32 bits, so when interfacing
 * with other things (notably Juce AudioBuffer) you have to expect that they
 * may be using double.
 */

// use trace() so we can see the results in VisualStudio output pane
#include "Trace.h"

// some macros used in old code
#define myuint32 unsigned long
#define myuint16 unsigned short
#define myint16 short

/**
 * Trace the sizes of my sized integer macros
 * Only for initial port testing.  Trace a few others
 * while we're here.
 *
 * Results under 64-bit Windows (Thor)
 *  Sizes: short 2 int 4 long 4 long long 8 float 4 double 8
 *     myuint32 4 myuint16 2 myint16 2
 */
void DataModelDump()
{
    short      s = 0;
    int        i = 0;
    long       l = 0;
    long long  ll = 0;
    float      f = 0.0f;
    double     d = 0.0f;

    myuint32 myu32 = 0;
    myuint16 myu16 = 0;
    myint16  my16  = 0;
    
    trace("Sizes: short %d int %d long %d long long %d float %d double %d\n",
          sizeof(s), sizeof(i), sizeof(l), sizeof(ll), sizeof(f), sizeof(d));

    trace("  myuint32 %d myuint16 %d myint16 %d\n",
          sizeof(myu32), sizeof(myu16), sizeof(my16));

    trace("  char %d char* %d float* %d\n", sizeof(char), sizeof(char*), sizeof(&f));
}
