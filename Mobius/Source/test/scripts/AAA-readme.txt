Sat Nov 26 21:28:24 2011

This directory contains the Mobius unit test scripts.  Be extremely
careful with these, in some cases development of the test scripts took
longer than the actual code that implements the features (hello sync!).

The tests use the built-in script function UnitTestSetup to put Mobius
into a default configuration.  Be very careful when modifying the methods
that implement this function!

The major test scripts are:

  layertests.mos        layers and many loop functions
  jumptests.mos         play jumps and many loop functions
  mutetests.mos         mute and mute cancel
  eventtests.mos        tests focused on event scheduling
  speedtests.mos        basic rate changes
  synctests.mos         output sync tests
  insynctests.mos       MIDI input sync tests, not used see miditests.txt
  vstsynctestes.mos     VST plugin sync tests, not used see hosttests.txt
  exprtest.mos          general scripts and expression language
  calltest.mos          used by exprtest.mos, not called directly
  functests.mos         not used, was supposed to have swtich tests
                          
The layertests.mos script takes two arguments that control its behavior,
one to disable layer flattening and another to run everything in reverse.
The following scripts call layertests.mos with the possible values
for these two arguments:

  testdriverflat.mos        normal flattening
  testdriverflatrev.mos     flatting, reverse
  testdriverseg.mos         no flattening (segments only)
  testdriversegrev.mos      no flattening, reverse

It is important to run all four variants.  This script will
run all four:

  testdriver.mos

The most important tests are layertest, jumptest, mutetest, and eventtests.
speedtest has a few speed things.  synctest is good for output sync
but is a bit touchy, make sure the devices are all set up properly.

insynctest and vstsynctest are not complete.  They have a few things
but there needs to be a lot more.  Slave sync testing is a manual
process, see hosttests.txt and miditests.txt for the manual test plans.

exprtest is important, there are only a few tests and they generally
only break after changing the script engine and sometimes after
renaming parameters referenced by the script.

calltest is used by exprtest to test calls, you do not run this
directly.

The sync tests are very sensitive to activity on the system, so they
may occasionally fail during the full suite, but run ok in isolation.
Failures that look like they are caused by a jump in drift from around
300 to 1000 and then back down are probably ok if the tests run
without error a few times in isolation.

These are wave files used with most of the tests:

  base.wav           the background for most tests
  baseL.wav          NOT USED, the usual background, left channel only
  baseR.wav          NOT USED
  short1.wav         used
  short2.wav         used
  medium.wav         used
  medium2.wav        used
  long.wav           used
  gcgroove.wav       used, a George Clinton sample from some library

The "manual" directory contains random test scripts or test plans
that are run manually.  See comments in those files for more information.

The "notes" directory contains random notes about the test framework
and test results.

These are random test files for one-shot testing:

  dedrift.mos         runs the DriftCorrect function
  

---------------------------------

