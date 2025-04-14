
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

void YanDialog::setWidth(int w)
{
    requestedWidth = w;
}

void YanDialog::setHeight(int h)
{
    requestedHeight = h;
}

void YanDialog::setTitleHeight(int h)
{
    titleHeight = h;
}

void YanDialog::setTitleColor(juce::Colour c)
{
    titleColor = c;
}

void YanDialog::setMessageColor(juce::Colour c)
{
    warningColor = c;
}

void YanDialog::setWarningColor(juce::Colour c)
{
    warningColor = c;
}

void YanDialog::setErrorColor(juce::Colour c)
{
    errorColor = c;
}

void YanDialog::setTitleGap(int h)
{
    titleGap = h;
}

void YanDialog::setMessageHeight(int h)
{
    messageHeight = h;
}

void YanDialog::setWarningHeight(int h)
{
    warningHeight = h;
}

void YanDialog::setErrorHeight(int h)
{
    errorHeight = h;
}

void YanDialog::setSectionHeight(int h)
{
    sectionHeight = h;
}

void YanDialog::setButtonGap(int h)
{
    buttonGap = h;
}

void YanDialog::setSerious(bool b)
{
    serious = b;
}

void YanDialog::setTitle(juce::String s)
{
    title = s;
}

void YanDialog::reset()
{
    title = "";
    messages.clear();
    warnings.clear();
    errors.clear();
    clearButtons();
    addButton("Ok");

    form.clear();
    if (form.getParentComponent() != nullptr) {
        form.getParentComponent()->removeChildComponent(&form);
    }
}

void YanDialog::clearMessages()
{
    messages.clear();
}

void YanDialog::addMessage(juce::String s)
{
    Message m (s);
    messages.add(m);
}

void YanDialog::addMessage(Message m)
{
    messages.add(m);
}

void YanDialog::addMessageGap(int height)
{
    Message m;
    m.messageHeight = height;
    messages.add(m);
}

void YanDialog::setMessage(juce::String s)
{
    messages.clear();
    addMessage(s);
}

void YanDialog::addWarning(juce::String s)
{
    Message m (s);
    warnings.add(m);
}

