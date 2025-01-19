/**
 * Manages a collection of ParameterForms and can swap between them
 * as things change.
 *
 * The SessionEditor has two of these, one for the Global parameters and
 * one for the Track parameters.
 *
 * Forms are created dynamically as selections are made in a corresponding
 * ParameterTree.
 *
 */

class SessionFormCollection : public juce::Component
{
  public:
    
    SessionFormCollection();
    ~SessionFormCollection() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    void show(class Provider* p, juce::String formName);

    void load(class ValueSet* src);
    void save(class ValueSet* dest);
    
  private:

    class ValueSet* sourceValues = nullptr;

    juce::OwnedArray<class ParameterForm> forms;
    juce::HashMap<juce::String,class ParameterForm*> formTable;
    class ParameterForm* currentForm = nullptr;
    
};
#endif

