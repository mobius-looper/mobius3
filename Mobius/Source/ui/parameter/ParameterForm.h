/**
 * Dynamic form containing fields for editing parameter symbols.
 *
 * The parameters to edit are injected from above using several interfaces.
 * Once constructed, field values are read from and saved to a ValueSet.
 *
 * Awareness of the surrounding context must be kept to a minimum to enable
 * it's use in several places.
 *
 * This is not specific to Session editing so it could be moved, but that is it's
 * primary use.
 *
 * There is some form wrapper support like a title which should be kept to a minimum
 * and be optional.  May want to factor this out.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "../common/YanForm.h"
#include "../common/YanParameter.h"

class ParameterForm : public juce::Component,
                      public juce::DragAndDropTarget,
                      public YanFieldLabel::Listener
                      //public juce::DragAndDropContainer
{
  public:

    ParameterForm();
    ~ParameterForm() {}

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void parameterFormDrop(ParameterForm* src, juce::String desc) = 0;
        virtual void parameterFormClick(ParameterForm* src, YanParameter* p, const juce::MouseEvent& e) {
            (void)src; (void)p; (void)e;
        }
    };
    void setListener(Listener* l);
    
    /**
     * Forms may have an optional title which is displayed above the form fields.
     * When there is a title the fields are inset.
     */
    void setTitle(juce::String s);

    /**
     * Adjust the insets.
     */
    void setTitleInset(int x);
    void setFormInset(int x);

    /**
     * True if the symbol fields can be dragged out.
     */
    void setDraggable(bool b);

    /**
     * True if fields can be locked and unlocked
     */
    void setLocking(bool b);

    /**
     * Add a list of editing fields for parameter symbols.
     * The fields are added in the same order as the array.
     */
    void add(juce::Array<class Symbol*>& symbols);

    /**
     * Add a random field not necessarily associated with a symbol.
     */
    void add(class YanField* f);

    /**
     * Add a spacer.
     */
    void addSpacer();

    /**
     * Add form fields from a form definition.
     */
    void build(class Provider* p, class TreeForm* formdef);
    
    /**
     * Load the values of symbol parameter fields from the value set.
     * Only fields added with Symbols can be loaded this way.
     */
    void load(class Provider* p, class ValueSet* values);

    /**
     * Save the values of symbol parameter fields to the value set.
     */
    void save(class ValueSet* values);

    /**
     * Strange interface for dynamic parameter forms.
     */
    class YanParameter* add(class Provider* p, class Symbol* s, class ValueSet* values);

    //class YanParameter* findFieldWithLabel(class YanFieldLabel* l);
    void remove(class YanParameter* p);
    bool remove(class Symbol* s);

    void yanFieldClicked(YanField* f, const juce::MouseEvent& e) override;

    //
    // Juce
    //

    void resized() override;
    void paint(juce::Graphics& g) override;

    void forceResize();

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

  private:

    Listener* listener = nullptr;
    bool draggable = false;
    bool locking = false;
    
    juce::String title;

    // this gives it a little border between the title and the container
    int titleInset = 20;

    // this needs to be large enough to include the title inset plus the
    // title height 
    int formInset = 42;
    
    YanForm form;
    juce::OwnedArray<class YanParameter> parameters;
    juce::OwnedArray<class YanField> others;
    
};
    