void YanDialog::addError(juce::String s)
{
    Message m (s);
    errors.add(m);
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
    buttonRow.clear();
    while (buttons.size() > 0) {
        juce::Button* b = buttons.removeAndReturn(0);
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
        form.forceResize();
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

    int width = (requestedWidth > 0) ? requestedWidth : getWidth();
    int height = (requestedHeight > 0) ? requestedHeight : getHeight();
    if (width == 0 || height == 0) {
        int newWidth = (width == 0) ? DefaultWidth : width;
        int newHeight = (height == 0) ? getDefaultHeight() : height;
        setSize(newWidth, newHeight);
    }
    show();
}

int YanDialog::getPreferredHeight()
{
    // dialog is sournded by a border and an inset
    // always with a gapped button row at the bottom
    int height = (BorderWidth * 2) + (MainInset * 2) + BottomGap + ButtonHeight + buttonGap;

    // title and leading messages
    height += getSpaceBeforeContent();
    
    // content may be representd with an arbitrary component or a YanForm
    // if either are present, a content inset is added
    int cheight = 0;
    if (content != nullptr) {
        cheight = content->getHeight();
        if (cheight == 0)
          cheight = DefaultContentHeight;
    }
    else if (form.getParentComponent() != nullptr) {
        cheight = form.getPreferredHeight();
    }

    if (cheight > 0) {
        height += cheight + (ContentInset * 2);
        // and a trailing gap if anything follows it
        if (errors.size() > 0 || warnings.size() > 0) {
            height += sectionGap;
        }
    }
    
    // errors and warnings are titled message sections
    if (errors.size() > 0) {
        height += getMessageSectionHeight(true, errors, errorHeight);
        // and a trailing gap if anything follows it
        if (warnings.size() > 0)
          height += sectionGap;
    }
    
    if (warnings.size() > 0) {
        height += getMessageSectionHeight(true, warnings, warningHeight);
    }
        
    return height;
}

int YanDialog::getHeightUpToContent()
{
    

int YanDialog::getMessageSectionHeight(bool hasTitle, Message& messages, int defaultHeight)
{
    int height = 0;

    if (hasTitle) {
        height += (sectionHeight + sectionGap);
    }

    for (auto m : messages) {

        int max = 0;

        if (m.prefixHeight > 0)
          max = m.prefixHeight;

        if (m.messageHeight > 0 && m.messageHeight > max)
          max = m.messageHeight;

        if (max == 0)
          max = defaultHeight;

        height += max;
    }

    return height;
}

int YanDialog::getSpaceBeforeContent()
{
    int height = 0;
    
    // title sits directly under the MainInset but has a trailing gap
    if (title.length() > 0)
      height += titleHeight + titleGap;

    // messages are variable with no title, and a trailing gap
    if (messages.size() > 0) {
        height += getMessageSectionHeight(false, messages, messageHeight);
        // technically this is only necessary if there is something following
        // it, but there almost always is, so leave it in unconditionally
        height += MessagePostGap;
    }

    return height;
}

/**
 * The only things that are Components are the ButtonRow
 * and the content or YanForm, everything else is drawn around it.
 *
 * The content and/or form is expected to size itself, we just set the top/left
 */
void YanDialog::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(BorderWidth + MainInset);

    area.removeFromBottom(BottomGap);
    buttonRow.setBounds(area.removeFromBottom(ButtonHeight));
    
    // often the same size on redisplay, force a refresh
    buttonRow.resized();

    // title and messages
    (void)area.removeFromTop(getSpaceBeforeContent());

    // content inset
    area = area.reduced(ContentInset);
    
    if (content != nullptr) {
        cheight = content->getHeight();
        // should have set this in getPreferredHeight
        if (cheight == 0)
          cheight = DefaultContentHeight;

        content->setBounds(area.getX(), area.getY(), area->getWidth(), cheight);
    }
    else if (form.getParentComponent() != nullptr) {
        form->setBounds(area.getX(), area.getY(), area->getWidth(), form.getPreferredHeight());
        // often the same so force a resize
        form->forceResize();
    }
    
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
        juce::Rectangle<int> titleArea = area.removeFromTop(titleHeight);
        g.setFont(JuceUtil::getFont(titleHeight));
        if (serious)
          g.setColour(juce::Colours::red);
        else
          g.setColour(titleColor);
        g.drawText(title, titleArea, juce::Justification::centred);
        (void)area.removeFromTop(titleGap);
    }

    if (messages.size() > 0) {
        juce::Rectangle<int> messageArea = area.removeFromTop(getMessagesHeight());

        // temporary background to test layout
        //g.setColour(juce::Colours::darkgrey);
        //g.fillRect(messageArea);

        int top = messageArea.getY();
        for (auto msg : messages) {
            renderMessage(g, msg, messageArea, top, messageHeight, messageColor);
            top += messageHeight;
        }
    }

    // leave space for the buttons
    juce::Rectangle<int> bottomArea = getLocalBounds();
    bottomArea = bottomArea.reduced(BorderWidth + MainInset);
    bottomArea.removeFromBottom(BottomGap + ButtonHeight + buttonGap);
    
    if (warnings.size() > 0) {
        int warnHeight = sectionHeight + sectionGap + getWarningsHeight();
        
        juce::Rectangle<int> warningArea = bottomArea.removeFromBottom(warnHeight);

        g.setColour(juce::Colours::yellow);
        g.setFont(JuceUtil::getFont(sectionHeight));
        int top = warningArea.getY();
        g.drawFittedText("Warning", warningArea.getX(), top,
                         warningArea.getWidth(), sectionHeight,
                         juce::Justification::centred, 1);
        top += (sectionHeight + sectionGap);

        for (auto msg : warnings) {
            renderMessage(g, msg, warningArea, top, warningHeight, warningColor);
            top += warningHeight;
        }
    }
    
    if (errors.size() > 0) {
        int errHeight = sectionHeight + sectionGap + getErrorsHeight();
        if (warnings.size() > 0)
          errHeight += sectionGap;
        
        juce::Rectangle<int> errorArea = bottomArea.removeFromBottom(errHeight);

        g.setColour(juce::Colours::red);
        g.setFont(JuceUtil::getFont(sectionHeight));
        int top = errorArea.getY();
        g.drawFittedText("Error", errorArea.getX(), top,
                         errorArea.getWidth(), sectionHeight,
                         juce::Justification::centred, 1);
        top += (sectionHeight + sectionGap);

        for (auto msg : errors) {
            renderMessage(g, msg, errorArea, top, errorHeight, errorColor);
            top += errorHeight;
        }
    }
}

void YanDialog::renderMessage(juce::Graphics& g, Message& m, juce::Rectangle<int>& area, int top, int height, juce::Colour defaultColor)
{
    if (m.prefix.length() > 0) {
        juce::Font f (JuceUtil::getFont(height));
        int pwidth = f.getStringWidth(m.prefix);
        int mwidth = f.getStringWidth(m.message);
        int gap = 12;
        int totalWidth = pwidth + mwidth + gap;
        int left = (area.getWidth() - totalWidth) / 2;
        if (left < 0) {
            left = 0;
            mwidth = area.getWidth() - pwidth - gap;
        }

        juce::Colour c = m.prefixColor;
        if (c == juce::Colour()) c = juce::Colours::blue;
        g.setColour(c);
        g.setFont(f);
        g.drawFittedText(m.prefix, left, top, pwidth, height,
                         juce::Justification::bottomLeft, 1);
        
        left += (pwidth + gap);

        c = m.messageColor;
        if (c == juce::Colour()) c = juce::Colours::white;
        g.setColour(c);
        int mheight = height;
        if (m.messageHeight > 0) mheight = m.messageHeight;
        g.setFont(JuceUtil::getFont(mheight));
        g.drawFittedText(m.message, left, top, mwidth, height,
                         juce::Justification::bottomLeft, 1);
    }
    else {
        g.setColour(defaultColor);
        g.setFont(JuceUtil::getFont(height));
        g.drawFittedText(m.message, area.getX(), top,
                         area.getWidth(), height,
                         juce::Justification::centred, 1);
    }
}

void YanDialog::buttonClicked(juce::Button* b)
{
    // things like Tasks that reuse the same dialog in a sequence
    // may want to adjust it and show it again in the close handler
    // so remove it first, so it can be added back
    // just hiding it isn't good, if the window reorganizes
    // while it is hidden it can mess up the z-order making the dialog
    // invisible
    //setVisible(false);
    getParentComponent()->removeChildComponent(this);

    int ordinal = buttons.indexOf(b);
    if (listener != nullptr)
      listener->yanDialogClosed(this, ordinal);
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
