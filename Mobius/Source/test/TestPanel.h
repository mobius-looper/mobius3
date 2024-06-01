/**
 * Control panel user interface for the test driver.
 * Pops up over the main display.
 */

#pragma once

#include "BasicTabs.h"
#include "BasicLog.h"
#include "BasicInput.h"

class TestPanel : public juce::Component, public juce::Button::Listener,
                  public TraceListener
{
  public:

    TestPanel(class TestDriver* driver);
    ~TestPanel();

    void show();
    void hide();
    void refreshTestButtons();
    juce::String getTestName();
    
    void resized() override;
    void paint(juce::Graphics& g) override;

    void log(juce::String msg);
    void clear();

    // TraceListener
    void traceEmit(const char* msg) override;
    
    // Button::Listener
    void buttonClicked(juce::Button* b) override;
    
    // drag and resize
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
  private:

    class TestDriver* driver;

    BasicTabs tabs;
    BasicLog rawlog;
    BasicLog summary;
    BasicInput testName {"Test", 20};
    
    juce::Array<juce::Button*> footerButtons;
    juce::TextButton endButton {"End"};

    juce::Array<juce::Button*> commandButtons;
    juce::TextButton installButton {"Reinstall"};
    juce::TextButton clearButton {"Clear"};
    juce::TextButton cancelButton {"Cancel"};

    juce::ToggleButton bypassButton {"Bypass"};
    bool bypass = false;
    
    juce::OwnedArray<class TestButton> testButtons;

    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ComponentBoundsConstrainer dragConstrainer;
    juce::ResizableBorderComponent resizer {this, &resizeConstrainer};
    juce::ComponentDragger dragger;
    bool dragging = false;

    // component layout
    void layoutButtonRow(juce::Array<juce::Button*>& buttons, juce::Rectangle<int>& area, int fixedWidth, bool center);
    void layoutOwnedRow(juce::OwnedArray<TestButton>& src, juce::Rectangle<int>& area);
    void layoutTestButtons(juce::Rectangle<int>& area, juce::OwnedArray<TestButton>& src);

    // button management
    void addFooter(juce::Button* b);
    void initCommandButtons();
    void addCommandButton(juce::Button* b);
    void addTestButton(class Symbol* s);
    
    void center();

};

class TestButton : public juce::TextButton
{
  public:

    TestButton(class Symbol* s);
    
    class Symbol* symbol = nullptr;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
