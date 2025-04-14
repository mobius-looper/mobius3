
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

    // no overrides for these
    warnings.title = "Warning";
    warnings.titleColor = juce::Colours::yellow;
    errors.title = "Error";
    errors.titleColor = juce::Colours::red;

    addChildComponent(title);
    addChildComponent(messages);
    addChildComponent(content);
    addChildComponent(errors);
    addChildComponent(warnings);
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

void YanDialog::setTestBorders(juce::Colour c)
{
    title.borderColor = c;
    content.borderColor = c;
    messages.borderColor = c;
    warnings.borderColor = c;
    errors.borderColor = c;
}

//////////////////////////////////////////////////////////////////////
//
// Overrides
//
//////////////////////////////////////////////////////////////////////

void YanDialog::setWidth(int w)
{
    requestedWidth = w;
}

void YanDialog::setHeight(int h)
{
    requestedHeight = h;
}

void YanDialog::setSerious(bool b)
{
    if (b)
      title.color = juce::Colours::red;
    else
      title.color = juce::Colours::green;
}

void YanDialog::setBorderColor(juce::Colour c)
{
    borderColor = c;
}

void YanDialog::setTitle(juce::String s)
{
    title.setVisible(true);
    title.title = s;
}

void YanDialog::setTitleHeight(int h)
{
    title.height = h;
}

void YanDialog::setTitleColor(juce::Colour c)
{
    title.color = c;
}

void YanDialog::setTitleGap(int h)
{
    title.postGap = h;
}

void YanDialog::setMessageHeight(int h)
{
    messages.messageHeight = h;
}

void YanDialog::setWarningHeight(int h)
{
    warnings.messageHeight = h;
}

void YanDialog::setErrorHeight(int h)
{
    errors.messageHeight = h;
}

void YanDialog::setSectionTitleHeight(int h)
{
    warnings.titleHeight = h;
    errors.titleHeight = h;
}

void YanDialog::setButtonGap(int h)
{
    buttonGap = h;
}

void YanDialog::setSectionGap(int h)
{
    // sure would be nice if inner classes could reference things
    // in the outer class like Java, C++ surely must be able to do this?
    content.postGap = h;
    messages.postGap = h;
    warnings.postGap = h;
    errors.postGap = h;
}

//////////////////////////////////////////////////////////////////////
//
// Additions
//
//////////////////////////////////////////////////////////////////////

void YanDialog::reset()
{
    title.clear();
    content.clear();
    messages.clear();
    warnings.clear();
    errors.clear();
    clearButtons();
    addButton("Ok");

    title.setVisible(false);
    messages.setVisible(false);
    content.setVisible(false);
    errors.setVisible(false);
    warnings.setVisible(false);
}

void YanDialog::clearMessages()
{
    messages.clear();
}

void YanDialog::addMessage(juce::String s)
{
    messages.setVisible(true);
    Message m (s);
    messages.add(m);
}

void YanDialog::addMessage(Message m)
{
    messages.setVisible(true);
    messages.add(m);
}

void YanDialog::addMessageGap(int height)
{
    messages.setVisible(true);
    Message m;
    m.messageHeight = height;
    messages.add(m);
}

void YanDialog::setMessage(juce::String s)
{
    messages.setVisible(true);
    messages.clear();
    addMessage(s);
}

void YanDialog::addWarning(juce::String s)
{
    warnings.setVisible(true);
    Message m (s);
    warnings.add(m);
}

void YanDialog::addError(juce::String s)
{
    errors.setVisible(true);
    Message m (s);
    errors.add(m);
}

void YanDialog::setContent(juce::Component* c)
{
    content.setVisible(true);
    content.setContent(c);
}

void YanDialog::addField(YanField* f)
{
    content.setVisible(true);
    content.addField(f);
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

//////////////////////////////////////////////////////////////////////
//
// Display and Layout
//
//////////////////////////////////////////////////////////////////////

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

    int width = requestedWidth;
    if (width == 0)
      width = DefaultWidth;

    int height = getPreferredHeight();
    setSize(width, height);
    show();

    //JuceUtil::dumpComponent(this);
}

