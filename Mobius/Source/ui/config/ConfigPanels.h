/**
 * Definitions for the container panels around various ConfigEditors.
 *
 * Since there are a lot of these and they're small, gather them in a single
 * file for easier maintenance.  All the interesting behavior is inside
 * the ConfigEditor subclasses, and the interaction between that and
 * the ConfigEditorContext, here implemented by ConfigPanel.
 *
 * todo: Given the regularity here, feels like templates would be in order?
 */

#pragma once

#include "ConfigPanel.h"

#include "ScriptConfigEditor.h"
#include "SampleEditor.h"
#include "KeyboardEditor.h"
#include "MidiEditor.h"
#include "HostEditor.h"
#include "ButtonEditor.h"
#include "MidiDeviceEditor.h"
#include "AudioEditor.h"
#include "DisplayEditor.h"
#include "PropertiesEditor.h"
#include "GroupEditor.h"
#include "SystemEditor.h"
#include "../session/SessionEditor.h"
#include "../parameter/OverlayEditor.h"

class ScriptPanel : public ConfigPanel
{
  public:
    ScriptPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("ScriptPanel");
        setEditor(&editor);
    }
    ~ScriptPanel() {}
 
  private:
    ScriptConfigEditor editor;
};

class SamplePanel : public ConfigPanel
{
  public:
    SamplePanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("SamplePanel");
        setEditor(&editor);
    }
    ~SamplePanel() {}
 
  private:
    SampleEditor editor;
};

class KeyboardPanel : public ConfigPanel
{
  public:
    KeyboardPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("KeyboardPanel");
        setEditor(&editor);
    }
    ~KeyboardPanel() {}
 
  private:
    KeyboardEditor editor;
};

class MidiPanel : public ConfigPanel
{
  public:
    MidiPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("MidiPanel");
        setEditor(&editor);
    }
    ~MidiPanel() {}
 
  private:
    MidiEditor editor;
};

class HostPanel : public ConfigPanel
{
  public:
    HostPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("HostPanel");
        setEditor(&editor);
    }
    ~HostPanel() {}
 
  private:
    HostEditor editor;
};

class ButtonPanel : public ConfigPanel
{
  public:
    ButtonPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("ButtonPanel");
        setEditor(&editor);
    }
    ~ButtonPanel() {}
 
  private:
    ButtonEditor editor;
};

class MidiDevicePanel : public ConfigPanel
{
  public:
    MidiDevicePanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("MidiDevicePanel");
        setEditor(&editor);
    }
    ~MidiDevicePanel() {}
 
  private:
    MidiDeviceEditor editor;
};

class AudioPanel : public ConfigPanel
{
  public:
    AudioPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("AudioPanel");
        setEditor(&editor);
    }
    ~AudioPanel() {}
 
  private:
    AudioEditor editor;
};

class DisplayPanel : public ConfigPanel
{
  public:
    DisplayPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("DisplayPanel");
        setEditor(&editor);
    }
    ~DisplayPanel() {}
 
  private:
    DisplayEditor editor;
};

class PropertiesPanel : public ConfigPanel
{
  public:
    PropertiesPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("PropertiesPanel");
        setEditor(&editor);
    }
    ~PropertiesPanel() {}
 
  private:
    PropertiesEditor editor;
};

class GroupPanel : public ConfigPanel
{
  public:
    GroupPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("GroupPanel");
        setEditor(&editor);
    }
    ~GroupPanel() {}
 
  private:
    GroupEditor editor;
};

class SessionPanel : public ConfigPanel
{
  public:
    SessionPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("Session");
        setEditor(&editor);
    }
    ~SessionPanel() {}
 
  private:
    SessionEditor editor;
};

class OverlayPanel : public ConfigPanel
{
  public:
    OverlayPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("Parameter Overlays");
        setEditor(&editor);
    }
    ~OverlayPanel() {}
 
  private:
    OverlayEditor editor;
};

class SystemPanel : public ConfigPanel
{
  public:
    SystemPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("Parameter Systems");
        setEditor(&editor);
    }
    ~SystemPanel() {}
 
  private:
    SystemEditor editor;
};
