/**
 * Extension of AudioProccessor that traces calls the host
 * and/or the Juce runtime makes to the plugin, so we can
 * watch what is happening, and notice when there is a virtual
 * method being called we might want to implement.
 *
 * This will also add itself as the AudioProcessorListener
 * to trace listener calls.
 * 
 * None of this is necessary, but helps understand how plugins
 * are being used within different hosts.
 *
 * Beyond this, there are lots of AudioProcessor methods that are
 * not overridden by the subclass.  They return information about
 * the plugin that seem mostly intended for the host, but may be
 * interesting for plugin code to learn about how it is presenting
 * itself to the host.
 *
 * todo: Add a general "what does this plugin look like" tool that
 * dumps as much information about the AudioProcessor that we can
 * pull from the interface.  
 *
 */

class NosyAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorListener
{
  public:
    
    NosyAudioProcessor(juce::AudioProcessor::BusesProperies& buses);
    virtual ~NosyAudioProcessor();

    //////////////////////////////////////////////////////////////////////
    //
    // Feature Interrogation
    //
    //////////////////////////////////////////////////////////////////////

    // don't bother with this one since the subclass always needs to override it
    //const juce::String getName() const override;
    
    juce::StringArray getAlternateDisplayNames() const override;
    
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    bool supportsMPE() const override;
    bool supportsDoublePrecisionProcessing() const override;
    
    double getTailLengthSeconds() const override;
    juce::CurveData getResponseCurve(juce::CurveData::Type type) const override;

    juce::AudioProcessor::AudioProcessorParameter* getBypassParameter() const;

    //////////////////////////////////////////////////////////////////////
    //
    // Commands
    //
    //////////////////////////////////////////////////////////////////////

    void reset() override;
    void setPlayHead(juce::AudioPlayHead* newPlayHead) override;

    void addListener(juce::AudioProcessorListener* newListener) override;
    void removeListener(juce::AudioProcessorListener* listenerToRemove) override;

    void refreshParameterList() override;

    void setNonRealtime(bool isNonRealtime) noexcept override;
    
    void updateTrackProperties(const juce::TrackProperties& properties) override;
    
    //////////////////////////////////////////////////////////////////////
    //
    // Block Processing
    //
    //////////////////////////////////////////////////////////////////////
    
    // we're skipping these for now since those are extensively traced in JuceAudioStream, but
    // it would be good to move all that down here so we can keep trace together
    // and not clutter up the main code 
    
    //void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    //void releaseResources() override;
    //void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // processBlock with <double> won't be called unless you override
    // supportsDoublePrecisionProcessing

    // this one is optional, but we probably want to do something about it
    void processBlockBypassed(juce::AudioBuffer<float> &buffer, juce::MidiBuffer* midiMessages) override;
    //////////////////////////////////////////////////////////////////////
    //
    // Editor
    //
    //////////////////////////////////////////////////////////////////////
       
    // these we don't need to trace since they're always implemented
    //juce::AudioProcessorEditor* createEditor() override;
    //bool hasEditor() const override;

    //////////////////////////////////////////////////////////////////////
    //
    // Programs
    //
    //////////////////////////////////////////////////////////////////////

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //////////////////////////////////////////////////////////////////////
    //
    // Buses
    //
    //////////////////////////////////////////////////////////////////////

    bool canAddBus(bool isInput) const override;
    bool canRemoveBus(bool isInput) const override;
    
    // Projucer generated code conditionalizes this with 
    // #ifndef JucePlugin_PreferredChannelConfigurations
    // 
    // this is the {2,2}{4,4} string you can set in Projecer that magically
    // gets the buses defined without building a BusesProperties, possibly
    // in this superclass method.  
    // I've stopped using that so we can unconditonally override it, but I guess
    // if you ever want to start using that again, you would need to cause
    // the AudioProcessor method to be called so we get the built-in behavior
    bool isBusesLayoutSupported (const juce::AudioProcessor::BusesLayout& layouts) const override;

    bool canApplyBusesLayout(const juce::AudioProcessor::BusesLayout& layout) const override;
    bool applyBusLayouts(const juce::AudioProcessor::BusesLayout& layouts) const override;
    bool canApplyBusCountChange(bool isInput, bool isAddingBuses,
                                juce::AudioProcessor::BusProperties& outNewBusProperties) override;
    
    //////////////////////////////////////////////////////////////////////
    //
    // State
    //
    //////////////////////////////////////////////////////////////////////
    
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    void getCurrentProgramStateInformation(juce::MemoryBlock& destData) override;
    void setCurrentProgramStateInformation(const void* data, int sizeInBytes) override;
    
    //////////////////////////////////////////////////////////////////////
    //
    // Notifications
    //
    //////////////////////////////////////////////////////////////////////
    
    void memoryWarningReceived() override;
    void numChannelsChanged() override;
    void numBusesChanged() override;
    void processorLayoutsChanged() override;
    void audioWorkgroupContextChanged(const juce::AudioWorkgroup& workgroup) override;

    //////////////////////////////////////////////////////////////////////
    //
    // Extensions
    //
    //////////////////////////////////////////////////////////////////////

    juce::AAXClientExtensions& getAAXClientExtensions() override;
    juce::VST2ClientExtensions& getVST2ClientExtensions() override;
    juce::VST3ClientExtensions& getVST3ClientExtensions() override;

    //////////////////////////////////////////////////////////////////////
    //
    // AudioProcessorListener
    //
    //////////////////////////////////////////////////////////////////////

    void audioProcessorParameterChanged(juce::AudioProcessor* p, int parameterIndex, float newValue);
    void audioProcessorChanged(juce::AudioProcessor* p, const juce::AudioProcessorListener::ChangeDetails& details);
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor* p, int parameterIndex);
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor* p, int parameterIndex);
    
  private:

};

