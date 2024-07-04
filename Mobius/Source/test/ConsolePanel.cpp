/**
 * A testing panel that shows an interactive console.
 * 
 */

#include <JuceHeader.h>

#include "../Supervisor.h"
#include "../util/Trace.h"
#include "../ui/JuceUtil.h"

#include "ConsolePanel.h"

ConsoleContent::ConsoleContent(ConsolePanel* parent)
{
    panel = parent;
    addAndMakeVisible(&console);
    console.setListener(this);
}

ConsoleContent::~ConsoleContent()
{
}

void ConsoleContent::showing()
{
    console.clear();
    console.add("Shall we play a game?");
    console.prompt();
}

void ConsoleContent::hiding()
{
}

void ConsoleContent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    console.setBounds(area);
}

void ConsoleContent::paint(juce::Graphics& g)
{
    (void)g;
}

void ConsoleContent::buttonClicked(juce::Button* b)
{
    (void)b;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Refresh the whole damn thing if anything changes.
 */
void ConsoleContent::update()
{
}

void ConsoleContent::consoleLine(juce::String line)
{
    parseLine(line);
}

//////////////////////////////////////////////////////////////////////
//
// Commands
//
//////////////////////////////////////////////////////////////////////

void ConsoleContent::parseLine(juce::String line)
{
    if (line == "?") {
        showHelp();
    }
    else if (line == "clear") {
        console.clear();
    }
    else if (line == "test") {
        doTest();
    }
    else if (line == "quit" || line == "exit") {
        panel->close();
    }
    else {
        console.add("Unknown command: " + line);
    }
}

void ConsoleContent::showHelp()
{
    console.add("?         help");
    console.add("clear     clear display");
    console.add("test      run a test");
    console.add("foo       command of mystery");
    console.add("quit      close the console");
}

void ConsoleContent::doTest()
{
    juce::CodeDocument doc;

    // doc.replaceAllContent("this (\"is\", 123, something) // comment?");
    // doc.replaceAllContent("var foo = $1 + 12 * (x / y); #something else");
    doc.replaceAllContent("!sustain 1014");

    juce::CodeDocument::Iterator iterator (doc);
    juce::CPlusPlusCodeTokeniser tokeniser;

    while (!iterator.isEOF()) {
        //console.add("Position " + juce::String(iterator.getPosition()));
        juce::CodeDocument::Position start = iterator.toPosition();
        int type = tokeniser.readNextToken(iterator);
        //console.add("Type " + juce::String(type));
        juce::CodeDocument::Position end = iterator.toPosition();
        juce::String token = doc.getTextBetween(start, end);
        console.add(tokenType(type) + ": " + token);
    }
    console.add("Final position " + juce::String(iterator.getPosition()));
}

juce::String ConsoleContent::tokenType(int type)
{
    juce::String str;
    switch (type) {
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_error:
            str = "error";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_comment:
            str = "comment";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_keyword:
            str = "keyword";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_operator:
            str = "operator";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_identifier:
            str = "identifier";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_integer:
            str = "integer";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_float:
            str = "float";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_string:
            str = "string";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_bracket:
            str = "bracket";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_punctuation:
            str = "punctuation";
            break;
        case juce::CPlusPlusCodeTokeniser::TokenType::tokenType_preprocessor:
            str = "preprocessor";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
