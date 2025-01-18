
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../../model/TreeForm.h"
#include "../JuceUtil.h"

#include "../common/YanField.h"
#include "../common/YanParameter.h"

#include "ParameterForm.h"

ParameterForm::ParameterForm()
{
    addAndMakeVisible(form);
}

void ParameterForm::setTitle(juce::String s)
{
    title = s;
}

void ParameterForm::setTitleInset(int i)
{
    titleInset = i;
}

void ParameterForm::setFormInset(int i)
{
    formInset = i;
}

void ParameterForm::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    if (title.length() > 0)
      area = area.reduced(titleInset);
    
    juce::Rectangle<int> center = area.reduced(formInset);
    form.setBounds(center);
}

void ParameterForm::paint(juce::Graphics& g)
{
    juce::Rectangle<int> area = getLocalBounds();

    if (title.length() > 0) {

        juce::Rectangle<int> titleArea = area.reduced(titleInset);
        juce::Font f (JuceUtil::getFont(20));
        g.setFont(f);
        g.setColour(juce::Colours::black);
        g.drawText (title, titleArea.getX(), titleArea.getY(), titleArea.getWidth(), 20,
                    juce::Justification::centredLeft, true);
    }

    // this was used for testing, don't need it if the YanForm takes up the entire area
    g.setColour(juce::Colours::grey);

    juce::Rectangle<int> center = area.reduced(formInset);
    g.fillRect(center.getX(), center.getY(), center.getWidth(), center.getHeight());

}

//////////////////////////////////////////////////////////////////////
//
// Field Addition
//
//////////////////////////////////////////////////////////////////////

void ParameterForm::add(juce::Array<Symbol*>& symbols)
{
    for (auto s : symbols) {
        YanParameter* field = new YanParameter(s->getDisplayName());
        parameters.add(field);
        field->init(s);
        form.add(field);
    }
}

void ParameterForm::addSpacer()
{
    YanSpacer* s = new YanSpacer();
    others.add(s);
    form.add(s);
}

/**
 * Build a form from a TreeForm
 */
void ParameterForm::add(TreeForm* formdef)
{
    (void)formdef;
}

//////////////////////////////////////////////////////////////////////
//
// Vavlue Transfer
//
//////////////////////////////////////////////////////////////////////

void ParameterForm::load(ValueSet* values)
{
    for (auto field : parameters) {
        Symbol* s = field->getSymbol();
        MslValue* v = nullptr;
        if (values != nullptr)
          v = values->get(s->name);
        field->load(v);
    }
}

void ParameterForm::save(ValueSet* values)
{
    for (auto field : parameters) {
        Symbol* s = field->getSymbol();
        MslValue v;
        field->save(&v);
        values->set(s->name, v);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
