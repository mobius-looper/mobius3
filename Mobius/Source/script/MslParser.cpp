/**
 * If you're a "language guy" you will probably notice that I am not one.
 *
 * I didn't follow any of the usual Rules For Parser Construction, that
 * I'm aware of.  It isn't pretty, there is an awkward combination of
 * parse rules split between the MslParser and the MslNode model itself.
 * But it gets the job done and is relatively simple.
 *
 * I really don't think we need to over-engineer this with BNF, Antlr, or
 * whatever it is the cool kids are using these days.  But if you have any suggestions
 * for an algorithm that accomplishes the same thing and pleases the
 * Parser Gods, let me know.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslModel.h"
#include "MslSymbol.h"
#include "MslScript.h"
#include "MslError.h"
#include "MslParser.h"

/**
 * Main entry point for parsing.
 * The source may come from a file or from scriptlet text in memory.
 *
 * An MslScript object is allocated and returned.  This becomes owned by
 * the caller and must be deleted.  Currently these are not pooled.
 *
 * If there were parse errors, the script object will have no root block and
 * errors will be left inside it.
 */
MslScript* MslParser::parse(juce::String source)
{
    // start with a new empty script
    script = new MslScript();
    script->root = new MslBlock();

    // the "stack"
    current = script->root;

    parseInner(source);

    // if errors were left in the script, don't return a partially constructed root block
    if (script->errors.size() > 0) {
        delete script->root;
        script->root = nullptr;
    }
    else {
        sift();
    }
    
    // shouldn't matter but don't retain pointers to things we're losing control over
    MslScript* result = script;
    script = nullptr;
    current = nullptr;

    return result;
}

/**
 * Entry point for dynamic "scriptlet sessions".
 * Here the MslScript is maintained outside the parser for an indefinite
 * period of time, and we can add to it.
 */
bool MslParser::parse(MslScript* argScript, juce::String source)
{
    bool success = false;
    
    // reset any lingering state from last time
    // retain sifted function and variable definitions
    script = argScript;
    script->errors.clear();
    delete script->root;
    script->root = new MslBlock();
    
    current = script->root;
    
    parseInner(source);

    if (script->errors.size() > 0) {
        delete script->root;
        script->root = nullptr;
    }
    else {
        // capture new functions and variables
        sift();
        success = true;
    }
    
    // clean up transient pointers
    script = nullptr;
    current = nullptr;

    return success;
}

/**
 * After parsing, move any functions and variables encountered up to the
 * Script.  The node tree may then be disposed of while retaining
 * the definitions.
 *
 * This is difficuclt to do during parsing due to the simplicity
 * of the stack being maintained as the node->parent reference rather
 * than allowing the top of the stack to be anywhere.
 *
 * Not entirely happy with this, but it gets this started.
 * Need a lot more thought into what functions in nested blocks mean.
 * Vars in nested blocks are retained in position as the variable/symbol override
 * is active only during that block.
 *
 * Only doing one level of sifting till we work that out.
 */
void MslParser::sift()
{
    int index = 0;
    while (index < script->root->size()) {
        MslNode* node = script->root->get(index);
        if (node->isFunction()) {
            script->root->remove(node);
            addFunction(static_cast<MslFunction*>(node));
        }
        else if (node->isInit()) {
            script->root->remove(node);
            // todo: if there is more than one, accumulate them?
            script->init.reset(node);
        }
        else {
            index++;
        }
    }
}

void MslParser::addFunction(MslFunction* func)
{
    // replace if it was defined again
    MslFunction* existing = nullptr;
    for (auto p : script->functions) {
        if (p->name == func->name) {
            existing = p;
            break;
        }
    }
    if (existing != nullptr) {
        Trace(2, "MslParser: Replacing function definition %s", func->name.toUTF8());
        script->functions.removeObject(existing);
    }
    script->functions.add(func);
}

//////////////////////////////////////////////////////////////////////
//
// Internals
//
//////////////////////////////////////////////////////////////////////

/**
 * Format a syntax error for a token and add it to the result.
 */
void MslParser::errorSyntax(MslToken& t, juce::String details)
{
    // would prefer to this be an array of objects
    MslError* e = new MslError(t.line, t.column, t.value, details);
    script->errors.add(e);
}

/**
 * Here if the error wasn't detected until after the token
 * was turned into a node.
 */
