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

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../util/List.h"
#include "../../util/XmlBuffer.h"
#include "../../util/XomParser.h"
#include "../../util/XmlModel.h"
#include "../../util/FileUtil.h"

#include "MobiusConfig.h"
#include "Preset.h"
#include "Setup.h"
#include "UserVariable.h"
#include "Binding.h"

#include "../ScriptConfig.h"
#include "../SampleConfig.h"
#include "../GroupDefinition.h"
#include "../Symbol.h"
#include "../ParameterProperties.h"

#include "XmlRenderer.h"

//////////////////////////////////////////////////////////////////////
//
// Public
//
//////////////////////////////////////////////////////////////////////

#define EL_MOBIUS_CONFIG "MobiusConfig"
#define EL_PRESET "Preset"

XmlRenderer::XmlRenderer(SymbolTable* st)
{
    symbols = st;
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

#if 0
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
#endif

void XmlRenderer::render(XmlBuffer* b, const char* name, int value)
{
    if (value > 0)
      b->addAttribute(name, value);
}

void XmlRenderer::render(XmlBuffer* b, SymbolId sid, int value)
{
    Symbol* s = symbols->getSymbol(sid);
    if (s == nullptr) {
        Trace(1, "XmlRenderer: Invalid symbol id %d", sid);
    }
    else {
        ParameterProperties* props = s->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "XmlRenderer: Symbol not a parameter %s", s->getName());
        }
        else if (props->type == UIParameterType::TypeEnum) {
            if (props->values.size() == 0) {
                Trace(1, "XmlRenderer: Attempt to render enum parameter without value list %s\n",
                      s->getName());
            }
            else {
                // should do some range checking here but we're only ever getting a value
                // from an object member cast as an int
                // shoudl be letting the
                // !! put this in UIParameter::getEnumValue
                b->addAttribute(s->getName(), props->getEnumName(value));
            }
        }
        else {
            // option to filter zero?
            // yes, lots of things are zero/false
            if (value > 0)
              b->addAttribute(s->getName(), value);
        }
    }
}

const char* XmlRenderer::getSymbolName(SymbolId sid)
{
    const char* name = nullptr;
    Symbol* s = symbols->getSymbol(sid);
    if (s == nullptr) {
        Trace(1, "XmlRenderer: Invalid symbol id %d", sid);
    }
    else {
        name = s->getName();
    }
    return name;
}

#if 0
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
#endif

void XmlRenderer::render(XmlBuffer* b, SymbolId sid, bool value)
{
    const char* name = getSymbolName(sid);
    if (name != nullptr && value)
      b->addAttribute(name, "true");
    //else
    //b->addAttribute(p->getName(), "false");
}

#if 0
void XmlRenderer::render(XmlBuffer* b, UIParameter* p, const char* value)
{
    // any filtering options?
    if (value != nullptr)
      b->addAttribute(p->getName(), value);
}
#endif

void XmlRenderer::render(XmlBuffer* b, SymbolId sid, const char* value)
{
    const char* name = getSymbolName(sid);
    if (name != nullptr && value != nullptr)
      b->addAttribute(name, value);
}

void XmlRenderer::render(XmlBuffer* b, const char* name, const char* value)
{
    if (value != nullptr)
      b->addAttribute(name, value);
}

/**
 * Most parameters are boolean, integer, or enumerations.
 * Parse and return an int which can then be cast by the caller.
 */
#if 0
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
#endif

int XmlRenderer::parse(XmlElement* e, SymbolId sid)
{
    int value = 0;

    Symbol* s = symbols->getSymbol(sid);
    if (s == nullptr) {
        Trace(1, "XmlRenderer: Invalid symbol id %d", sid);
    }
    else {
        ParameterProperties* props = s->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "XmlRenderer: Symbol not a parameter %s", s->getName());
        }
        else {
            const char* str = e->getAttribute(s->getName());
            if (str != nullptr) {
                if (props->type == UIParameterType::TypeBool) {
                    value = !strcmp(str, "true");
                }
                else if (props->type == UIParameterType::TypeInt) {
                    value = atoi(str);
                }
                else if (props->type == UIParameterType::TypeEnum) {
                    value = props->getEnumOrdinal(str);
                    if (value < 0) {
                        // invalid enum name, leave zero
                        Trace(1, "XmlRenderer: Invalid enumeration value %s for %s\n", str, s->getName());
                    }
                }
                else {
                    // error: should not have called this method
                    Trace(1, "XmlRenderer: Can't parse parameter %s as int\n", s->getName());
                }
            }
            else {
                // there was no attribute
                // note that by returning zero here it will initialize the bool/int/enum
                // to that value rather than selecting a default value or just leaving it alone
                // okay for now since the element is expected to have all attributes
            }
        }
    }
    return value;
}

