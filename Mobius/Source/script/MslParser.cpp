/**
 * The MSL language parser.
 * 
 * It parses a string of text and produces the MslCompilation which
 * contains the parse trees broken up into several components.
 * See MslCompilation.h for what those are.
 *
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
 * Roughly speaking the language is C-like with a memory model and interpreter
 * that resembles Lisp.  The primary objects are the block (list) and the symbol.
 * 
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "MslModel.h"
#include "MslCompilation.h"
#include "MslSymbol.h"
#include "MslError.h"
#include "MslParser.h"

void MslParser::setVariableCarryover(bool b)
{
    variableCarryover = b;
}

/**
 * Main entry point for parsing.
 * The source may come from a file or from scriptlet text in memory.
 *
 * An MslCompilation object is allocated and returned.  This becomes owned by
 * the caller and must be deleted.  Currently these are not pooled.
 *
 * If there were parse errors, the compilation object will containa list of
 * error messages, and the various node fields will be empty to prevent
 * accidental evaluation.
 *
 * The text is first parsed into a "root block" which represents the top-level
 * statements.  If the parse was succesfull the root block is reorganized to
 * break apart the raw parse tree into various independent components.
 */
MslCompilation* MslParser::parse(juce::String source)
{
    init();
    
    script = new MslCompilation();
    root = new MslBlockNode();

    // the "stack"
    current = root;

    parseInner(source);

    if (script->errors.size() == 0) {
        sift();
    }
    else {
        delete root;
        root = nullptr;
    }

    // capture the result and clear out intermediate state
    MslCompilation* result = script;
    script = nullptr;
    root = nullptr;
    current = nullptr;
    
    return result;
}

/**
 * Clear out any lingering parse state before or after parsing.
 * The parser is normally a one-use stack object so this shouldn't be necessary.
 */
void MslParser::init()
{
    delete script;
    script =  nullptr;
    delete root;
    root = nullptr;
    current = nullptr;
}

/**
 * After parsing, reorganize the raw parse tree into various independent
 * components.
 *
 * The "body" of the script are any top-level statements in the root block
 * that may be evaluated.  These are repackaged into an MslFunction so the
 * body block can be used by the interpreter in the same way as declared functions.
 *
 * Top-level function definitions are extracted from the body and left on a list
 * since these are not evaluated in lexical order.
 * todo: once local function declarations are possible, this sifting should be
 * done on each MslBlockNode instead.  The only things that need special extraction
 * are functions declared as exported.
 *
 * Declarations will have been removed at this point and left in the MslCompilation.
 */
void MslParser::sift()
{
    int index = 0;
    while (index < root->size()) {
        MslNode* node = root->get(index);

        MslFunctionNode* f = node->getFunction();
        if (f != nullptr) {
            root->remove(node);
            functionize(f);
        }
        else {
            // unlike function nodes, we don't remove VariableNodes from the tree
            // since they can have initialization blocks that need to be evaluated
            // the special "carryover" option effectively makes all top-level variables
            // static variables so their values will be held in an MslVariable that
            // spans sessions
            MslVariableNode* v = node->getVariable();
            if (v != nullptr &&
                (v->isStatic() || variableCarryover)) {
                variableize(v);
            }
            index++;
        }
    }
    
    // what remains becomes the body block for the script
    // which may be evaluated if it has a reference name
    embody();
}

/**
 * Convert an MslFunctionNode into an MslFunction.
 * It has already been removed from the parse tree.
 *
 * The body block is moved from the OwnedArray in the node
 * to the unique_ptr in the Function.  
 * Formerly did the init block here too but that is now left in place.
 *
 * The empty node husk is then discarded.
 *
 * On it's own this doesn't do much more than the node model did
 * but MslFunction is used for other things and this gives them
 * a common interface.
 */
void MslParser::functionize(MslFunctionNode* node)
{
    MslFunction* function = new MslFunction();

    function->name = node->name;

    function->setNode(node);

    // if there was already a definition for this function
    // replace it, this could be considered an error
    MslFunction* existing = nullptr;
    for (auto f : script->functions) {
        if (f->name == function->name) {
            existing = f;
            break;
        }
    }
    
    if (existing != nullptr) {
        Trace(2, "MslParser: Replacing function definition %s", function->name.toUTF8());
        script->functions.removeObject(existing);
    }
    script->functions.add(function);
}

/**
 * Similar treatment for top-level variables.
 * The difference is that the VariableNode is left in the tree
 * since it may have an initialization block that needs to be evaluated.
 */