void MslParser::errorSyntax(MslNode* node, juce::String details)
{
    MslError* e = new MslError(node->token.line, node->token.column, node->token.value, details);
    script->errors.add(e);
}

/**
 * Return true if a close braken token just received matches
 * the bracket of the given block.  If it doesn't match
 * add an error and return false.
 */
bool MslParser::matchBracket(MslToken& t, MslNode* block)
{
    // let's try to avoid a specific MslBlock just store the
    // bracketing char
    bool match = (((t.value == "}") && (block->token.value == "{")) ||
                  ((t.value == ")") && (block->token.value == "(")) ||
                  ((t.value == "]") && (block->token.value == "[")));

    if (!match)
      errorSyntax(t, "Mismatched brackets");

    return match;
}

/**
 * Primary parse loop
 */
void MslParser::parseInner(juce::String source)
{
    tokenizer.setContent(source);

    while (tokenizer.hasNext() && script->errors.size() == 0) {
        MslToken t = tokenizer.next();

        // some nodes consume tokens without creating more nodes
        // parser passed so the node can add an error if it wants to
        // ugly
        if (current->wantsToken(this, t) || script->errors.size() > 0)
          continue;

        // todo: if this is a keyword like "else" that doesn't
        // make sense on it's own, then raise an error, otherwies
        // it will just become a Symbol named "else"

        switch (t.type) {
            
            // why would hasNext be true, then have nothing?
            // maybe if there is whitespace at the end of the line
            // that hasn't been consumed?
            case MslToken::Type::End: break;
                
            case MslToken::Type::Error: {
                // c++ tokenizer treats $ as an Error rather than punctuation
                // but we'll use it for references
                if (t.value == "$") {
                    // reserve these for positional argument references
                    // but may have other uses
                    // could just make this a symbol but that that's so overloaded already
                    MslReference* r = new MslReference(t);
                    // this will also consume the next token
                    current = push(r);
                }
                else { 
                    errorSyntax(t, "Unexpected syntax");
                }
            }
                break;

            case MslToken::Type::Comment:
                break;
                
            case MslToken::Type::Processor:
                parseDirective(t);
                break;

            case MslToken::Type::String:
                current = push(new MslLiteral(t));
                break;

            case MslToken::Type::Int: {
                // would be convenient to pass the entire MslToken in but I
                // don't want a dependency on that model yet
                MslLiteral* l = new MslLiteral(t);
                l->isInt = true;
                current = push(l);
            }
                break;

            case MslToken::Type::Float: {
                MslLiteral* l = new MslLiteral(t);
                l->isFloat = true;
                current = push(l);
            }
                break;
                
            case MslToken::Type::Bool: {
                MslLiteral* l = new MslLiteral(t);
                l->isBool = true;
                current = push(l);
            }
                break;

            case MslToken::Type::Symbol: {
                // if the symbol name matches a keyword, a specific node class
                // is pushed, otherwise it becomes a generic symbol node
                MslNode* node = checkKeywords(t);
                if (node == nullptr)
                  node = new MslSymbol(t);
                    
                current = push(node);

                // hack for In that creates it's own child node for the
                // target sequence
                if (node->children.size() > 0)
                  current = node->children[node->children.size() - 1];
            }
                break;

            case MslToken::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlock* block = new MslBlock(t);
                    current = push(block);
                }
                else {
                    // walk up to the nearest block, closing along the way
                    while (!current->isBlock()) {
                        current->locked = true;
                        current = current->parent;
                    }
                    if (matchBracket(t, current)) {
                        // block finished
                        current->locked = true;
                        current = current->parent;
                    }
                    // else, will have left an error
                }
            }
                break;

            case MslToken::Type::Operator: {
                if (t.value == "=") {
                    if (current->isVariable()) {
                        // these are weird, we don't need an assignment node, just
                        // wait for an expression
                    }
                    else {
                        // these behave sort of like expression operators
                        // but they must have a symbol on the LHS
                        MslAssignment* ass = new MslAssignment(t);

                        // pop up till we're wanted, more than one level?
                        // this still feels weird
                        if (!current->wantsNode(ass)) {
                            current->locked = true;
                            current = current->parent;
                        }

                        MslNode* last = current->getLast();
                        if (last == nullptr || !last->isSymbol()) {
                            errorSyntax(t, "Assignment must have a symbol");
                            delete ass;
                        }
                        else {
                            current = subsume(ass, last);
                        }
                    }
                }
                else {
                    MslOperator* op = new MslOperator(t);

                    // pop up till we're wanted, more than one level?
                    // this still feels weird
                    if (!current->wantsNode(op)) {
                        current->locked = true;
                        current = current->parent;
                    }

                    // work backward until we can subsume something or hit a wall
                    MslNode* last = current->getLast();
                    if (!operandable(last)) {
                        unarize(t, op);
                    }
                    else {
                        // either subsume the last node, or if we're adjacent to an
                        // operator of lower priority, it's second operand
                        MslNode* operand = last;
                        if (operand->isOperator()) {
                            if (precedence(op->token.value, operand->token.value) >= 0) {
                                if (last->children.size() == 2)
                                  operand = last->getLast();
                                else
                                  errorSyntax(t, "Invalid expression, missing operand");
                            }
                        }

                        current = subsume(op, operand);
                    }
                }
            }
                break;

            case MslToken::Type::Punctuation: {
                if (t.value == "," || t.value == ";") {
                    // statement separator
                    // see if we can avoid commas but allow them
                    // it just moves to the next node
                    // another other than comma is unexpected

                    // hmm, logic unclear here
                    // if this is a universal receptor like a block, just keep going
                    // but if this is expecting somethig else, end it prematurely
                    // would be better to have an isReady or isWaiting method?
                    // feels like this needs to recurse up, we have to do that for
                    // Sequence which is hacked
                    if (!current->isBlock()) {
                        current->locked = true;
                        current = current->parent;

                        // maybe call wantsNode here instead
                        if (current->isSequence()) {
                            MslSequence* seq = static_cast<MslSequence*>(current);
                            seq->armed = true;
                        }
                        
                    }
                }
                else if (t.value == ":") {
                    MslKeyword* k = new MslKeyword(t);
                    current = push(k);
                }
                else {
                    errorSyntax(t, "Unexpected punctuation");
                }
            }
                break;

        }

        // can't go on without something to fill
        if (current == nullptr)
          errorSyntax(t, "Invalid expression");
    }
}

