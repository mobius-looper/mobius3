/**
 * Somewhat like TransportElement but shows the satus of
 * MIDI sync being received.
 */

#include <JuceHeader.h>

#include "UIAtom.h"
#include "UIAtomList.h"
#include "UIElement.h"
#include "../../Provider.h"

class MidiSyncElement : public UIElement, public Provider::HighRefreshListener
{
  public:
    
    MidiSyncElement(class Provider* p, class UIElementDefinition* d);
    ~MidiSyncElement();

    void configure() override;

    // respond to update notifications if interested
    void update(class MobiusView* view) override;

    // tell us about yourself
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;

    void highRefresh(class PriorityState* state) override;
    
  private:

    UIAtomRadar radar;
    UIAtomFlash light;
    UIAtomFloat tempoAtom;
    UIAtomLabeledNumber bpb;
    UIAtomLabeledNumber bars;
    UIAtomLabeledNumber beat;
    UIAtomLabeledNumber bar;

    UIAtomList topRow;
    UIAtomList bottomRow;
    UIAtomList column;
    UIAtomSpacer spacer;
    
    int tempoValue = 0;
    int lastBeat = 0;
    int lastBar = 0;
    int lastLoop = 0;
    int lastBpb = 0;
    int lastBars = 0;
    bool lastStarted = false;
    
    void sizeAtom(juce::Rectangle<int> area, juce::Component* comp);
    void updateRadar(class MobiusView* v);
};

    