void MslParser::variableize(MslVariableNode* node)
{
    // using the constructor that doesn't take a pool since we don't
    // have access to the Environment, set the pool later
    MslVariable* variable = new MslVariable();

    variable->name = node->name;

    // do we even need this?  I don't think so, unlike MslFunctionNode
    // there is no publicly accessible body
    variable->setNode(node);

    // point back to this so we know it keeps values in a special place
    node->staticVariable = variable;

    // if there was already a definition for this variable
    // replace it, this could be considered an error, though
    // perhaps more likely for variables sine they have initializers
    // and you could declare/re-initialize it more than once?
    MslVariable* existing = nullptr;
    for (auto v : script->variables) {
        if (v->name == variable->name) {
            existing = v;
            break;
        }
    }
    
    if (existing != nullptr) {
        Trace(2, "MslParser: Replacing variable definition %s", variable->name.toUTF8());
        script->variables.removeObject(existing);
    }
    script->variables.add(variable);
}

/**
 * Convert the remaining top-level nodes into a function.
 * The function name is taken from the name declaration if
 * one was present.
 *
 * We may have allocated the MslFunction if we had to save
 * an argument declaration.
 */
void MslParser::embody()
{
    MslFunction* bodyfunc = script->getBodyFunction();
    if (bodyfunc == nullptr) {
        bodyfunc = new MslFunction();
        script->setBodyFunction(bodyfunc);
    }
    
    bodyfunc->name = script->name;

    // todo: script file argument should eventually be defined
    // with a declaration of some kind

    // there should be nothing left in the root block at this point
    // that needs sifting put the entire thing in the function
    bodyfunc->setBody(root);
    root = nullptr;
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
    // let's try to avoid a specific MslBlockNode just store the
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

    // scope keyword intermediate parser
    // an oddment to "look backward" at previous tokens that were'nt acted upon
    // when encountered
    MslScopedNode scopeHolder;
    bool awaitingQualifiedName = false;

    while (tokenizer.hasNext() && script->errors.size() == 0) {
        MslToken t = tokenizer.next();

        if (script->errors.size() > 0)
          continue;

        if (awaitingQualifiedName) {
            // we just received ":" after a symbol and now require
            // the next ADJACENT token to be another symbol
            if (t.type != MslToken::Type::Symbol)
              errorSyntax(t, "Expecting qualified symbol name");
            else
              current->token.value += t.value;
            awaitingQualifiedName = false;
            continue;
        }

        // some nodes consume tokens without creating more nodes
        // parser passed so the node can add an error if it wants to
        // ugly
        if (current->wantsToken(this, t))
          continue;

        MslPropertyNode* pnode = current->wantsProperty(this, t);
        // node parser may raise an error
        if (script->errors.size() > 0)
          continue;

        // node parser may promote this to a property
        if (pnode != nullptr) {
            // note we don't use push() here, it is already owned
            current = pnode;
            continue;
        }

        // kludge for waits, keywords come in while child nodes are on the stack
        // and if the child node doesn't want it, it becomes a MslSymbol which
        // can then be passed to wantsNode, but we don't have a way to keep it out
        // of the child list, if a child doesn't want a token, ask the parent and if
        // it does, pop and give it do it
        if (current->parent != nullptr && current->parent->wantsToken(this, t)) {
            current = current->parent;
            continue;
        }

        // if we encouter a scope token on the lhs of a function or variable
        // remember it
        if (scopeHolder.wantsToken(this, t))
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
                    MslReferenceNode* r = new MslReferenceNode(t);
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
                current = push(new MslLiteralNode(t));
                break;

            case MslToken::Type::Int: {
                // would be convenient to pass the entire MslToken in but I
                // don't want a dependency on that model yet
                MslLiteralNode* l = new MslLiteralNode(t);
                l->isInt = true;
                current = push(l);
            }
                break;

            case MslToken::Type::Float: {
                MslLiteralNode* l = new MslLiteralNode(t);
                l->isFloat = true;
                current = push(l);
            }
                break;
                
            case MslToken::Type::Bool: {
                MslLiteralNode* l = new MslLiteralNode(t);
                l->isBool = true;
                current = push(l);
            }
                break;

            case MslToken::Type::Symbol: {
                // special case for the few symbols we treat as operators
                MslOperators opcode = MslOperatorNode::mapOperatorSymbol(t.value);
                if (opcode != MslUnknown) {
                    parseOperator(t, opcode);
                }
                else {
                    // if the symbol name matches a keyword, a specific node class
                    // is pushed, otherwise it becomes a generic symbol node
                    MslNode* node = checkKeywords(t);
                    if (node == nullptr)
                      node = new MslSymbolNode(t);
                    else {
                        // for "function" and "variable" keywords, consume the pending
                        // scope qualifier
                        MslScopedNode* sn = node->getScopedNode();
                        if (sn != nullptr)
                          scopeHolder.transferScope(sn);
                    }
                    
                    current = push(node);

                    // hack for In that creates it's own child node for the
                    // target sequence
                    if (node->children.size() > 0)
                      current = node->children[node->children.size() - 1];
                }
            }
                break;

            case MslToken::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlockNode* block = new MslBlockNode(t);
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
                        MslAssignmentNode* ass = new MslAssignmentNode(t);

                        // pop up till we're wanted, more than one level?
                        // this still feels weird
                        if (!current->wantsNode(this, ass)) {
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
                    MslOperators opcode = MslOperatorNode::mapOperator(t.value);
                    if (opcode != MslUnknown) {
                        parseOperator(t, opcode);
                    }
                    else {
                        // this was tokenized as a C++ operator but we didn't handle it
                        // probably an error, but there may be some we want to ignore?
                        errorSyntax(t, "Unsupported operator");
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

                    MslSequenceNode* seq = current->getSequence();
                    if (seq != nullptr) {
                        // !! logic is a mess here, why would you need to
                        // do this down below as well?
                        seq->armed = true;
                    }
                    else if (!current->isBlock()) {
                        current->locked = true;
                        current = current->parent;

                        // maybe call wantsNode here instead
                        seq = current->getSequence();
                        if (seq != nullptr)
                          seq->armed = true;
                    }
                }
                else if (t.value == ":") {
                    // the tokenizer splits foo:bar into three tokens
                    // but when they are all adjacent they need to be treated
                    // as a single qualified symbol name
                    if (current->isSymbol() &&
                        ((current->token.column + current->token.value.length()) == t.column)) {
                        // they are adjacent
                        current->token.value += t.value;
                        awaitingQualifiedName = true;
                    }
                    else {
                        MslKeywordNode* k = new MslKeywordNode(t);
                        current = push(k);
                    }
                }
                else {
                    errorSyntax(t, "Unexpected punctuation");
                }
            }
                break;

        }

        // if we get here with a pending scope token that was not consumed,
        // then it was misplaced, e.g. "public wait"
        if (scopeHolder.hasScope())
          errorSyntax(t, "Misplaced scope qualifier");

        while (current != nullptr && current->isFinished()) {
            // this one will accept no more, remove it from the stack so the parent
            // can consume tokens without promoting them to Symbols
            current = current->parent;
        }

        // can't go on without something to fill
        if (current == nullptr)
          errorSyntax(t, "Invalid expression");
    }
}

