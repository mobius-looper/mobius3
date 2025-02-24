
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "../session/ParameterForm.h"

#include "DynamicFormCollection.h"

DynamicFormCollection::DynamicFormCollection()
{
}

void DynamicFormCollection::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    for (auto form : forms)
      form->setBounds(area);
}

void DynamicFormCollection::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void DynamicFormCollection::load(Provider* p, ValueSet* src)
{
    // have to save these for later since forms are allocated
    // incrementally as tree nodes are clicked
    provider = p;
    sourceValues = src;

    // after being displayed once, we may have a partial set of forms
    // that can be loaded now
    for (auto form : forms)
      form->load(p, sourceValues);
}

void DynamicFormCollection::save(ValueSet* dest)
{
    for (auto form : forms)
      form->save(dest);
}

void DynamicFormCollection::cancel()
{
    // forget this, just reload
    sourceValues = nullptr;
}

void DynamicFormCollection::decache()
{
    // first save them
    if (sourceValues != nullptr)
      save(sourceValues);

    formTable.clear();
    forms.clear();
    currentForm = nullptr;
}

ParameterForm* DynamicFormCollection::getForm(juce::String name)
{
    return formTable[name];
}

/**
 * The way in which new forms are added and loaded is a little weird.
 * The fields they contain is determined by ParameterTreeForms by digging
 * symbols out of the selected container in the ParameterEditorTree.
 * It will ask us if we have a form, and if not build one and call addForm.
 * In addition to just adding it as a child component, this is also where
 * the values in the form need to be loaded since the form didn't exist
 * at the point of initialize()
 */
void DynamicFormCollection::addForm(juce::String name, ParameterForm* form)
{
    ParameterForm* existing = formTable[name];
    if (existing != nullptr) {
        // shouldn't be seeing this
        Trace(1, "DynamicFormCollection: Replacing form %s", name.toUTF8());
        removeChildComponent(existing);
        formTable.remove(name);
        forms.removeObject(existing, true);
    }
    forms.add(form);
    formTable.set(name, form);
    form->setBounds(getLocalBounds());
    addChildComponent(form);

    // give it the values we were given on load()
    form->load(provider, sourceValues);
}

void DynamicFormCollection::show(Provider* p, juce::String formName)
{
    (void)p;
    
    ParameterForm* form = getForm(formName);
    if (form == nullptr) {
        // these have to be preconstructed
        Trace(1, "DynamicFormCollection: Unknown form %s", formName.toUTF8());
    }
    else if (form == currentForm) {
        // Trace(2, "SPE: Form already displayed for category %s", category.toUTF8());
    }
    else {
        //Trace(2, "DynamicFormCollection: Changing form %s", formName.toUTF8());
        if (currentForm != nullptr)
          currentForm->setVisible(false);
        // probably need a refresh?
        form->setVisible(true);
        currentForm = form;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
