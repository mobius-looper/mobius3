/**
 * An evolving system to run Mobius engine tests.
 *
 * Old comments from UnitTests that need to be rewritten...
 *  
 * Code related to running the unit tests.
 * This does some sensitive reach-arounds to Mobius without
 * going through KernelCommunicator so be careful...
 *
 * There is one singleton UnitTests object containing code to
 * implement various unit testing features that would not normally be active.
 * It is a part of MobiusShell.
 * 
 * The engine may be placed in "unit test mode" during which it forces the
 * installation of a Preset and a Setup with a known configuration, sets
 * a few global parameters, loads a set of Samples, and loads a set of Scripts.
 * The scripts may in turn add buttons to the UI.  This configuration takes the
 * place of the copy of MobiusConfig managed by the MobiusKernel and shared
 * with the Mobius core.
 *
 * Note that it does not replace the config managed by MobiusShell.  There was no
 * good reason for that other than it wasn't immediately necessary and saves some work
 * but might want to do that for consistency.  That also gives us a way to restore
 * the original config when unit test mode is canceled.
 *
 * This modified configuration is only active in memory, it is not saved
 * on the file system, and it will be lost if you edit the configuration in the UI
 * and push a new MobiusConfig down.  That effectively disables unit test mode.
 *
 * Unit test mode can be enabled in one of two ways.  First, by binding an action in
 * the UI to the UnitTestMode function.  This would typically be bound to a UI
 * action button.  Second, it may be enabled from a script with the old UnitTestSetup
 * statement.  This was a built-in script statement and not a Function.  Now that
 * we have the Function it could be replaced.
 *
 * While in unit test mode, the behavior of the SaveCapture, SaveLoop, and LoadLoop
 * functions are different.  It will load and save files relative to the "unit test root"
 * rather than normal installation root.  This is used so that the large library
 * of unit test sample and captured recording files can be located in a directory
 * apart from the main installation.  This file redirect is handled by KernelEventHandler.
 *
 * Configuration of samples and scripts is done with a configuration "overlay".  This
 * is an xml file found in the unit test root named mobius-overlay.xml.  It contains
 * a sparse MobiusConfig object with just the information that the tests need, primarily
 * this is a SampleConfig, a ScriptConfig, and a few global parameters.  When unit
 * test mode is activated, this file is ready and it's contents are merged with
 * the full MobiusConfig read from the mobius.xml file from the installation root.
 *
 * There is not currently a way to cancel unit test mode without restarting.
 * The test scripts call UnitTestSetup in every script, so we only do setup once
 * and then assume it remains stable.  May need to do partial reinitialization
 * of the Preset and the Setup but the samples and scripts can be reused.
 *
 * The same applies to the UnitTestMode function from the UI, though there it would
 * be convenient for the function to act as a toggle turning it on and off.
 * 
 */

#include <JuceHeader.h>

#include "../util/Util.h"
#include "../util/Trace.h"

#include "../model/old/MobiusConfig.h"
#include "../model/old/Preset.h"
#include "../model/old/Setup.h"
#include "../model/old/XmlRenderer.h"

#include "../model/ScriptConfig.h"
#include "../model/SampleConfig.h"
#include "../model/Symbol.h"
#include "../model/UIAction.h"
#include "../model/UIConfig.h"
#include "../model/SessionHelper.h"

#include "../mobius/MobiusInterface.h"
#include "../mobius/MobiusShell.h"
#include "../mobius/Audio.h"
#include "../mobius/AudioFile.h"
#include "../mobius/AudioPool.h"
#include "../mobius/SampleReader.h"
#include "../mobius/SampleManager.h"
#include "../mobius/WaveFile.h"
#include "../mobius/core/Mobius.h"

#include "../Supervisor.h"

#include "AudioDifferencer.h"
#include "TestDriver.h"

/**
 * Names that used to live somewhere and need someplace better
 */
#define UNIT_TEST_SETUP_NAME "UnitTestSetup"
#define UNIT_TEST_PRESET_NAME "UnitTestPreset"

int BlockNumber = 0;


TestDriver::TestDriver(Supervisor* super)
{
    supervisor = super;
}

TestDriver::~TestDriver()
{
}

Supervisor* TestDriver::getSupervisor()
{
    return supervisor;
}

void TestDriver::initialize(juce::Component* parent)
{
    // add our control panel to the parent component
    // could defer this until started
    parent->addChildComponent(controlPanel);
}

bool TestDriver::isActive()
{
    return active;
}

void TestDriver::start()
{
    if (!active) {
        // make sure we're starting out silent
        // come back with me now, to a time when memset ruled the earth
        memset(dummyInputBuffer, 0, sizeof(dummyInputBuffer));
        memset(dummyOutputBuffer, 0, sizeof(dummyOutputBuffer));
        
        // splice us into the MobiusListener and MobiusAudioListener chain
        defaultAudioListener = supervisor->overrideAudioListener(this);
        defaultMobiusListener = supervisor->overrideMobiusListener(this);

        installTestConfiguration();

        controlPanel.show();
        
        active = true;
    }
}

