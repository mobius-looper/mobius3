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
