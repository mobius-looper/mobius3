
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
    okButton.addListener(this);
    cancelButton.addListener(this);
    buttonRow.setCentered(true);
    buttonRow.add(&okButton);
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

void YanDialog::setMessage(juce::String s)
{
    message = s;
}

void YanDialog::setMessageHeight(int h)
{
    messageHeight = h;
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

void YanDialog::addButton(juce::String text)
{
    buttonRow.remove(&okButton);
    juce::TextButton* b = new juce::TextButton(text);
    b->addListener(this);
    buttons.add(b);
    buttonRow.add(b);
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
        int newHeight = (height == 0) ? DefaultHeight : height;
        setSize(newWidth, newHeight);
    }
    show();
}

void YanDialog::show()
{
    resized();
    JuceUtil::centerInParent(this);
    setVisible(true);
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

    if (message.length() > 0) {
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

int YanDialog::getMessageHeight()
{
    return (messageHeight > 0) ? messageHeight : MessageHeight;
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
          g.setColour(juce::Colours::white);
        g.drawText(title, titleArea, juce::Justification::centred);
        (void)area.removeFromTop(TitleMessageGap);
    }

    if (message.length() > 0) {
        juce::Rectangle<int> messageArea = area.removeFromTop(getMessageHeight());

        g.setColour(juce::Colours::grey);
        g.fillRect(messageArea);
        
        g.setColour(juce::Colours::black);
        //g.drawText(message, messageArea, juce::Justification::centred);

        g.setFont(JuceUtil::getFont(MessageFontHeight));
        g.drawFittedText(message, messageArea.getX(), messageArea.getY(),
                         messageArea.getWidth(),messageArea.getHeight(),
                         juce::Justification::centred,
                         5, // max lines
                         1.0f);
        
    }
}

void YanDialog::buttonClicked(juce::Button* b)
{
    int ordinal = 0;
    
    if (b != &okButton)
      ordinal = buttons.indexOf(static_cast<juce::TextButton*>(b));
    
    if (listener != nullptr)
      listener->yanDialogClosed(this, ordinal);
    
    setVisible(false);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