/**
 * All this does right now is splice out the listeners.
 * The configuration we installed will still remain, need to fix!
 */
void TestDriver::stop()
{
    if (active) {
        controlPanel.hide();
        supervisor->cancelListenerOverrides();
        defaultAudioListener =  nullptr;
        defaultMobiusListener = nullptr;
        active = false;
    }
}

/**
 * Called by the control panel when it closes itself.
 */
void TestDriver::controlPanelClosed()
{
    stop();
}

/**
 * So TestPanel can send actions.
 */
MobiusInterface* TestDriver::getMobius()
{
    return supervisor->getMobius();
}

/**
 * !!!!  Severe hackery for AudioPool::analyze
 */
AudioPool* TestDriver::getAudioPool()
{
    AudioPool* pool = nullptr;
    MobiusShell* shell = getMobiusShell();
    if (shell != nullptr)
      pool = shell->getAudioPool();
    return pool;
}

MobiusShell* TestDriver::getMobiusShell()
{
    return dynamic_cast<MobiusShell*>(supervisor->getMobius());
}

void TestDriver::captureConfiguration(UIConfig* config)
{
    juce::String testName = controlPanel.getTestName();
    config->put("testName", testName);
}

//////////////////////////////////////////////////////////////////////
//
// Test Execution
//
//////////////////////////////////////////////////////////////////////

/**
 * Turn bypass mode on and off in response to something in TestPanel.
 *
 * To make log messages interleave accurately we can enable "test mode"
 * in MobiusShell/MobiusKernel that allows normally asynchronous KernelEvents
 * like SaveCapture and DiffAudio to be processed synchronously in the same thread.
 */
void TestDriver::setBypass(bool b)
{
    if (b) {
        Trace(2, "TestDriver: Entering bypass mode\n");
        if (waitingId > 0) {
            // should have canceled this when we left bypass mode
            Trace(1, "TestDriver: Canceling lingering wait %d\n", waitingId);
            waitingId = 0;
        }
    }
    else {
        Trace(2, "TestDriver: Exiting bypass mode\n");
        if (waitingId > 0) {
            // this is harmless I suppose, the engine will start
            // getting live audio blocks and should eventually finish the script
            Trace(2, "TestDriver: Canceling test wait %d\n", waitingId);
            waitingId = 0;
        }
    }
    
    bypass = b;
    // hmm, this didn't work, the script didn't wait properly or something
    // and we thought it never finished
    //supervisor->getMobius()->setTestMode(true);
}

/**
 * Called by TestPanel when a test button is clicked.
 * This sends a script action to the engine which should
 * eventually call back to the mobiusScriptFinished
 * MobiusListener method when it finishes.
 */
void TestDriver::runTest(Symbol* s, juce::String testName)
{
    MobiusInterface* mobius = supervisor->getMobius();
    
    // if we did a global reset to stop a runaway test script, detect
    // that and cancel the last wait
    if (waitingId > 0 && mobius->isGlobalReset()) {
        Trace(2, "TestDriver: Canceling test wait after GlobalReset\n");
        waitingId = 0;
    }
    
    if (waitingId > 0) {
        Trace(1, "TestDriver: Ignoring request to run test %s, still waiting on %d\n",
              s->name.toUTF8(), waitingId);

        // todo: I suppose something could be misconfigured or broken
        // and we could make progress at least by canceling the old wait
        // and starting a new one
    }
    else {
        UIAction action;
        action.symbol = s;

        // if a targeted test name was typed in, use the Warp statement
        // to do just that one
        if (testName.length() > 0)
          CopyString(testName.toUTF8(), action.arguments, sizeof(action.arguments));

        // add a tracking id to detect completion
        action.requestId = requestIdCounter++;
    
        Trace(2, "TestDriver: Starting test script %s id %d\n", s->name.toUTF8(),
              action.requestId);

        mobius->doAction(&action);

        // when in bypass mode, simulate the audio stream by rapidly pumping
        // audio buffers at the engine during each maintenance thread cycle
        // remember the script id we launched so we can stop when it finishes
        BlockNumber = 0;
        if (bypass) {
            waitingId = action.requestId;
            waitStart = supervisor->getMillisecondCounter();
        }
    }
}

/**
 * MobiusListener callback when a script with a requestId finishes.
 * If this is the script we've been waiting on, cancel the wait state
 * and do post-test analysis.
 *
 * It isn't obvious but we are in the MainThread "maintenance thread" here.
 * MobiusListener calls are done by MobiusShell when it consumes KernelEvents
 * queued by MobiusKernel in the audio thread.  This happens inside
 * MobiusInterface::performMaintenance, which is called by Supervisor::advance()
 * whih is called every MainThread refresh cycle.
 */
