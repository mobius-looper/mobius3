## Mobius 3

This repository contains the complete source code for building the Mobius 3
standalone application and the plugins.  To build, you must have a few
3rd party libraries and tools that are not included in this repository due
to licensing issues.

### Juce

Mobius requires the Juce libraries, and currently uses Projucer to drive the
build process in Visual Studio and Xcode.  It does not yet use CMake but
that is planned.

Juce is readily avilable online.  It doesn't matter where it goes, but when
you run Projucer for the first time you will need to configure a few things
in the "Global Paths" dialog.  This configuration is stored on your local build
machine and are not in the .jucer files in the repository.

### SDKs

Mobius may use two SDKs from Steinberg, one for ASIO and one for VST2.

ASIO is officially supported and required for Windows.  It is not needed for Mac.
VST2 is not officially supported and is optional.

The .jucer files in the repository have assumptions about the locations of these
libraries by using relative search paths for the header files.  It is recommended
that you follow the same directory structure to avoid having to modify the .jucer
files.  Since the .jucer files are under source control, they are shared by everyone
and modifying them for local search paths will make other developers angry.

There must be a directory named "mobius3-sdk" that is a peer to the "mobius3"
directory from github.  Within that msut be two directories:

    asiosdk_2.3.3_2019-06-14
    vst2.4

The first is the standard ASIO distribution from Steinberg.  The second contains
dark magic that you should not be toying with, but I can't stop you if you do.

**NOTE:** At the moment, the .jucer files do not have relative paths to those libraries.
They expect them both to be under "c:/dev/mobius3-sdk".  You will have to use that directory
or modify the Projucer configuration.  We should be using relative paths here but the path
is relative to the something deep inside the Build directory, it is NOT relative to the
Mobius or MobiusPlugin directories.

### ASIO

To build the standalone application on Windows you must have the ASIO SDK source
from Steinberg.  This still readily avaialble at https://www.steinberg.net/developers/.

If you place it in the recommended location you should not have to do anything.
If you need to install it somewhere else, go to Projucer and bring up the Mobius project.

In the Exporters section, expand Visual Studio 2022 and click Debug.  Scroll down
to Header Search Paths and enter the absolte or relative path to the "common" directory
of the standard SDK distribution.  Example: c:\dev\asiosdk_2.3.3_2019-06-14\common.

Since I am not building Release binaries without debugging symbols you don't need to
do this for the Release exporter, but it doesn't hurt anything.

While ASIO is technically available on Mac, Juce does not support it, and it is widely
viewed as unnecessary because CoreAudio offers similar performance.

### VST2

Abandon hope, all ye who enter here.

Steinberg no longer supports VST2, and it really, **REALLY**, does not want you to use it.

We are legally not allowed to distribute VST2 plugins, but if you want to build one for your
own use, Steinberg will probably not be pulling up to your home in an unmarked black van at 2am
and kicking down your door.  Probably.

You will have to locate a copy of the old VST 2.4 SDK header files from the "dark web".  They are
not hard to find, but I can't tell you where to look.  I have a friend who did it once, but
he felt bad afterward.

If you have no respect for the law, and manage to get your filthy hands on a copy, it could
hypothetically be placed in the mobius3-sdk directory using this structure:

    mobius3-sdk/vst2.4/pluginterfaces/vst2.x

The files required files are aeffect.h and aeffectx.h.

You must use the "pluginterfaces/vst2.x" directory names, and those two .h files must
be immediately inside it.  This is the path used by the Juce #includes so you can't
just point Projucer at the original distribution you somehow managed to find.

Once you have done this and hidden the evidence, go to Projucer and open
the MobiusPlugin project.  Click the top-level gear icon and scroll down
to "Header Search Paths".  There is normally one entry here, a relative
path for the main Mobius source code "../../../Mobius/Source".  Add another
line with the location of your ill-gotten gains.


