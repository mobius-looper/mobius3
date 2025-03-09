
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"

#include "YanDialog.h"

YanDialog::YanDialog()
{
    init();
}

YanDialog::YanDialog(Listener* l)
{
    listener = l;
    init();
}

void YanDialog::init()
{
    // always start with an Ok button
    buttonRow.setCentered(true);
    addAndMakeVisible(buttonRow);
}

YanDialog::~YanDialog()
{
}

void YanDialog::setId(int i)
{
    id = i;
}

int YanDialog::getId()
{
    return id;
}

void YanDialog::setListener(Listener* l)
{
    listener = l;
}

void YanDialog::setSerious(bool b)
{
    serious = b;
}

void YanDialog::setTitle(juce::String s)
{
    title = s;
}

void YanDialog::clearMessages()
{
    messages.clear();
}

void YanDialog::addMessage(juce::String s)
{
    messages.add(s);
}

void YanDialog::setMessage(juce::String s)
{
    messages.clear();
    messages.add(s);
}

void YanDialog::setContent(juce::Component* c)
{
    if (form.getParentComponent() != nullptr) {
        Trace(1, "YanDialog: Attempt to set content after internal form was added");
    }
    else {
        content = c;
        addAndMakeVisible(c);
    }
}

void YanDialog::addField(YanField* f)
{
    if (content != nullptr) {
        Trace(1, "YanDialog: Attempt to add fields after content was assigned");
    }
    else {
        form.add(f);
        if (form.getParentComponent() == nullptr) {
            addAndMakeVisible(form);
        }
    }
}

void YanDialog::clearButtons()
{
    buttonNames.clear();
    while (buttons.size() > 0) {
        juce::Button* b = buttons.removeAndReturn(0);
        buttonRow.remove(b);
        delete b;
    }
}

void YanDialog::addButton(juce::String text)
{
    juce::TextButton* b = new juce::TextButton(text);
    b->addListener(this);
    buttonNames.add(text);
    buttons.add(b);
    buttonRow.add(b);
}

void YanDialog::setButtons(juce::String csv)
{
    clearButtons();
    juce::StringArray list = juce::StringArray::fromTokens(csv, ",", "");
    for (auto name : list)
      addButton(name);
}

void YanDialog::show()
{
    if (getParentComponent() == nullptr) {
        Trace(1, "YanDialog: Parent component not set");
    }
    else if (buttonNames.size() == 0) {
        Trace(1, "YanDialog: Dialog has no close buttons");
    }
    else {
        resized();
        JuceUtil::centerInParent(this);
        setVisible(true);
    }
}

void YanDialog::show(juce::Component* parent)
{
    juce::Component* current = getParentComponent();
    if (current != parent) {
        if (current == nullptr)
          parent->addAndMakeVisible(this);
        else {
            Trace(2, "YanDialog: Reparenting dialog");
            current->removeChildComponent(this);
            parent->addAndMakeVisible(this);
        }
    }

    int width = getWidth();
    int height = getHeight();
    if (width == 0 || height == 0) {
        int newWidth = (width == 0) ? DefaultWidth : width;
        int newHeight = (height == 0) ? getDefaultHeight() : height;
        setSize(newWidth, newHeight);
    }
    show();
}

int YanDialog::getDefaultHeight()
{
    int height = (BorderWidth * 2) + (MainInset * 2) + BottomGap + ButtonHeight + ButtonGap;

    if (title.length() > 0)
      height += TitleHeight + TitlePostGap;

    int mheight = getMessageHeight();
    if (mheight > 0) {
        height += mheight + MessagePostGap;
    }

    // user defined content, this may be our form or something custom
    int cheight = 0;
    if (content != nullptr) {
        cheight = content->getHeight();
        if (cheight == 0)
          cheight = DefaultContentHeight;
    }
    else if (form.getParentComponent() != nullptr) {
        cheight = form.getPreferredHeight();
    }
    if (cheight > 0)
      height += cheight + (ContentInset * 2);

    return height;
}

/**
 * Splitting up messages is still experimental.
 * If there is more than one message, then the message array lenght
 * defines the number of rows.
 * Otherwise the user may specify a number of rows, and there is a minimum number.
 */
int YanDialog::getMessageHeight()
{
    return (messages.size() * MessageFontHeight);
}

void YanDialog::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(BorderWidth + MainInset);

    area.removeFromBottom(BottomGap);
    buttonRow.setBounds(area.removeFromBottom(ButtonHeight));
    (void)area.removeFromBottom(ButtonGap);
    
    if (title.length() > 0) {
        (void)area.removeFromTop(TitleHeight + TitlePostGap);
    }

    if (messages.size() > 0) {
        int mheight = getMessageHeight();
        (void)area.removeFromTop(mheight + MessagePostGap);
    }

    area = area.reduced(ContentInset);
    if (content != nullptr)
      content->setBounds(area);
    else
      form.setBounds(area);
    
    //JuceUtil::dumpComponent(this);
}

void YanDialog::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    g.fillAll(juce::Colours::black);
    
    if (borderColor == juce::Colour())
      borderColor = juce::Colours::white;

    g.setColour(borderColor);
    g.drawRect(area, BorderWidth);

    area = area.reduced(BorderWidth + MainInset);
    
    if (title.length() > 0) {
        juce::Rectangle<int> titleArea = area.removeFromTop(TitleHeight);
        g.setFont(JuceUtil::getFont(TitleHeight));
        if (serious)
          g.setColour(juce::Colours::red);
        else
          g.setColour(juce::Colours::green);
        g.drawText(title, titleArea, juce::Justification::centred);
        (void)area.removeFromTop(TitlePostGap);
    }

    if (messages.size() > 0) {
        juce::Rectangle<int> messageArea = area.removeFromTop(getMessageHeight());

        // temporary background to test layout
        //g.setColour(juce::Colours::darkgrey);
        //g.fillRect(messageArea);

        g.setColour(juce::Colours::white);
        g.setFont(JuceUtil::getFont(MessageFontHeight));
        int top = messageArea.getY();
        for (auto msg : messages) {
            g.drawFittedText(msg, messageArea.getX(), top,
                             messageArea.getWidth(), MessageFontHeight,
                             juce::Justification::centred, 1);
            top += MessageFontHeight;
        }
    }
}

void YanDialog::buttonClicked(juce::Button* b)
{
    int ordinal = buttons.indexOf(b);
    if (listener != nullptr)
      listener->yanDialogClosed(this, ordinal);

    // just hiding it isn't good, if the window reorganizes
    // while it is hidden it can mess up the z-order making the dialog
    // invisible
    //setVisible(false);
    getParentComponent()->removeChildComponent(this);
}

void YanDialog::cancel()
{
    juce::Component* parent = getParentComponent();
    if (parent != nullptr)
      parent->removeChildComponent(this);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
