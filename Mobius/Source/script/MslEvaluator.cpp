#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/Symbol.h"
#include "../model/UIParameter.h"

#include "MslModel.h"
#include "MslSession.h"
#include "MslEvaluator.h"

MslValue MslEvaluator::start(MslSession* s, MslNode* node)
{
    session = s;
    
    // initialize stacks etc...
    errors.clear();
    result.setNull();

    // will need to work out suspend, and deferred results
    node->visit(this);

    // check errors etc...

    return result;
}

juce::StringArray* MslEvaluator::getErrors()
{
    return &errors;
}

//////////////////////////////////////////////////////////////////////
//
// MslNode eval targets
//
//////////////////////////////////////////////////////////////////////

void MslEvaluator::mslVisit(MslLiteral* lit)
{
    // todo, need to be more type aware of this literal
    // will need a result stack eventually too
    result.setJString(lit->token);
}

void MslEvaluator::mslVisit(MslBlock* block)
{
    for (auto child : block->children) {
        child->visit(this);
    }
}

/**
 * Now it gets interesting.
 * If we had a linker could have decorated the node with
 * resolved things.
 *
 * Punt up to the Session for all symbol handling until this stabilizes
 */
void MslEvaluator::mslVisit(MslSymbol* node)
{
    session->eval(node, result);
}

void MslEvaluator::mslVisit(MslAssignment* node)
{
    result.setNull();
    
    MslNode* target = (node->children.size() > 0) ? node->children[0] : nullptr;
    MslNode* value = (node->children.size() > 1) ? node->children[1] : nullptr;

    if (target == nullptr)
      errors.add("Assignment without target");
    else if (value == nullptr)
      errors.add("Assignment without value");
    else {
        // this must get to a symbol
        // like expressions, we don't have a way to pass quoted symbols without
        // evaluating so it has to be immediate
        if (!target->isSymbol()) {
            errors.add("Assignment target not a symbol");
        }
        else {
            MslSymbol* snode = static_cast<MslSymbol*>(target);
            value->visit(this);
            // should be doing this everywhere we do pre-emptive evaluation!
            if (errors.size() == 0) {
                session->assign(snode, result.getInt());
                // what is the result of an assignment?
            }
        }
    }
}

void MslEvaluator::mslVisit(MslVar* node)
{
    (void)node;
    result.setNull();
}

/**
 * Shouldn't actually have these now if they were sifted up to the Script.
 */
void MslEvaluator::mslVisit(MslProc* node)
{
    (void)node;
    result.setNull();
}

//////////////////////////////////////////////////////////////////////
//
// Expressions
//
//////////////////////////////////////////////////////////////////////

// if we allow async functions in expressions, then this
// will need to be much more complicated and use the stack
// once we add procs, we can't control what the proc will do so need
// to set a flag indicating "inside expression evaluator" to prevent async

MslOperators MslEvaluator::mapOperator(juce::String& s)
{
    MslOperators op = MslUnknown;
    
    if (s == "+") op = MslPlus;
    else if (s == "-") op = MslMinus;
    else if (s == "*") op = MslMult;
    else if (s == "/") op = MslDiv;
    else if (s == "=") op = MslEq;
    else if (s == "==") op = MslDeq;
    else if (s == "!=") op = MslNeq;
    else if (s == ">") op = MslGt;
    else if (s == ">=") op = MslGte;
    else if (s == "<") op = MslLt;
    else if (s == "<=") op = MslLte;
    else if (s == "!") op = MslNot;
    else if (s == "&&") op = MslAnd;
    else if (s == "||") op = MslOr;

    // will they try to use this?
    else if (s == "&") op = MslAmp;

    return op;
}    

/**
 * Be relaxed about this.  The only things we care about really
 * are numeric values and enumeration symbols coerced from/to ordinals.
 * Would be nice to do enum wrapping, but I don't think that belongs here.
 */
