/*
 * An XML generator for configuration objects.
 * Formerly this was embedded within each configuration class but
 * I like making model transformations more encapsulated to avoid
 * class clutter and making it more obvious how to do other types
 * of transforms, such as DTOs for the editor.
 *
 * The main object is MobiusConfig which contains several things
 *
 * MobiusConfig
 *   global paraeters
 *   Preset
 *     preset parameters
 *   Setup
 *     setup parameters, wait are these exposed as parameters?
 *     SetupTrack
 *       track parameters
 *       UserVariables
 *   BindingConfig
 *     Binding
 *   ScriptConfig
 *     ScriptRef
 *   SampleConfig
 *     Sample
 *
 * This also ControlSurface which was an experiment that never went
 * anywhere.
 *
 * Many things in MobiusConfig are defined as UIParameters whih means
 * they can be accessed in scripts and bindings.  The things that aren't
 * can only be changed in the UI.
 *
 */

// necessary to get FILE* for XMlParser.h
#include <stdio.h>
// for atoi
#include <stdlib.h>
// for strcmp
#include <string.h>

#include "../util/Trace.h"
#include "../util/List.h"
#include "../util/XmlBuffer.h"
#include "../util/XomParser.h"
#include "../util/XmlModel.h"
#include "../util/FileUtil.h"

#include "MobiusConfig.h"
#include "Preset.h"
#include "Setup.h"
#include "UserVariable.h"
#include "Binding.h"
#include "ScriptConfig.h"
#include "SampleConfig.h"
#include "UIParameter.h"
#include "GroupDefinition.h"

#include "XmlRenderer.h"

//////////////////////////////////////////////////////////////////////
//
// Public
//
//////////////////////////////////////////////////////////////////////

#define EL_MOBIUS_CONFIG "MobiusConfig"
#define EL_PRESET "Preset"

XmlRenderer::XmlRenderer()
{
}

XmlRenderer::~XmlRenderer()
{
}

//////////////////////////////////////////////////////////////////////
//
// Object renderers and cloners
//
// Really shouldn't need clone methods if we made all the objects
// copyable.
//
//////////////////////////////////////////////////////////////////////

char* XmlRenderer::render(MobiusConfig* c)
{
	char* xml = nullptr;
    XmlBuffer b;

    render(&b, c);
    xml = b.stealString();
    return xml;
}

MobiusConfig* XmlRenderer::parseMobiusConfig(const char* xml)
{
    MobiusConfig* config = nullptr;
	XomParser* parser = new XomParser();
    XmlDocument* doc = parser->parse(xml);
	
    if (doc == nullptr) {
        Trace(1, "XmlRender: Parse error %s\n", parser->getError());
    }
    else {
        XmlElement* e = doc->getChildElement();
        if (e == nullptr) {
            Trace(1, "XmlRender: Missing child element\n");
        }
        else if (!e->isName(EL_MOBIUS_CONFIG)) {
            Trace(1, "XmlRenderer: Document is not a MobiusConfig: %s\n", e->getName());
        }
        else {
            config = new MobiusConfig();
			parse(e, config);
        }
    }

    delete doc;
	delete parser;

    return config;
}

/**
 * Don't need XML based cloners any more but could use this
 * to verify that the class cloners work properly
 */
#if 0
Preset* XmlRenderer::clone(Preset* src)
{
    Preset* copy = nullptr;

    XmlBuffer b;
    render(&b, src);

	XomParser parser;
    // nicer if the parser owns the document so it can be
    // deleted with it, when would we not want that
	XmlDocument* doc = parser.parse(b.getString());
    if (doc != nullptr) {
        XmlElement* e = doc->getChildElement();
        if (e != nullptr) {
            copy = new Preset();
            parse(e, copy);
        }
        delete doc;
    }
    
    return copy;
}

Setup* XmlRenderer::clone(Setup* src)
{
    Setup* copy = nullptr;

    XmlBuffer b;
    render(&b, src);

	XomParser parser;
    // nicer if the parser owns the document so it can be
    // deleted with it, when would we not want that
	XmlDocument* doc = parser.parse(b.getString());
    if (doc != nullptr) {
        XmlElement* e = doc->getChildElement();
        if (e != nullptr) {
            copy = new Setup();
            parse(e, copy);
        }
        delete doc;
    }
    
    return copy;
}
#endif

/**
 * Really hating this repetition, figure out a way to share this
 */
MobiusConfig* XmlRenderer::clone(MobiusConfig* src)
{
    MobiusConfig* copy = nullptr;

    XmlBuffer b;
    render(&b, src);

	XomParser parser;
    // nicer if the parser owns the document so it can be
    // deleted with it, when would we not want that
	XmlDocument* doc = parser.parse(b.getString());
    if (doc != nullptr) {
        XmlElement* e = doc->getChildElement();
        if (e != nullptr) {
            copy = new MobiusConfig();
            parse(e, copy);
        }
        delete doc;
    }
    
    return copy;
}

//////////////////////////////////////////////////////////////////////
//
// Common Utilities
//
//////////////////////////////////////////////////////////////////////

#define EL_STRING "String"

void XmlRenderer::render(XmlBuffer* b, UIParameter* p, int value)
{
    if (p->type == UIParameterType::TypeEnum) {
        if (p->values == nullptr) {
            Trace(1, "XmlRenderer: Attempt to render enum parameter without value list %s\n",
                  p->getName());
        }
        else {
            // should do some range checking here but we're only ever getting a value
            // from an object member cast as an int
            // shoudl be letting the 
            // !! put this in UIParameter::getEnumValue
            b->addAttribute(p->getName(), p->getEnumName(value));
        }
    }
    else {
        // option to filter zero?
        // yes, lots of things are zero/false
        if (value > 0)
          b->addAttribute(p->getName(), value);
    }
}

void XmlRenderer::render(XmlBuffer* b, UIParameter* p, bool value)
{
    // old way used ExValue.getString which converted false to "false"
    // and wrote that, XmlBuffer suppresses it
    // continue to supress or included it for clarity?
    if (value)
      b->addAttribute(p->getName(), "true");
    //else
    //b->addAttribute(p->getName(), "false");
}

