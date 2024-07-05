
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
            
            case Token::Type::End: {
                // why would hasNext be true, then have nothing?
                // maybe if there is whitespace at the end of the line
                // that hasn't been consumed?
            }
                break;
                
            case Token::Type::Error: {
                errorSyntax(t, "Unexpected syntax");
            }
                break;

            case Token::Type::Comment: {
                // ignore
            }
                break;

            case Token::Type::String: {
                current->add(new MslLiteral(t.value));
            }
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
                MslSymbol* s = new MslSymbol(t.value);
                current->add(s);
            }
                break;

            case Token::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlock* block = new MslBlock(t.value);
                    if (t.value == "(") {
                        // if the last was a symbol, this becomes symbol arguments
                        // otherwise it's just a boolean expression block
                        MslNode* last = current->getLast();
                        if (last != nullptr && last->isSymbol()) {
                            MslSymbol* s = static_cast<MslSymbol*>(last);
                            if (s->arguments != nullptr) {
                                // already had arguments, must be foo()()
                                current->add(block);
                            }
                            else {
                                block->parent = s;
                                s->arguments = block;
                            }
                        }
                        else {
                            // else a parthesized expression block
                            current->add(block);
                        }
                    }
                    else {
                        // if the last was a proc symbol will want to omake this the
                        // proc body, else an expression block
                        current->add(block);
                    }
                    // always pushes
                    current = block;
                }
                else if (matchBracket(t, current)) {
                    current = current->parent;
                    if (current == nullptr)
                      errorSyntax(t, "Orphan node");
                    else if (current->isSymbol())
                      current = current->parent;
                }
            }
                break;

            case Token::Type::Punctuation: {
                // see if we can avoid commas but allow them
                // it just moves to the next node
                // another other than comma is unexpected
                if (t.value != ",") {
                    errorSyntax(t, "Unexpected punctuation");
                }
                else if (current->isOperator()) {
                    // pop op
                    current = current->parent;
                }
            }
                break;

            case Token::Type::Operator: {
                MslNode* last = current->getLast();
                if (last == nullptr) {
                    errorSyntax(t, "Invalid expression");
                }
                else {
                    current->remove(last);
                    MslOperator* op = new MslOperator(t.value);
                    current->add(op);
                    op->add(last);
                    current = op;
                }
            }
                break;

            case Token::Type::Processor: {
                // todo: need some interesting ones
            }
                break;
        }

        if (current == nullptr) {
            // something wrong with the parent chain
            errorSyntax(t, "Dangling block");
        }
        else  {
            // todo: completion checking should be something
            // every node does?
            // it's the lookahead problem
            // you either always assume something is done at the bottom
            // or you have to close it on the next token
            if (current->isOperator()) {
                MslOperator* op = static_cast<MslOperator*>(current);
                if (op->isComplete()) {
                    // pop
                    current = op->parent;
                    if (current == nullptr)
                      errorSyntax(t, "Invalid expression");
                }
            }
        }
    }

    if (errors.size() > 0)
      delete root;

    return root;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