void MslEvaluator::mslVisit(MslOperator* opnode)
{
    result.setNull();
    
    MslOperators op = mapOperator(opnode->token);
    MslNode* node1 = (opnode->children.size() > 0) ? opnode->children[0] : nullptr;
    MslNode* node2 = (opnode->children.size() > 1) ? opnode->children[1] : nullptr;
    
    switch (op) {
        case MslUnknown:
            errors.add(juce::String("Unknown operator " + opnode->token));
            break;
        case MslPlus:
            result.setInt(evalInt(node1) + evalInt(node2));
            break;
        case MslMinus:
            result.setInt(evalInt(node1) - evalInt(node2));
            break;
        case MslMult:
            result.setInt(evalInt(node1) * evalInt(node2));
            break;
        case MslDiv: {
            int divisor = evalInt(node2);
            if (divisor == 0) {
                // we're obviously not going to throw if they made an error
                result.setInt(0);
                errors.add("Divide by zero");
            }
            else {
                result.setInt(evalInt(node1) / divisor);
            }
        }
            break;

            // for direct comparison, be smarter about coercion
            // = and == are the same right now, but that probably won't work
        case MslEq:
            compare(node1, node2, true);
            break;
        case MslDeq:
            compare(node1, node2, true);
            break;
            
        case MslNeq:
            compare(node1, node2, false);
            break;
            
        case MslGt:
            result.setInt(evalInt(node1) > evalInt(node2));
            break;
        case MslGte:
            result.setInt(evalInt(node1) >= evalInt(node2));
            break;
        case MslLt:
            result.setInt(evalInt(node1) < evalInt(node2));
            break;
        case MslLte:
            result.setInt(evalInt(node1) <= evalInt(node2));
            break;
        case MslNot:
            // here we check to make sure the node only has one child
            result.setInt(evalBool(node1));
            break;
        case MslAnd:
            result.setInt(evalBool(node1) && evalBool(node2));
            break;
        case MslOr:
            result.setInt(evalBool(node1) || evalBool(node2));
            break;
            
            // unclear about this, treat it as and
        case MslAmp:
            result.setInt(evalBool(node1) && evalBool(node2));
            break;
    }
    
}

int MslEvaluator::evalInt(MslNode* node)
{
    int ival = 0;
    if (node != nullptr) {
        node->visit(this);
        ival = result.getInt();
    }
    return ival;
}

bool MslEvaluator::evalBool(MslNode* node)
{
    bool bval = false;
    if (node != nullptr) {
        bval = (evalInt(node) > 0 ? 1 : 0);
    }
    return bval;
}

/**
 * Semi-smart comparison that deals with strings and symbols.
 * See compareSymbol for why this is complicated.
 */
void MslEvaluator::compare(MslNode* node1, MslNode* node2, bool equal)
{
    if (node1 == nullptr || node2 == nullptr) {
        // ahh, null
        // I suppose two nulls are equal?
        // these will coerce down to numeric zero and be equal
        if (equal)
          result.setInt(evalInt(node1) == evalInt(node2));
        else
          result.setInt(evalInt(node1) != evalInt(node2));
    }
    else if (node1->isSymbol() || node2->isSymbol()) {
        compareSymbol(node1, node2, equal);
    }
    else if (isString(node1) || isString(node2)) {
        MslValue val1;
        node1->visit(this);
        val1 = result;
        node2->visit(this);
        if (equal)
          result.setInt(StringEqualNoCase(val1.getString(), result.getString()));
        else
          result.setInt(!StringEqualNoCase(val1.getString(), result.getString()));
    }
}

bool MslEvaluator::isString(MslNode* node)
{
    bool itis = false;
    if (node != nullptr) {
        if (node->isLiteral()) {
            // guess it doesn't really matter if the token was
            // a quoted string or not, visit(literal) will coerce
            // it to a string anyway
            itis = true;
        }
    }
    return itis;
}

