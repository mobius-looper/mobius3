// OBSOLETE: DELTEE

/**
 * This is behaving much like YanParameterForm except that it doesn't require
 * a Provider to lookup the Symbol.
 *
 * Don't need both, pick a style.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../JuceUtil.h"

#include "../common/YanField.h"
#include "../common/YanParameter.h"

#include "SessionEditorForm.h"

SessionEditorForm::SessionEditorForm()
{
    addAndMakeVisible(form);
}

void SessionEditorForm::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    juce::Rectangle<int> center = area.reduced(100);
    form.setBounds(center);
}

void SessionEditorForm::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();

    juce::Rectangle<int> titleArea = area.reduced(20);
    juce::Font f (JuceUtil::getFont(20));
    g.setFont(f);
    g.setColour(juce::Colours::black);
    g.drawText (category, titleArea.getX(), titleArea.getY(), titleArea.getWidth(), 20,
                juce::Justification::centredLeft, true);

    // don't need this now that the YanForm goes here
    g.setColour(juce::Colours::grey);

    juce::Rectangle<int> center = area.reduced(100);
    g.fillRect(center.getX(), center.getY(), center.getWidth(), center.getHeight());

}

void SessionEditorForm::initialize(juce::String c, juce::Array<Symbol*>& symbols)
{
    category = c;
    
    Trace(2, "SEF: Building form for category %s", category.toUTF8());
    for (auto s : symbols) {
        Trace(2, "  %s", s->getName());

        YanParameter* field = new YanParameter(s->getDisplayName());
        fields.add(field);
        
        field->init(s);
        
        form.add(field);
    }
    resized();
    
    // for some weird reason setting bounds on the new YanForm does not trigger a
    // resized traversal in Juce, how you get resized flowing is still a mystery
    form.resized();
}

void SessionEditorForm::load(ValueSet* values)
{
    for (auto field : fields) {
        Symbol* s = field->getSymbol();
        MslValue* v = nullptr;
        if (values != nullptr)
          v = values->get(s->name);
        field->load(v);
    }
}

void SessionEditorForm::save(ValueSet* values)
{
    for (auto field : fields) {
        Symbol* s = field->getSymbol();
        MslValue v;
        field->save(&v);
        values->set(s->name, v);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
