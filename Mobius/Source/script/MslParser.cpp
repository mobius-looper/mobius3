/**
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

#include "MslModel.h"
#include "MslScript.h"
#include "MslParser.h"

/**
 * Primary entry point for parsing script files.
 * Reading the file contents is handled above.
 */
MslScript* MslParser::parseFile(juce::String path, juce::String content)
{
    script = new MslScript();
    script->filename = path;

    script->root = new MslBlock("");
    // the "stack"
    current = script->root;

    parse(content);

    // errors will have been left in the Script
    MslScript* retval = script;
    script = nullptr;
    return retval;
}

/**
 * The first of two entry points for the interactive script console.
 * This prepares the parser to extend an existing script.
 *
 * todo: think more about the interface for interactive scripts.
 * We need the outer Script to hold the proc, vars, and directives
 * but each line of script source is typically evaluated immediately
 * then is no longer necessary.  Maintaining that in the MslScript has
 * some nice features though, we can elect to save the console session to
 * a file, or use the top-level nodes as a history to "replay" previous statements.
 *
 * Currently, the nodes created during each call to consume() are placed inside the
 * Script and retained.
 */
void MslParser::prepare(MslScript* src)
{
    script = src;

    if (script->root == nullptr) {
        script->root = new MslBlock("");
    }
    
    current = script->root;
    errors.clear();
}


/**
 * Second of two interfaces for the interative script console.
 * After using prepare() to arm the existing Script, this parses
 * one or more lines of text and advances the parse state.
 *
 * todo: since the Script is the container of errors, we've got the problem
 * of retaining errors from the last extension or resetting them and returning
 * only new errors.  Having the full history can be nice for files, less
 * important for the console.
 */
void MslParser::consume(juce::String content)
{
    // start over with errors
    script->errors.clear();

    if (script == nullptr) {
        errors.add(juce::String("Parser has not been prepared"));
    }
    else {
        // reset parse state, the last block should already have been sifted
        delete script->root;
        script->root = new MslBlock("");
        current = script->root;
        
        parse(content);

        sift();
    }
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
 * Format a syntax error and add it to the Script being parsed.
 */
void MslParser::errorSyntax(MslToken& t, juce::String details)
{
    // would prefer to this be an array of objec
    MslError e (tokenizer.getLine(), tokenizer.getColumn(), t.value, details);
    script->errors.add(e);
    
    // Format errors for immediatel display on the console
    // not necessary, but need to retool the console to use
    // the Script error list
    juce::String error = "Error at ";

    if (tokenizer.getLines() == 1)
      error += "character " + juce::String(tokenizer.getColumn());
    else
      error += "line " + juce::String(tokenizer.getLine()) +
          " character " + juce::String(tokenizer.getColumn());

    error += ": " + t.value;

    if (details.length() > 0)
      errors.add(details);
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
 * Primary parse loop for lines of scripts.
 */
void MslParser::parse(juce::String source)
{
    tokenizer.setContent(source);
    errors.clear();

    while (tokenizer.hasNext() && errors.size() == 0) {
        MslToken t = tokenizer.next();

        if (current->wantsToken(t))
          continue;

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
                // todo: need some interesting ones
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
 * results "invalid uniary" is misleading if the non-operandable just doesn't even
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

// usual processing for a new node
MslNode* MslParser::push(MslNode* node)
{
    // do we keep recuring up here?
    if (!current->wantsNode(node)) {
        current->locked = true;
        current = current->parent;
        // todo: should we keep recur
    }

    current->add(node);
    current = node;

    return current;
}

MslNode* MslParser::checkKeywords(juce::String token)
{
    MslNode* keyword = nullptr;

    if (token == "var")
      keyword = new MslVar(token);
    
    else if (token == "proc")
      keyword = new MslProc(token);

    return keyword;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
