 /**
 * Value container easilly stack allocated and guaranteed never
 * to do memory allocation.  juce::String is not allowed in an evaluation context.
 *
 * With the introduction of MslSession I needed to maintain lists as values.
 * To get things fleshed out, I'm using juce::Array for this which DOES allocate
 * memmory, but that can be addressed later.
 */

#pragma once

class MslValue
{
  public:
    MslValue() {setNull();}
    ~MslValue() {}

    static const int MaxString = 1024;

    enum Type {
        Null,
        Int,
        Float,
        Bool,
        String,
        Error,
        Enum
    };

    Type type = Null;

    void setNull() {
        type = Null;
        // not necessary but looks better in the debugger
        ival = 0;
        fval = 0.0f;
        string[0] = 0;
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

    // sigh, if we have both names with the same sig
    // it's ambiguous becasue Juce::String has a coersion operator
    // and we don't want to use that at runtime in the engine
    void setJString(juce::String s) {
        setString(s.toUTF8());
    }

    void setString(const char* s) {
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

    void setEnum(const char* s, int i) {
        setString(s);
        ival = i;
        type = Enum;
    }

    void setError(const char* s) {
        setString(s);
        type = Error;
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

  private:

    int ival = 0;
    float fval = 0.0f;
    char string[MaxString];
};

/**
 * Runtime evaluation results are usually atomic values and lists
 * of atomics, but sometimes they are lists of lists.
 *
 * Easiest way to represt that is a tree though the notion
 * that each node can have both a value and sub-values won't happen.
 * Dust off the old notes and look at how Lisp did this, feels
 * like a "cons" could be used here.
 */
class MslValueTree
{
  public:

    MslValueTree() {}
    ~MslValueTree() {}

    MslValue value;
    juce::OwnedArray<MslValueTree> list;

    void add(MslValue& v) {
        MslValueTree* t = new MslValueTree();
        t->value = v;
        list.add(t);
    }

    void add(MslValueTree* l) {
        list.add(l);
    }

};


    

    
