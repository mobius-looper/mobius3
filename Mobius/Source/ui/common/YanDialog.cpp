
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
    okButton.addListener(this);
    cancelButton.addListener(this);
    buttons.setCentered(true);
    buttons.add(&okButton);
    addAndMakeVisible(buttons);

    // important to do this after structuring the buttons
    // if you do it before resized() is called immediately
    // which gives the button row a size but it has no children
    // later when children are added and the dialog is resized,
    // it doesn't cascade because the size hasn't changed
    setBounds(0, 0, DefaultWidth, DefaultHeight);
}

YanDialog::~YanDialog()
{
}

void YanDialog::setListener(Listener* l)
{
    listener = l;
}

void YanDialog::setTitle(juce::String s)
{
    title = s;
}

void YanDialog::setMessage(juce::String s)
{
    message = s;
}

void YanDialog::setContent(juce::Component* c)
{
    content = c;
    addAndMakeVisible(c);
}

void YanDialog::setButtonStyle(ButtonStyle s)
{
    buttonStyle = s;
    if (s == ButtonsOkCancel)
      buttons.add(&cancelButton);
}

void YanDialog::show()
{
    JuceUtil::centerInParent(this);
    setVisible(true);
    resized();
}

void YanDialog::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(BorderWidth);

    area.removeFromBottom(2);
    buttons.setBounds(area.removeFromBottom(20));
    
    if (title.length() > 0) {
        area = area.reduced(TitleInset);
        (void)area.removeFromTop(TitleHeight);
    }

    if (content != nullptr) {
        area = area.reduced(ContentInset);
        content->setBounds(area);
    }
    
    JuceUtil::dumpComponent(this);
}

#if 0
void YanDialog::layoutButtons(juce::Rectangle<int> area)
{
    int buttonWidth = 100;
    int buttonCount = 1;
    if (buttonStyle == ButtonsOkCancel) buttonCount++;
    int buttonGap = 4;
    int buttonsWidth = (buttonWidth * buttonCount) + (buttonGap * (buttonCount - 1));
    int buttonOffset = area.getWidth() - (buttonsWidth / 2);
    int buttonLeft = area.getX() + buttonOffset;
    okButton.setBounds(buttonLeft, area.getY(), buttonWidth, area.getHeight());
    if (buttonStyle == ButtonsOkCancel) {
        buttonLeft += (buttonWidth + buttonGap);
        cancelButton.setBounds(buttonLeft, area.getY(), buttonWidth, area.getHeight());
    }
}
#endif

void YanDialog::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    g.fillAll (juce::Colours::black);
    
    if (borderColor == juce::Colour())
      borderColor = juce::Colours::yellow;

    g.setColour(borderColor);
    g.drawRect(area, BorderWidth);

    if (title.length() > 0) {
        (void)area.reduced(TitleInset);
        juce::Rectangle<int> titleArea = area.removeFromTop(TitleHeight).reduced(2);
        g.setColour(juce::Colours::white);
        g.drawText(title, titleArea, juce::Justification::centred);
    }
}

void YanDialog::buttonClicked(juce::Button* b)
{
    if (b == &okButton) {
        if (listener != nullptr)
          listener->yanDialogOk(this);
    }
    setVisible(false);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
