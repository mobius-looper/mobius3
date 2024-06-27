
#pragma once

#include "BasePanel.h"

class BindingSummary : public juce::Component, public juce::TableListBoxModel
{
  public:

    BindingSummary();
    ~BindingSummary();

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

class MidiSummaryPanel : public BasePanel
{
  public:
    
    MidiSummaryPanel() {
        setTitle("MIDI Bindings");
        setContent(&content);
        setSize(600, 600);
    }

    void showing() override {
        content.prepare(true);
    }

  private:
    BindingSummary content;
};

class KeyboardSummaryPanel : public BasePanel
{
  public:
    
    KeyboardSummaryPanel() {
        setTitle("Keyboard Bindings");
        setContent(&content);
        setSize(600, 600);
    }

    void showing() override {
        content.prepare(false);
    }

  private:
    BindingSummary content;
};

    
