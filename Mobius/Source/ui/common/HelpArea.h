/**
 * Component for displaying dynamic help text with support for HelpText catalogs.
 */

#pragma once

class HelpArea : public juce::Component
{
  public:

    HelpArea();
    ~HelpArea() {}

    void setCatalog(class HelpCatalog* cat);
    void setBackground(const juce::Colour color);
    void showText(juce::String text);
    void showHelp(juce::String key);

    void resized();
    void paint(juce::Graphics& g);

    void clear() {
        area.clear();
        lastKey = "";
    }
    
  private:

    juce::TextEditor area;
    class HelpCatalog* catalog = nullptr;
    juce::String lastKey;
    
};

