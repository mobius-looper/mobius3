/**
 * Configuration model for Samples that can be sent to the Mobius engine
 * for playback.
 */

#include <JuceHeader.h>

#include "../util/Util.h"

#include "SampleConfig.h"

//////////////////////////////////////////////////////////////////////
//
// SampleConfig
//
//////////////////////////////////////////////////////////////////////

SampleConfig::SampleConfig()
{
}

SampleConfig::SampleConfig(SampleConfig* src)
{
    for (auto s : src->getSamples())
      add(new Sample(s));
}

SampleConfig::~SampleConfig()
{
}

void SampleConfig::clear()
{
    samples.clear();
}

void SampleConfig::add(Sample* neu)
{
    samples.add(neu);
}

juce::OwnedArray<Sample>& SampleConfig::getSamples()
{
    return samples;
}

void SampleConfig::toXml(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement(XmlName);
    parent->addChildElement(root);
    
    for (auto s : samples) {
        
        juce::XmlElement* sel = new juce::XmlElement(Sample::XmlName);
        root->addChildElement(sel);

        sel->setAttribute("file", s->file);

        // only one used is button, but keep them going for awhile
        if (s->sustain) sel->setAttribute("sustain", "true");
        if (s->loop) sel->setAttribute("loop", "true");
        if (s->concurrent) sel->setAttribute("concurrent", "true");
        if (s->button) sel->setAttribute("button", "true");
    }
}

void SampleConfig::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(Sample::XmlName)) {
            Sample* def = new Sample();
            samples.add(def);

            def->file = el->getStringAttribute("file");
            def->sustain = el->getBoolAttribute("sutain");
            def->loop = el->getBoolAttribute("loop");
            def->concurrent = el->getBoolAttribute("concurrent");
            def->button = el->getBoolAttribute("button");
        }
        else {
            errors.add(juce::String("SampleConfig: Unexpected XML tag name: " +
                                    el->getTagName()));
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Sample
//
//////////////////////////////////////////////////////////////////////

Sample::Sample()
{
}

Sample::Sample(juce::String argFile)
{
    file = argFile;
}

Sample::Sample(Sample* src)
{
    file = src->file;
	sustain = src->sustain;
	loop = src->loop;
	concurrent = src->concurrent;
    button = src->button;
    // we have not historically copied the loaded data
}

Sample::~Sample()
{
    delete data;
}

void Sample::setData(float* argData, int argFrames)
{
    delete data;
    data = argData;
    frames = argFrames;
}

float* Sample::getData()
{
    return data;
}

int Sample::getFrames()
{
    return frames;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
