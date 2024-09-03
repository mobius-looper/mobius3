
#include <JuceHeader.h>

#include "../script/ScriptRegistry.h"
#include "../script/MslScriptUnit.h"
#include "../script/MslError.h"

#include "ScriptDetails.h"

#define RowHeight 20
#define LabelWidth 40

ScriptDetails::ScriptDetails()
{
}

ScriptDetails::~ScriptDetails()
{
}

void ScriptDetails::resized()
{
}

void ScriptDetails::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();

    g.fillAll (juce::Colours::black);

    if (regfile != nullptr) {
        
        paintDetail(g, area, "Name", regfile->name);
        paintDetail(g, area, "Path", regfile->path);
        juce::String added = regfile->added.toString(true, true, false, false);
        paintDetail(g, area,  "Added", added);
        paintDetail(g, area, "Author", regfile->author);

        // errors
        area.removeFromTop(RowHeight);
        juce::OwnedArray<MslError>* errors = nullptr;
        if (regfile->unit != nullptr)
          errors = &(regfile->unit->errors);
        else
          errors = &(regfile->oldErrors);

        if (errors != nullptr) {
            for (auto error : *errors)
              paintError(g, area, error);
        }

        // collisions
        if (regfile->unit != nullptr) {
            for (auto collision : regfile->unit->collisions)
              paintCollision(g, area, collision);
        }
    }
}

void ScriptDetails::paintDetail(juce::Graphics& g, juce::Rectangle<int>& area,
                                    juce::String label, juce::String text)
{
    int top = area.getY();
    int labelLeft = area.getX();
    int textLeft = LabelWidth + 8;
    int textWidth = area.getWidth() - textLeft;
    
    g.setColour(juce::Colours::orange);
    g.drawText (label, labelLeft, top, LabelWidth, RowHeight, juce::Justification::centredRight, true);

    g.setColour(juce::Colours::white);
    g.drawText (text, textLeft, top, textWidth, RowHeight, juce::Justification::centredLeft, true);

    area.removeFromTop(RowHeight);
}

void ScriptDetails::paintError(juce::Graphics& g, juce::Rectangle<int>& area,
                                   MslError* error)
{
    int left = 8;
    int width = area.getWidth() - left;
    char buffer[1024];

    if (strlen(error->token) > 0) 
      snprintf(buffer, sizeof(buffer), "Line %d column %d: %s: %s", error->line, error->column,
               error->token, error->details);
    else
      snprintf(buffer, sizeof(buffer), "Line %d column %d: %s", error->line, error->column,
               error->details);

    g.setColour(juce::Colours::red);
    g.drawText (juce::String(buffer), left, area.getY(), width, RowHeight, juce::Justification::centredLeft, true);
    
    area.removeFromTop(RowHeight);
}

void ScriptDetails::paintCollision(juce::Graphics& g, juce::Rectangle<int>& area,
                                       MslCollision* col)
{
    int left = 8;
    int width = area.getWidth() - left;
    char buffer[1024];
    
    snprintf(buffer, sizeof(buffer), "Name collision on %s with file %s",
             (const char*)col->name.toUTF8(), (const char*)col->otherPath.toUTF8());

    g.setColour(juce::Colours::red);
    g.drawText (juce::String(buffer), left, area.getY(), width, RowHeight,
                juce::Justification::centredLeft, true);
    
    area.removeFromTop(RowHeight);
}

void ScriptDetails::load(ScriptRegistry::File* file)
{
    regfile = file;
    repaint();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
