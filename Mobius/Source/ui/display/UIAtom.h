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

    void setText(juce::String s);
    void setOnColor(juce::Colour c);
    void setOffColor(juce::Colour c);
    void setOn(bool b);
    
    virtual void resized() override;
    virtual void paint(juce::Graphics& g) override;

  private:

    juce::String text;
    juce::Colour onColor;
    juce::Colour offColor;
    bool on = false;
    
};
