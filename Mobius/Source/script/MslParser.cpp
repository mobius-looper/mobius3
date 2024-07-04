
#include <JuceHeader.h>

#include "MslModel.h"
#include "MslParser.h"


juce::StringArray MslParser::getErrors()
{
    return errors;
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
    bool match = ((t.value == "}") && (block->bracket == "{") ||
                  (t.value == ")") && (block->bracket == "(") ||
                  (t.value == "]") && (block->bracket == "["));

    if (!match)
      errorSyntax(t, "Mismatched brackets");

    return match;
}

MslNode* MslParser::parse(juce::String source)
{
    MslNode* root = new MslBlock();
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

            case Token::Type::Symbol: {
                current->add(new MslSymbol(t.value));
            }
                break;

            case Token::Type::String: {
                current->add(new MslLiteral(t.value));
            }
                break;

            case Token::Type::Int: {
                current->add(new MslLiteral(t.value, t.value.getIntValue()));
            }
                break;

            case Token::Type::Float: {
                current->add(new MslLiteral(t.value, t.value.getFloatValue()));
            }
                break;
                
            case Token::Type::Bool: {
                current->add(new MslLiteral(t.value, t.getBool()));
            }
                break;

            case Token::Type::Bracket: {
                if (t.isOpen()) {
                    MslBlock* block = new MslBlock();
                    block->bracket = t.value;
                    current->add(block);
                    current = block;
                }
                else if (matchBracket(t, current)) {
                    if (current->parent == nullptr) {
                        // shouldn't see this if bracket matching worked right
                        errorSyntax(t, "End bracket at root");
                    }
                    else {
                        current = current->parent;
                    }
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
                    if (current == nullptr)
                      errorSyntax(t, "Invalid expression");
                }
            }
                break;

            case Token::Type::Operator: {
                MslNode* parent = current->parent;
                if (parent == nullptr) {
                    errorSyntax(t, "Unexpected operator");
                }
                else {
                    MslOperator* op = new MslOperator(t.value);
                    parent->remove(current);
                    parent->add(op);
                    op->add(current);
                    current = op;
                }
            }
                break;

            case Token::Type::Processor: {
                // todo: need some interesting ones
            }
                break;
        }

        // todo: completion checking should be something
        // every node does?
        // it's the lookahead problem
        // you either always assume something is done at the bottom
        // or you have to close it on the next token
        
        if (current->isOperator()) {
            MslOperator* op = static_cast<MslOperator*>(current);
            if (op->isComplete()) {
                // pop
                current = current->parent;
                if (current == nullptr)
                  errorSyntax(t, "Invalid expression");
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
