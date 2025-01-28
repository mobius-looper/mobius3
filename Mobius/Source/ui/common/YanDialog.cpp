
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
        if (form.getParentComponent() == nullptr)
          addAndMakeVisible(form);
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
    int height = BorderWidth + 4 + 20 + TitleInset + TitleHeight + TitleMessageGap;
    height += getMessageHeight();

    if (DefaultHeight > height)
      height = DefaultHeight;
    
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
    area = area.reduced(BorderWidth);

    area.removeFromBottom(4);
    buttonRow.setBounds(area.removeFromBottom(20));
    
    if (title.length() > 0) {
        area = area.reduced(TitleInset);
        (void)area.removeFromTop(TitleHeight);
    }

    if (messages.size() > 0) {
        if (title.length() > 0)
          (void)area.removeFromTop(TitleMessageGap);
        (void)area.removeFromTop(getMessageHeight());
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
    
    g.fillAll (juce::Colours::black);
    
    if (borderColor == juce::Colour())
      borderColor = juce::Colours::yellow;

    g.setColour(borderColor);
    g.drawRect(area, BorderWidth);

    area = area.reduced(BorderWidth);
    
    if (title.length() > 0) {
        area = area.reduced(TitleInset);
        juce::Rectangle<int> titleArea = area.removeFromTop(TitleHeight);
        g.setFont(JuceUtil::getFont(TitleHeight));
        if (serious)
          g.setColour(juce::Colours::red);
        else
          g.setColour(juce::Colours::green);
        g.drawText(title, titleArea, juce::Justification::centred);
        (void)area.removeFromTop(TitleMessageGap);
    }

    if (messages.size() > 0) {
        juce::Rectangle<int> messageArea = area.removeFromTop(getMessageHeight());

        // temporary background to test layout
        g.setColour(juce::Colours::darkgrey);
        g.fillRect(messageArea);

        g.setColour(juce::Colours::white);
        g.setFont(JuceUtil::getFont(MessageFontHeight));
        int top = messageArea.getX();
        for (auto msg : messages) {
            g.drawFittedText(msg, top, messageArea.getY(),
                             messageArea.getWidth(), MessageFontHeight,
                             juce::Justification::centred,
                             1, // max lines
                             1.0f);
            top += MessageFontHeight;
        }
    }
}

void YanDialog::buttonClicked(juce::Button* b)
{
    int ordinal = buttons.indexOf(b);
    if (listener != nullptr)
      listener->yanDialogClosed(this, ordinal);

    setVisible(false);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
