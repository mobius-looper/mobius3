
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "../session/ParameterForm.h"

#include "ParameterFormCollection.h"

ParameterFormCollection::ParameterFormCollection()
{
}

void ParameterFormCollection::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    for (auto form : forms)
      form->setBounds(area);
}

void ParameterFormCollection::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void ParameterFormCollection::load(Provider* p, ValueSet* src)
{
    sourceValues = src;
    for (auto form : forms)
      form->load(p, sourceValues);
}

void ParameterFormCollection::save(ValueSet* dest)
{
    for (auto form : forms)
      form->save(dest);
}

void ParameterFormCollection::cancel()
{
    // forget this, just reload
    sourceValues = nullptr;
}

void ParameterFormCollection::decache()
{
    // first save them
    if (sourceValues != nullptr)
      save(sourceValues);

    formTable.clear();
    forms.clear();
    currentForm = nullptr;
}

ParameterForm* ParameterFormCollection::getForm(juce::String name)
{
    return formTable[name];
}

void ParameterFormCollection::addForm(juce::String name, ParameterForm* form)
{
    ParameterForm* existing = formTable[name];
    if (existing != nullptr) {
        // shouldn't be seeing this
        Trace(1, "ParameterFormCollection: Replacing form %s", name.toUTF8());
        formTable.remove(name);
        forms.removeObject(existing, true);
    }
    forms.add(form);
    formTable.set(name, form);
}

void ParameterFormCollection::show(Provider* p, juce::String formName)
{
    (void)p;
    
    ParameterForm* form = getForm(formName);
    if (form == nullptr) {
        // these have to be preconstructed
        Trace(1, "ParameterFormCollection: Unknown form %s", formName.toUTF8());
    }
    else if (form == currentForm) {
        // Trace(2, "SPE: Form already displayed for category %s", category.toUTF8());
    }
    else {
        //Trace(2, "ParameterFormCollection: Changing form %s", formName.toUTF8());
        currentForm->setVisible(false);
        // probably need a refresh?
        form->setVisible(true);
        currentForm = form;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