void TestDriver::mobiusScriptFinished(int requestId)
{
    if (waitingId == 0) {
        if (bypass) {
            // this could happen if you had a test running and then
            // entered bypass mode in the middle of it, it should finish
            // faster once that happens since we're going to be pumping blocks,
            // but we didn't record the script id so we couldn't wait on it
            Trace(1, "TestDriver: Script finished that we weren't waiting for %d\n",
                  requestId);
        }
        else {
            // this is normal, we're not in bypass mode so we just ignore
            // any script notifications
            Trace(2, "TestDriver: Script finished %d\n", requestId);
            doTestAnalysis();
        }
    }
    else if (waitingId != requestId) {
        // this is not normal, we set up a wait state, but something else finished
        // this might happen if test scripts asynchronously launch other scripts,
        // but those wouldn't have a requestId so we wouldn't have received
        // a notification
        Trace(1, "TestDriver: Unexpected script finished %d, still waiting on %d\n",
              requestId, waitingId);
    }
    else {
        // a normal wait completion
        // it donesn't matter if bypass is on or off at this point, normally
        // it will be on, but you could have turned it off and just let the live
        // audio blocks slowly complete the test
        Trace(2, "TestDriver: Finished waiting for %d\n", waitingId);
        waitingId = 0;
        doTestAnalysis();
    }
}

/**
 * Called periodically by Supervisor which was poked by MainThrad.
 * The UI thread has been locked if you want to display things.
 */
void TestDriver::advance()
{
    if (active) {
        if (waitingId > 0) {
            int msec = supervisor->getMillisecondCounter();
            int delta = msec - waitStart;
            // wait at most 10 seconds
            // actually this is way to short for larger tests like layertest
            // added the Cancel button to abort the test, so we don't really\
            // need a timeout any more
            int timeoutSeconds = 60 * 10;
            if (delta >= (timeoutSeconds * 1000)) {
                Trace(1, "TestDriver: Timeout waiting for script %d\n", waitingId);
                cancel();
            }
            else {
                if (bypass) {
                    pumpBlocks();
                    if (waitingId == 0) {
                        // I don't think we can get here
                        // we were waiting, and we sent a bunch of blocks into the engine
                        // this may have caused the script to finish, but the MobiusListener
                        // method will not have been called yet, instead a KernelEvent will have
                        // been queued and it won't be processed until the next call to
                        // Mobius::performMaintenance, which is what calls mobiusScriptFinished above
                        // since mobiusScriptFinished can't have been called, the waitId can't
                        // have been cleared, unless something nefarious is going on
                        Trace(1, "TestDriver: Unexpected completion of wait state during advance %d\n");
                    }
                }
                else {
                    // must have turned off bypass mode while a test was running
                    // just let it complete with live audio blocks
                }
            }
        }
        else {
            avoidMemoryLeak();
        }
    }
    else if (waitingId > 0) {
        // must have closed the test panel while a test was still in progress
        // cancel it?
        Trace(1, "TestDriver: Canceling test wait after becomming inactive %d\n",
              waitingId);
        waitingId = 0;
    }
}

/**
 * Simulate the reception of a live audio block by calling MobiusListener
 * as if it were in the audio thread receiving blocks from JuceAudiostream.
 *
 * The goal of bypass mode is to pump blocks into the engine as fast as possible
 * so the script finishes quickly without having to wait for real-time audio blocks.
 * We could just pump blocks until the script finishes but if something is misconfigured
 * we don't want to go into an infiite loop either, and it would be good to let
 * the maintenance thread breathe once in awhile.  At a sample rate of 44100 and
 * a block size of 256 there are 172.2 blocks per second, or 5.8 milliseconds per block.
 *
 * So if we pump 172 blocks every 1/10th maintenance cycle, the test effectively
 * runs 10 times faster than it normally would.
 */
void TestDriver::pumpBlocks()
{
    // todo: pump some buffers if we're in bypass mode
    // see method comments for some of the math involved here
    int blocksToSimulate = 172;
    //int blocksToSimulate = 91;
    for (int i = 0 ; i < blocksToSimulate ; i++) {
        pumpBlock();
        BlockNumber++;
        // stop when waitingId goes off, but this shouldn't happen
        if (waitingId == 0) {
            Trace(1, "Canceling test wait unexpected\n");
            break;
        }
    }
}

void TestDriver::pumpBlock()
{
    // need to clear the input buffer every time since there
    // can be lingering sample content that was injected by the test
    // less clear if this is necessary for the output buffer
    // unless you want to monitor it
    memset(dummyInputBuffer, 0, sizeof(dummyInputBuffer));
    memset(dummyOutputBuffer, 0, sizeof(dummyOutputBuffer));
        
    defaultAudioListener->processAudioStream(this);
}

/**
 * The number of blocks we'll send to MobiusKernel after a
 * test finishes in bypass mode.  See comments above
 * avoidMemoryLeak for gory details.
 */
const int MemoryLeakCheckCount = 4;

/**
 * After a test has finished in either live or bypass mode
 * Do any post-completion analysis of the results.
 */
void TestDriver::doTestAnalysis()
{
    Trace(2, "TestDriver: Analyzing test results\n");
    // see comments above avoidMemoryLeak
    checkMemoryLeak = MemoryLeakCheckCount;
}