void XmlRenderer::render(XmlBuffer* b, UIParameter* p, const char* value)
{
    // any filtering options?
    if (value != nullptr)
      b->addAttribute(p->getName(), value);
}

/**
 * Most parameters are boolean, integer, or enumerations.
 * Parse and return an int which can then be cast by the caller.
 */
int XmlRenderer::parse(XmlElement* e, UIParameter* p)
{
    int value = 0;

    const char* str = e->getAttribute(p->getName());
    if (str != nullptr) {
        if (p->type == UIParameterType::TypeBool) {
            value = !strcmp(str, "true");
        }
        else if (p->type == UIParameterType::TypeInt) {
            value = atoi(str);
        }
        else if (p->type == UIParameterType::TypeEnum) {
            value = p->getEnumOrdinal(str);
            if (value < 0) {
                // invalid enum name, leave zero
                Trace(1, "XmlRenderer: Invalid enumeration value %s for %s\n", str, p->getName());
            }
        }
        else {
            // error: should not have called this method
            Trace(1, "XmlRenderer: Can't parse parameter %s as int\n", p->getName());
        }
    }
    else {
        // there was no attribute
        // note that by returning zero here it will initialize the bool/int/enum
        // to that value rather than selecting a default value or just leaving it alone
        // okay for now since the element is expected to have all attributes
    }

    return value;
}

/**
 * Parse a string attribute.
 * Can return the constant element attribute value, caller is expected
 * to copy it.
 */
const char* XmlRenderer::parseString(XmlElement* e, UIParameter* p)
{
    const char* value = nullptr;

    if (p->type == UIParameterType::TypeString ||
        p->type == UIParameterType::TypeStructure) {
        value = e->getAttribute(p->getName());
    }
    else {
        Trace(1, "XmlRenderer: Can't parse parameter %s value as a string\n", p->getName());
    }
    return value;
}

/**
 * Parse a list of <String> eleemnts within a given element.
 * Used mostly in MobiusConfig for function name lists.
 * TODO: I'm leaning toward CSVs for these
 */
StringList* XmlRenderer::parseStringList(XmlElement* e)
{
    StringList* names = new StringList();
    for (XmlElement* child = e->getChildElement() ; 
         child != nullptr ; 
         child = child->getNextElement()) {
        // assumed to be <String>xxx</String>
        const char* name = child->getContent();
        if (name != nullptr) 
          names->add(name);
    }
    return names;
}

void XmlRenderer::renderList(XmlBuffer* b, const char* elname, StringList* list)
{
	if (list != nullptr && list->size() > 0) {
		b->addStartTag(elname, true);
		b->incIndent();
		for (int i = 0 ; i < list->size() ; i++) {
			const char* name = list->getString(i);
			b->addElement(EL_STRING, name);
		}
		b->decIndent();
		b->addEndTag(elname, true);
	}		
}

//////////////////////////////////////////////////////////////////////
//
// Structure (formerly Bindable)
//
//////////////////////////////////////////////////////////////////////

#define ATT_NAME "name"
#define ATT_ORDINAL "ordinal"

/**
 * For Bindables, add the name.
 * The ordinal is runtime only but old comments say to include
 * it if the name is not set.  Can't think of the circumstances
 * where that would be necessary.
 */
void XmlRenderer::renderStructure(XmlBuffer* b, Structure* structure)
{
    const char* name = structure->getName();
    if (name != nullptr)
	  b->addAttribute(ATT_NAME, name);
	else
	  b->addAttribute(ATT_ORDINAL, structure->ordinal);
}

void XmlRenderer::parseStructure(XmlElement* e, Structure* structure)
{
	structure->setName(e->getAttribute(ATT_NAME));
    if (structure->getName() == nullptr)
      structure->ordinal = e->getIntAttribute(ATT_ORDINAL);
}

//////////////////////////////////////////////////////////////////////
//
// MobiusConfig
//
//////////////////////////////////////////////////////////////////////

#define EL_MOBIUS_CONFIG "MobiusConfig"
#define EL_PRESET "Preset"
#define EL_SETUP "Setup"

#define ATT_VERSION "version"
#define ATT_SETUP "setup"
#define ATT_MIDI_CONFIG "midiConfig"
#define ATT_UI_CONFIG  "uiConfig"
#define ATT_PLUGIN_HOST_REWINDS "pluginHostRewinds"

#define ATT_NO_SYNC_BEAT_ROUNDING "noSyncBeatRounding"

#define ATT_BINDINGS "bindings"
#define ATT_BINDING_OVERLAYS "bindingOverlays"

#define EL_FOCUS_LOCK_FUNCTIONS "FocusLockFunctions"
// old name for FocusLockFunctions
#define EL_GROUP_FUNCTIONS "GroupFunctions"
#define EL_MUTE_CANCEL_FUNCTIONS "MuteCancelFunctions"
#define EL_CONFIRMATION_FUNCTIONS "ConfirmationFunctions"
#define EL_ALT_FEEDBACK_DISABLES "AltFeedbackDisables"

// old name
#define EL_BINDING_CONFIG "BindingConfig"
// new name
#define EL_BINDING_SET "BindingSet"

#define EL_SCRIPT_CONFIG "ScriptConfig"
#define EL_SCRIPT_REF "ScripRef"
#define ATT_FILE "file"

// old name was just "Samples" not bothering with an upgrade
#define EL_SAMPLE_CONFIG "SampleConfig"

#define EL_CONTROL_SURFACE "ControlSurface"
#define ATT_NAME "name"

#define ATT_EDPISMS "edpisms"
#define ATT_CC_THRESHOLD "controllerActionThreshold"

#define EL_GROUP_DEFINITION "GroupDefinition"

