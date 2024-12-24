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

    virtual int getPreferredHeight();
    virtual int getPreferredWidth();
    
    // do Jucy things
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& e) override;

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
    void setOn(bool b);
    bool isOn();
    
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

  private:

    Shape shape = Circle;
    juce::Colour onColor;
    juce::Colour offColor;
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
    
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

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
    
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

  private:

    juce::String text;
    juce::Colour onColor;
    juce::Colour offColor;
    juce::Colour backColor;
    bool on = false;
    bool flash = false;
    int ticks = 0;
    
    void drawTextBackground(juce::Graphics& g);
};