/**
 * Called by TestPanel when the Cancel button is clicked.
 * Since we wait forever for the test script to complete, if something
 * is broken this can be used to break us out of the wait and stop pumping
 * blocks.
 */
void TestDriver::cancel()
{
    if (active) {
        if (waitingId > 0) {
            Trace(2, "TestDriver: Canceling test %d\n", waitingId);
            waitingId = 0;

            // if the script is still active, cancel it by sending down
            // a GlobalReset
            UIAction action;
            action.symbol = supervisor->getSymbols()->intern("GlobalReset");
            MobiusInterface* mobius = supervisor->getMobius();
            mobius->doAction(&action);

            if (bypass) {
                // have to coninue pumping a few blocks to let the
                // GlobalReset get processed and the event lists get
                // cleaned up
                // see comments above avoidMemoryLeak
                checkMemoryLeak = MemoryLeakCheckCount;
            }
        }
    }
}

/**
 * Buckle up...
 * 
 * Called during the advance() cycle when we're not waiting on anything.
 * This is a rather ugly hack to avoid a random memory leak warning when you're
 * in bypass mode and shut down the app suddenly.   What can happen is this...
 * The test runs and either ends or is canceled.  As soon as we receive notification
 * that the test is finished, we stop pumping blocks if we're in bypass mode.
 * Without blocks being sent, MobiusKernel is effectively dead and won't do
 * anything.  It is common at the end of a test to have fired off a final
 * KernelEvent asking the shell to diff some test files.  And if you cancel
 * manually there can be other random KernelEvents queued for the shell.
 *
 * While the MobiusKernel is halted and not receiving blocks, we do still
 * allow the maintenance thread to call Mobius::performMaintenance which
 * will allow MobiusShell to find these queued events and do any remaining
 * processing from the scripts.  The convention is then to RETURN the
 * KernelEvent to MobiusKernel so that it may be put back in the event pool
 * and resused.  But since MobiusKernel has been suspended, those events
 * will remain on the queue until you run another test, or exit bypass mode.
 * If you close the app while in that state, VisualStudio will whine about a leak
 * because KernelEvents don't know how to delete everything that may be
 * attached to them.
 *
 * What this does is continue pumping a few residual blocks after a test is
 * complete or canceled to give MobiusKernel a chance to clean up.  We can't do
 * this immedately after we receive notice that the is complete because MobiusShell
 * runs in the maintenance thread and we have to let that call
 * MobiusShell::performMaintenance a few times to get into this state.
 * The exact number is unclear, and I'm tired so we'll do it a few times then stop.
 * 
 * None of this is necessary, and won't happen if you close the test panel
 * prior to exiting, but I hate those leak messges.
 */
void TestDriver::avoidMemoryLeak()
{
    if (bypass && checkMemoryLeak > 0) {
        pumpBlock();
        checkMemoryLeak--;
    }
}    

//////////////////////////////////////////////////////////////////////
//
// Simulated MobiusAudiostream
//
// Pretend we're JuceAudioStream and return empty input buffers when
// asked for them
//
//////////////////////////////////////////////////////////////////////

/**
 * The number of frames in the next audio block.
 * This is long for historical reasons, it doesn't need to be because int and long
 * are the same size.
 */
int TestDriver::getInterruptFrames()
{
    return 256;
}

/**
 * Access the interleaved input and output buffers for a "port".
 * Ports are arrangements of stereo pairs of mono channels.
 *
 * Don't need to simulate ports here, just return the same empty buffer
 * for all of them.
 */
void TestDriver::getInterruptBuffers(int inport, float** input, 
                                     int outport, float** output)
{
    (void)inport;
    (void)outport;
    if (input != nullptr) *input = dummyInputBuffer;
    if (output != nullptr) *output = dummyOutputBuffer;
}

/**
 * This will be interesting to simulate.
 */
juce::MidiBuffer* TestDriver::getMidiMessages()
{
    return nullptr;
}

//
// Stream Time
// This isn't implemented in JuceAudioStream yet, so don't bother
// with it here.
//

double TestDriver::getStreamTime()
{
    return 0.0f;
}

double TestDriver::getLastInterruptStreamTime()
{
    return 0.0f;
}

int TestDriver::getSampleRate()
{
    return 44100;
}

//////////////////////////////////////////////////////////////////////
//
// Listener Interception
//
//////////////////////////////////////////////////////////////////////

//
// MobiusAudioListener
//

/**
 * We will be inserted between JuceAudioStream and MobiusAudioStream
 * Simply pass through unless the "bypass" option is enabled.
 */
void TestDriver::processAudioStream(MobiusAudioStream* stream)
{
    if (!bypass) {
        defaultAudioListener->processAudioStream(stream);
        BlockNumber++;
    }
}

//
// MobiusListener
//

void TestDriver::mobiusTimeBoundary()
{
    // technically should be using defaultMobiusListener
    // but we know who it is
    supervisor->mobiusTimeBoundary();
}