void XmlRenderer::render(XmlBuffer* b, MobiusConfig* c)
{
	b->addOpenStartTag(EL_MOBIUS_CONFIG);
	b->setAttributeNewline(true);

    b->addAttribute(ATT_VERSION, c->getVersion());

    render(b, UIParameterQuickSave, c->getQuickSave());
    render(b, UIParameterNoiseFloor, c->getNoiseFloor());

    render(b, UIParameterInputLatency, c->getInputLatency());
    render(b, UIParameterOutputLatency, c->getOutputLatency());
    // don't bother saving this until it can have a more useful range
	//render(UIParameterFadeFrames, c->getFadeFrames());
    render(b, UIParameterMaxSyncDrift, c->getMaxSyncDrift());
    render(b, UIParameterTrackCount, c->getCoreTracksDontUseThis());
    
    // UIParameter is gone, and this shouldn't be used any more, but the
    // upgrader still needs to parse it?
    //render(b, UIParameterGroupCount, c->getTrackGroupsDeprecated());
    if (c->getTrackGroupsDeprecated() > 0)
      b->addAttribute("groupCount", c->getTrackGroupsDeprecated());
    
    render(b, UIParameterMaxLoops, c->getMaxLoops());
    render(b, UIParameterLongPress, c->getLongPress());
    render(b, UIParameterMonitorAudio, c->isMonitorAudio());
	b->addAttribute(ATT_PLUGIN_HOST_REWINDS, c->isHostRewinds());
    render(b, UIParameterAutoFeedbackReduction, c->isAutoFeedbackReduction());
    // don't allow this to be persisted any more, can only be set in scripts
	//render(IsolateOverdubsParameter->getName(), mIsolateOverdubs);
    render(b, UIParameterSpreadRange, c->getSpreadRange());
    //render(b, UIParameterTraceLevel, c->getTraceDebugLevel());
    render(b, UIParameterSaveLayers, c->isSaveLayers());
    render(b, UIParameterDriftCheckPoint, c->getDriftCheckPoint());

    render(b, UIParameterGroupFocusLock, c->isGroupFocusLock());

    b->addAttribute(ATT_NO_SYNC_BEAT_ROUNDING, c->isNoSyncBeatRounding());

    //render(b, UIParameterSampleRate, c->getSampleRate());

    // active setup name
    // old notes say if the preset has been overridden this is not
    // saved in the config
    b->addAttribute(ATT_SETUP, c->getStartingSetupName());

    // not an official Parameter yet
    if (c->isEdpisms())
      b->addAttribute(ATT_EDPISMS, "true");

    b->addAttribute(ATT_CC_THRESHOLD, c->mControllerActionThreshold);

	b->add(">\n");
	b->incIndent();

	for (Preset* p = c->getPresets() ; p != nullptr ; p = (Preset*)(p->getNext()))
	  render(b, p);

	for (Setup* s = c->getSetups() ; s != nullptr ; s = (Setup*)(s->getNext()))
	  render(b, s);

	for (BindingSet* bs = c->getBindingSets() ; bs != nullptr ; bs = (BindingSet*)(bs->getNext()))
	  render(b, bs);

	if (c->getScriptConfigObsolete() != nullptr)
      render(b, c->getScriptConfigObsolete());

	if (c->getSampleConfig() != nullptr)
      render(b, c->getSampleConfig());

    for (auto group : c->dangerousGroups)
      render(b, group);
#if 0
    // never really implemented these
	for (ControlSurfaceConfig* cs = c->getControlSurfaces() ; cs != nullptr ; cs = cs->getNext())
      render(b, cs);
#endif

    // though they are top-level parameters, put these last since
    // they are long and not as interesting as the main child objects
    // TODO: just use csv like SustainFunctions
    renderList(b, EL_FOCUS_LOCK_FUNCTIONS, c->getFocusLockFunctions());
    renderList(b, EL_MUTE_CANCEL_FUNCTIONS, c->getMuteCancelFunctions());
    renderList(b, EL_CONFIRMATION_FUNCTIONS, c->getConfirmationFunctions());
    renderList(b, EL_ALT_FEEDBACK_DISABLES, c->getAltFeedbackDisables());

	b->decIndent();
	b->setAttributeNewline(false);

	b->addEndTag(EL_MOBIUS_CONFIG);
}

