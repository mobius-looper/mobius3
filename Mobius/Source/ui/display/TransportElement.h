/**
 * A UI element that dislays the status of the internal SyncMaster/Transport
 * and provides buttons to control it.
 *
 * This differs from the generic UIElements in that it was designed for a specific
 * purpose and can only be used for the built-in Transport.  It is only allowed in the
 * StatusArea and there can only be one of them.
 */

#include <JuceHeader.h>

#include "UIAtom.h"
#include "UIAtomList.h"
#include "UIElement.h"
#include "../../Provider.h"
#include "../common/YanPopup.h"

class TransportElement : public UIElement, public UIAtomButton::Listener,
                         public YanPopup::Listener,                         
                         public Provider::HighRefreshListener
{
  public:

    typedef enum {
        MenuMidi = 1,
        MenuClocks,
        MenuBeats,
        MenuBars
    } MenuItems;
    
    TransportElement(class Provider* p, class UIElementDefinition* d);
    ~TransportElement();

    void configure() override;

    // respond to update notifications if interested
    void update(class MobiusView* view) override;

    // tell us about yourself
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    // do Jucy things
    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    void atomButtonPressed(UIAtomButton* b) override;
    void yanPopupSelected(class YanPopup* src, int id) override;

    void highRefresh(class PriorityState* state) override;
    
  private:

    UIAtomRadar radar;
    UIAtomFlash light;
    UIAtomButton start;
    UIAtomButton tap;
    UIAtomFloat tempoAtom;
    UIAtomLabeledNumber bpb;
    UIAtomLabeledNumber bars;
    UIAtomLabeledNumber beat;
    UIAtomLabeledNumber bar;

    UIAtomList topRow;
    UIAtomList bottomRow;
    UIAtomList column;
    UIAtomSpacer spacer;

    YanPopup popup {this};
    
    int tempoValue = 0;
    juce::uint32 tapStart = 0;
    int lastBeat = 0;
    int lastBar = 0;
    int lastLoop = 0;
    int lastBpb = 0;
    int lastBars = 0;

    bool midiEnabled = false;
    bool clocksEnabled = false;
    
    void sizeAtom(juce::Rectangle<int> area, juce::Component* comp);
    void updateRadar(class MobiusView* v);
};

    


