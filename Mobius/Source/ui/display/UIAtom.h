/**
 * Fundamental UI widgets that display something in a useful way but don't
 * know how they are used.
 */

#pragma once

#include <JuceHeader.h>

class UIAtom : public juce::Component
{
  public:

    UIAtom();
    ~UIAtom();

    virtual int getMinWidth();
    void setMinWidth(int w);
    virtual int getMinHeight();
    void setMinHeight(int h);
    
    virtual int getMaxWidth();
    void setMaxWidth(int w);
    virtual int getMaxHeight();
    void setMaxHeight(int h);

    virtual void setLayoutHeight(int h);

    // try to avoid these
    //int getPreferredWidth();
    //void setPreferredWidth(int w);
    //int getPreferredHeight();
    //void setPreferredHeight(int h);
    
    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // layout manager state, think...
    float proportion =  0.0f;

  protected:

    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    //int preferredWidth = 0;
    //int preferredHeight = 0;

};

class UIAtomList : public UIAtom
{
  public:

    void setVertical();
    void setHorizontal();

    int getMinHeight() override;
    int getMinWidth() override;
    void setLayoutHeight(int h) override;
    
    void add(UIAtom* a);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    // could just use the component list, but might want other
    // things in there
    juce::Array<UIAtom*> atoms;

    bool vertical = false;

    void layoutHorizontal(juce::Rectangle<int> area);
    void layoutVertical(juce::Rectangle<int> area);

};

class UIAtomLight : public UIAtom
{
  public:

    UIAtomLight();
    ~UIAtomLight();

    typedef enum {
        Circle,
        Square,
        Triangle,
        Star
    } Shape;

    void setShape(Shape s);
    void setOnColor(juce::Colour c);
    void setOffColor(juce::Colour c);
    void setOutlineColor(juce::Colour c);
    void setOn(bool b);
    bool isOn();
    int getMinWidth() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    Shape shape = Circle;
    juce::Colour onColor;
    juce::Colour offColor;
    juce::Colour outlineColor;
    bool on = false;
    
    void setBorderColor(juce::Graphics& g);
    void setFillColor(juce::Graphics& g);

};

class UIAtomFlash : public UIAtomLight
{
  public:

    UIAtomFlash();
    ~UIAtomFlash();

    void setDecay(int msec);
    void flash();
    void flash(juce::Colour c);
    void advance();
    
  private:

    int decay = 0;
    int count = 0;
    
};

class UIAtomButton : public UIAtom
{
  public:

    UIAtomButton();
    ~UIAtomButton();

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void atomButtonPressed(UIAtomButton* b) = 0;
    };

    void setListener(Listener* l);
    void setText(juce::String s);
    void setOnText(juce::String s);
    void setOnColor(juce::Colour c);
    void setOffColor(juce::Colour c);
    void setBackColor(juce::Colour c);
    void setOverColor(juce::Colour c);
    void setOutlineColor(juce::Colour c);
    void setToggle(bool b);
    void setOn(bool b);
    bool isOn();
    
    int getMinWidth() override;
    void resized() override;
    void paint(juce::Graphics& g) override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
  private:

    Listener* listener = nullptr;

    juce::String text;
    juce::String onText;
    juce::Colour onColor;
    juce::Colour offColor;
    juce::Colour backColor;
    juce::Colour overColor;
    juce::Colour outlineColor;
    
    bool on = false;
    bool over = false;
    bool toggle = false;
    
    void drawBackground(juce::Graphics& g);
    
};

class UIAtomText : public UIAtom
{
  public:

    UIAtomText();
    ~UIAtomText();

    void setText(juce::String s);
    void setOnColor(juce::Colour c);
    void setOffColor(juce::Colour c);
    void setBackColor(juce::Colour c);
    void setOn(bool b);
    void setFlash(bool b);
    void advance();
    
    int getMinWidth() override;
    void resized() override;
    void paint(juce::Graphics& g) override;

  protected:

    juce::String text;
    juce::Colour onColor;
    juce::Colour offColor;
    juce::Colour backColor;
    bool on = false;
    bool flash = false;
    int ticks = 0;
    
    void drawTextBackground(juce::Graphics& g);
};

class UIAtomLabeledText : public UIAtomText
{
  public:

    UIAtomLabeledText();
    ~UIAtomLabeledText();

    void setLabel(juce::String s);
    void setLabelColor(juce::Colour c);
    int getMinWidth() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::String label;
    juce::Colour labelColor;
    
};

class UIAtomLabeledText2 : public UIAtom
{
  public:

    UIAtomLabeledText2();
    ~UIAtomLabeledText2();

    void setLabel(juce::String s);
    void setText(juce::String s);
    void setLabelColor(juce::Colour c);
    int getMinWidth() override;
    int getMinHeight() override;
    
    void resized() override;

  private:

    UIAtomList row;
    UIAtomText label;
    UIAtomText text;
};

class UIAtomRadar : public UIAtom
{
  public:

    UIAtomRadar();
    ~UIAtomRadar();

    void setColor(juce::Colour c);
    void setRange(int r);
    void setLocation(int r);
    int getMinWidth() override;
    
    void paint(juce::Graphics& g) override;

  private:

    juce::Colour color;
    int range = 0;
    int location = 0;
    
};