void XmlRenderer::parse(XmlElement* e, MobiusConfig* c)
{
    c->setVersion(e->getIntAttribute(ATT_VERSION));
    
    // save this for upgrade
    // this is part of OldBinding, get rid of this?
    // c->setSelectedMidiConfig(e->getAttribute(ATT_MIDI_CONFIG));
    
	c->setQuickSave(parseString(e, UIParameterQuickSave));

	c->setNoiseFloor(parse(e, UIParameterNoiseFloor));
	c->setInputLatency(parse(e, UIParameterInputLatency));
	c->setOutputLatency(parse(e, UIParameterOutputLatency));
	c->setMaxSyncDrift(parse(e, UIParameterMaxSyncDrift));
	c->setCoreTracks(parse(e, UIParameterTrackCount));
    
	//c->setTrackGroupsDeprecated(parse(e, UIParameterGroupCount));
	c->setTrackGroupsDeprecated(e->getIntAttribute("groupCount"));
    
	c->setMaxLoops(parse(e, UIParameterMaxLoops));
	c->setLongPress(parse(e, UIParameterLongPress));

	c->setMonitorAudio(parse(e, UIParameterMonitorAudio));
	c->setHostRewinds(e->getBoolAttribute(ATT_PLUGIN_HOST_REWINDS));
	c->setAutoFeedbackReduction(parse(e, UIParameterAutoFeedbackReduction));

    // don't allow this to be persisted any more, can only be set in scripts
	//setIsolateOverdubs(e->getBoolAttribute(IsolateOverdubsParameter->getName()));
	c->setSpreadRange(parse(e, UIParameterSpreadRange));
	//c->setTracePrintLevel(parse(e, UIParameterTracePrintLevel));
	//c->setTraceDebugLevel(parse(e, UIParameterTraceLevel));
	c->setSaveLayers(parse(e, UIParameterSaveLayers));
	c->setDriftCheckPoint((DriftCheckPoint)parse(e, UIParameterDriftCheckPoint));

    // this isn't a parameter yet
    c->setNoSyncBeatRounding(e->getBoolAttribute(ATT_NO_SYNC_BEAT_ROUNDING));

    // not an official parameter yet
    c->setEdpisms(e->getBoolAttribute(ATT_EDPISMS));

    c->mControllerActionThreshold = e->getIntAttribute(ATT_CC_THRESHOLD);
    
	//c->setSampleRate((AudioSampleRate)parse(e, UIParameterSampleRate));

    // fade frames can no longer be set high so we don't bother exposing it
	//setFadeFrames(e->getIntAttribute(FadeFramesParameter->getName()));

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_PRESET)) {
			Preset* p = new Preset();
            parse(child, p);
			c->addPreset(p);
		}
		else if (child->isName(EL_SETUP)) {
			Setup* s = new Setup();
            parse(child, s);
			c->addSetup(s);
		}
		else if (child->isName(EL_BINDING_CONFIG) ||
                 child->isName(EL_BINDING_SET)) {
			BindingSet* bs = new BindingSet();
            parse(child, bs);
			c->addBindingSet(bs);
		}
		else if (child->isName(EL_SCRIPT_CONFIG)) {
			ScriptConfig* sc = new ScriptConfig();
            parse(child, sc);
            c->setScriptConfigObsolete(sc);
		}
		else if (child->isName(EL_SAMPLE_CONFIG)) {
			SampleConfig* sc = new SampleConfig();
            parse(child, sc);
            c->setSampleConfig(sc);
		}
		else if (child->isName(EL_GROUP_DEFINITION)) {
			GroupDefinition* gd = new GroupDefinition();
            parse(child, gd);
            c->dangerousGroups.add(gd);
		}

        // never did fully support this 
		//else if (child->isName(EL_CONTROL_SURFACE)) {
        //ControlSurfaceConfig* cs = new ControlSurfaceConfig();
        //parse(child, cs);
        //c->addControlSurface(cs);
        //}


		else if (child->isName(EL_FOCUS_LOCK_FUNCTIONS) ||
                 child->isName(EL_GROUP_FUNCTIONS)) {
            // changed the name in 1.43
            c->setFocusLockFunctions(parseStringList(child));
		}
		else if (child->isName(EL_MUTE_CANCEL_FUNCTIONS)) {
            c->setMuteCancelFunctions(parseStringList(child));
		}
		else if (child->isName(EL_CONFIRMATION_FUNCTIONS)) {
            c->setConfirmationFunctions(parseStringList(child));
		}
		else if (child->isName(EL_ALT_FEEDBACK_DISABLES)) {
            c->setAltFeedbackDisables(parseStringList(child));
		}
        else {
            Trace(1, "XmlRenderer: Unknnown element %s\n", child->getName());
        }
	}

    // formerly had to do these last after the object lists
    // were built, now they're just names
    c->setStartingSetupName(e->getAttribute(ATT_SETUP));
}

//////////////////////////////////////////////////////////////////////
//
// Preset
//
//////////////////////////////////////////////////////////////////////

void XmlRenderer::render(XmlBuffer* b, Preset* p)
{
	b->addOpenStartTag(EL_PRESET);
	b->setAttributeNewline(true);

	// name, number
	renderStructure(b, p);

    render(b, UIParameterAltFeedbackEnable, p->isAltFeedbackEnable());
    render(b, UIParameterBounceQuantize, p->getBounceQuantize());
    render(b, UIParameterEmptyLoopAction, p->getEmptyLoopAction());
    render(b, UIParameterEmptyTrackAction, p->getEmptyTrackAction());
    render(b, UIParameterLoopCount, p->getLoops());
    render(b, UIParameterMaxRedo, p->getMaxRedo());
    render(b, UIParameterMaxUndo, p->getMaxUndo());
    render(b, UIParameterMultiplyMode, p->getMultiplyMode());
    render(b, UIParameterMuteCancel, p->getMuteCancel());
    render(b, UIParameterMuteMode, p->getMuteMode());
    render(b, UIParameterNoFeedbackUndo, p->isNoFeedbackUndo());
    render(b, UIParameterNoLayerFlattening, p->isNoLayerFlattening());
    render(b, UIParameterOverdubQuantized, p->isOverdubQuantized());
    render(b, UIParameterOverdubTransfer, p->getOverdubTransfer());
    render(b, UIParameterPitchBendRange, p->getPitchBendRange());
    //render(b, UIParameterPitchSequence, p->getPitchSequence());
    render(b, UIParameterPitchShiftRestart, p->isPitchShiftRestart());
    render(b, UIParameterPitchStepRange, p->getPitchStepRange());
    render(b, UIParameterPitchTransfer, p->getPitchTransfer());
    render(b, UIParameterQuantize, p->getQuantize());
    render(b, UIParameterSpeedBendRange, p->getSpeedBendRange());
    render(b, UIParameterSpeedRecord, p->isSpeedRecord());
    //render(b, UIParameterSpeedSequence, p->getSpeedSequence());
    render(b, UIParameterSpeedShiftRestart, p->isSpeedShiftRestart());
    render(b, UIParameterSpeedStepRange, p->getSpeedStepRange());
    render(b, UIParameterSpeedTransfer, p->getSpeedTransfer());
    render(b, UIParameterTimeStretchRange, p->getTimeStretchRange());
    render(b, UIParameterRecordResetsFeedback, p->isRecordResetsFeedback());
    render(b, UIParameterRecordTransfer, p->getRecordTransfer());
    render(b, UIParameterReturnLocation, p->getReturnLocation());
    render(b, UIParameterReverseTransfer, p->getReverseTransfer());
    render(b, UIParameterRoundingOverdub, p->isRoundingOverdub());
    render(b, UIParameterShuffleMode, p->getShuffleMode());
    render(b, UIParameterSlipMode, p->getSlipMode());
    render(b, UIParameterSlipTime, p->getSlipTime());
    render(b, UIParameterSoundCopyMode, p->getSoundCopyMode());
    render(b, UIParameterSubcycles, p->getSubcycles());
    //render(b, UIParameterSustainFunctions, p->getSustainFunctions());
    render(b, UIParameterSwitchDuration, p->getSwitchDuration());
    render(b, UIParameterSwitchLocation, p->getSwitchLocation());
    render(b, UIParameterSwitchQuantize, p->getSwitchQuantize());
    render(b, UIParameterSwitchVelocity, p->isSwitchVelocity());
    render(b, UIParameterTimeCopyMode, p->getTimeCopyMode());
    render(b, UIParameterTrackLeaveAction, p->getTrackLeaveAction());
    render(b, UIParameterWindowEdgeAmount, p->getWindowEdgeAmount());
    render(b, UIParameterWindowEdgeUnit, p->getWindowEdgeUnit());
    render(b, UIParameterWindowSlideAmount, p->getWindowSlideAmount());
    render(b, UIParameterWindowSlideUnit, p->getWindowSlideUnit());

	b->add("/>\n");
	b->setAttributeNewline(false);
}

