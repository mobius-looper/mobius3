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

#include "NewConfigPanel.h"

#include "GlobalEditor.h"
#include "PresetEditor.h"
#include "SetupEditor.h"
#include "ScriptEditor.h"
#include "SampleEditor.h"
#include "KeyboardEditor.h"
#include "MidiEditor.h"
#include "HostEditor.h"
#include "ButtonEditor.h"
#include "MidiDeviceEditor.h"
#include "AudioEditor.h"

class ScriptPanel : public NewConfigPanel
{
  public:
    ScriptPanel() {
        setName("ScriptPanel");
        setEditor(&editor);
    }
    ~ScriptPanel() {}
 
  private:
    ScriptEditor editor;
};

class SamplePanel : public NewConfigPanel
{
  public:
    SamplePanel() {
        setName("SamplePanel");
        setEditor(&editor);
    }
    ~SamplePanel() {}
 
  private:
    SampleEditor editor;
};

class PresetPanel : public NewConfigPanel
{
  public:
    PresetPanel() {
        setName("PresetPanel");
        setEditor(&editor);
    }
    ~PresetPanel() {}
 
  private:
    PresetEditor editor;
};

class SetupPanel : public NewConfigPanel
{
  public:
    SetupPanel() {
        setName("SetupPanel");
        setEditor(&editor);
    }
    ~SetupPanel() {}
 
  private:
    SetupEditor editor;
};

class GlobalPanel : public NewConfigPanel
{
  public:
    GlobalPanel() {
        setName("GlobalPanel");
        setEditor(&editor);
    }
    ~GlobalPanel() {}
 
  private:
    GlobalEditor editor;
};

class KeyboardPanel : public NewConfigPanel
{
  public:
    KeyboardPanel() {
        setName("KeyboardPanel");
        setEditor(&editor);
    }
    ~KeyboardPanel() {}
 
  private:
    KeyboardEditor editor;
};

class MidiPanel : public NewConfigPanel
{
  public:
    MidiPanel() {
        setName("MidiPanel");
        setEditor(&editor);
    }
    ~MidiPanel() {}
 
  private:
    MidiEditor editor;
};

class HostPanel : public NewConfigPanel
{
  public:
    HostPanel() {
        setName("HostPanel");
        setEditor(&editor);
    }
    ~HostPanel() {}
 
  private:
    HostEditor editor;
};

class ButtonPanel : public NewConfigPanel
{
  public:
    ButtonPanel() {
        setName("ButtonPanel");
        setEditor(&editor);
    }
    ~ButtonPanel() {}
 
  private:
    ButtonEditor editor;
};

class MidiDevicePanel : public NewConfigPanel
{
  public:
    MidiDevicePanel() {
        setName("MidiDevicePanel");
        setEditor(&editor);
    }
    ~MidiDevicePanel() {}
 
  private:
    MidiDeviceEditor editor;
};

class AudioPanel : public NewConfigPanel
{
  public:
    AudioPanel() {
        setName("AudioPanel");
        setEditor(&editor);
    }
    ~AudioPanel() {}
 
  private:
    AudioEditor editor;
};
