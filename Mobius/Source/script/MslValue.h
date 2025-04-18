/**
 * This is the fundamental model for dealing with the values of things in the
 * MSL interpreter.  Examples include: the value of a parameter, the return value
 * of a function, the value of a function argument, the list of all arguments to a function.
 *
 * There are lots of ways to do this, but what makes MSL unusual is the overarching
 * implementation rule that manipulation of data values must not do any dynamic memory
 * allocation.  While this rule can be violated (and I often have in the past) it is
 * generally considered a Very Bad Thing to do in code that needs to run in the audio
 * block processing thread.  This is because dynamic memory allocation consumes an
 * unpredictable amount of time and can have interactions with other system threads
 * that prevent the audio processor from finishing in time to meet the demand to fill
 * the next audio block to send to the audio interface.  You do not want that to happen.
 *
 * While not a hard rule, a lesser goal is to avoid when possible restricties on
 * the sizes of value collections, aka lists or arrays of things.
 *
 * Things like the std:: collection classes, juce::var, juce::Array, and juce::String  are extemely
 * convenient when dealign with things like this but they all have at least the POTENTIAL
 * to do dynamic memory allocation if you are not very careful with them.
 *
 * So what we have here is essentially a model for dealing with atomic values like
 * integers and strings, as well as linked lists of those values, and the fundamnental
 * "cell" in that model can be maintained in a pool so you can create and free them
 * as necessary without needing to use new/delete/malloc/free.  Of course those cells
 * have to come from somewhere, and since we need an unknown number of them they can't
 * be on the stack.  This object pool must therefore be filled using dynamic allocation,
 * but in a thread other than the audio processing thread that does not have the same
 * restrictions.
 *
 * If you're familiar with data structures, you might notice this resembles "s expressions"
 * or "cons cells" in Lisp, which is a way I like to think about things.  It isn't as general
 * as that, but it gets the job done and the languge hides most of the implementation details.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * The fundemtal value containing object.
 * The value may be an "atom" which is one of a few intrinisic data types,
 * or it may contain a "list" of other values.  This is like the "car" in Lisp.
 * Values may also BE on a list, as represented by the "next" pointer.  This
 * is like the "cdr" in Lisp.
 *
 * When dealing with values in MSL at runtime, you are almost always dealing with
 * an atom or a list of atoms.  Lists of lists are rare, but are sometimes necessary
 * temporarily in the intpreter.  There is no syntax to represent an array or list
 * as a data value in MSL, but that may change in the future.
 *
 * For convenience, string values do have a maximum size, but use of string literals
 * is rare in MSL and symbolic references are normally handled with interned Symbols.
 *
 * Enums are a little weird in that they have two values, an integer "ordinal" and
 * a string "name".  This because while most code deals with ordinal numbers, users
 * expect to be dealing with symbolic names, the use of either representation will
 * depend on contxt.
 */
class MslValue
{
  public:
    MslValue();
    ~MslValue();
    void copy(MslValue* src);
    
    static const int MaxString = 1024;

    enum Type {
        Null,
        Int,
        Float,
        Bool,
        String,
        Enum,
        List,
        // a string that used : prefix quoting, can have
        // special meaning when parsing keyword argument lists
        Keyword,
        // may not need this but keep it around
        Symbol
    };

    // The "cdr"
    MslValue* next = nullptr;

    // The "car"
    // sublist maintenance isn't very well controlled through methods
    // revisit this
    MslValue* list = nullptr;
    
    // The "atom"

    Type type = Null;
    
    void setNull() {
        type = Null;
        // not necessary to clear these but looks better in the debugger
        ival = 0;
        fval = 0.0f;
        string[0] = 0;
        // these are more complicated,code that uses pooled values shold be reclaiming
        // these before setting to null
        delete next;
        next = nullptr;
        delete list;
        list = nullptr;
    }

    bool isNull() {
        return (type == Null);
    }

    void setInt(int i) {
        setNull();
        ival = i;
        type = Int;
    }

    void setFloat(float f) {
        setNull();
        fval = f;
        type = Float;
    }

    void setBool(bool b) {
        setNull();
        ival = (int)b;
        type = Bool;
    }

    // sigh, if we have two setString() methods with different signatures
    // it's ambiguous becasue Juce::String has a coersion operator
    // and we don't want to use that at runtime in the engine
    void setJString(juce::String s) {
        setString(s.toUTF8());
    }

    void setString(const char* s) {
        if (s == string) {
            // someone did this v->setString(v->getString())
            // or more likely v->setEnum(v->getString(), ordinal)
            // leave it alone
        }
        else {
            setNull();
            if (s == nullptr) {
                strcpy(string, "");
            }
            else {
                strncpy(string, s, sizeof(string));
                if (strlen(string) > 0)
                  type = String;
            }
        }
    }
    
    void setKeyword(const char* s) {
        setString(s);
        type = Keyword;
    }

    // true if this is a String or Keyword that can be treated as one
    // simplifies some evaluation logic
    bool isStringy() {
        return (type == String || type == Keyword);
    }

    void setEnum(const char* s, int i) {
        setString(s);
        ival = i;
        type = Enum;
    }

    // hack to fix enumerations where the name is right but the
    // number is wrong to avoid repeated logging every time this is encountered
    void fixEnum(int i) {
        if (type == Enum) 
          ival = i;
    }

    const char* getString() {
        const char* result = nullptr;
        if (type != Null) {
            if (type == Int) {
                snprintf(string, sizeof(string), "%d", ival);
            }
            else if (type == Float) {
                snprintf(string, sizeof(string), "%f", fval);
            }
            else if (type == Bool) {
                snprintf(string, sizeof(string),
                         (ival > 0) ? "true" : "false");
            }
            result = string;
        }
        return result;
    }

    int getInt() {
        int result = 0;
        if (type == Int || type == Bool || type == Enum) {
            result = ival;
        }
        else if (type == Float) {
            result = (int)fval;
        }
        else if (type != Null) {
            result = atoi(string);
        }
        return result;
    }

    float getFloat() {
        float result = 0.0f;
        // punt, we don't use this yet
        return result;
    }

    bool getBool() {
        // this one will be weird if you try to use it on an Enum
        // since zero is a valid ordinal
        return (getInt() > 0);
    }

    //
    // List utilities
    // Ambiguity over what we're dealing with here, the list the value is ON
    // or the list the value HAS, don't like it
    // Starting think a concrete MslValueList container would be better
    // 

    void append(MslValue* v);
    int size();
    MslValue* get(int index);
    MslValue* getLast();

  private:

    // keep these private to enforce use of the methods to keep
    // type Type in sync with the value
    // by convention once "list" becomes non-null Type is implicitly List
    // though those should be kept in sync
    // "bool" is just 0 or 1
    
    int ival = 0;
    float fval = 0.0f;
    char string[MaxString];
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
    
