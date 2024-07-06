
#pragma once

#include "MslModel.h"

/**
 * Value container easilly stack allocated and guaranteed never
 * to do memory allocation.  juce::String is not allowed in an evaluation context.
 */
class MslValue
{
  public:
    MslValue() {}
    ~MslValue() {}

    static const int MaxString = 1024;

    enum Type {
        Null,
        Int,
        Float,
        Bool,
        String,
        Error
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
        if (type == Int || type == Bool) {
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
 * Object that provides access to the world outside the evaluator.
 */
class MslContext
{
  public:
    virtual ~MslContext() {}

    // todo: what about error handling, I guess could use MslValue
    virtual void mslInvoke(class Symbol* s, MslValue& result) = 0;
    virtual void mslQuery(class Symbol* s, MslValue& result) = 0;

};

/**
 * This should live inside MslParser and it should do the work.
 */
enum MslOperators {
    
    MslUnknown,
    MslPlus,
    MslMinus,
    MslMult,
    MslDiv,
    MslEq,
    MslDeq,
    MslNeq,
    MslGt,
    MslGte,
    MslLt,
    MslLte,
    MslNot,
    MslAnd,
    MslOr,
    MslAmp
};

class MslEvaluator : public MslVisitor
{
  public:

    MslEvaluator() {}
    MslEvaluator(MslContext* c) {context = c;}
    ~MslEvaluator() {}

    bool trace = false;
    
    MslValue start(class MslNode* node);

    // todo: error accululation and presentation can't use memory
    // allocation so will need to work on this
    juce::StringArray* getErrors();

    // evaluation visitor targets

    void mslVisit(MslLiteral* node);
    void mslVisit(MslSymbol* node);
    void mslVisit(MslBlock* node);
    void mslVisit(MslOperator* node);
    void mslVisit(MslAssignment* node);
    void mslVisit(MslVar* node);
    void mslVisit(MslProc* node);
    
  private:

    MslContext* context = nullptr;
    MslValue result;
    juce::StringArray errors;
    
    void eval(class Symbol* s);
    void invoke(Symbol* s);
    void query(Symbol* s);
    void assign(Symbol* s, int value);

    MslOperators mapOperator(juce::String& s);

    int evalInt(MslNode* node);
    bool evalBool(MslNode* node);
    void compare(MslNode* node1, MslNode* node2, bool equal);
    bool isString(MslNode* node);
    void compareSymbol(MslNode* node1, MslNode* node2, bool equal);

    MslSymbol* getResolvedParameter(MslNode* node1, MslNode* node2);
    MslSymbol* getResolvedParameter(MslNode* node);
    MslNode* getUnresolved(MslNode* node1, MslNode* node2);
    MslNode* getUnresolved(MslNode* node);
    
    void resolve(MslSymbol* node);
};
