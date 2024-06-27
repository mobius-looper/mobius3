/**
 * Common subclass for all popup panels.
 * 
 * Panels have a title bar at the top, a row of close buttons at the bottom,
 * and a colored border.
 *
 * Panels can be dragged from the title bar, and resized at the border.
 *
 * Subclasses should put child components within the content component which
 * will be sized to fit within the borders and header/footer components.
 *
 * ConfigPanel should eventually use this but it is older and needs more
 * refactoring.
 *
 * There are two ways a subclass can add content to the middle of the panel:
 *    - direct with resized() override
 *    - content subcomponent
 *
 * Unclear what the best way is, try both.
 *
 * With a content subcomponent, the subclass defines a juce::Compoennt to
 * hold the content with it's own resize(),  paint(), and whatnot then
 * calls BasePanel::setContent.  BasePanel will call setBounds on that
 * component as it is resized.
 *
 * With direct, there is no content component, the subclass adds children
 * directly to the BasePanel and just override resized() to to position them.
 * It has to call up to BasePanel::reaized() to do the header/footer/border.
 * The area left for content will be left in contentArea.
 *
 * todo: for simple panels with just an "Ok" button to close, it would save
 * space and look better to have a window-style X in the upper right of the title bar.
 * 
 */

#pragma once

#include <JuceHeader.h>

// move this to ui/common
#include "../test/BasicButtonRow.h"

class BasePanel : public juce::Component, public juce::Button::Listener
{
  public:

    static const int HeaderHeight = 24;
    static const int FooterHeight = 24;
    static const int BorderWidth = 4;

    BasePanel();
    virtual ~BasePanel();

    int getId() {
        return id;
    }
    void setId(int i) {
        id = i;
    }

    void setContent(juce::Component* c);
    void addButton(juce::Button* b);

    // panels should have a title, could make this part of the constructor
    void setTitle(juce::String title);

    // todo: flexible buttons
    // by default panels have a single Ok button

    // panels by default have a white border, alert panels will be yellow
    void setAlert();
    void setBorderColor(juce::Colour c);
    void followContentMouse();

    // make the panel visible
    // won't be necessary once PanelFactory is done...
    void show();
    void close();

    // subclass overrides this if it has more preparations to make
    virtual void showing();
    virtual void hiding();
    virtual void update();
    virtual void footerButton(juce::Button* b);
    
    // Component
    void resized() override;
    void paint(juce::Graphics& g) override;

    // ButtonListener
    void buttonClicked(juce::Button* b) override;

    // Drag and Resize
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

    
  private:

    int id = 0;

    juce::String title;
    juce::Colour borderColor;
    BasicButtonRow closeButtons;
    juce::TextButton okButton {"OK"};
    juce::Component* contentComponent = nullptr;
    juce::Rectangle<int> contentArea;

    juce::ComponentBoundsConstrainer dragConstrainer;
    juce::ComponentBoundsConstrainer resizeConstrainer;
    juce::ResizableBorderComponent resizer {this, &resizeConstrainer};
    juce::ComponentDragger dragger;
    bool dragging = false;
    bool shown = false;

};
