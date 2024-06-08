+++
title = 'Documentation'
date = 2024-05-21T21:43:45-05:00
draft = false
+++

## Comming Soon

## Upgrading Old Installations

There is a utility that can help upgrade old Mobius 2.5 configuration files.  It should copy all Preset and Track Setups, and most MIDI, keyboard, and host parameter bindings.

UI configuration is not copied other than the command buttons at the top.

MIDI bindings need an adjustment to the channel numbers stored in the mobius.xml file, so it is important that you use the upgrade utility rather than copying and pasting XML from the old mobius.xml file into the new one.  If you are comfortable with XML editing, what you need to do is add 1 to any channel='x' numbers you see in the <Binding> elements.

Several functions have changed names, and some are no longer availble.  The upgrade utility will attempt to upgrade the names.  If you see warnings in the upgrade log that are not related to missing scripts, plase let me know.

There are three phases to the upgrade:

- **Loading** - mobius.xml and ui.xml file are loaded and analyzed but not installed
- **Installation** - Previously analyzed files are installed in the Mobius 3 configuration
- **Undo** - Previously installed Mobius 3 upgrades are removed

To run the upgrade utility, bring up the **Configuration** menu and select **Upgrade Configuration**.  A popup window appears with a large log area in the center and a row of command buttons at the top.  The buttons do the following things:

- **Load Current** - Attempts to locate the old configuration files from their standard locations and load them.  You will see error messages in the log if the files can't be found.
- **Load File** - Load a file that you select.  Use this option of Load Current was unable to find the files in the usual locations.
- **Install** - Installs the upgraded configuration after a sucessful analysis
- **Undo** - Undoes a previous upgrade install

The log area will display messages about what the utility found in your old files.  Normally Presets and Track Setups are added without any problems, but if you already have one of those with the same name as the ones in the old files, the prefix "Upgrade:" will be added to the upgraded objects.  The most common case of this is an object named "Default" since there is already a standard Preset/Setup with that name in a new installation.

If you have scripts, the utility will verify the file paths to the script files and attempt to load them.  Warning messages will be displayed in the log if any files cannot be found.  All the script file references will be imported, but you will have to correct the path names in the Script configuration window after installing.

The most common source of warnings is when importing MIDI and keyboard bindings.  A few function names have changed, and the upgrader will correct those, but I might have missed some and a few functions no longer exist.  It you see warnings about unresolved functions or parameters that are **not** related to scripts, I'd like to hear about them.

If you have bindings to scripts, and the script files could not be found, you will likely see warnings about unresolved bindings with names that would appear inside the script file next to the **!name** statement.  The bindings will still be imported, but you will need to correct the paths to the script files before they can be used.

Keyboard bindings may not work, depending on which keys were used.  The internal key numbers used to idenfity keys is different in Mobius 2.5 and Mobius 3.  Simple letter and number key bindings should work, as well as common keys like `escape` and `backspace`.  But the `Fn` function keys, keys in the number pad, keys in the `Ins`, `Home` area and the arrow keys may not work without redefining them.  Any key binding that used the Shift, Ctrl, Alt, or Commmand modifiers will have to be redefined.

A few of you made use of an old feature called **Binding Overlays** where you could have multiple sets of MIDI bindings that were merged with the first set of "Common Bindings".  The binding overlays will be imported by the ugprader, but you will not be able to activate them yet.  I'm working on that now.

Command buttons displayed at the top of the UI should all be imported, but they are collected under a new **Button Set** with the name "Upgrade".  Button sets are a new feature in Mobius 3 where you can define any number of different button collections and switch between them.  To see and use your old buttons, you will need to select the "Upgrade" set under the **Display** menu.





