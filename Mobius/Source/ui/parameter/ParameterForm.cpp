
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ParameterProperties.h"
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

void ParameterForm::setDraggable(bool b)
{
    draggable = b;
}

void ParameterForm::setLocking(bool b)
{
    locking = b;
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
    
    // fields that have dynamic widths depending on what is loaded
    // into them, such as YanCombos with YanParameterHelpers often need
    // to have their size recalculated after loading.  But since the
    // bounds of the outer form didn't change the setBounds() call above
    // won't trigger a resized walk over the children.  This will probably
    // cause a redundant resized walk most of the time, could avoid it by
    // testing for the setBounds() size being equal above
    form.forceResize();
}

void ParameterForm::forceResize()
{
    resized();
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

/**
 * Here from a YanField/YanFieldLabel if we're interestesed in passing along
 * clicks on the labels.
 */
void ParameterForm::yanFieldClicked(YanField* f, const juce::MouseEvent& e)
{
    if (listener != nullptr) {
        // dangerous
        YanParameter* yp = static_cast<YanParameter*>(f);
        listener->parameterFormClick(this, yp, e);
    }
}

/**
 * Find a paraemter within the form that displays a certain Symbol.
 */
YanParameter* ParameterForm::find(Symbol* s)
{
    YanParameter* found = nullptr;
    for (auto p : parameters) {
        if (p->getSymbol() == s) {
            found = p;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Field Addition
//
//////////////////////////////////////////////////////////////////////

/**
 * This is what eventually happens when you drag a symbol from a ParameterTree
 * onto the form.
 *
 * If we already have this symbol in the form, ignore it, otherwise add it.
 * Weird interface, a combination of adding and loading.
 */
YanParameter* ParameterForm::add(Provider* p, Symbol* s, ValueSet* values)
{
    YanParameter* result = nullptr;
    YanField* existing = form.find(s->getDisplayName());
    if (existing == nullptr) {
        YanParameter* field = new YanParameter(s->getDisplayName());
        field->init(p, s);

        // be notified of clicks on the label which we pass back to our listener
        field->setLabelListener(this);
        
        // if this is a draggable form, the drag scription is the canonical symbol name,
        // not the displayName used for the label
        if (draggable) 
          field->setDragDescription(s->name);
        
        parameters.add(field);
        form.add(field);

        // values is optional depending on who is calling this
        // if the field isn't loaded now, then it needs to be refresh()'d
        // or load()'d later
        if (p != nullptr && values != nullptr) {
            MslValue* v = values->get(s->name);
            field->load(v);
        }
        
        forceResize();

        result = field;
    }
    return result;
}

#if 0
void ParameterForm::add(juce::Array<Symbol*>& symbols)
{
    for (auto s : symbols) {
        YanParameter* field = new YanParameter(s->getDisplayName());
        parameters.add(field);
        field->init(provider, s);
        form.add(field);
    }
}
#endif

void ParameterForm::addSpacer()
{
    YanSpacer* s = new YanSpacer();
    others.add(s);
    form.add(s);
}

/**
 * Build a form from a TreeForm
 */
void ParameterForm::build(Provider* p, TreeForm* formdef)
{
    juce::String suppressPrefix = formdef->suppressPrefix;
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
                juce::String label = s->displayName;
                if (s->parameterProperties != nullptr) {
                    label = s->parameterProperties->displayName;
                    if (suppressPrefix.length() > 0 && label.indexOf(suppressPrefix) == 0) {
                        label = label.fromFirstOccurrenceOf(suppressPrefix + " ", false, false);
                    }
                }
                
                YanParameter* field = new YanParameter(label);
                parameters.add(field);
                field->init(p, s);
                form.add(field);
            }
        }
    }
}

/**
 * After dragging a field out a form the drag watcher may ask to remove
 * the field entirely.
 */
bool ParameterForm::remove(Symbol* s)
{
    YanParameter* found = nullptr;
    
    for (auto p : parameters) {
        if (p->getSymbol() == s) {
            found = p;
            break;
        }
    }

    if (found != nullptr) {
        form.remove(found);
        parameters.removeObject(found, true);
    }

    return (found != nullptr);
}
                     
//////////////////////////////////////////////////////////////////////
//
// Value Transfer
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

    // force it to resize, important for combo boxes that may change
    // widths after loading
    forceResize();
}

void ParameterForm::save(ValueSet* values)
{
    for (auto field : parameters) {
        Symbol* s = field->getSymbol();
        // if the field is marked defaulted, any prior
        // value it had in the ValueSet must be removed
        if (field->isDefaulted()) {
            values->remove(s->name);
        }
        else {
            MslValue v;
            field->save(&v);
            values->set(s->name, v);
        }
    }
}

/**
 * Iterate the Refresher over all of the YanParameter fields.
 */
void ParameterForm::refresh(Refresher* r)
{
    for (auto field : parameters) {
        r->parameterFormRefresh(this, field);
    }
}
 
//////////////////////////////////////////////////////////////////////
//
// Drag and Drop
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true if we're interested in this thing from that thing.
 * There are two possible source components:
 *
 *    juce::ValueTreeItem when dragging a symbol node from the parameter tree
 *    into the form.
 *
 *    YanFieldLabel when dragging one of the form fields onto ourselves.
 *
 * The second case is obscure since we need to be both a Target and Container to allow
 * both dragging in and out.  We don't support any useful options for dragging within
 * the form so those can be ignored.  If you don't, then itemDropped will cakk back
 * up to the Listener which will try to add a symbol field we already have, which will be
 * ignored, but still can bypass all that.
 */
bool ParameterForm::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    bool interested = false;
    juce::Component* c = details.sourceComponent.get();

    // so...dynamic_cast is supposed to be evil, but we've got a problem here
    // how do you know what this thing is if all Juce gives you is a Component?
    // I suppose we could search upward and see if we are in the parent hierarchy.
    YanFieldLabel * l = dynamic_cast<YanFieldLabel*>(c);
    if (l == nullptr) {
        interested = true;
    }

    return interested;
}

/**
 * So we don't have enough awareness to fully process the drop, so forward back
 * to a Listener.
 */
void ParameterForm::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    if (listener != nullptr)
      listener->parameterFormDrop(this, details.description.toString());
}

void ParameterForm::setListener(Listener* l)
{
    listener = l;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
