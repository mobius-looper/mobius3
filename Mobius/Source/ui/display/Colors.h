/*
 * Constants for display item colors until we have
 * something more flexible.
 *
 * Changed from "int" to "juce::uint32" to avoid a compiler warning
 * on the cast when passing juce::Colour()
 */

#pragma once

#include <JuceHeader.h>

const juce::uint32 MobiusBlue = 0xFF8080FF;
const juce::uint32 MobiusGreen = 0xFF00b000;
const juce::uint32 MobiusRed = 0xFFf40b74;
const juce::uint32 MobiusYellow = 0xFFFFFF00;
const juce::uint32 MobiusPink = 0xFFFF8080;
const juce::uint32 MobiusDarkYellow = 0xFFe0bd00;


class Colors
{
  public:

    static juce::Colour getLoopColor(class MobiusLoopState* loop);
};
