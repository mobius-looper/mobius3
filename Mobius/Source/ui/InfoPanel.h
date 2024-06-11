
#pragma once

#include "BasePanel.h"

class InfoContent : public juce::Component, public juce::TableListBoxModel
{
  public:

    InfoContent();
    ~InfoContent();

    void prepare(bool midi);
    
    void resized() override;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber,
                            int /*width*/, int /*height*/,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    
  private:

    bool midi = false;
    juce::Array<class Binding*> things;
    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};

    void addBindings(class BindingSet* set);
    void initTable();
    void initColumns();
    juce::String getCellText(int row, int columnId);
    juce::String renderMidiTrigger(class Binding* b);
};

class InfoPanel : public BasePanel
{
  public:

    InfoPanel() {
        setContent(&content);
        setSize(600, 600);
    }
    ~InfoPanel() {}

    void showMidi() {
        if (!isVisible()) {
            setTitle("MIDI Bindings");
            content.prepare(true);
            resized();
            show();
        }
    }
    
    void showKeyboard() {
        if (!isVisible()) {
            setTitle("Keyboard Bindings");
            content.prepare(false);
            resized();
            show();
        }
    }
    
  private:

    InfoContent content;
};

    