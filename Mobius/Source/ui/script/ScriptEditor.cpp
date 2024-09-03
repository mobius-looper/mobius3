
#include <JuceHeader.h>

#include "ScriptEditor.h"

ScriptEditor::ScriptEditor()
{
    addAndMakeVisible(details);

    editor.setMultiLine(true);
    editor.setReadOnly(false);
    editor.setScrollbarsShown(true);
    editor.setCaretVisible(true);
    addAndMakeVisible(editor);
}

ScriptEditor::~ScriptEditor()
{
}

void ScriptEditor::load(ScriptRegistry::File* file)
{
    details.load(file);

    if (file->unit != nullptr) {
        editor.setText(file->unit->source);
    }
    else if (file->old) {
        juce::File f (file->path);
        juce::String source = f.loadFileAsString();
        editor.setText(source);
    }
}

void ScriptEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    details.setBounds(area.removeFromTop(details.getPreferredHeight()));
    editor.setBounds(area);
}
