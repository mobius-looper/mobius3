
#include <JuceHeader.h>

#include "MslModel.h"
#include "MslParser.h"


juce::StringArray* MslParser::getErrors()
{
    return &errors;
}

void MslParser::errorSyntax(Token& t, juce::String details)
{
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

bool MslParser::matchBracket(Token& t, MslNode* block)
{
    // let's try to avoid a specific MslBlock just store the
    // bracketing char
    bool match = ((t.value == "}") && (block->token == "{") ||
                  (t.value == ")") && (block->token == "(") ||
                  (t.value == "]") && (block->token == "["));

    if (!match)
      errorSyntax(t, "Mismatched brackets");

    return match;
}

MslNode* MslParser::parse(juce::String source)
{
    MslNode* root = new MslBlock("");
    MslNode* current = root;
    
    tokenizer.setContent(source);
    errors.clear();

    while (tokenizer.hasNext() && errors.size() == 0) {
        Token t = tokenizer.next();

        switch (t.type) {
            
            // why would hasNext be true, then have nothing?
            // maybe if there is whitespace at the end of the line
            // that hasn't been consumed?
            case Token::Type::End: break;
                
            case Token::Type::Error:
                errorSyntax(t, "Unexpected syntax");
                break;

            case Token::Type::Comment:
                break;
                
            case Token::Type::Processor:
                // todo: need some interesting ones
                break;

            case Token::Type::String:
                current->add(new MslLiteral(t.value));
                break;

            case Token::Type::Int: {
                // would be convenient to pass the entire Token in but I
                // don't want a dependency on that model yet
                MslLiteral* l = new MslLiteral(t.value);
                l->isInt = true;
                current->add(l);
            }
                break;

            case Token::Type::Float: {
                MslLiteral* l = new MslLiteral(t.value);
                l->isFloat = true;
                current->add(l);
            }
                break;
                
            case Token::Type::Bool: {
                MslLiteral* l = new MslLiteral(t.value);
                l->isBool = true;
                current->add(l);
            }
                break;

            case Token::Type::Symbol: {
                MslNode* node = checkKeywords(t.value);
                if (node == nullptr)
                  node = new MslSymbol(t.value);
                    
                current = push(current, node);
                
            }
                break;

            case Token::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlock* block = new MslBlock(t.value);
                    current = push(current, block);
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

            case Token::Type::Operator: {
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
                        unarize(t, current, op);
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

            case Token::Type::Punctuation: {
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

    if (errors.size() > 0) {
        delete root;
        root = nullptr;
    }
    
    return root;
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
void MslParser::unarize(Token& t, MslNode* current, MslOperator* possible)
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
MslNode* MslParser::push(MslNode* current, MslNode* node)
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