void XmlRenderer::parse(XmlElement* e, Preset* p)
{
	parseStructure(e, p);

    p->setAltFeedbackEnable(parse(e, UIParameterAltFeedbackEnable));
    p->setBounceQuantize(parse(e, UIParameterBounceQuantize));
    p->setEmptyLoopAction(parse(e, UIParameterEmptyLoopAction));
    p->setEmptyTrackAction(parse(e, UIParameterEmptyTrackAction));
    p->setLoops(parse(e, UIParameterLoopCount));
    p->setMaxRedo(parse(e, UIParameterMaxRedo));
    p->setMaxUndo(parse(e, UIParameterMaxUndo));
    p->setMultiplyMode(parse(e, UIParameterMultiplyMode));
    p->setMuteCancel(parse(e, UIParameterMuteCancel));
    p->setMuteMode(parse(e, UIParameterMuteMode));
    p->setNoFeedbackUndo(parse(e, UIParameterNoFeedbackUndo));
    p->setNoLayerFlattening(parse(e, UIParameterNoLayerFlattening));
    p->setOverdubQuantized(parse(e, UIParameterOverdubQuantized));
    p->setOverdubTransfer(parse(e, UIParameterOverdubTransfer));
    p->setPitchBendRange(parse(e, UIParameterPitchBendRange));
    //p->setPitchSequence(parseString(e, UIParameterPitchSequence));
    p->setPitchShiftRestart(parse(e, UIParameterPitchShiftRestart));
    p->setPitchStepRange(parse(e, UIParameterPitchStepRange));
    p->setPitchTransfer(parse(e, UIParameterPitchTransfer));
    p->setQuantize(parse(e, UIParameterQuantize));
    p->setSpeedBendRange(parse(e, UIParameterSpeedBendRange));
    p->setSpeedRecord(parse(e, UIParameterSpeedRecord));
    //p->setSpeedSequence(parseString(e, UIParameterSpeedSequence));
    p->setSpeedShiftRestart(parse(e, UIParameterSpeedShiftRestart));
    p->setSpeedStepRange(parse(e, UIParameterSpeedStepRange));
    p->setSpeedTransfer(parse(e, UIParameterSpeedTransfer));
    p->setTimeStretchRange(parse(e, UIParameterTimeStretchRange));
    p->setRecordResetsFeedback(parse(e, UIParameterRecordResetsFeedback));
    p->setRecordTransfer(parse(e, UIParameterRecordTransfer));
    p->setReturnLocation(parse(e, UIParameterReturnLocation));
    p->setReverseTransfer(parse(e, UIParameterReverseTransfer));
    p->setRoundingOverdub(parse(e, UIParameterRoundingOverdub));
    p->setShuffleMode(parse(e, UIParameterShuffleMode));
    p->setSlipMode(parse(e, UIParameterSlipMode));
    p->setSlipTime(parse(e, UIParameterSlipTime));
    p->setSoundCopyMode(parse(e, UIParameterSoundCopyMode));
    p->setSubcycles(parse(e, UIParameterSubcycles));
    //p->setSustainFunctions(parseString(e, UIParameterSustainFunctions));
    p->setSwitchDuration(parse(e, UIParameterSwitchDuration));
    p->setSwitchLocation(parse(e, UIParameterSwitchLocation));
    p->setSwitchQuantize(parse(e, UIParameterSwitchQuantize));
    p->setSwitchVelocity(parse(e, UIParameterSwitchVelocity));
    p->setTimeCopyMode(parse(e, UIParameterTimeCopyMode));
    p->setTrackLeaveAction(parse(e, UIParameterTrackLeaveAction));
    p->setWindowEdgeAmount(parse(e, UIParameterWindowEdgeAmount));

    // ugh, I seem to have made redundant setters for all these that take an int
    // rather than an enum, but not these, why?  Kind of like not having the duplication
    p->setWindowEdgeUnit((WindowUnit)parse(e, UIParameterWindowEdgeUnit));
    p->setWindowSlideAmount(parse(e, UIParameterWindowSlideAmount));
    p->setWindowSlideUnit((WindowUnit)parse(e, UIParameterWindowSlideUnit));
}

//////////////////////////////////////////////////////////////////////
//
// Setup
//
//////////////////////////////////////////////////////////////////////

#define EL_SETUP "Setup"

#define ATT_MIDI_CONFIG "midiConfig"

#define ATT_NAME "name"
#define ATT_ACTIVE "active"
#define ATT_TRACK_GROUPS "trackGroups"
#define ATT_RESET_RETAINS "resetRetains"
#define ATT_ACTIVE "active"

#define EL_SETUP_TRACK "SetupTrack"
#define EL_VARIABLES "Variables"
#define ATT_GROUP_NAME "groupName"