/**
 * Handle the processing of an operator token, or one of the
 * symbols we treat as an operator.
 */
void MslParser::parseOperator(MslToken& t, MslOperators opcode)
{
    MslOperatorNode* op = new MslOperatorNode(t);
    op->opcode = opcode;

    // pop up till we're wanted, more than one level?
    // this still feels weird
    if (!current->wantsNode(this, op)) {
        current->locked = true;
        current = current->parent;
    }

    MslNode* last;
    if (current->isOperator())
      last = current;
    else
      last = current->getLast();
    
    // work backward until we can subsume something or hit a wall
    if (!operandable(last)) {
        unarize(t, op);
    }
    else {
        // either subsume the last node, or if we're adjacent to an
        // operator of lower priority, it's second operand
        MslNode* operand = last;
        MslOperatorNode* other = operand->getOperator();
        if (other != nullptr) {
            // old way
            //if (precedence(op->token.value, operand->token.value) >= 0) {
            if (precedence(opcode, other->opcode) >= 0) {
                if (last->children.size() == 2)
                  operand = last->getLast();
                else
                  errorSyntax(t, "Invalid expression, missing operand");
            }
        }

        current = subsume(op, operand);
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

/**
 * New precendence calculator using opcodes.
 */
int MslParser::precedence(MslOperators op1, MslOperators op2)
{
    int i1 = (int)op1;
    int i2 = (int)op2;

    if (i1 == i2)
      return 0;
    else if (i1 < i2)
      return -1;
    else
      return 1;
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
void MslParser::unarize(MslToken& t, MslOperatorNode* possible)
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
    while (!receiver->wantsNode(this, node)) {
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
      keyword = new MslVariableNode(t);
    
    else if (t.value == "function" || t.value == "func" || t.value == "proc")
      keyword = new MslFunctionNode(t);
    
    else if (t.value == "init")
      keyword = new MslInitNode(t);
    
    else if (t.value == "if")
      keyword = new MslIfNode(t);

    else if (t.value == "else")
      keyword = new MslElseNode(t);
    
    else if (t.value == "case")
      keyword = new MslCaseNode(t);
    
    else if (t.value == "end")
      keyword = new MslEndNode(t);
    
    else if (t.value == "wait")
      keyword = new MslWaitNode(t);
    
    else if (t.value == "print" || t.value == "echo")
      keyword = new MslPrintNode(t);

    else if (t.value == "trace")
      keyword = new MslTraceNode(t);
     
    else if (t.value == "context")
      keyword = new MslContextNode(t);
    
    else if (t.value == "AddResult")
      keyword = new MslResultNode(t);
    
    else if (t.value == "in") {
        // this one is weird because not only does it create a node
        // for this token, it also pushes another node under it, an MslSequence
        keyword = new MslInNode(t);
        keyword->add(new MslSequenceNode());
    }
    
    return keyword;
}

void MslParser::parseDirective(MslToken& t)
{
    int space = t.value.indexOfChar(0, ' ');
    juce::String directive;
    juce::String remainder;
    if (space < 0)
      directive = t.value.trim();
    else {
        directive = t.value.substring(0, space);
        remainder = t.value.substring(space);
    }

    if (directive.equalsIgnoreCase("#name")) {
        script->name = remainder.trim();
    }
    else if (directive.equalsIgnoreCase("#arguments")) {
        // better name for this would be parseSignature ?
        parseArguments(t, space, remainder);
    }
    else if (directive.equalsIgnoreCase("#sustain")) {
        // argument has to be a number for now, not an expression
        // though it could be handy to be able to reference a global varialbe
        // so you could change the sustain interval in several scripts at once
        script->sustain = true;
        script->sustainInterval = parseNumber(t, remainder);
    }
    else if (directive.equalsIgnoreCase("#repeat")) {
        // argument has to be a number for now, not an expression
        // though it could be handy to be able to reference a global varialbe
        // so you could change the sustain interval in several scripts at once
        script->repeat = true;
        script->repeatTimeout = parseNumber(t, remainder);
    }
    else if (directive.equalsIgnoreCase("#usage")) {
        script->usage = remainder.trim();
    }
    else if (directive.equalsIgnoreCase("#namespace")) {
        // sigh, namespace is a reserved word
        script->package = remainder.trim();
    }
    else if (directive.equalsIgnoreCase("#using")) {
        // so is "using"
        script->usingNamespaces.add(remainder.trim());
    }
    else if (directive.equalsIgnoreCase("#library")) {
        script->library = true;
    }
    else {
        Trace(1, "MslParser: Unknown directive %s", directive.toUTF8());
        errorSyntax(t, juce::String("Unknown directive ") + directive);
    }
}

int MslParser::parseNumber(MslToken& t, juce::String s)
{
    // does this need to be trimmed first, might want some syntax checking
    int value = s.getIntValue();
    if (value < 0)
      errorSyntax(t, "Not a well formed number");
    return value;
}

//////////////////////////////////////////////////////////////////////
//
// Signatures
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a bit of a hack to parse the #signature directive into what looks
 * like the argument declaration block of an MslFunctionNode.  This allows
 * a script file to be called with arguments as if it were wrapped in a
 * MslFunctionNode which makes it easier for the MslSymbolNode evaluator to deal with.
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
 * ugh, since we're using the parser in an unusual way, it's doing a lot of
 * work that we don't want.  We just want the root block, but with the
 * new interface, that gets packaged up in a sifted MslCompilation and
 * we have to dig it out.
 *
 * Really need some kind of backdoor parser for this...
 *
 */
void MslParser::parseArguments(MslToken& t, int offset, juce::String remainder)
{
    MslParser p;
    MslCompilation* c = p.parse(remainder);
    if (c->errors.size() > 0) {
        errorSyntax(t, "Parse error in directive");
        while (c->errors.size() > 0) {
            // steal it
            MslError* e = c->errors.removeAndReturn(0);
            // adjust the token location for the containing directive
            e->line = t.line;
            e->column += offset;
            script->errors.add(e);
        }
    }
    else {
        // begin the walk of shame down to the damn thing we need
        // this is an MslFunction
        MslFunction* f = c->getBodyFunction();
        if (f != nullptr) {
            // this is the root block of the function, which will become the signature block
            MslBlockNode* sig = f->releaseBody();

            // where this goes is in the declaration block
            // of the body function for this compilation unit, whew
            f = script->getBodyFunction();
            if (f == nullptr) {
                f = new MslFunction();
                script->setBodyFunction(f);
            }
            f->setDeclaration(sig);
        }
    }
    delete c;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
