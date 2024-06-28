
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "TestConfigPanel.h"

TestConfigContent::TestConfigContent()
{
}

TestConfigContent::~TestConfigContent()
{
}

void TestConfigContent::showing()
{
    Trace(2, "TestConfigContent: showing\n");
}

void TestConfigContent::hiding()
{
    Trace(2, "TestConfigContent: hiding\n");
}

void TestConfigContent::load()
{
    Trace(2, "TestConfigContent: load\n");
}

void TestConfigContent::save()
{
    Trace(2, "TestConfigContent: save\n");
}

void TestConfigContent::cancel()
{
    Trace(2, "TestConfigContent: cancel\n");
}

void TestConfigContent::revert()
{
    Trace(2, "TestConfigContent: revert\n");
}

void TestConfigContent::objectSelectorSelect(int ordinal)
{
    (void)ordinal;
    Trace(2, "TestConfigContent: objectSelectorSelect\n");
}

void TestConfigContent::objectSelectorRename(juce::String newName)
{
    Trace(2, "TestConfigContent: objectSelectorRename\n");
}

void TestConfigContent::objectSelectorNew()
{
    Trace(2, "TestConfigContent: objectSelectorNew\n");
}

void TestConfigContent::objectSelectorDelete()
{
    Trace(2, "TestConfigContent: objectSelectorDelete\n");
}

void TestConfigContent::objectSelectorCopy()
{
    Trace(2, "TestConfigContent: objectSelectorCopy\n");
}