/**
 * Here from the Echo statement to display debugging trace.
 */ 
void TestDriver::mobiusEcho(juce::String msg)
{
    controlPanel.log(msg);
}

/**
 * Here from the Message statement which is used less often
 * than Echo in scripts.  We'll add it to the log and also
 * pass it along to Supervisor to show in the UI.
 */
void TestDriver::mobiusMessage(juce::String msg)
{
    controlPanel.log(msg);
    supervisor->mobiusMessage(msg);
}

void TestDriver::mobiusAlert(juce::String msg)
{
    // these are more serious then Echo messages
    // just log them since Supervisor will popup up an alert dialog
    controlPanel.log(msg);
    // supervisor->mobiusAlert(msg);
}

void TestDriver::mobiusDoAction(UIAction* action)
{
    supervisor->mobiusDoAction(action);
}

void TestDriver::mobiusPrompt(MobiusPrompt* prompt)
{
    supervisor->mobiusPrompt(prompt);
}

void TestDriver::mobiusMidiReceived(juce::MidiMessage& msg)
{
    (void)msg;
}

void TestDriver::mobiusStateRefreshed(class SystemState* state)
{
    (void)state;
}

void TestDriver::mobiusSetFocusedTrack(int index)
{
    (void)index;
}

void TestDriver::mobiusGlobalReset()
{
}

void TestDriver::mobiusTestStart(juce::String name)
{
}

void TestDriver::mobiusTestStop(juce::String name)
{
}

void TestDriver::mobiusSaveAudio(Audio* content, juce::String fileName)
{
    bool normalMode = false;
    if (normalMode) {
        SessionHelper helper(supervisor);
        const char* quickfile = helper.getString(ParamQuickSave);
        if (quickfile == nullptr) {
            // this is what old code used, better name might
            // just be "quicksave" to show where it came from
            quickfile = "mobiusloop";
        }
        // this did an insane amount of work, see capture at the bottom of this file
        // juce::File file = getSaveFile(e->arg1, quickfile, ".wav");
        // AudioFile::write(file, content);
    }
    else {
        // old code allowed the fileName to be unspecified
        if (fileName.length() == 0) fileName = "testloop";

        // outside Test Mode this called KernelEventHandler::getSaveFile
        juce::File file = getResultFile(fileName);
        AudioFile::write(file, content);
    }
}

/**
 * This one Supervisor will need to implement and put the
 * file somewhere appropriate.
 * The old UnitTests redireted it relative to the results directory.
 *
 * Here from the SaveAudioRecording script statement for test scripts
 * or the SaveLoop function for normal bindings.
 */
void TestDriver::mobiusSaveCapture(Audio* content, juce::String fileName)
{
    bool normalMode = false;
    if (normalMode) {
        // this just captures approxomately what we once did
        // file = getSaveFile(e->arg1, "capture", ".wav");
        // AudioFile::write(file, content);
    }
    else {
        // old code allowed the fileName to be unspecified and
        // it defaulted to "testcapture"
        if (fileName.length() == 0) fileName = "testcapture";

        juce::File file = getResultFile(fileName);
        AudioFile::write(file, content);
    }
}

void TestDriver::mobiusDiff(juce::String result, juce::String expected, bool reverse)
{
    AudioDifferencer differ (this);

    // scripts may pass both names or just one
    juce::File resultFile = getResultFile(result);
    juce::File expectedFile;
    if (expected.length() > 0)
      expectedFile = getExpectedFile(expected);
    else
      expectedFile = getExpectedFile(result);

    differ.diff(resultFile, expectedFile, reverse);
}

void TestDriver::mobiusDiffText(juce::String result, juce::String expected)
{
    // same little missing name dance as audio diff
    juce::File resultFile = getResultFile(result);
    juce::File expectedFile;
    if (expected.length() > 0)
      expectedFile = getExpectedFile(expected);
    else
      expectedFile = getExpectedFile(result);

    diffText(resultFile, expectedFile);
}
                                                                                    
Audio* TestDriver::mobiusLoadAudio(juce::String fileName)
{
    return nullptr;
}

void TestDriver::mobiusDynamicConfigChanged()
{
    supervisor->mobiusDynamicConfigChanged();
}

//////////////////////////////////////////////////////////////////////
//
// Engine Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Hook for TestPanel to force installation again to pick up
 * script changes.
 */
void TestDriver::reinstall()
{
    installed = false;
    installTestConfiguration();
}

/**
 * Install scripts, samples and various expected configuration objects
 * in the engine.
 *
 * This is done once when test mode is activated for the first time.
 * Thereafter you an go in and out of test mode without having to send
 * the configuration again.  The Reload Test Configuration command button
 * can be used to force a reload.
 */
