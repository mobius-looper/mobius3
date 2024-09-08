/**
 * Details panel for the script editor.
 *
 * This shows things in the MslDetails object attached to the regisry File.
 * !! there is potential danger here if the MslDetails are replaced due
 * to reloading at the same moment this component is painting.  The old
 * details will be deleted.
 *
 * It would be best if the editor copied the details and refreshed them
 * periodically.  Alternately put a csect around it, or keep old versions
 * around and GC them.
 */

#include <JuceHeader.h>

#include "../../script/ScriptRegistry.h"
#include "../../script/MslDetails.h"
#include "../../script/MslError.h"
#include "../../script/MslCollision.h"

#include "ScriptDetails.h"

#define RowHeight 20
#define LabelWidth 40

ScriptDetails::ScriptDetails()
{
}

ScriptDetails::~ScriptDetails()
{
}

void ScriptDetails::setIncludeAll(bool b)
{
    includeAll = b;
}

void ScriptDetails::load(ScriptRegistry::File* file)
{
    regfile = file;
    repaint();
}

int ScriptDetails::getPreferredHeight()
{
    // always name, path
    int height = RowHeight * 2;
    if (includeAll) {
        // dates, Author
        height += (RowHeight * 2);
    }

    int nerrors = 0;
    if (regfile != nullptr) {
        MslDetails* details = regfile->getDetails();
        if (details != nullptr) {
            nerrors += details->errors.size();
            nerrors += details->collisions.size();
        }
    }
    if (nerrors > 0)
      height += (RowHeight * nerrors);
    
    return height;
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
        if (includeAll) {
            juce::String added = regfile->added.toString(true, true, false, false);
            paintDetail(g, area,  "Added", added);
            paintDetail(g, area, "Author", regfile->author);
        }

        if (regfile->hasErrors()) {
            //area.removeFromTop(RowHeight);
            MslDetails* details = regfile->getDetails();
            if (details != nullptr) {
                for (auto error : details->errors)
                  paintError(g, area, error);
                
                for (auto collision : details->collisions)
                  paintCollision(g, area, collision);
            }
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
    
    snprintf(buffer, sizeof(buffer), "Name collision on \"%s\" with file %s",
             (const char*)col->name.toUTF8(), (const char*)col->otherPath.toUTF8());

    g.setColour(juce::Colours::red);
    g.drawText (juce::String(buffer), left, area.getY(), width, RowHeight,
                juce::Justification::centredLeft, true);
    
    area.removeFromTop(RowHeight);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
