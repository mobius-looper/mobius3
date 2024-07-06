
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
    bool separatorReceived = false;

    while (tokenizer.hasNext() && errors.size() == 0) {
        Token t = tokenizer.next();

        // if we found a separator on the last token, prevent nodes
        // from including another block
        // todo: rework this to use locked=true instead
        bool nodeClosed = false;
        if (separatorReceived) {
            nodeClosed = true;
            separatorReceived = false;
        }
        
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
                MslNode* keyword = checkKeywords(t.value);
                if (keyword != nullptr)
                  current->add(keyword);
                else {
                    MslSymbol* s = new MslSymbol(t.value);
                    current->add(s);
                }
            }
                break;

            case Token::Type::Bracket: {
                if (t.isOpen()) {
                    // open bracket, who gets to receive it?
                    // even if the last node is willing if we got a separator
                    // it won't get it
                    MslBlock* block = new MslBlock(t.value);
                    MslNode* last = current->getLast();
                    if (!nodeClosed && last != nullptr && last->isOpen(block))
                      last->add(block);
                    else
                      current->add(block);
                    current = block;
                }
                else {
                    // close bracket, it must match the current block
                    if (matchBracket(t, current)) {
                        // block finished, pop the parse stack
                        current = current->parent;
                        // if this isn't a block, we just allowed it
                        // to receive a block and it may be done, pop again
                        // can we generalize this to be not type-specicic
                        // how about isOpen() to anything?
                        // or maybe isWaiting()
                        if (!current->isBlock())
                          current = current->parent;
                    }
                    // else, will have left an error
                }
            }
                break;

            case Token::Type::Operator: {
                MslNode* last = current->getLast();
                if (last == nullptr) {
                    // todo: exceptions here are unaries, don't need those yet
                    errorSyntax(t, "Invalid expression");
                }
                else {
                    MslNode* op = nullptr;
                    if (t.value == "=")
                      op = new MslAssignment(t.value);
                    else
                      op = new MslOperator(t.value);

                    // here we do extension or subsumption
                    // todo: extension happens when the new node type has
                    // a higher precedence than the previous node type
                    // assuming subsumption
                    current->remove(last);
                    current->add(op);
                    op->add(last);
                    // kludge, lock symbol objects that have already been pushed
                    // since we just ut it back into position
                    last->locked = true;
                    
                    current = op;
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
                    // force close operators and symbols, really anything?
                    separatorReceived = true;
                }
#if 0                
                else if (current->isOperator()) {
                    // is this right?
                    // pop op
                    current = current->parent;
                }
#endif                
            }
                break;

        }

        // check for node completion
        // if something is no longer receptive to ANY block it can be popped
        // I'm looking at you Operator
        if (!current->isOpen(nullptr))
          current = current->parent;

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