void TestDriver::installTestConfiguration()
{
    if (!installed) {

        // now we need to dive down and fuck with the core's MobiusConfig
        MobiusShell* shell = getMobiusShell();
        if (shell == nullptr) {
            Trace(1, "TestDriver: Unable to access MobiusShell\n");
        }
        else {

            // !!!!!!!! this is no longer working after the Session migration
            
            MobiusKernel* kernel = shell->getKernel();
            MobiusConfig* kernelConfig = kernel->getMobiusConfigForTestDriver();
    
            // special Setup and Preset
            installPresetAndSetup(kernelConfig);
        
            MobiusConfig* overlay = readConfigOverlay();
            if (overlay != nullptr) {

                // todo: rather than just replace the set of samples/scripts
                // with what is in the overlay, could merge them with
                // the active configs so we don't lose anything

                // load and install the samples
                // NOTE WELL: This makes use of some dangerous back doors left over
                // from when this code existed under MobiusShell
                // retaining that to get this working under TestDriver but need to decide
                // the best way for this to work
                // we could just go through MobiusInterface::installSamples but that
                // does an asynchronous thread transition we would need to wait for

                SampleManager* manager = shell->compileSamples(overlay->getSampleConfig());
                shell->sendSamples(manager, true);

                // load and install the scripts
                // note that since we're bypassing installScripts, we don't get
                // expansion of directories.  The test overlay doresn't use those
                // but it could be nice
                // !! todo: this is all oriented around ScriptRegistry now, though
                // these back doors still work, the entire test driver will need to
                // be redesigned once the test scripts are ported to .msl
                Scriptarian* scriptarian = shell->compileScripts(overlay->getScriptConfigObsolete());
                // todo: we have a way to return errors in the ScriptConfig now, should report them
                shell->sendScripts(scriptarian, true);

                // if we decide to defer DynamicConfigChanged notification
                // this is where you would do it
                // hmm, since all the send() functions do is updateDynamicConfig
                // and send notifications could just do that here and
                // avoid the kludgey "saveMode" flag
            }
            
            delete overlay;
        }

        installed = true;
    }
}

/**
 * Initialize the unit test setup and preset within a config object.
 * The MobiusConfig here is the one actually installed in MobiusKernel
 * and Mobius.
 */
void TestDriver::installPresetAndSetup(MobiusConfig* config)
{
    // boostrap a preset
    Preset* p = config->getPreset(UNIT_TEST_PRESET_NAME);
    if (p != NULL) {
        p->reset();
    }
    else {
        p = new Preset();
        p->setName(UNIT_TEST_PRESET_NAME);
        config->addPreset(p);
    }

    // boostrap a setup
    Setup* s = config->getSetup(UNIT_TEST_SETUP_NAME);
    if (s != NULL) {
        s->reset(p);
    }
    else {
        s = new Setup();
        s->setName(UNIT_TEST_SETUP_NAME);
        s->reset(p);
        config->addSetup(s);
    }

    // install the preset in the default
    s->setDefaultPresetName(UNIT_TEST_PRESET_NAME);

    // and install the setup as the startup
    config->setStartingSetupName(UNIT_TEST_SETUP_NAME);

    
    // this will propagate the changes to the tracks
    // Insane Hackery Alert
    // Since we directly modified the MobiusConfig in the core
    // we can bypass the usual config propatation layers
    // and cause it to be activated
    MobiusShell* shell = getMobiusShell();
    MobiusKernel* kernel = shell->getKernel();
    Mobius* mobius = kernel->getCore();

    // this is gone, and the whole unit test subsystem needs to
    // have a mighty adaptation
    (void)mobius;
    //mobius->setActiveSetup(UNIT_TEST_SETUP_NAME);
}

/**
 * Read the sparse MobiusConfig object from the test directory.
 * Sample and script paths are relative, and usually just leaf file
 * names.  Make them absolute paths to pass to the script/sample installer.
 */
MobiusConfig* TestDriver::readConfigOverlay()
{
    MobiusConfig* overlay = nullptr;

    juce::File root = getTestRoot();
    juce::File file = root.getChildFile("mobius-overlay.xml");
    if (file.existsAsFile()) {
        juce::String xml = file.loadFileAsString();
        XmlRenderer xr (supervisor->getSymbols());
        overlay = xr.parseMobiusConfig(xml.toUTF8());

        // resolve sample paths
        SampleConfig* samples = overlay->getSampleConfig();
        if (samples != nullptr) {
            Sample* sample = samples->getSamples();
            while (sample != nullptr) {
                const char* path = sample->getFilename();
                // these are expected to be relative to UnitTestRoot
                // could be smarter about absolute paths or $ references
                // but don't really need that yet
                file = root.getChildFile(path);
                if (!file.existsAsFile()) {
                    Trace(1, "TestDriver: Unable to resolve sample file %s\n",
                          file.getFullPathName().toUTF8());
                }
                else {
                    sample->setFilename(file.getFullPathName().toUTF8());
                }
                sample = sample->getNext();
            }
        }

        // same for scripts
        ScriptConfig* scripts = overlay->getScriptConfigObsolete();
        if (scripts != nullptr) {
            ScriptRef* script = scripts->getScripts();
            while (script != nullptr) {
                // weirdly doesn't use the same method name
                const char* path = script->getFile();
                file = root.getChildFile(path);
                if (!file.existsAsFile()) {
                    Trace(1, "TestDriver: Unable to resolve script file %s\n",
                          file.getFullPathName().toUTF8());
                }
                else {
                    script->setFile(file.getFullPathName().toUTF8());
                }
                script = script->getNext();
            }
        }
    }

    return overlay;
}