/**
 * Return true if the prior node is something that can participate
 * as an operand.
 *
 * Need more here, while the operandable/unarize may work, the error that
 * results "invalid unary" is misleading if the non-operandable just doesn't even
 * make sense in this context.  Add another level of error checking here.
 *
 * Blocks are necessary to get ().  Unclear if there are cases where {} should
 * be allowed but if you take the position that {} is just a multi-valued node
 * whose value is the last node within it, it would work.
 */
bool MslParser::operandable(MslNode* node)
{
    return (node != nullptr &&
            (node->operandable() || node->isBlock()));
}

/**
 * Given two operator tokens, return 1 if the first is higher than the second,
 * -1 if the first is lower than the second, and 0 if they are equal.
 * We only have to deal with a subset of the C operators: the mdas in pemdas
 */
int MslParser::precedence(juce::String op1, juce::String op2)
{
    if (op1 == "*") {
        if (op2 == "*")
          return 0;
        else
          return 1;
    }
    else if (op1 == "/") {
        if (op2 == "*")
          return -1;
        else if (op2 == "/")
          return 0;
        else
          return 1;
    }
    else if (op1 == "+") {
        if (op2 == "*" || op2 == "/")
          return -1;
        else if (op2 == "+")
          return 0;
        else
          return 1;
    }
    else if (op1 == "-") {
        if (op2 == "-")
          return 0;
        else
          return -1;
    }

    // else could check for unsupported operators but can do that above
    return 0;
}

MslNode* MslParser::subsume(MslNode* op, MslNode* operand)
{
    MslNode* parent = operand->parent;
    parent->remove(operand);
    parent->add(op);
    op->add(operand);
    return op;
}

/**
 * In a syntactical context that requires a unary, we allow a subset
 */
void MslParser::unarize(MslToken& t, MslOperator* possible)
{
    if (t.value == "!" || t.value == "-" || t.value == "+") {
        // worth having a special node type for these?
        possible->unary = true;
        possible->locked = true;
        current->add(possible);
    }
    else {
        errorSyntax(t, "Invalid unary operator");
        delete possible;
    }
}

/**
 * Add a new node to the next available receiver, and push it on the stack.
 */