/**
 * Comparison of nodes involving Parameter symbols is more complicated due
 * to the normal use of enumeration values in the comparison rather than ordinals.
 *
 * So while the ordinal value of the switchQuantize parameter might be 3 no
 * one ever types that, they say "if switchQuantize == loop"
 *
 * There are various more elegant ways to handle this: we could treat enumeration values
 * in scripts as symbols consistently, intern them, and then do equality on the Symbols.
 * What I'm going to do is sort of like an operator overload on == that looks to see if
 * one side is a parameter symbol and if so arrange to coerce the other side into the
 * ordinal for comparison.  It gets the job done, in this case I care more about the
 * syntax than I do about the cleanliness of the evaluator.
 *
 * "loop" (without quotes) will be treated by the parser as an MslSymbol node that
 * is unresolved.
 *
 * Should do something similar for other likely comparisons, though anything other
 * than == and != don't make much sense since lexical or ordinal ordering isn't obvious.
 *
 * What this can't do is treat Symbols as return values from an expression.
 * So this
 *
 *    if quantize==Loop
 *
 * can work but this
 * 
 *    if (quantize)==Loop
 *
 * won't work since any node surroudng the symbol will hide the symbol.  By the point
 * of comparison it will already have been evaluated to an ordinal and the parameter-ness
 * of it has been lost.
 *
 * Might be nice to pass those around, similar to a Lisp quoted symbol.
 *
 * It would also work to treat parameter values symbolically rather than ordinals if they
 * are enumerations.  This would however result in interning a boatload of Symbols
 * for every enumeration in every parameter, which polutes the namespace.  So
 * "var loop" would hide the symbol representing the "loop" enumeration value of
 * switchQuantize.  And no, no one is going to understand namespace qualfiers and packages.
 *
 * A perhaps nicer alternative to this would be to allow values of enumerated
 * symbols to just always use the string representation but that requires changes
 * to the Query interface, which would be nice, but isn't there yet.
 *
 * And...having done all this shit, that's exactly what we're doing.  Since we control
 * the context and do the Query, we can coerce it to the enumeration.  So we have
 * the opposite problem now, the Query will have a nice symbol, and the unresolved
 * symbol will have been coerced to an ordinal.  Fuck.  This now reduces
 * to a string comparison.
 */ 
void MslEvaluator::compareSymbol(MslNode* node1, MslNode* node2, bool equal)
{
    MslSymbol* param = getResolvedParameter(node1, node2);
    MslNode* other = getUnresolved(node1, node2);

    if (param == nullptr || other == nullptr) {
        // this is not a combo we can reason with, revert to numeric
        if (equal)
          result.setInt(evalInt(node1) == evalInt(node2));
        else
          result.setInt(evalInt(node1) != evalInt(node2));
    }
    else {
        UIParameter* p = param->symbol->parameter;
        juce::String s = other->token;
        int otherOrdinal = p->getEnumOrdinal(s.toUTF8());
        int paramOrdinal = -1;
        // now get the parameter symbol ordinal
        param->visit(this);
        if (result.type == MslValue::Type::Int)
          paramOrdinal = result.getInt();
        else if (result.type == MslValue::Type::String) {
            paramOrdinal = p->getEnumOrdinal(result.getString());
        }

        if (equal)
          result.setInt(paramOrdinal == otherOrdinal);
        else
          result.setInt(paramOrdinal != otherOrdinal);
    }
}

MslSymbol* MslEvaluator::getResolvedParameter(MslNode* node1, MslNode* node2)
{
    MslSymbol* param = getResolvedParameter(node1);
    if (param == nullptr)
      param = getResolvedParameter(node2);
    return param;
}

// need to move MslSymbol resolution up to session so it can deal with vars !!

MslSymbol* MslEvaluator::getResolvedParameter(MslNode* node)
{
    MslSymbol* param = nullptr;
    if (node->isSymbol()) {
        MslSymbol* symnode = static_cast<MslSymbol*>(node);

        session->resolve(symnode);
        
        if (symnode->symbol != nullptr && symnode->symbol->parameter != nullptr)
          param = symnode;
    }
    return param;
}

MslNode* MslEvaluator::getUnresolved(MslNode* node1, MslNode* node2)
{
    MslNode* unresolved = getUnresolved(node1);
    if (unresolved == nullptr)
      unresolved = getUnresolved(node2);
    return unresolved;
}

MslNode* MslEvaluator::getUnresolved(MslNode* node)
{
    MslNode* unresolved = nullptr;
    if (node->isLiteral()) {
        unresolved = node;
    }
    else if (node->isSymbol()) {
        MslSymbol* symnode = static_cast<MslSymbol*>(node);
        if (symnode->symbol == nullptr)
          unresolved = node;
    }
    return unresolved;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
