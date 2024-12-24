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
    void setOnColor(juce::Colour c);
    void setOffColor(juce::Colour c);
    void setOn(bool b);
    
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
    juce::Colour onColor;
    juce::Colour offColor;
    juce::Colour backColor;
    juce::Colour overColor;
    bool on = false;
    bool over = false;
    
};

class UIAtomToggle : public UIAtomButton
{
  public:


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
    
};