void XmlRenderer::render(XmlBuffer* b, Setup* setup)
{
	b->addOpenStartTag(EL_SETUP);
	b->setAttributeNewline(true);

	renderStructure(b, setup);

    // these haven't been defined as Parameters, now that we're
    // doing that for the sync options could do these...
    b->addAttribute(ATT_ACTIVE, setup->getActiveTrack());
    b->addAttribute(ATT_BINDINGS, setup->getBindings());

    render(b, UIParameterDefaultPreset, setup->getDefaultPresetName());
    
    // these are a csv while the function lists in MobiusConfig
    // are String lists, should be consistent, I'm liking csv for brevity
    b->addAttribute(ATT_RESET_RETAINS, setup->getResetRetains());

    //render(b, UIParameterBeatsPerBar, setup->getBeatsPerBar());
    // why is the name pattern not followed here?
    render(b, UIParameterDefaultSyncSource, setup->getSyncSource());
    render(b, UIParameterDefaultTrackSyncUnit, setup->getSyncTrackUnit());
    //render(b, UIParameterManualStart, setup->isManualStart());
    //render(b, UIParameterMaxTempo, setup->getMaxTempo());
    //render(b, UIParameterMinTempo, setup->getMinTempo());
    //render(b, UIParameterMuteSyncMode, setup->getMuteSyncMode());
    render(b, UIParameterRealignTime, setup->getRealignTime());
    render(b, UIParameterResizeSyncAdjust, setup->getResizeSyncAdjust());
    render(b, UIParameterSlaveSyncUnit, setup->getSyncUnit());
    render(b, UIParameterSpeedSyncAdjust, setup->getSpeedSyncAdjust());

    b->add(">\n");
	b->incIndent();

    for (SetupTrack* t = setup->getTracks() ; t != nullptr ; t = t->getNext())
	  render(b, t);

	b->decIndent();
	b->setAttributeNewline(false);
	b->addEndTag(EL_SETUP, true);
}

void XmlRenderer::parse(XmlElement* e, Setup* setup)
{
	parseStructure(e, setup);

	setup->setActiveTrack(e->getIntAttribute(ATT_ACTIVE));
	setup->setBindings(e->getAttribute(ATT_BINDINGS));

    setup->setDefaultPresetName(parseString(e, UIParameterDefaultPreset));
    
    setup->setResetRetains(e->getAttribute(ATT_RESET_RETAINS));

    //setup->setBeatsPerBar(parse(e, UIParameterBeatsPerBar));
    setup->setSyncSource((OldSyncSource)parse(e, UIParameterDefaultSyncSource));
    setup->setSyncTrackUnit((SyncTrackUnit)parse(e, UIParameterDefaultTrackSyncUnit));
    //setup->setManualStart(parse(e, UIParameterManualStart));
    //setup->setMaxTempo(parse(e, UIParameterMaxTempo));
    //setup->setMinTempo(parse(e, UIParameterMinTempo));
    //setup->setMuteSyncMode(parse(e, UIParameterMuteSyncMode));
    setup->setRealignTime(parse(e, UIParameterRealignTime));
    setup->setResizeSyncAdjust(parse(e, UIParameterResizeSyncAdjust));
    setup->setSyncUnit((OldSyncUnit)parse(e, UIParameterSlaveSyncUnit));
    setup->setSpeedSyncAdjust(parse(e, UIParameterSpeedSyncAdjust));

    SetupTrack* tracks = nullptr;
    SetupTrack* last = nullptr;
	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {
        // todo: should verify the element name
		SetupTrack* t = new SetupTrack();
        parse(child, t);
		if (last == nullptr)
		  tracks = t;
		else
		  last->setNext(t);
		last = t;
	}
    setup->setTracks(tracks);
}

void XmlRenderer::render(XmlBuffer* b, SetupTrack* t)
{
	b->addOpenStartTag(EL_SETUP_TRACK);

    const char* name = t->getName();
    if (name != nullptr)
	  b->addAttribute(ATT_NAME, name);

    // in the old model, this was driven from Parameters
    // in TRACK scope that did not have the transient flag set
    // this was only InputPort, OutputPort, and PresetNumber
    // actually there are a lot missing and not just ones with transient

    render(b, UIParameterTrackPreset, t->getTrackPresetName());
    render(b, UIParameterFocus, t->isFocusLock());
    render(b, UIParameterMono, t->isMono());

    // groups are now referenced by name
    render(b, UIParameterGroup, t->getGroupNumberDeprecated());
    if (t->getGroupName().length() > 0) {
        juce::String gname = t->getGroupName();
        b->addAttribute(ATT_GROUP_NAME, (const char*)(gname.toUTF8()));
    }
    render(b, UIParameterInput, t->getInputLevel());
    render(b, UIParameterOutput, t->getOutputLevel());
    render(b, UIParameterFeedback, t->getFeedback());
    render(b, UIParameterAltFeedback, t->getAltFeedback());
    render(b, UIParameterPan, t->getPan());

    render(b, UIParameterAudioInputPort, t->getAudioInputPort());
    render(b, UIParameterAudioOutputPort, t->getAudioOutputPort());
    render(b, UIParameterPluginInputPort, t->getPluginInputPort());
    render(b, UIParameterPluginOutputPort, t->getPluginOutputPort());

    render(b, UIParameterOldSyncSource, t->getSyncSource());
    render(b, UIParameterOldTrackSyncUnit, t->getSyncTrackUnit());

    UserVariables* uv = t->getVariables();
    if (uv == nullptr) {
        b->add("/>\n");
    }
    else {
		b->add(">\n");
		b->incIndent();

        render(b, uv);

		b->decIndent();
		b->addEndTag(EL_SETUP_TRACK);
	}
}