int XmlRenderer::parse(XmlElement* e, const char* name)
{
    const char* value = e->getAttribute(name);
    return (value == nullptr) ? 0 : atoi(value);
}

/**
 * Parse a string attribute.
 * Can return the constant element attribute value, caller is expected
 * to copy it.
 */
#if 0
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
#endif

const char* XmlRenderer::parseString(XmlElement* e, SymbolId sid)
{
    const char* value = nullptr;

    Symbol* s = symbols->getSymbol(sid);
    if (s == nullptr) {
        Trace(1, "XmlRenderer: Invalid symbol id %d", sid);
    }
    else {
        ParameterProperties* props = s->parameterProperties.get();
        if (props == nullptr) {
            Trace(1, "XmlRenderer: Symbol not a parameter %s", s->getName());
        }
        else if (props->type == UIParameterType::TypeString ||
                 props->type == UIParameterType::TypeStructure) {
            value = e->getAttribute(s->getName());
        }
        else {
            Trace(1, "XmlRenderer: Can't parse parameter %s value as a string\n", s->getName());
        }
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

    // this isn't a parqameter any more, ignore and make them set it again
    //render(b, ParamQuickSave, c->getQuickSave());
    render(b, ParamNoiseFloor, c->getNoiseFloor());

    render(b, ParamInputLatency, c->getInputLatency());
    render(b, ParamOutputLatency, c->getOutputLatency());
    // don't bother saving this until it can have a more useful range
	//render(UIParameterFadeFrames, c->getFadeFrames());
    render(b, ParamMaxSyncDrift, c->getMaxSyncDrift());
    render(b, "trackCount", c->getCoreTracksDontUseThis());
    
    // UIParameter is gone, and this shouldn't be used any more, but the
    // upgrader still needs to parse it?
    //render(b, UIParameterGroupCount, c->getTrackGroupsDeprecated());
    if (c->getTrackGroupsDeprecated() > 0)
      b->addAttribute("groupCount", c->getTrackGroupsDeprecated());
    
    render(b, "maxLoops", c->getMaxLoops());
    render(b, ParamLongPress, c->getLongPress());
    render(b, ParamMonitorAudio, c->isMonitorAudio());
	b->addAttribute(ATT_PLUGIN_HOST_REWINDS, c->isHostRewinds());
    render(b, ParamAutoFeedbackReduction, c->isAutoFeedbackReduction());
    // don't allow this to be persisted any more, can only be set in scripts
	//render(IsolateOverdubsParameter->getName(), mIsolateOverdubs);
    render(b, ParamSpreadRange, c->getSpreadRange());
    //render(b, UIParameterTraceLevel, c->getTraceDebugLevel());
    render(b, ParamSaveLayers, c->isSaveLayers());
    //render(b, ParamDriftCheckPoint, c->getDriftCheckPoint());

    //render(b, ParamGroupFocusLock, c->isGroupFocusLock());

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
    
	//c->setQuickSave(parseString(e, ParamQuickSave));

	c->setNoiseFloor(parse(e, ParamNoiseFloor));
	c->setInputLatency(parse(e, ParamInputLatency));
	c->setOutputLatency(parse(e, ParamOutputLatency));
	c->setMaxSyncDrift(parse(e, ParamMaxSyncDrift));
	c->setCoreTracks(parse(e, "trackCount"));
    
	//c->setTrackGroupsDeprecated(parse(e, UIParameterGroupCount));
	c->setTrackGroupsDeprecated(e->getIntAttribute("groupCount"));
    
	c->setMaxLoops(parse(e, "maxLoops"));
	c->setLongPress(parse(e, ParamLongPress));

	c->setMonitorAudio(parse(e, ParamMonitorAudio));
	c->setHostRewinds(e->getBoolAttribute(ATT_PLUGIN_HOST_REWINDS));
	c->setAutoFeedbackReduction(parse(e, ParamAutoFeedbackReduction));

    // don't allow this to be persisted any more, can only be set in scripts
	//setIsolateOverdubs(e->getBoolAttribute(IsolateOverdubsParameter->getName()));
	c->setSpreadRange(parse(e, ParamSpreadRange));
	//c->setTracePrintLevel(parse(e, UIParameterTracePrintLevel));
	//c->setTraceDebugLevel(parse(e, UIParameterTraceLevel));
	c->setSaveLayers(parse(e, ParamSaveLayers));
	//c->setDriftCheckPoint((DriftCheckPoint)parse(e, ParamDriftCheckPoint));

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

    render(b, ParamAltFeedbackEnable, p->isAltFeedbackEnable());
    render(b, ParamBounceQuantize, p->getBounceQuantize());
    render(b, ParamEmptyLoopAction, p->getEmptyLoopAction());
    render(b, ParamEmptyTrackAction, p->getEmptyTrackAction());
    render(b, ParamLoopCount, p->getLoops());
    render(b, ParamMaxRedo, p->getMaxRedo());
    render(b, ParamMaxUndo, p->getMaxUndo());
    render(b, ParamMultiplyMode, p->getMultiplyMode());
    render(b, ParamMuteCancel, p->getMuteCancel());
    render(b, ParamMuteMode, p->getMuteMode());
    render(b, ParamNoFeedbackUndo, p->isNoFeedbackUndo());
    render(b, ParamNoLayerFlattening, p->isNoLayerFlattening());
    render(b, ParamOverdubQuantized, p->isOverdubQuantized());
    render(b, ParamOverdubTransfer, p->getOverdubTransfer());
    render(b, ParamPitchBendRange, p->getPitchBendRange());
    //render(b, UIParameterPitchSequence, p->getPitchSequence());
    render(b, ParamPitchShiftRestart, p->isPitchShiftRestart());
    render(b, ParamPitchStepRange, p->getPitchStepRange());
    render(b, ParamPitchTransfer, p->getPitchTransfer());
    render(b, ParamQuantize, p->getQuantize());
    render(b, ParamSpeedBendRange, p->getSpeedBendRange());
    render(b, ParamSpeedRecord, p->isSpeedRecord());
    //render(b, UIParameterSpeedSequence, p->getSpeedSequence());
    render(b, ParamSpeedShiftRestart, p->isSpeedShiftRestart());
    render(b, ParamSpeedStepRange, p->getSpeedStepRange());
    render(b, ParamSpeedTransfer, p->getSpeedTransfer());
    render(b, ParamTimeStretchRange, p->getTimeStretchRange());
    render(b, ParamRecordResetsFeedback, p->isRecordResetsFeedback());
    render(b, ParamRecordTransfer, p->getRecordTransfer());
    render(b, ParamReturnLocation, p->getReturnLocation());
    render(b, ParamReverseTransfer, p->getReverseTransfer());
    render(b, ParamRoundingOverdub, p->isRoundingOverdub());
    render(b, ParamShuffleMode, p->getShuffleMode());
    render(b, ParamSlipMode, p->getSlipMode());
    render(b, ParamSlipTime, p->getSlipTime());
    render(b, ParamSoundCopyMode, p->getSoundCopyMode());
    render(b, ParamSubcycles, p->getSubcycles());
    //render(b, UIParameterSustainFunctions, p->getSustainFunctions());
    render(b, ParamSwitchDuration, p->getSwitchDuration());
    render(b, ParamSwitchLocation, p->getSwitchLocation());
    render(b, ParamSwitchQuantize, p->getSwitchQuantize());
    render(b, ParamSwitchVelocity, p->isSwitchVelocity());
    render(b, ParamTimeCopyMode, p->getTimeCopyMode());
    render(b, ParamTrackLeaveAction, p->getTrackLeaveAction());
    render(b, ParamWindowEdgeAmount, p->getWindowEdgeAmount());
    render(b, ParamWindowEdgeUnit, p->getWindowEdgeUnit());
    render(b, ParamWindowSlideAmount, p->getWindowSlideAmount());
    render(b, ParamWindowSlideUnit, p->getWindowSlideUnit());

	b->add("/>\n");
	b->setAttributeNewline(false);
}

void XmlRenderer::parse(XmlElement* e, Preset* p)
{
	parseStructure(e, p);

    p->setAltFeedbackEnable(parse(e, ParamAltFeedbackEnable));
    p->setBounceQuantize(parse(e, ParamBounceQuantize));
    p->setEmptyLoopAction(parse(e, ParamEmptyLoopAction));
    p->setEmptyTrackAction(parse(e, ParamEmptyTrackAction));
    p->setLoops(parse(e, ParamLoopCount));
    p->setMaxRedo(parse(e, ParamMaxRedo));
    p->setMaxUndo(parse(e, ParamMaxUndo));
    p->setMultiplyMode(parse(e, ParamMultiplyMode));
    p->setMuteCancel(parse(e, ParamMuteCancel));
    p->setMuteMode(parse(e, ParamMuteMode));
    p->setNoFeedbackUndo(parse(e, ParamNoFeedbackUndo));
    p->setNoLayerFlattening(parse(e, ParamNoLayerFlattening));
    p->setOverdubQuantized(parse(e, ParamOverdubQuantized));
    p->setOverdubTransfer(parse(e, ParamOverdubTransfer));
    p->setPitchBendRange(parse(e, ParamPitchBendRange));
    //p->setPitchSequence(parseString(e, UIParameterPitchSequence));
    p->setPitchShiftRestart(parse(e, ParamPitchShiftRestart));
    p->setPitchStepRange(parse(e, ParamPitchStepRange));
    p->setPitchTransfer(parse(e, ParamPitchTransfer));
    p->setQuantize(parse(e, ParamQuantize));
    p->setSpeedBendRange(parse(e, ParamSpeedBendRange));
    p->setSpeedRecord(parse(e, ParamSpeedRecord));
    //p->setSpeedSequence(parseString(e, UIParameterSpeedSequence));
    p->setSpeedShiftRestart(parse(e, ParamSpeedShiftRestart));
    p->setSpeedStepRange(parse(e, ParamSpeedStepRange));
    p->setSpeedTransfer(parse(e, ParamSpeedTransfer));
    p->setTimeStretchRange(parse(e, ParamTimeStretchRange));
    p->setRecordResetsFeedback(parse(e, ParamRecordResetsFeedback));
    p->setRecordTransfer(parse(e, ParamRecordTransfer));
    p->setReturnLocation(parse(e, ParamReturnLocation));
    p->setReverseTransfer(parse(e, ParamReverseTransfer));
    p->setRoundingOverdub(parse(e, ParamRoundingOverdub));
    p->setShuffleMode(parse(e, ParamShuffleMode));
    p->setSlipMode(parse(e, ParamSlipMode));
    p->setSlipTime(parse(e, ParamSlipTime));
    p->setSoundCopyMode(parse(e, ParamSoundCopyMode));
    p->setSubcycles(parse(e, ParamSubcycles));
    //p->setSustainFunctions(parseString(e, UIParameterSustainFunctions));
    p->setSwitchDuration(parse(e, ParamSwitchDuration));
    p->setSwitchLocation(parse(e, ParamSwitchLocation));
    p->setSwitchQuantize(parse(e, ParamSwitchQuantize));
    p->setSwitchVelocity(parse(e, ParamSwitchVelocity));
    p->setTimeCopyMode(parse(e, ParamTimeCopyMode));
    p->setTrackLeaveAction(parse(e, ParamTrackLeaveAction));
    p->setWindowEdgeAmount(parse(e, ParamWindowEdgeAmount));

    // ugh, I seem to have made redundant setters for all these that take an int
    // rather than an enum, but not these, why?  Kind of like not having the duplication
    p->setWindowEdgeUnit((WindowUnit)parse(e, ParamWindowEdgeUnit));
    p->setWindowSlideAmount(parse(e, ParamWindowSlideAmount));
    p->setWindowSlideUnit((WindowUnit)parse(e, ParamWindowSlideUnit));
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

//const char* UIParameterDefaultSyncSourceClassValues[] = {"default","none","track","out","host","midi","transport",nullptr};

const char* XmlRenderer::render(OldSyncSource src)
{
    const char* value = "???";
    switch (src) {
        case SYNC_DEFAULT: value = "default"; break;
        case SYNC_NONE: value="none"; break;
        case SYNC_TRACK: value="track"; break;
        case SYNC_OUT: value="out"; break;
        case SYNC_HOST: value="host"; break;
        case SYNC_MIDI: value="midi"; break;
        case SYNC_TRANSPORT: value="transport"; break;
    }
    return value;
}

OldSyncSource XmlRenderer::parseOldSyncSource(const char* value)
{
    OldSyncSource src = SYNC_NONE;
    if (StringEqual(value, "default"))
      src = SYNC_DEFAULT;
    else if (StringEqual(value, "none"))
      src = SYNC_NONE;
    else if (StringEqual(value, "track"))
      src = SYNC_TRACK;
    else if (StringEqual(value, "out"))
      src = SYNC_OUT;
    else if (StringEqual(value, "host"))
      src = SYNC_HOST;
    else if (StringEqual(value, "midi"))
      src = SYNC_MIDI;
    else if (StringEqual(value, "transport"))
      src = SYNC_TRANSPORT;
    return src;
}

const char* XmlRenderer::render(OldSyncUnit unit)
{
    const char* value = "???";
    switch (unit) {
        case SYNC_UNIT_BEAT: value = "beat"; break;
        case SYNC_UNIT_BAR: value="bar"; break;
    }
    return value;
}

const char* XmlRenderer::render(SyncTrackUnit unit)
{
    const char* value = "???";
    switch (unit) {
        case TRACK_UNIT_DEFAULT: value = "default"; break;
        case TRACK_UNIT_SUBCYCLE: value="subcycle"; break;
        case TRACK_UNIT_CYCLE: value="cycle"; break;
        case TRACK_UNIT_LOOP: value="loop"; break;
    }
    return value;
}

SyncTrackUnit XmlRenderer::parseSyncTrackUnit(const char* value)
{
    SyncTrackUnit unit = TRACK_UNIT_DEFAULT;
    if (StringEqual(value, "default"))
      unit = TRACK_UNIT_DEFAULT;
    else if (StringEqual(value, "subcycle"))
      unit = TRACK_UNIT_SUBCYCLE;
    else if (StringEqual(value, "cycle"))
      unit = TRACK_UNIT_CYCLE;
    else if (StringEqual(value, "loop"))
      unit = TRACK_UNIT_LOOP;
    return unit;
}

void XmlRenderer::render(XmlBuffer* b, Setup* setup)
{
	b->addOpenStartTag(EL_SETUP);
	b->setAttributeNewline(true);

	renderStructure(b, setup);

    // these haven't been defined as Parameters, now that we're
    // doing that for the sync options could do these...
    b->addAttribute(ATT_ACTIVE, setup->getActiveTrack());
    //b->addAttribute(ATT_BINDINGS, setup->getBindings());

    render(b, "defaultPreset", setup->getDefaultPresetName());
    
    // these are a csv while the function lists in MobiusConfig
    // are String lists, should be consistent, I'm liking csv for brevity
    //b->addAttribute(ATT_RESET_RETAINS, setup->getResetRetains());

    //render(b, UIParameterBeatsPerBar, setup->getBeatsPerBar());
    // why is the name pattern not followed here?
    render(b, "defaultSyncSource", render(setup->getSyncSource()));
    render(b, "defaultTrackSyncUnit", render(setup->getSyncTrackUnit()));
    //render(b, UIParameterManualStart, setup->isManualStart());
    //render(b, UIParameterMaxTempo, setup->getMaxTempo());
    //render(b, UIParameterMinTempo, setup->getMinTempo());
    //render(b, UIParameterMuteSyncMode, setup->getMuteSyncMode());
    render(b, "realignTime", setup->getRealignTime());
    render(b, "resizeSyncAdjust", setup->getResizeSyncAdjust());
    render(b, "slaveSyncUnit", setup->getSyncUnit());
    render(b, "speedSyncAdjust", setup->getSpeedSyncAdjust());

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
	//setup->setBindings(e->getAttribute(ATT_BINDINGS));

    setup->setDefaultPresetName(e->getAttribute("defaultPreset"));
    
    //setup->setResetRetains(e->getAttribute(ATT_RESET_RETAINS));

    //setup->setBeatsPerBar(parse(e, UIParameterBeatsPerBar));
    setup->setSyncSource(parseOldSyncSource(e->getAttribute("defaultSyncSource")));
    setup->setSyncTrackUnit(parseSyncTrackUnit(e->getAttribute("defaultTrackSyncUnit")));
    //setup->setManualStart(parse(e, UIParameterManualStart));
    //setup->setMaxTempo(parse(e, UIParameterMaxTempo));
    //setup->setMinTempo(parse(e, UIParameterMinTempo));
    //setup->setMuteSyncMode(parse(e, UIParameterMuteSyncMode));
    //setup->setRealignTime(parse(e, "realignTime"));
    //setup->setResizeSyncAdjust(parse(e, "resizeSyncAdjust"));
    //setup->setSyncUnit((OldSyncUnit)parse(e, "slaveSyncUnit"));
    //setup->setSpeedSyncAdjust(parse(e, "speedSyncAdjust"));

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

    render(b, "trackPreset", t->getTrackPresetName());
    render(b, ParamFocus, t->isFocusLock());
    render(b, ParamMono, t->isMono());

    // groups are now referenced by name
    render(b, "group", t->getGroupNumberDeprecated());
    if (t->getGroupName().length() > 0) {
        juce::String gname = t->getGroupName();
        b->addAttribute(ATT_GROUP_NAME, (const char*)(gname.toUTF8()));
    }
    render(b, ParamInput, t->getInputLevel());
    render(b, ParamOutput, t->getOutputLevel());
    render(b, ParamFeedback, t->getFeedback());
    render(b, ParamAltFeedback, t->getAltFeedback());
    render(b, ParamPan, t->getPan());

    render(b, ParamAudioInputPort, t->getAudioInputPort());
    render(b, ParamAudioOutputPort, t->getAudioOutputPort());
    render(b, ParamPluginInputPort, t->getPluginInputPort());
    render(b, ParamPluginOutputPort, t->getPluginOutputPort());

    render(b, "syncSource", render(t->getSyncSource()));
    render(b, "trackSyncUnit", render(t->getSyncTrackUnit()));

    b->add("/>\n");
#if 0
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
#endif    
}

void XmlRenderer::parse(XmlElement* e, SetupTrack* t)
{
	t->setName(e->getAttribute(ATT_NAME));

    // if we're reading an old mobius.xml for upgrade, the track name
    // attribute changed
    const char* oldName = e->getAttribute("trackName");
    if (oldName != nullptr)
      t->setName(oldName);
    
    t->setTrackPresetName(e->getAttribute("trackPreset"));
    t->setFocusLock(parse(e, ParamFocus));
    t->setMono(parse(e, ParamMono));

    // should stop having group numbers eventually
    t->setGroupNumberDeprecated(e->getIntAttribute("group"));
    const char* groupName = e->getAttribute(ATT_GROUP_NAME);
    if (groupName != nullptr)
      t->setGroupName(juce::String(groupName));
    
    t->setInputLevel(parse(e, ParamInput));
    t->setOutputLevel(parse(e, ParamOutput));
    t->setFeedback(parse(e, ParamFeedback));
    t->setAltFeedback(parse(e, ParamAltFeedback));
    t->setPan(parse(e, ParamPan));

    t->setAudioInputPort(parse(e, ParamAudioInputPort));
    t->setAudioOutputPort(parse(e, ParamAudioOutputPort));
    t->setPluginInputPort(parse(e, ParamPluginInputPort));
    t->setPluginOutputPort(parse(e, ParamPluginOutputPort));

    t->setSyncSource((OldSyncSource)parseOldSyncSource(e->getAttribute("syncSource")));
    t->setSyncTrackUnit((SyncTrackUnit)parseSyncTrackUnit(e->getAttribute("trackSyncUnit")));

    // should only have a single UserVariables 
	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {
#if 0
		if (child->isName(EL_VARIABLES)) {
            UserVariables* uv = new UserVariables();
            parse(child, uv);
            t->setVariables(uv);
		}
#endif        
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
