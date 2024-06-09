
#pragma once

class InfoFooter : public juce::Component
{
  public:
    InfoFooter() {}
    ~InfoFooter() {}
};
    

class InfoPanel : public juce::Component, juce::Button::Listener, public juce::TableListBoxModel
{
  public:

    InfoPanel();
    ~InfoPanel();

    void setContent(juce::Component* content);
    void show(bool doMidi = false);

    void resized() override;
    void paint(juce::Graphics& g) override;
    void buttonClicked(juce::Button* b) override;

    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber,
                            int /*width*/, int /*height*/,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;

    // drag and resize
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
  private:

    bool midi = false;
    juce::Array<class Binding*> things;

    juce::TableListBox table { {} /* component name */, this /* TableListBoxModel */};
    InfoFooter footer;
    juce::TextButton okButton {"OK"};

    int centerLeft(juce::Component& c);
    int centerLeft(juce::Component* container, juce::Component& c);
    int centerTop(juce::Component* container, juce::Component& c);
    void centerInParent(juce::Component& c);
    void centerInParent();

    void addBindings(class BindingSet* set);
    void initTable();
    void initColumns();
    juce::String getCellText(int row, int columnId);
    juce::String renderMidiTrigger(class Binding* b);

    // I'm tired of duplicating this in a bunch of places
    // factor out a common superclass so we can share this shit
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ComponentBoundsConstrainer dragConstrainer;
    juce::ResizableBorderComponent resizer {this, &resizeConstrainer};
    juce::ComponentDragger dragger;
    bool dragging = false;
};

