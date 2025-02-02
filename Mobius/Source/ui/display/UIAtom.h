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

    // sizing tools
    int getStringWidth(juce::String& s);
    int getNumberTextWidth(int digits);

    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

    //
    // Layout State
    // All very experimental...
    //
    
    // vertical scaling factor that will be applied to this atom when
    // it is in a container with vertical orientation, must be a fraction under 0
    float verticalProportion = 0.0f;
    
    // transient state used by the layout manager
    float proportion =  0.0f;

  protected:

    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;

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

class UIAtomNumber : public UIAtomText
{
  public:

    UIAtomNumber();
    ~UIAtomNumber();

    void setDigits(int d);
    void setValue(int v);
    
    int getMinWidth() override;
    
  protected:

    int digits = 0;
};

class UIAtomFloat : public UIAtomText
{
  public:

    UIAtomFloat();
    ~UIAtomFloat();

    void setDigits(int decimal, int fraction);
    void setInvisibleZero(bool b);
    void setValue(float v);
    
    int getMinWidth() override;
    
  protected:

    int decimals = 0;
    int fractions = 0;
    bool invisibleZero = false;

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
