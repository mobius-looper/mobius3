/**
 * Simple utility to assist in formatting indented text files
 * containing diagnostic dumps of complex structures.
 * Like the internal state of a looper or something.
 */

#pragma once

#include <JuceHeader.h>

class StructureDumper
{
  public:

    StructureDumper();
    virtual ~StructureDumper();

    juce::String getText();
    void clear();

    void inc();
    void dec();
    void noIndent();

    void start(juce::String s);
    void add(juce::String s);
    void newline();
    
    void line(juce::String s);
    void line(juce::String name, juce::String value);
    void line(juce::String name, int value);
    
    void add(juce::String name, juce::String value);
    void add(juce::String name, int value);
    void addb(juce::String name, bool value);

    
    void setRoot(juce::File r);
    void write(juce::File file);
    virtual void write(juce::String filename);
    void test();

  private:

    juce::File root;
    juce::String buffer;
    int indent = 0;
    
};
    
    
