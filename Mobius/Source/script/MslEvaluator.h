/**
 * todo: think about the distinction between MslSession and MslEvaluator
 * they are almost the same thing.  The only thing evaluator really does is
 * provide the visitor interface and the logic for each stateement evaluation.
 *
 * but the call stack, error list and other evaluation artificats are held in the session
 * is this distinction useful?
 */
  
#pragma once

#include "MslModel.h"
#include "MslValue.h"

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

    MslEvaluator(class MslSession* s);
    ~MslEvaluator();

    bool trace = false;
    
    MslValue start(class MslNode* node);

    // MslVisitor targets

    void mslVisit(MslLiteral* node);
    void mslVisit(MslSymbol* node);
    void mslVisit(MslBlock* node);
    void mslVisit(MslOperator* node);
    void mslVisit(MslAssignment* node);
    void mslVisit(MslVar* node);
    void mslVisit(MslProc* node);
    void mslVisit(MslIf* node);
    void mslVisit(MslElse* node);
    
  private:
    
    class MslSession* session = nullptr;
    MslValue result;

    Symbol* resolve(MslSymbol* node);
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
    
};
