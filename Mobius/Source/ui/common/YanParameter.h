/**
 * YanForm field for editing Symbol parameters.
 *
 * It may take on several internal representations appropriate for the
 * parameter type.  It reads and writes values to a ValueSet.
 *
 * A Provider must be required on initialization in case the Parameter has
 * a displayHelper, typicall used for combo boxes that show names of structures
 * like ParameterSets or GroupDefinitions.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "YanField.h"

class YanParameter : public YanField,
                     public YanCombo::Listener,
                     public YanInput::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanParameterChanged(YanParameter* p) = 0;
    };
    void setListener(Listener* l);

    YanParameter(juce::String label);
    ~YanParameter();

    void init(class Provider* p, class Symbol* s);

    // options for SessionTrackEditor forms only
    // ugly: these must be doen AFTER init()
    void setDefaulted(bool b);
    bool isDefaulted();
    void setOccluded(bool b);
    bool isOccluded();
    void setOcclusionSource(juce::String src);
    
    // overrides the handler from YanField
    // ordinally should not be called directly
    void setDisabled(bool b) override;
    
    int getPreferredComponentWidth() override;
    void resized() override;

    Symbol* getSymbol() {
        return symbol;
    }

    void load(class MslValue* v);
    void save(class MslValue* v);

    void yanComboSelected(YanCombo* c, int selection) override;
    void yanInputChanged(YanInput* i) override;

  private:

    typedef enum {
        TypeText,
        TypeCombo,
        TypeCheckbox,
        TypeFile
    } Type;

    Type type = TypeText;
    
    void initCombo(class Provider* p, class Symbol* s);
    
    Listener* listener = nullptr;
    Symbol* symbol = nullptr;
    juce::StringArray structureNames;
    bool isText = false;
    bool isCombo = false;
    bool isCheckbox = false;
    bool defaulted = false;
    bool occluded = false;
    juce::String occlusionSource;
    
    // various renderings
    YanCombo combo {""};
    YanInput input {""};
    YanCheckbox checkbox {""};
    
};

