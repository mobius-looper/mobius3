
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "ParameterForm.h"

#include "ParameterFormCollection.h"

ParameterFormCollection::ParameterFormCollection()
{
}

ParameterFormCollection::~ParameterFormCollection()
{
}

void ParameterFormCollection::initialize(Provider* p, Factory* f, ValueSet* vs)
{
    provider = p;
    factory = f;
    valueSet = vs;
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

void ParameterFormCollection::load(ValueSet* vs)
{
    valueSet = vs;
    for (auto form : forms)
      form->load(provider, valueSet);
}
    
void ParameterFormCollection::save(ValueSet* dest)
{
    // if an alternate destination was not providied,
    // save to the same set we had at initialization
    // !! is this ever not the case?
    if (dest == nullptr)
      dest = valueSet;

    if (dest == nullptr)
      Trace(1, "ParameterFormCollection: Save without a ValueSet");
    else {
        for (auto form : forms)
          form->save(dest);
    }
}

void ParameterFormCollection::cancel()
{
    // forget this, just reload
    valueSet = nullptr;
}

void ParameterFormCollection::decache()
{
    // first save them
    if (valueSet != nullptr)
      save(valueSet);

    formTable.clear();
    forms.clear();
    currentForm = nullptr;
}

void ParameterFormCollection::add(juce::String formName, ParameterForm* form)
{
    if (form == nullptr) {
        Trace(1, "ParameterFormCollection::add Missing form");
    }
    else {
        forms.add(form);
        formTable.set(formName, form);
        addAndMakeVisible(form);

        if (valueSet != nullptr)
          form->load(provider, valueSet);

        form->setBounds(getLocalBounds());
        // trouble getting this fleshed out dynamically
        form->resized();
    }
}

ParameterForm* ParameterFormCollection::getCurrentForm()
{
    return currentForm;
}

void ParameterFormCollection::show(juce::String formName)
{
    if (formName == "none") {
        // common for interior nodes in trees that won't have forms
        // ideally this could auto-expand down to the first child node that has a form
    }
    else {
        ParameterForm* form = formTable[formName];

        if (form == nullptr) {

            if (factory == nullptr) {
                // if they dodn't give a factory, then it was expected to have
                // been populated with the necessary forms
                Trace(1, "ParameterFormCollection: Unknown form %s", formName.toUTF8());
            }
            else {
                form = factory->parameterFormCollectionCreate(formName);
                if (form == nullptr) {
                    Trace(1, "ParameterFormCollection: Factory failed to create form  %s",
                          formName.toUTF8());
                }
                else {
                    add(formName, form);
                }
            }
        }

        if (form != nullptr && form != currentForm) {

            if (currentForm != nullptr)
              currentForm->setVisible(false);

            form->setVisible(true);
            currentForm = form;
        }
        else if (form == nullptr) {
            // don't keep showing the current form if we failed to find one
            if (currentForm != nullptr) {
                currentForm->setVisible(false);
                currentForm = nullptr;
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