//////////////////////////////////////////////////////////////////////
//
// Files
//
//////////////////////////////////////////////////////////////////////

/**
 * Derive where the root of the unit test files are.
 * For initial testing, I'm wiring it under the source tree
 * which won't last long.
 */
juce::File TestDriver::getTestRoot()
{
    juce::File root = supervisor->getRoot();

    // hack, if we're using mobius-redirect and have already
    // redirected to a directory named "test" don't add
    // an additional subdir
    juce::String last = root.getFileNameWithoutExtension();
    if (last != juce::String("test"))
      root = root.getChildFile("test");

    return root;
}

/**
 * Given a base file name from a script, locate the full
 * path name to that file from the "results" folder
 * of the unit test root where the result files will be written.
 */
juce::File TestDriver::getResultFile(juce::String name)
{
    juce::File file = getTestRoot().getChildFile("results").getChildFile(name);

    // tests don't usually have an extension so add it
    // assuming a .wav file, will need more when we start dealing with projects
    file = addExtensionWav(file);

    return file;
}

juce::File TestDriver::addExtensionWav(juce::File file)
{
    if (file.getFileExtension().length() == 0)
      file = file.withFileExtension(".wav");

    return file;
}

/**
 * Given a base file name from a script, locate the full
 * path name to that file from the "master" folder
 * of the unit test root where the comparision files are read.
 *
 * We have historically called this "expected" rather than
 * "master" which I ususlly say in comments.
 *
 * Since the database of these is large and maintained in a different
 * Github repository, we support redirection.  If the file exists under
 * TestRoot it is used, otherwise we look for a file named "redirect" and assume
 * the contents of that is the full path of the folder where the file can be found.
 *
 * I'm liking this redirect notion. Generalize this into a common utility
 * and revisit mobius-redirect to use the same code.
 * 
 */
juce::File TestDriver::getExpectedFile(juce::String name)
{
    juce::File expected = getTestRoot().getChildFile("expected");
    juce::File file = expected.getChildFile(name);

    file = addExtensionWav(file);

    if (!file.existsAsFile()) {
        // not here check for redirect
        juce::File redirect = followRedirect(expected);
        file = redirect.getChildFile(name);
        file = addExtensionWav(file);
    }

    return file;
}

/**
 * This is basically the same as RootLoctor::checkRedirect
 * find a way to share.
 */
juce::File TestDriver::followRedirect(juce::File root)
{
    juce::File redirected = root;

    juce::File redirect = root.getChildFile("redirect");
    if (redirect.existsAsFile()) {
        
        juce::String content = redirect.loadFileAsString().trim();
        content = findRedirectLine(content);

        if (content.length() == 0) {
            Trace(1, "TestDriver: Redirect file found but was empty");
        }
        else {
            juce::File possible;
            if (juce::File::isAbsolutePath(content)) {
                possible = juce::File(content);
            }
            else {
                // this is the convention used by mobius-redirect
                // if the redirect file contents is relative make it
                // relative to the starting root
                possible = root.getChildFile(content);
            }
        
            if (possible.isDirectory()) {
                // RootLocator allow chains of redirection, unit tests don't
                redirected = possible;
            }
            else {
                Trace(1, "TestDriver: Redirect file found, but directory does not exist: %s\n",
                      possible.getFullPathName().toUTF8());
            }
        }
    }
    
    return redirected;
}

/**
 * Helper for followRedirect()
 * After loading the redirect file contents, look for a line
 * that is not commented out, meaning starts with a #
 */
juce::String TestDriver::findRedirectLine(juce::String src)
{
    juce::String line;

    juce::StringArray tokens;
    tokens.addTokens(src, "\n", "");
    for (int i = 0 ; i < tokens.size() ; i++) {
        line = tokens[i];
        if (!line.startsWith("#")) {
            break;
        }
    }
    
    return line;
}

//////////////////////////////////////////////////////////////////////
//
// Differencing
//
// AudioDifferencer does the binary audio fuzzy differenccing.
// Here we implement the text diff.
//
//////////////////////////////////////////////////////////////////////

/**
 * Special entry point for the temporary TestDiff intrinsic function
 * so we can run AudioDifferencer tests without having to record something live.
 *
 * todo: copied from UnitTests, decide if we still want this...
 */
#if 0
void TestDriver::analyzeDiff(UIAction* action)
{
    AudioDifferencer differ;
    differ.analyze(this, action);
}
#endif

/**
 * I think this was used to difference Project files.
 * Old code did a binary comparision even though it was text.
 * Use juce::File for this since these are far less touchy than
 * Audio files.
 */
