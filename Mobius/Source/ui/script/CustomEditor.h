/**
 * An extension of juce::TextEditor that adds Emacs-ish key bindings.
 *
 * In theory could add other editor styles, or one-off custom bindings,
 * but really, who could possibly want anything besides Emacs?
 *
 */

#pragma once

class CustomEditor : public juce::TextEditor,
                     public juce::TextEditor::Listener,
                     public juce::KeyListener
{
  public:

    CustomEditor();
    ~CustomEditor();

    // by default this will support the standard keys, emacs has to be enabled
    void setEmacsMode(bool b);

    void textEditorTextChanged(juce::TextEditor& te) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* c) override;
    
  private:

    bool emacs = false;
    bool selecting = false;
    
};

