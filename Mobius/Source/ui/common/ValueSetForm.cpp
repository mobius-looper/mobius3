
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/ValueSet.h"
#include "../../model/Form.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "../common/YanField.h"

#include "ValueSetField.h"
#include "ValueSetForm.h"

ValueSetForm::ValueSetForm()
{
    addAndMakeVisible(form);
}

void ValueSetForm::setTitleInset(int i)
{
    titleInset = i;
}

void ValueSetForm::setFormInset(int i)
{
    formInset = i;
}

void ValueSetForm::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    if (title.length() > 0)
      area = area.reduced(titleInset);
    
    juce::Rectangle<int> center = area.reduced(formInset);
    form.setBounds(center);
    
    // fields that have dynamic widths depending on what is loaded
    // into them, such as YanCombos with YanParameterHelpers often need
    // to have their size recalculated after loading.  But since the
    // bounds of the outer form didn't change the setBounds() call above
    // won't trigger a resized walk over the children.  This will probably
    // cause a redundant resized walk most of the time, could avoid it by
    // testing for the setBounds() size being equal above
    form.forceResize();
}

void ValueSetForm::forceResize()
{
    resized();
}

void ValueSetForm::paint(juce::Graphics& g)
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

/**
 * Build a form from a TreeForm
 */
void ValueSetForm::build(Provider* p, Form* formdef)
{
    title = formdef->title;
    
    for (auto fdef : formdef->fields) {

        juce::String label = fdef->displayName;
        if (label.length() == 0) label = fdef->name;
        
        ValueSetField* field = new ValueSetField(label);
        fields.add(field);
        field->init(p, fdef);
        form.add(field);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Value Transfer
//
//////////////////////////////////////////////////////////////////////

void ValueSetForm::load(ValueSet* values)
{
    for (auto field : fields) {
        Field* fdef = field->getDefinition();
        MslValue* v = nullptr;
        if (values != nullptr)
          v = values->get(fdef->name);

        field->load(v);
    }

    // force it to resize, important for combo boxes that may change
    // widths after loading
    forceResize();
}

void ValueSetForm::save(ValueSet* values)
{
    for (auto field : fields) {
        Field* fdef = field->getDefinition();
        MslValue v;
        field->save(&v);
        values->set(fdef->name, v);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