int YanDialog::getPreferredHeight()
{
    int height = (BorderWidth * 2) + (MainInset * 2);

    height += title.getPreferredHeight();

    height += messages.getPreferredHeight();

    height += content.getPreferredHeight();

    height += errors.getPreferredHeight();

    height += warnings.getPreferredHeight();

    height += (buttonGap + ButtonHeight + ButtonBottomGap);
    
    return height;
}

void YanDialog::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(BorderWidth + MainInset);

    area.removeFromBottom(ButtonBottomGap);
    buttonRow.setBounds(area.removeFromBottom(ButtonHeight));
    // often the same size on redisplay, force a refresh
    buttonRow.resized();

    title.setBounds(area.removeFromTop(title.getPreferredHeight()));
    messages.setBounds(area.removeFromTop(messages.getPreferredHeight()));
    content.setBounds(area.removeFromTop(content.getPreferredHeight()));
    errors.setBounds(area.removeFromTop(errors.getPreferredHeight()));
    warnings.setBounds(area.removeFromTop(warnings.getPreferredHeight()));
}

void YanDialog::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();
    
    g.fillAll(juce::Colours::black);
    
    if (borderColor == juce::Colour())
      borderColor = juce::Colours::white;

    g.setColour(borderColor);
    g.drawRect(area, BorderWidth);
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

//////////////////////////////////////////////////////////////////////
//
// Title
//
//////////////////////////////////////////////////////////////////////

void YanDialog::TitleSection::clear()
{
    title = "";
}

int YanDialog::TitleSection::getPreferredHeight()
{
    int componentHeight = 0;
    if (title.length() > 0) {
        componentHeight = height;
        componentHeight += postGap;
    }
    return componentHeight;
}

