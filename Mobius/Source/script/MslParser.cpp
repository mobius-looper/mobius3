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
 *
 * dev notes:
 * I experimented a bit with allowing the parser to support incremental parsing
 * for the console where partial nodes would be retained between calls to consume()
 * and could be completed in the next console line.  This sort of worked
 * but led to unpredictable results, and I decided I don't like building
 * out an actual Script with the entire line history in it.
 *
 * So now, it resets at the start of every consume() but it does retain top level
 * procs and vars.
 *
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslModel.h"
#include "MslScript.h"
#include "MslError.h"
#include "MslParser.h"

/**
 * Main entry point for parsing files.
 * The result object and it's contents are owned by the receiver and must
 * be deleted.
 */
MslParserResult* MslParser::parse(juce::String source)
{
    if (result != nullptr) {
        // lingering result from a previous parse, shouldn't happen
        Trace(1, "MslParser: Someone didn't clean up a prior result, harm them.");
        delete result;
    }

    result = new MslParserResult();

    // flesh out a script skeleton
    MslScript* neu = new MslScript();
    neu->root = new MslBlock("");
    
    result->script.reset(neu);

    // these pointers are transient runtime state and
    // do not imply ownership

    // the "stack"
    script = neu;
    current = neu->root;

    parseInner(source);

    // if there were fatal errors, do not return the partial script
    if (result->errors.size() > 0)
      result->script = nullptr;


    // return what we built, but don't remember the old pointer between calls
    MslParserResult* retval = result;
    result = nullptr;

    return retval;
}

/**
 * Entry point for dynamic "scriptlet sessions".
 * Here the MslScript is maintained outside the parser for an indefinite
 * period of time, and we can add to it.
 */
MslParserResult* MslParser::parse(MslScript* scriptlet, juce::String source)
{
    if (result != nullptr) {
        // incremental parsing shouldn't see this either
        Trace(1, "MslParser: Someone didn't clean up a prior result, harm them.");
        delete result;
    }
    result = new MslParserResult();
    
    if (scriptlet == nullptr) {
        Trace(1, "MslParser: Dynamic parse with no scriptlet");
    }
    else {
        // reset parse state, the last block should already have been sifted
        // todo: this needs MUCH more work
        delete scriptlet->root;
        scriptlet->root = new MslBlock("");

        // set up the stack
        script = scriptlet;
        current = scriptlet->root;
        
        parse(source);

        sift();
    }

    // note that this result does not contain the script, only the error list
    MslParserResult* retval = result;
    result = nullptr;
    return retval;
}

/**
 * After parsing, move any procs and vars encountered up to the
 * Script.  The node tree may then be disposed of while retaining
 * the definitions.
 *
 * This is difficuclt to do during parsing due to the simplicity
 * of the stack being maintained as the node->parent reference rather
 * than allowing the top of the stack to be anywhere.
 *
 * Not entirely happy with this, but it gets this started.
 * Need a lot more thought into what procs in nested blocks mean.
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
        if (node->isProc()) {
            script->root->remove(node);
            script->procs.add(static_cast<MslProc*>(node));
        }
        else if (node->isVar()) {
            script->root->remove(node);
            script->vars.add(static_cast<MslVar*>(node));
        }
        else {
            index++;
        }
    }
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
    MslError* e = new MslError(tokenizer.getLine(), tokenizer.getColumn(), t.value, details);
    result->errors.add(e);
}

/**
 * Here if the error wasn't detected until after the token
 * was turned into a node.
 */
void MslParser::errorSyntax(MslNode* node, juce::String details)
{
    MslError* e = new MslError(tokenizer.getLine(), tokenizer.getColumn(), node->token, details);
    result->errors.add(e);
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
    bool match = (((t.value == "}") && (block->token == "{")) ||
                  ((t.value == ")") && (block->token == "(")) ||
                  ((t.value == "]") && (block->token == "[")));

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

    // is this the right place for this?
    result->errors.clear();

    while (tokenizer.hasNext() && result->errors.size() == 0) {
        MslToken t = tokenizer.next();

        // some nodes consume tokens without creating more nodes
        if (current->wantsToken(t))
          continue;

        // todo: if this is a keyword like "else" that doesn't
        // make sense on it's own, then raise an error, otherwies
        // it will just become a Symbol named "else"

        switch (t.type) {
            
            // why would hasNext be true, then have nothing?
            // maybe if there is whitespace at the end of the line
            // that hasn't been consumed?
            case MslToken::Type::End: break;
                
            case MslToken::Type::Error:
                errorSyntax(t, "Unexpected syntax");
                break;

            case MslToken::Type::Comment:
                break;
                
            case MslToken::Type::Processor:
                // todo: this is where directives need to be parsed
                break;

            case MslToken::Type::String:
                current->add(new MslLiteral(t.value));
                break;

            case MslToken::Type::Int: {
                // would be convenient to pass the entire MslToken in but I
                // don't want a dependency on that model yet
                MslLiteral* l = new MslLiteral(t.value);
                l->isInt = true;
                current->add(l);
            }
                break;

            case MslToken::Type::Float: {
                MslLiteral* l = new MslLiteral(t.value);
                l->isFloat = true;
                current->add(l);
            }
                break;
                
            case MslToken::Type::Bool: {
                MslLiteral* l = new MslLiteral(t.value);
                l->isBool = true;
                current->add(l);
            }
                break;

            case MslToken::Type::Symbol: {
                // if the symbol name matches a keyword, a specific node class
                // is pushed, otherwise it becomes a generic symbol node
                MslNode* node = checkKeywords(t.value);
                if (node == nullptr)
                  node = new MslSymbol(t.value);
                    
                current = push(node);
                
            }
                break;

            case MslToken::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlock* block = new MslBlock(t.value);
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
                    if (current->isVar()) {
                        // these are weird, we don't need an assignment node, just
                        // wait for an expression
                    }
                    else {
                        // these behave sort of like expression operators
                        // but they must have a symbol on the LHS
                        MslAssignment* ass = new MslAssignment(t.value);

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
                    MslOperator* op = new MslOperator(t.value);

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
                            if (precedence(op->token, operand->token) >= 0) {
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
                // see if we can avoid commas but allow them
                // it just moves to the next node
                // another other than comma is unexpected
                if (t.value != "," && t.value != ";") {
                    errorSyntax(t, "Unexpected punctuation");
                }
                else {
                    // hmm, logic unclear here
                    // if this is a universal receptor like a block, just keep going
                    // but if this is expecting somethig else, end it prematurely
                    // would be better to have an isReady or isWaiting method?
                    if (!current->isBlock()) {
                        current->locked = true;
                        current = current->parent;
                    }
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
 */
bool MslParser::operandable(MslNode* node)
{
    return (node != nullptr && (node->isLiteral() || node->isSymbol() || node->isOperator()));
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
 */
MslNode* MslParser::checkKeywords(juce::String token)
{
    MslNode* keyword = nullptr;

    if (token == "var")
      keyword = new MslVar(token);
    
    else if (token == "proc")
      keyword = new MslProc(token);
    
    else if (token == "if")
      keyword = new MslIf(token);

    else if (token == "else")
      keyword = new MslElse(token);

    return keyword;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