void XmlRenderer::parse(XmlElement* e, SetupTrack* t)
{
	t->setName(e->getAttribute(ATT_NAME));

    // if we're reading an old mobius.xml for upgrade, the track name
    // attribute changed
    const char* oldName = e->getAttribute("trackName");
    if (oldName != nullptr)
      t->setName(oldName);
    
    t->setTrackPresetName(parseString(e, UIParameterTrackPreset));
    t->setFocusLock(parse(e, UIParameterFocus));
    t->setMono(parse(e, UIParameterMono));

    // should stop having group numbers eventually
    t->setGroupNumberDeprecated(parse(e, UIParameterGroup));
    const char* groupName = e->getAttribute(ATT_GROUP_NAME);
    if (groupName != nullptr)
      t->setGroupName(juce::String(groupName));
    
    t->setInputLevel(parse(e, UIParameterInput));
    t->setOutputLevel(parse(e, UIParameterOutput));
    t->setFeedback(parse(e, UIParameterFeedback));
    t->setAltFeedback(parse(e, UIParameterAltFeedback));
    t->setPan(parse(e, UIParameterPan));

    t->setAudioInputPort(parse(e, UIParameterAudioInputPort));
    t->setAudioOutputPort(parse(e, UIParameterAudioOutputPort));
    t->setPluginInputPort(parse(e, UIParameterPluginInputPort));
    t->setPluginOutputPort(parse(e, UIParameterPluginOutputPort));

    t->setSyncSource((OldSyncSource)parse(e, UIParameterOldSyncSource));
    t->setSyncTrackUnit((SyncTrackUnit)parse(e, UIParameterOldTrackSyncUnit));

    // should only have a single UserVariables 
	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_VARIABLES)) {
            UserVariables* uv = new UserVariables();
            parse(child, uv);
            t->setVariables(uv);
		}
	}
}

#define EL_VARIABLES "Variables"
#define EL_VARIABLE "Variable"
#define ATT_VALUE "value"

void XmlRenderer::render(XmlBuffer* b, UserVariables* container)
{
	b->addOpenStartTag(EL_VARIABLES);
    b->incIndent();
    
    for (UserVariable* v = container->getVariables() ; v != nullptr ; v = v->getNext()) {

        b->addOpenStartTag(EL_VARIABLE);
        b->addAttribute(ATT_NAME, v->getName());

        // note that we'll lose the type during serialization
        // gak this is ugly
        ExValue exv;
        v->getValue(&exv);
        const char* value = exv.getString();
        if (value != nullptr)
          b->addAttribute(ATT_VALUE, value);

        b->add("/>\n");
    }
    
    b->decIndent();
    b->addEndTag(EL_VARIABLES);
}

void XmlRenderer::parse(XmlElement* e, UserVariables* container)
{
    UserVariable* list = nullptr;
	UserVariable* last = nullptr;

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		UserVariable* v = new UserVariable();
        v->setName(child->getAttribute(ATT_NAME));

        // we don't save the type, so a round trip will always stringify
        ExValue exv;
        exv.setString(e->getAttribute(ATT_VALUE));
        v->setValue(&exv);
        
		if (last == nullptr)
		  list = v;
		else
		  last->setNext(v);
		last = v;
	}

    container->setVariables(list);
}

//////////////////////////////////////////////////////////////////////
//
// BindingSet
//
//////////////////////////////////////////////////////////////////////

#define EL_BINDING "Binding"

#define ATT_DISPLAY_NAME "displayName"
#define ATT_TRIGGER "trigger"
#define ATT_TRIGGER_RELEASE "release"
#define ATT_VALUE "value"
#define ATT_CHANNEL "channel"
#define ATT_TRIGGER_VALUE "triggerValue"
#define ATT_TRIGGER_PATH "triggerPath"
#define ATT_TRIGGER_TYPE "triggerType"
#define ATT_TARGET_PATH "targetPath"
#define ATT_TARGET "target"
#define ATT_ACTION "action"
#define ATT_OPERATION "op"
#define ATT_ARGS "args"
#define ATT_SCOPE "scope"
#define ATT_TRACK "track"
#define ATT_GROUP "group"

void XmlRenderer::render(XmlBuffer* b, BindingSet* c)
{
	b->addOpenStartTag(EL_BINDING_SET);

	renderStructure(b, c);
    b->addAttribute("overlay", c->isOverlay());
    
	b->add(">\n");
	b->incIndent();

	for (Binding* binding = c->getBindings() ; binding != nullptr ; binding = binding->getNext()) {
        // this was annoying during testing, should be checking validity above
        // so we can at least see what went wrong
        // if (binding->isValid()) {
        render(b, binding);
        // }
    }

	b->decIndent();
	b->addEndTag(EL_BINDING_SET);
}

/**
 * Note that Binding is shared by both BindingSet and OscConfig
 *
 * What is now "symbol name" has historically been saved as just "name"
 * which is usually obvious.  Continue with that.
 */
void XmlRenderer::render(XmlBuffer* b, Binding* binding)
{
    b->addOpenStartTag(EL_BINDING);

    b->addAttribute(ATT_NAME, binding->getSymbolName());
    b->addAttribute(ATT_SCOPE, binding->getScope());

    if (binding->trigger != nullptr) {
        b->addAttribute(ATT_TRIGGER, binding->trigger->getName());
    }

    if (binding->triggerMode != nullptr) {
        b->addAttribute(ATT_TRIGGER_TYPE, binding->triggerMode->getName());
    }

    if (binding->release)
      b->addAttribute(ATT_TRIGGER_RELEASE, binding->release);

    if (binding->triggerValue > 0)
      b->addAttribute(ATT_VALUE, binding->triggerValue);

    if (binding->trigger != nullptr && Trigger::isMidi(binding->trigger) &&
        binding->midiChannel > 0) {
        b->addAttribute(ATT_CHANNEL, binding->midiChannel);
    }
            
    b->addAttribute(ATT_ARGS, binding->getArguments());

    b->add("/>\n");
}

void XmlRenderer::parse(XmlElement* e, BindingSet* c)
{
	parseStructure(e, c);
    c->setOverlay(e->getBoolAttribute("overlay"));

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_BINDING)) {
			Binding* mb = new Binding();

            parse(child, mb);
            
			// can't filter bogus functions yet, scripts aren't loaded
			c->addBinding(mb);
		}
	}
}

void XmlRenderer::parse(XmlElement* e, Binding* b)
{
    // trigger
    b->trigger = Trigger::find(e->getAttribute(ATT_TRIGGER));
    b->release = e->getBoolAttribute(ATT_TRIGGER_RELEASE);
    b->triggerMode = TriggerMode::find(e->getAttribute(ATT_TRIGGER_TYPE));
    b->triggerValue = e->getIntAttribute(ATT_VALUE);
    b->midiChannel = e->getIntAttribute(ATT_CHANNEL);

    // target
    b->setSymbolName(e->getAttribute(ATT_NAME));
    b->setArguments(e->getAttribute(ATT_ARGS));
    
    // scope
    b->setScope(e->getAttribute(ATT_SCOPE));
}

