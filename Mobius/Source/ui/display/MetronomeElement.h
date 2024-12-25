/**
 * A UI element that dislays the status of the internal Metronome track
 * and provides buttons to control it.
 *
 * This differs from the generic UIElements in that it was designed for a specific
 * purpose and can only be used for the built-in Metronome.  It is only allowed in the
 * StatusArea and there can only be one of them.
 */

#include <JuceHeader.h>

#include "UIAtom.h"
#include "UIElement.h"
#include "../../Provider.h"

class MetronomeElement : public UIElement, public UIAtomButton::Listener,
                         public Provider::HighRefreshListener
{
  public:
    
    MetronomeElement(class Provider* p, class UIElementDefinition* d);
    ~MetronomeElement();

    void configure() override;

    // respond to update notifications if interested
    void update(class MobiusView* view) override;

    // tell us about yourself
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;

    void atomButtonPressed(UIAtomButton* b);

    void highRefresh(class MobiusPriorityState* state);
    
  private:

    UIAtomFlash light;
    UIAtomButton start;
    UIAtomButton tap;
    UIAtomText tempoAtom;
    int tempoValue = 0;
    int tapStart = 0;
    
    void sizeAtom(juce::Rectangle<int> area, juce::Component* comp);
};

    


