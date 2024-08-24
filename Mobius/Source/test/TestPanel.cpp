/**
 * Panel to run tests and display results.
 *
 * Who should be in charge here.  Either
 *    - panel is in charge when it is visible and activates TestDriver
 *    - Supervisor/Test driver is in charge and asks the panel to open
 *
 * Leaning toward Supervisor control so the UI could in theory be closed while tests
 * are running.  But then we need a way to activate it, which initially will always
 * be from MainMenu.  So let's go with panel control.
 *
 */

#include <JuceHeader.h>

#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../model/UIConfig.h"
#include "../ui/JuceUtil.h"

#include "../Supervisor.h"

#include "TestDriver.h"

#include "TestPanel.h"

const int TestPanelHeaderHeight = 20;
const int TestPanelFooterHeight = 20;
const int TestPanelTestButtonHeight = 30;
const int TestPanelCommandButtonHeight = 30;

TestPanel::TestPanel(TestDriver* d)
{
    driver = d;

    addAndMakeVisible(resizer);
    resizer.setBorderThickness(juce::BorderSize<int>(4));

    // keeps the resizer from warping this out of existence
    resizeConstrainer.setMinimumHeight(20);
    resizeConstrainer.setMinimumWidth(20);

    // todo: it's not obvious how you use the constrainer to do
    // the obvious thing, keep the corners within the paranet component
    // while dragging.  I can maybe see how to do this if we could take
    // control when theh resize starts and set the constrainer so the
    // max heights are within limits relative to it's current position
    // but don't see a hook for that, and setting it here won't adapt to changes
    // in position without also resizing to get into resized()
    // maybe checkBounds(), 

    // two logs, one the full trace log and the other a summary that is much shorter
    tabs.add("Log", &rawlog);
    tabs.add("Summary", &summary);
    addAndMakeVisible(tabs);
    
    addFooter(&endButton);

    bypassButton.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::black);
    bypassButton.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    bypassButton.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::black);
    bypassButton.addListener(this);
    addAndMakeVisible(bypassButton);

    initCommandButtons();

    // test name input field
    addAndMakeVisible(&testName);

    // as large as the config panels for now, adjust this
    // and nice to make resizeable and draggable
    setSize (900, 600);
}

TestPanel::~TestPanel()
{
    GlobalTraceListener = nullptr;
}

TestButton::TestButton(Symbol* s)
{
    symbol = s;
    setButtonText(s->getName());
}

/**
 * The usual self-sizing and centering.
 */
void TestPanel::show()
{
    center();

    refreshTestButtons();

    UIConfig* config = driver->getSupervisor()->getUIConfig();
    testName.setText(config->get("testName"));
    
    setVisible(true);
    // log("Test mode activated");

    // start intercepting trace messages
    // we're the only thing doing this right now
    // so don't need to save/restore the previous value
    GlobalTraceListener = this;
}

void TestPanel::hide()
{
    GlobalTraceListener = nullptr;
    setVisible(false);
}

juce::String TestPanel::getTestName()
{
    return testName.getText();
}

/**
 * Intercepts Trace log flushes and puts them in the raw log.
 */
void TestPanel::traceEmit(const char* msg)
{
    juce::String jmsg(msg);
    
    // these will usually have a newline already so don't add another
    // one which log() will do
    rawlog.add(jmsg);

    // look for a few keywords for interesting messages
    if (jmsg.contains("ERROR") ||
        jmsg.startsWith("TestStart") ||
        jmsg.contains("Warp") ||
        jmsg.contains("Alert"))
      summary.add(jmsg);
}

/**
 * Self-centering within the parent
 * I've done this for ConfigPanels forever but don't like it,
 * especially if you want this draggable.  How do you
 * center it when it is displayed for the first time but then
 * allowed to drag?  Can't do it in the constructor because we won't
 * necessarily have a parent then or the parent won't have a size yet.
 *
 * Needs to be in JuceUtil if there isnt anything built-in to do this.
 */
void TestPanel::center()
{
    int pwidth = getParentWidth();
    int pheight = getParentHeight();
    
    int mywidth = getWidth();
    int myheight = getHeight();
    
    if (mywidth > pwidth) mywidth = pwidth;
    if (myheight > pheight) myheight = pheight;

    int left = (pwidth - mywidth) / 2;
    int top = (pheight - myheight) / 2;
    
    setTopLeftPosition(left, top);
}

void TestPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    resizer.setBounds(area);

    area.removeFromTop(TestPanelHeaderHeight);

    juce::Rectangle<int> footerArea = area.removeFromBottom(TestPanelFooterHeight);
    int fixedButtonWidth = 40;
    layoutButtonRow(footerButtons, footerArea, fixedButtonWidth, true);

    // one or more rows of variable test buttons
    layoutTestButtons(area, testButtons);

    // put the test name box under the test buttons
    // it will have sized itself
    testName.setTopLeftPosition(area.getX(), area.getY());
    area.removeFromTop(testName.getHeight());

    // row of command buttons
    juce::Rectangle<int> commandRow = area.removeFromTop(TestPanelCommandButtonHeight);
    layoutButtonRow(commandButtons, commandRow, 0, false);

    int bypassWidth = 100;
    juce::Rectangle<int> bypassArea = commandRow.removeFromRight(bypassWidth);
    bypassButton.setBounds(bypassArea);

    // log gets what's left over
    // remove a little from the left/right edge so the resize component can shine through
    area.removeFromLeft(4);
    area.removeFromRight(4);
    tabs.setBounds(area);
}

/**
 * Typical button sizing crap, surely there is an easier built-in way to do this
 * only have one right now, but support more and give them a uniform size
 * and center them at the bottom.
 */
void TestPanel::layoutButtonRow(juce::Array<juce::Button*>& buttons, juce::Rectangle<int>& area, int fixedWidth, bool center)
{
    int buttonsWidth = 0;
    for (auto button : buttons) {
        int width = fixedWidth;
        if (width == 0) {
            //juce::Font font (area.getHeight() - 4);
            //juce::Font font (area.getHeight() * 0.75);
            juce::Font font (JuceUtil::getFontf(area.getHeight() * 0.75f));
            //width = font.getStringWidth(button->getButtonText()) + 20;
            width = font.getStringWidth(button->getButtonText());
        }
        button->setSize(width, area.getHeight() - 4);
        buttonsWidth += button->getWidth();
    }

    int buttonLeft = 10;
    if (center)
      buttonLeft = (area.getWidth() / 2) - (buttonsWidth / 2);
    
    for (auto button : buttons) {
        button->setTopLeftPosition(buttonLeft, area.getY() + 2);
        buttonLeft += button->getWidth();
    }
}

/**
 * Layout the test buttons in one or more rows at the top
 * Can't seem to get these to surround the text tightly, the stock renderer seems
 * to add padding.  There will be more padding the longer the text is, getStringWidth
 * won't be accurate.
 */
void TestPanel::layoutTestButtons(juce::Rectangle<int>& area, juce::OwnedArray<TestButton>& buttons)
{
    int buttonLeft = area.getX();
    juce::Rectangle<int> testRow = area.removeFromTop(TestPanelTestButtonHeight);
    for (auto button : buttons) {
        juce::Font font (JuceUtil::getFontf(testRow.getHeight() * 0.75f));
        int width = font.getStringWidth(button->getButtonText());
        if (buttonLeft + width >= testRow.getWidth()) {
            // overflow, add another row
            testRow = area.removeFromTop(TestPanelTestButtonHeight);
            buttonLeft = testRow.getX();
        }
        button->setSize(width, testRow.getHeight() - 4);
        button->setTopLeftPosition(buttonLeft, testRow.getY() + 2);
        buttonLeft += button->getWidth();
    }
}

/**
 * wtf: I have three "rows" of buttons, two are juce::Array of static object
 * pointers and one is an OwnedArray of dynamic objects.  layoutButtonRow
 * takes a reference to a juce::Array<juce::Button*> but you can't pass
 * a juce::OwnedArray to that, I guess because OwnedArray isn't a subtype
 * of juce::Array, or maybe it's the downcase from TestButton to Button.
 * Whatever, I don't feel like spending a day in c++ container hell right now
 * so do a temporary conversion and get on with it.  Explore whether
 * you can pass an iterator into the layout method rather than the array
 * itself.
 */
void TestPanel::layoutOwnedRow(juce::OwnedArray<TestButton>& src, juce::Rectangle<int>& area)
{
    // this doesn't work, no surprise
    // juce::Array<juce::Button*> array (src);
    juce::Array<juce::Button*> array;
    for (auto button : src)
      array.add(button);
      
    layoutButtonRow(array, area, 0, false);
}

void TestPanel::paint(juce::Graphics& g)
{
    // todo: figure out how opaque components work so we dont' have to do this
    g.setColour(juce::Colours::white);
    g.fillRect(getLocalBounds());
        
    juce::Rectangle<int> area = getLocalBounds();
    juce::Rectangle<int> header = area.removeFromTop(TestPanelHeaderHeight);
    g.setColour(juce::Colours::blue);
    g.fillRect(header);
    juce::Font font (JuceUtil::getFontf(TestPanelHeaderHeight * 0.8f));
    g.setFont(font);
    g.setColour(juce::Colours::white);
    g.drawText(" Test Driver", header, juce::Justification::centred);

    juce::Rectangle<int> footer = area.removeFromBottom(TestPanelFooterHeight);
    g.setColour(juce::Colours::beige);
    g.fillRect(footer);
}