//////////////////////////////////////////////////////////////////////
//
// ScriptConfig
//
//////////////////////////////////////////////////////////////////////

#define EL_SCRIPT_CONFIG "ScriptConfig"
#define EL_SCRIPT_REF "ScripRef"
#define ATT_FILE "file"

void XmlRenderer::render(XmlBuffer* b, ScriptConfig* c)
{
    if (c->getScripts() != nullptr) {
        // should not be seeing these any more
        Trace(1, "XmlRenderer: Serializing a ScriptConfig for some reason");
    
        b->addStartTag(EL_SCRIPT_CONFIG);
        b->incIndent();

        for (ScriptRef* ref = c->getScripts() ; ref != nullptr ; ref = ref->getNext()) {
            b->addOpenStartTag(EL_SCRIPT_REF);
            b->addAttribute(ATT_FILE, ref->getFile());
            b->addAttribute("test", ref->isTest());
            b->add("/>\n");
        }

        b->decIndent();
        b->addEndTag(EL_SCRIPT_CONFIG);
    }
}

void XmlRenderer::parse(XmlElement* e, ScriptConfig* c)
{
    ScriptRef* list = nullptr;
    ScriptRef* last = nullptr;

    for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
         child = child->getNextElement()) {
        ScriptRef* ref = new ScriptRef();
        ref->setFile(child->getAttribute(ATT_FILE));
        ref->setTest(child->getBoolAttribute("test"));
        if (last == nullptr)
          list = ref;   
        else
          last->setNext(ref);
        last = ref;
    }

    c->setScripts(list);
}

//////////////////////////////////////////////////////////////////////
//
// SampleConfig
//
//////////////////////////////////////////////////////////////////////

// old name, now render EL_SAMPLE_CONFIG
#define EL_SAMPLE "Sample"
#define ATT_PATH "path"
#define ATT_SUSTAIN "sustain"
#define ATT_LOOP "loop"
#define ATT_CONCURRENT "concurrent"
#define ATT_SAMPLE_BUTTON "button"

void XmlRenderer::render(XmlBuffer* b, SampleConfig* c)
{
    // I changed the class name to SampleConfig but for backward
    // compatibility the element and class name were originally Samples
	b->addStartTag(EL_SAMPLE_CONFIG);
	b->incIndent();

    for (Sample* s = c->getSamples() ; s != nullptr ; s = s->getNext()) {

        b->addOpenStartTag(EL_SAMPLE);
        b->addAttribute(ATT_PATH, s->getFilename());
        b->addAttribute(ATT_SUSTAIN, s->isSustain());
        b->addAttribute(ATT_LOOP, s->isLoop());
        b->addAttribute(ATT_CONCURRENT, s->isConcurrent());
        b->addAttribute(ATT_SAMPLE_BUTTON, s->isButton());
        // note that the data block is NOT serialized or parsed
        b->add("/>\n");
    }

	b->decIndent();
	b->addEndTag(EL_SAMPLE_CONFIG);
}

void XmlRenderer::parse(XmlElement* e, SampleConfig* c)
{
    Sample* samples = nullptr;
	Sample* last = nullptr;

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		Sample* s = new Sample();

        s->setFilename(child->getAttribute(ATT_PATH));
        s->setSustain(child->getBoolAttribute(ATT_SUSTAIN));
        s->setLoop(child->getBoolAttribute(ATT_LOOP));
        s->setConcurrent(child->getBoolAttribute(ATT_CONCURRENT));
        s->setButton(child->getBoolAttribute(ATT_SAMPLE_BUTTON));
        
        if (last == nullptr)
		  samples = s;
		else
		  last->setNext(s);
		last = s;
	}

    c->setSamples(samples);
}

//////////////////////////////////////////////////////////////////////
//
// GroupDefinition
//
//////////////////////////////////////////////////////////////////////

// old name, now render EL_SAMPLE_CONFIG
#define ATT_REPLICATED_FUNCTIONS "replicatedFunctions"
#define ATT_REPLICATED_PARAMETERS "replicatedParameters"
#define ATT_COLOR "color"
#define ATT_REPLICATION "replication"

void XmlRenderer::render(XmlBuffer* b, GroupDefinition* g)
{
    // I changed the class name to SampleConfig but for backward
    // compatibility the element and class name were originally Samples
	b->addOpenStartTag(EL_GROUP_DEFINITION);

    const char* name = g->name.toUTF8();
    b->addAttribute(ATT_NAME, name);
    b->addAttribute(ATT_COLOR, g->color);
    b->addAttribute(ATT_REPLICATION, g->replicationEnabled);
    if (g->replicatedFunctions.size() > 0) {
        juce::String csv = g->replicatedFunctions.joinIntoString(",");
        b->addAttribute(ATT_REPLICATED_FUNCTIONS, (const char*)(csv.toUTF8()));
    }
    if (g->replicatedParameters.size() > 0) {
        juce::String csv = g->replicatedParameters.joinIntoString(",");
        b->addAttribute(ATT_REPLICATED_PARAMETERS, (const char*)(csv.toUTF8()));
    }
    b->add("/>\n");
}

void XmlRenderer::parse(XmlElement* e, GroupDefinition* g)
{
    g->name = juce::String(e->getAttribute(ATT_NAME));
    g->color = e->getIntAttribute(ATT_COLOR);
    g->replicationEnabled = e->getBoolAttribute(ATT_REPLICATION);
    juce::String csv = juce::String(e->getAttribute(ATT_REPLICATED_FUNCTIONS));
    if (csv.length() > 0)
      g->replicatedFunctions = juce::StringArray::fromTokens(csv, ",", "");
    csv = juce::String(e->getAttribute(ATT_REPLICATED_PARAMETERS));
    if (csv.length() > 0)
      g->replicatedParameters = juce::StringArray::fromTokens(csv, ",", "");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
