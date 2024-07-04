/**
 * A TextEditor extension that behaves like a simple command line console
 */

#pragma once

#include <JuceHeader.h>

class Console : public juce::TextEditor, public juce::TextEditor::Listener
{
  public:
    
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void consoleLine(juce::String line) = 0;
    };
    
    Console();
    ~Console();

    void setListener(Listener* l) {
        listener = l;
    }
    
    void add(const juce::String& m);
    void addAndPrompt(const juce::String& m);
    void prompt();
    void newline();

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& te) override;
    void textEditorReturnKeyPressed(juce::TextEditor& te) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& te) override;
    void textEditorFocusLost(juce::TextEditor& te) override;

  private:
    
    juce::String getLastLine();
    Listener* listener = nullptr;
    
};
