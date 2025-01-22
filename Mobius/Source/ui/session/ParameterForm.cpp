
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../../model/TreeForm.h"
#include "../../Provider.h"
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
        // really need this to be configurable
        g.setColour(juce::Colours::white);
        g.drawText (title, titleArea.getX(), titleArea.getY(), titleArea.getWidth(), 20,
                    juce::Justification::centredLeft, true);
    }

    // this was used for testing, don't need it if the YanForm takes up the entire area
    g.setColour(juce::Colours::black);

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
void ParameterForm::add(Provider* p, TreeForm* formdef)
{
    for (auto name : formdef->symbols) {
        if (name == TreeForm::Spacer) {
            addSpacer();
        }
        else if (name.startsWith(TreeForm::Section)) {
            addSpacer();
            juce::String sectionLabel = name.fromFirstOccurrenceOf(TreeForm::Section, false, false);
            YanSection* sec = new YanSection(sectionLabel);
            others.add(sec);
            form.add(sec);
            addSpacer();
        }
        else {
            Symbol* s = p->getSymbols()->find(name);
            if (s == nullptr) {
                Trace(1, "ParameterForm: Unknown symbol %s", name.toUTF8());
            }
            else if (s->parameterProperties == nullptr) {
                Trace(1, "ParameterForm: Symbol is not a parameter %s", name.toUTF8());
            }
            else {
                YanParameter* field = new YanParameter(s->getDisplayName());
                parameters.add(field);
                field->init(s);
                form.add(field);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Value Transfer
//
//////////////////////////////////////////////////////////////////////

void ParameterForm::load(Provider* p, ValueSet* values)
{
    for (auto field : parameters) {
        Symbol* s = field->getSymbol();
        MslValue* v = nullptr;
        if (values != nullptr)
          v = values->get(s->name);

        field->load(p, v);
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