void TestDriver::diffText(juce::File result, juce::File expected)
{
    // getFullPathName does not seem to be stable, so get it
    // every time just before it is used
    if (!result.existsAsFile()) {
        const char* path = result.getFullPathName().toUTF8();
        Trace(1, "TestDriver: Diff file not found: %s\n", path);
    }
    else if (!expected.existsAsFile()) {
        // expected file not there, could bootstrap it?
        const char* path = expected.getFullPathName().toUTF8();
        Trace(1, "TestDriver: Diff file not found: %s\n", path);
    }
    else if (result.getSize() != expected.getSize()) {
        const char* path1 = result.getFullPathName().toUTF8();
        const char* path2 = expected.getFullPathName().toUTF8();
        Trace(1, "TestDriver: Diff files differ in size: %s, %s\n", path1, path2);
    }
    else {
        // old tool did a byte-by-byte comparison and printed
        // the byte number where they differed, hasIdenticalContent
        // just returns true/false, go with that unless you need it
        if (!result.hasIdenticalContentTo(expected)) {
            const char* path1 = result.getFullPathName().toUTF8();
            const char* path2 = expected.getFullPathName().toUTF8();
            Trace(1, "TestDriver: Diff files are not identical: %s\n", path1, path2);
        }
    }
}

// scraped of the original getSaveFile from Github
// that was lost along the way
// here only for reference, I don't think it needs to
// be this complicated

#if 0
/**
 * Build the path to a file for save or load.
 * 
 * Old code allowed the path to be absolute, relative
 * to the CWD if it began with "./" or relative to the
 * home directory.  Home directory was from the MOBIUS_HOME
 * environment variable or a complex process that tried to locate
 * the the .dll or use the registry.  We'll now get this from
 * MobiusContainer and let Juce locate the standard locations.
 *
 * If that didn't work we used getRecordingPath which
 * first looked for MobiusConfig::quickSaveFile, stripped
 * off the leaf file name and appended "recording.wav".
 *
 * Now we'll call this "capture.wav".  Comments say
 * Quick Save used a counter for name uniqueness, capture
 * didn't have that.
 *
 * UnitTests have their own version of this with more assumptions.
 *
 * update: this isn't necessary any more TestDriver will set things up.
 */
juce::File KernelEventHandler::getSaveFile(const char* name, const char* defaultName,
                                           const char* extension)
{
    MobiusContainer* container = shell->getContainer();
    juce::File root = container->getRoot();
    juce::File file;

    if (strlen(name) > 0) {
        // name passed as a function arg
        if (juce::File::isAbsolutePath(name)) {
            juce::File maybe = juce::File(name);
            // Ambiguity about what this could mean: c:/foo/bar
            // Is that the "bar" base file name in the c:/foo directory
            // or is it an unspecified file name in the c:/foo/bar directory?
            // If the name was not absolute we assumed it was a file name,
            // possibly without an extension, and possibly under one or more
            // parent directories to be placed relative to the config directory
            // example: SaveAudioRecording ./$(testname)$(suffix)rec
            // to be consistent with that common use assume that an absolute path
            // includes the leaf file name, but possibly without an extension
            // any reason not to just force it to .wav ?
            if (maybe.getFileExtension().length() == 0)
              maybe = maybe.withFileExtension(extension);

            if (maybe.existsAsFile()) {
                file = maybe;
            }
            else {
                // not a file yet, I don't want to auto create parent directories
                // if the user typed it in wrong, so make sure the parent exists
                juce::File parent = maybe.getParentDirectory();
                if (parent.isDirectory()) {
                    file = maybe;
                }
                else {
                    // this is the first time we need an alert
                    // for unit tests, just trace and move on but for
                    // user initiated will need to display something
                    Trace(1, "TestDriver: Invalid file path: %s\n", maybe.getFullPathName().toUTF8());
                }
            }
        }
        else {
            // not absolute, put it under the root config directory
            MobiusContainer* container = shell->getContainer();
            juce::File root = container->getRoot();
            juce::File maybe = root.getChildFile(name);
            if (maybe.getFileExtension().length() == 0)
              maybe = maybe.withFileExtension(extension);

            if (maybe.existsAsFile()) {
                file = maybe;
            }
            else {
                // since we've extended the root which must exist, can allow
                // the automatic creation of subdirectries
                file = maybe;
            }
        }
    }
    else {
        // no name passed, use the defaults
        file = root.getChildFile(defaultName).withFileExtension(extension);

        // !! todo: old code checked the file for uniqueness and if it was
        // already there added a numeric qualifier, need to do this for both
        // quickSave and capture
    }

    // seems like an odd way to test for being set
    if (file == juce::File()) {
        // unable to verify a location, should have already traced why
    }
    else {
        juce::Result result = file.create();
        if (!result.wasOk()) {
            Trace(1, "TestDriver: Unable to create file: %s\n", file.getFullPathName().toUTF8());
            file = juce::File();
        }
    }

    return file;
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
