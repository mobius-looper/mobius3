
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

void ParameterFormCollection::initialize(Factory* f, ValueSet* vs)
{
    factory = f;
    valueSet = vs;
}

/**
 * Option to use with form collections where the same parameter
 * may appear in more than one form.  Since changing the parameter
 * in one form need to be reflected in other forms, whenever the displayed
 * form changes, it is saved and the new form is reloaded.
 */
void ParameterFormCollection::setDuplicateParameters(bool b)
{
    duplicateParameters = b;
}

void ParameterFormCollection::setFlatStyle(bool b)
{
    if (flatStyle != b) {
        if (flatStyle) {
            if (flatForm != nullptr) {
                save(nullptr);
                flatForm->setVisible(false);
            }
            // supposed to have at least one by now, but if we don't
            // we don't know what to ask for
            if (forms.size() > 0) {
                currentForm = forms[0];
                currentForm->setVisible(true);
                load(nullptr);
            }
        }
        else {
            if (currentForm != nullptr) {
                currentForm->setVisible(false);
                currentForm = nullptr;
            }
            if (flatForm != nullptr)
              flatForm->setVisible(true);
        }
        flatStyle = b;
    }
}

void ParameterFormCollection::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    for (auto form : forms)
      form->setBounds(area);
    if (flatForm != nullptr)
      flatForm->setBounds(area);
}

void ParameterFormCollection::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void ParameterFormCollection::load(ValueSet* vs)
{
    if (vs != nullptr)
      valueSet = vs;

    if (flatStyle) {
        if (flatForm != nullptr)
          flatForm->load(valueSet);
    }
    else {
        for (auto form : forms)
          form->load(valueSet);
    }
    
}

void ParameterFormCollection::refresh(ParameterForm::Refresher* r)
{
    if (flatStyle) {
        if (flatForm != nullptr)
          flatForm->refresh(r);
    }
    else {
        for (auto form : forms)
          form->refresh(r);
    }
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
        if (flatStyle) {
            if (flatForm != nullptr)
              flatForm->save(dest);
        }
        else {
            for (auto form : forms)
              form->save(dest);
        }
    }
}

void ParameterFormCollection::cancel()
{
    valueSet = nullptr;
    // since forms are how highly sensitive to the Session contents
    // we need to rebuild them every time, just reloading new values over the
    // top of them isn't enough
    forms.clear();
    formTable.clear();
    currentForm = nullptr;
    flatForm.reset();
}

void ParameterFormCollection::decache()
{
    // first save them
    if (valueSet != nullptr)
      save(valueSet);

    formTable.clear();
    forms.clear();
    currentForm = nullptr;
    flatForm.reset();
}

void ParameterFormCollection::add(juce::String formName, ParameterForm* form)
{
    if (form == nullptr) {
        Trace(1, "ParameterFormCollection::add Missing form");
    }
    else {
        forms.add(form);
        formTable.set(formName, form);
        addChildComponent(form);
        form->setBounds(getLocalBounds());
        // trouble getting this fleshed out dynamically
        form->resized();

        if (!flatStyle)
          form->setVisible(true);
        
    }
}

void ParameterFormCollection::addFlat(ParameterForm* form)
{
    if (form == nullptr) {
        Trace(1, "ParameterFormCollection::addFLat Missing form");
    }
    else if (form == flatForm.get()) {
        Trace(1, "ParameterFormCollection::addFLat Already added");
    }
    else {
        if (flatForm != nullptr) {
            Trace(1, "ParameterFormCollection::addFLat Already have a flat form");
            removeChildComponent(flatForm.get());
        }
        flatForm.reset(form);
        addChildComponent(form);
        form->setBounds(getLocalBounds());
        // trouble getting this fleshed out dynamically
        form->resized();

        if (!flatStyle)
          form->setVisible(true);
        
    }
}

ParameterForm* ParameterFormCollection::getCurrentForm()
{
    if (flatStyle)
      return flatForm.get();
    else
      return currentForm;
}

void ParameterFormCollection::show(juce::String formName)
{
    if (flatStyle) {
        if (flatForm == nullptr) {
            ParameterForm* form = factory->parameterFormCollectionCreateFlat();
            if (form == nullptr)
              Trace(1, "ParameterFormCollection: Flat form not created");
            else {
                addFlat(form);
            }
        }
        if (flatForm != nullptr)
          flatForm->setVisible(true);
    }
    else if (formName == "none") {
        // common for interior nodes in trees that won't have forms
        // ideally this could auto-expand down to the first child node that has a form
    }
    else {
        ParameterForm* form = formTable[formName];
        bool created = false;
        
        if (form == nullptr) {

            if (factory == nullptr) {
                // if they dodn't give a factory, then it was expected to have
                // been populated with the necessary forms
                Trace(1, "ParameterFormCollection: Unknown form %s", formName.toUTF8());
            }
            else {
                // note that the form is expected to be in a loaded/refreshed
                // state after creation, we don't do it for you
                form = factory->parameterFormCollectionCreate(formName);
                if (form == nullptr) {
                    Trace(1, "ParameterFormCollection: Factory failed to create form  %s",
                          formName.toUTF8());
                }
                else {
                    add(formName, form);
                    created = true;
                }
            }
        }

        if (form != nullptr && form != currentForm) {

            if (currentForm != nullptr) {

                // hack for parameters that can be in more than one form
                if (duplicateParameters && valueSet != nullptr)
                  currentForm->save(valueSet);
                
                currentForm->setVisible(false);
            }

            form->setVisible(true);
            currentForm = form;

            if (duplicateParameters && valueSet != nullptr && !created)
              currentForm->load(valueSet);
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