//////////////////////////////////////////////////////////////////////
// Drag
//////////////////////////////////////////////////////////////////////

// Working pretty well, but you can drag it completely out of the
// containing window.  Need to prevent dragging when it reaches some
// threshold.  If that isn't possible, let it finish, then snap it back to
// ensure at least part of it is visible.


void TestPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.getMouseDownY() < TestPanelHeaderHeight) {
        dragger.startDraggingComponent(this, e);

        // the first argu is "minimumWhenOffTheTop" set
        // this to the full height and it won't allow dragging the
        // top out of boundsa
        dragConstrainer.setMinimumOnscreenAmounts(getHeight(), 100, 100, 100);
        
        dragging = true;
    }
}

void TestPanel::mouseDrag(const juce::MouseEvent& e)
{
    // dragger.dragComponent(this, e, nullptr);
    dragger.dragComponent(this, e, &dragConstrainer);
    
    if (!dragging)
      Trace(1, "TestPanel: mosueDrag didn't think it was dragging\n");
}

// don't need any of this logic, left over from when I was learning
void TestPanel::mouseUp(const juce::MouseEvent& e)
{
    if (dragging) {
        if (e.getDistanceFromDragStartX() != 0 ||
            e.getDistanceFromDragStartY() != 0) {

            // is this the same, probably not sensitive to which button
            if (!e.mouseWasDraggedSinceMouseDown()) {
                Trace(1, "TestPanel: Juce didn't think it was dragging\n");
            }
            
            //Trace(2, "TestPanel: New location %d %d\n", getX(), getY());
            
            //area->saveLocation(this);
            dragging = false;
        }
        else if (e.mouseWasDraggedSinceMouseDown()) {
            Trace(1, "TestPanel: Juce thought we were dragging but the position didn't change\n");
        }
    }
    else if (e.mouseWasDraggedSinceMouseDown()) {
        Trace(1, "TestPanel: Juce thought we were dragging\n");
    }

    dragging = false;
}

//////////////////////////////////////////////////////////////////////
// Log
//////////////////////////////////////////////////////////////////////

/**
 * traceEmit is what puts things in rawlog and optionally
 * adds important things to the summary.
 *
 * log() will always add to the summary.
 * Here from TestDriver::mobiusMessage which is what script Echo statements
 * end up calling.
 */
void TestPanel::log(juce::String msg)
{
    summary.add(msg);
}

void TestPanel::clear()
{
    rawlog.clear();
    summary.clear();
}

//////////////////////////////////////////////////////////////////////
//
// Buttons
//
//////////////////////////////////////////////////////////////////////

/**
 * Add a bottom to the panel footer.
 */
void TestPanel::addFooter(juce::Button* b)
{
    b->addListener(this);
    addAndMakeVisible(b);
    footerButtons.add(b);
}

/**
 * Initialize the command buttons.
 */
void TestPanel::initCommandButtons()
{
    addCommandButton(&clearButton);
    addCommandButton(&installButton);
    addCommandButton(&cancelButton);
}

void TestPanel::addCommandButton(juce::Button* b)
{
    b->addListener(this);
    addAndMakeVisible(b);
    commandButtons.add(b);
}

/**
 * Build a row of text buttons to run each test script.
 */
void TestPanel::refreshTestButtons()
{
    for (auto button : testButtons)
      removeChildComponent(button);
    testButtons.clear();

    for (auto symbol : driver->getSupervisor()->getSymbols()->getSymbols()) {
        if (symbol->script && symbol->script->test) {
            addTestButton(symbol);
        }
    }

    // just changing child components does not trigger a resized
    // not sure if this is the "right way" but just do it
    resized();
}

void TestPanel::addTestButton(Symbol* s)
{
    TestButton*tb = new TestButton(s);
    testButtons.add(tb);
    
    tb->addListener(this);
    addAndMakeVisible(tb);
}

/**
 * You can turn off test mode with the main menu, but also through a
 * button in the control panel.
 * Supervisor doesn't need any special notification about this,
 * it just notices that isActive returns false.
 */
void TestPanel::buttonClicked(juce::Button* b)
{

    if (b == &endButton) {
        driver->controlPanelClosed();
    }
    else if (b == &clearButton) {
        clear();
    }
    else if (b == &installButton) {
        driver->reinstall();
    }
    else if (b == &bypassButton) {
        driver->setBypass(bypassButton.getToggleState());
    }
    else if (b == &cancelButton) {
        driver->cancel();
    }
    else {
        // must be a test button
        TestButton* tb = dynamic_cast<TestButton*>(b);
        if (tb == nullptr) {
            // must be an unhandled command button
            Trace(1, "TestPanel: Not a test button %s\n", b->getButtonText().toUTF8());
        }
        else {
            driver->runTest(tb->symbol, testName.getText());
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