MslNode* MslParser::push(MslNode* node)
{
    // kind of dangerous loop, but we must eventually hit the upper
    // block which has an insatiable need
    MslNode* receiver = current;
    while (!receiver->wantsNode(node)) {
        receiver->locked = true;
        if (receiver->parent != nullptr) 
          receiver = receiver->parent;
        else {
            // token has been consumed by now and that's where the line
            // numbers are, need to save the entire token on the node!!
            errorSyntax(node, "Unmatched token");
            receiver = nullptr;
            break;
        }
    }

    if (receiver != nullptr) {
        receiver->add(node);
        current = node;
    }
    
    return current;
}

/**
 * Convert a keyword token into a specific node class.
 *
 * Functions and variables are commonly aliased, could be using
 * startsWith here too.
 */
MslNode* MslParser::checkKeywords(MslToken& t)
{
    MslNode* keyword = nullptr;

    if (t.value == "var" || t.value == "variable")
      keyword = new MslVariable(t);
    
    else if (t.value == "function" || t.value == "func" || t.value == "proc")
      keyword = new MslFunction(t);
    
    else if (t.value == "init")
      keyword = new MslInit(t);
    
    else if (t.value == "if")
      keyword = new MslIf(t);

    else if (t.value == "else")
      keyword = new MslElse(t);
    
    else if (t.value == "end")
      keyword = new MslEnd(t);
    
    else if (t.value == "wait")
      keyword = new MslWaitNode(t);
    
    else if (t.value == "print" || t.value == "echo")
      keyword = new MslPrint(t);
    
    else if (t.value == "context")
      keyword = new MslContextNode(t);
    
    else if (t.value == "in") {
        // this one is weird because not only does it create a node
        // for this token, it also pushes another node under it, an MslSequence
        keyword = new MslIn(t);
        keyword->add(new MslSequence());
    }
    
    return keyword;
}

void MslParser::parseDirective(MslToken& t)
{
    int space = t.value.indexOfChar(0, ' ');
    juce::String directive = t.value.substring(0, space);
    juce::String remainder = t.value.substring(space);

    if (directive.equalsIgnoreCase("#name")) {
        script->name = remainder.trim();
    }
    else if (directive.equalsIgnoreCase("#arguments")) {
        parseArguments(t, space, remainder);
    }
    else {
        Trace(1, "MslParser: Unknown directive %s", directive.toUTF8());
        errorSyntax(t, juce::String("Unknown directive ") + directive);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Signatures
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a bit of a hack to parse the #signature directive into what looks
 * like the argument declaration block of an MslFunction.  This allows
 * a script file to be called with arguments as if it were wrapped in a
 * MslFunction which makes it easier for the MslSymbol evaluator to deal with.
 * Since the text isn't part of the text of the file currently being
 * parsed, we create a new parser just for this so we don't mess up the state
 * of the main parser.
 *
 * This is evolving along with the notion of "options" and scripts as "classes".
 *
 * Canonical syntax:
 *
 *    script foo (a b c) {
 *    }
 *
 * Alternative:
 *
 *    script foo {
 *      signature (a b c)
 *      option (quantize, sustain)
 *
 * Directives:
 *
 *    #signature foo(a b c)
 *    #arguments a b c
 *
 * Script arguments are different than functions because they are usually optional
 * and it's inconvenient to make them type a=null b=null...
 *
 * Error handling is weird.  Add a general error about the directive itself
 * to provide some context, then adjust the parse errors for the argument string
 * so that the column numbers are offset by the length of the #arguments prefix.
 *
 * Work on this so the same thing can be used for argument signatures for
 * external functions.
 *
 */
void MslParser::parseArguments(MslToken& t, int offset, juce::String remainder)
{
    MslParser p;
    MslScript* s = p.parse(remainder);
    if (s->errors.size() > 0) {
        errorSyntax(t, "Parse error in directive");
        while (s->errors.size() > 0) {
            // steal it
            MslError* e = s->errors.removeAndReturn(0);
            // adjust the token location for the containing directive
            e->line = t.line;
            e->column += offset;
            script->errors.add(e);
        }
    }
    else {
        // the root block is the argument list, steal it from the transient script
        // and leave it on the one we're parsing
        script->arguments.reset(s->root);
        s->root = nullptr;
    }
    delete s;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