void YanDialog::TitleSection::paint(juce::Graphics& g)
{
    if (title.length() > 0) {
        juce::Rectangle<int> area = getLocalBounds();
        if (borderColor != juce::Colour()) {
            g.setColour(borderColor);
            g.drawRect(area, BorderWidth);
        }
        area.removeFromBottom(postGap);

        if (backgroundColor != juce::Colour()) {
            g.setColour(backgroundColor);
            g.fillRect(area);
        }

        g.setFont(JuceUtil::getFont(area.getHeight()));
        g.setColour(color);
        g.drawText(title, area, juce::Justification::centred);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Messages
//
//////////////////////////////////////////////////////////////////////

void YanDialog::MessageSection::add(Message& m)
{
    messages.add(m);
}

void YanDialog::MessageSection::clear()
{
    messages.clear();
}

int YanDialog::MessageSection::getPreferredHeight()
{
    int height = 0;

    if (messages.size() > 0) {
        if (title.length() > 0) {
            height += (titleHeight + titlePostGap);
        }

        for (auto m : messages) {
            height += getMessageHeight(m);
        }
    }

    if (height > 0)
      height += postGap;
    
    return height;
}

int YanDialog::MessageSection::getMessageHeight(Message& m)
{
    int height = 0;
    
    // copy the default heights over so we don't have to keep testing every time
    if (m.prefix.length() > 0) {
        if (m.prefixHeight == 0)
          m.prefixHeight = messageHeight;
        height = m.prefixHeight;
    }

    if (m.message.length() > 0) {
        if (m.messageHeight == 0)
          m.messageHeight = messageHeight;
        
        if (m.messageHeight > height)
          height = m.messageHeight;
    }
    return height;
}

void YanDialog::MessageSection::paint(juce::Graphics& g)
{
    if (messages.size() > 0) {
        juce::Rectangle<int> area = getLocalBounds();

        if (borderColor != juce::Colour()) {
            g.setColour(borderColor);
            g.drawRect(area, BorderWidth);
        }

        if (title.length() > 0) {
            juce::Rectangle<int> titleArea = area.removeFromTop(titleHeight);
            g.setColour(titleColor);
            g.setFont(JuceUtil::getFont(titleHeight));
            g.drawFittedText(title, titleArea.getX(), titleArea.getY(),
                             titleArea.getWidth(), titleArea.getHeight(),
                             juce::Justification::centred, 1);
            area.removeFromTop(titlePostGap);
        }

        int top = area.getY();
        for (auto m : messages) {

            int mheight = getMessageHeight(m);
            int left = 0;
            int pwidth = 0;
            int mwidth = 0;
            int prefixGap = 8;
            
            if (m.prefix.length() > 0) {
                juce::Font f (JuceUtil::getFont(m.prefixHeight));
                pwidth = f.getStringWidth(m.prefix);
            }

            if (m.message.length() > 0) {
                juce::Font f (JuceUtil::getFont(m.messageHeight));
                mwidth = f.getStringWidth(m.message);
            }

            int totalWidth = mwidth;
            if (pwidth > 0)
              totalWidth += (pwidth + prefixGap);
            
            left = (area.getWidth() - totalWidth) / 2;
            if (left < 0) {
                left = 0;
                mwidth = area.getWidth();
                if (pwidth > 0)
                  mwidth -= (pwidth + prefixGap);
            }

            if (pwidth > 0) {
                juce::Colour c = m.prefixColor;
                if (c == juce::Colour()) c = juce::Colours::blue;
                g.setColour(c);
                juce::Font f (JuceUtil::getFont(m.prefixHeight));
                g.setFont(f);
                g.drawFittedText(m.prefix, left, top, pwidth, mheight,
                                 juce::Justification::bottomLeft, 1);
        
                left += (pwidth + prefixGap);
            }

            if (mwidth > 0) {
                juce::Colour c = m.messageColor;
                if (c == juce::Colour()) c = juce::Colours::white;
                g.setColour(c);
                juce::Font f (JuceUtil::getFont(m.messageHeight));
                g.setFont(JuceUtil::getFont(m.messageHeight));
                g.drawFittedText(m.message, left, top, mwidth, mheight,
                                 juce::Justification::centredLeft, 1);
            }

            top += mheight;
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Content
//
//////////////////////////////////////////////////////////////////////

void YanDialog::ContentSection::addField(YanField* f)
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

void YanDialog::ContentSection::setContent(juce::Component* c)
{
    if (form.getParentComponent() != nullptr) {
        Trace(1, "YanDialog: Attempt to set content after internal form was added");
    }
    else {
        content = c;
        addAndMakeVisible(c);
    }
}

void YanDialog::ContentSection::clear()
{
    form.clear();
    // using getParentComponent as a way to say "is being used" is awkward
    if (form.getParentComponent() != nullptr) {
        form.getParentComponent()->removeChildComponent(&form);
    }
}

int YanDialog::ContentSection::getPreferredHeight()
{
    int height = 0;
    
    if (content != nullptr) {
        height = content->getHeight();
        // should have set this in getPreferredHeight
        if (height == 0)
          height = ContentDefaultHeight;
    }
    else if (form.getParentComponent() != nullptr) {
        height = form.getPreferredHeight();
    }

    if (height > 0)
      height += (ContentInset * 2);
    
    return height;
}

void YanDialog::ContentSection::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    area = area.reduced(ContentInset);

    if (content != nullptr) {
        content->setBounds(area);
    }
    else if (form.getParentComponent() != nullptr) {
        form.setBounds(area);
        // often the same so force a resize
        form.forceResize();
    }
}

void YanDialog::ContentSection::paint(juce::Graphics& g)
{
    if (borderColor != juce::Colour()) {
        juce::Rectangle<int> area = getLocalBounds();
        g.setColour(borderColor);
        g.drawRect(area, BorderWidth);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
