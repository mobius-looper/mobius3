# The Mobius Scripting Language (MSL)

MSL was designed to be a simple yet flexible language for writing short customizations
for the Mobius looper.  While it was created for Mobius, it is intended for use by other
applications so some of the terms used will be different from the related concepts in Mobius.

The scripting environment lives within an application that can be controlled primarily in
two ways, Through *Functions* and *Parameters*.

A function is a command that causes the application to do something.  Examples in Mobius
include *Record*, *Overdub*, and *Multiply*.

A parameter is a named value that influences the way functions behave.  Examples in Mobius
include *quantize*, *muteMode*, and *syncSource*.

To write MSL scripts for Mobius, you will need to know the names of the functions and parameters
that are available.  These are described in detail in the Reference Manual (todo: link)

# MSL Terminology

MSL scripts are usually text files that are installed in the Mobius application.  The
installation process is described in the Managing Scripts (todo: link) guide.

Small MSL scripts may also be defined in "scriptlets".  Scriptlets are usually a single line of text
that is entered in the Mobius UI at various places.  The most common location is as the
value of a *Binding* for MIDI or keyboard events.

The most basic use of MSL scripts is to cause a function to be performed or to change the value
of a parameter.  Causing a function to happen is referred to using several words in speech
and writing.  Examples include:

   * executing
   * performing
   * running
   * calling
   * invoking

When talking about MSL functions, these words all mean the same thing.  In the script documentation
we will most often use the phrase "calling a function" since that is what is common when
talking about programming languages.

A function always has a name.  When you want to call a function you need to know its name.

A parameter has a name and a value.  When you want to use or change the value of a parameter you need
to know both the name and the value.  Changing the value of a parameter is usually referred
to as "setting a parameter" or "assigning a parameter".  Other terms used when talking about
parameters are:

   * setting
   * assigning
   * getting
   * using
   * binding
   * overriding

These all mean similar things, they either access or modify the value of a parameter.

# MSL Language Design For Programmers

The next few paragraphs are intended for experienced programmers to help them more
quickly understand how the language is structured.  If you are not a programmer, you can skip
this section.

There are many programming and scripting languages in the world.  Why did we need another one?
There were several important requirements for a scripting language used in the context
of the Mobius application:

    * easy to undertand by people who are not programmers
    * fast and predictable execution time
    * carefully controlled memory allocation

When designing the syntax of the language the first point is the most important.  Things
that make perfect sense to experienced programmers are confusing to non-programmers.
Unusual punctuation characters and the names of keywords are two examples.   Why are "=" and "=="
different?  What does "!=" mean?  What does "for" or "case" mean?  Why does this have to come
before that?

MSL is what could be referred to as a "domain specific language".  It isn't attempting to
be a general purpose programming language.  MSL programs are small, rarely over a dozen lines
and often a single line embedded in a data structure somewhere.  It has just three primary jobs:
to call Mobius functions and set Mobius parameters and to organize those with basic conditional logic.

This usage simplicity gives us the opportunity to make design choices that would not be suitable
for more complex languages.  Some of these choices will make programmers cringe.  Yes, I know there
is an ambiguity here or an inconsistency there.  Will the user care?  Will they even get into a
situation where an inconsistency exists?  Most likely no.  If it makes sense to your mom, who cares?

When designing the execution behavior of the languge, the second two points above are important.
MSL is unusual in that it must execute in the *audio thread*.  If you're not familiar with
audio applications, this is a special system thread that must respond to the audio interface hardware
that is sending and receiving blocks of audio data.  You might think of it as an "interrupt handler"
from the olden days.  

Code that runs in the audio thread is far more constrained than code running in a user interface.
It must be fast, it can't just stop and take its sweet time doing garbage collection because this
might cause it to miss the transfer of an audio block.  When this happens you get discontinuities
in the audio stream that sound like "clicks and pops".  That can't happen in audio applications.

Related to this is memory allocation.  One of the cardinal rules of code running in the audio thread
is that it make minimal use of system resources, most notably memory allocation.  It is *possible*
to do memory allocation, but you *shouldn't* because under the right conditions this can cause
expensive reorganization of the free pool that can exceed the time allowed to respond to audio blocks.

If you have looked at any of the old Mobius code, you will see that I used to do memory allocation
in the audio thread *all the time*.  It is seductive.  It makes life so much more convenient
and it works most of the time.  But sometimes it doesn't.  And if it doesn't when you are performing
at a televised award show, the user will hate you.

Why does this need to run in the audio thread at all?  It's a good question.  Lots of applications
have scripting or "macro" languages and they don't have this requirement.  The answer is *sample accurate timing*.  Often when you want to execute a function or change a parameter, when that actually
happens isn't that important.  If you're using a MIDI footswitch to execute the *Record* function
there is all sorts of hardware and software between the physical switch and Mobius code that adds
unpredictable latency to the time the function actually happens.  Usually this is so small that the
user won't care.

But when writing scripts that do complex manipulation of audio data, it is often necessary to be very precise about when things happen.  Imagine cutting and pasting fragments of audio data in DAW tracks.  When you're editing, you can't usually be just "close enough".  Something has to happen *exactly* on this bar, or at *exactly* sample 45783, or *exactly* at the end of this loop.  What makes MSL scripts
unique as far as I know in the world of audio applications, is it's ability to do sample accurate loop editing "live" in real time.  In order to do that, it must run in the audio thread.

## MSL Language Influences

If you have programming experience, you may notice that MSL has been
heavily influenced by the programming language known as Lisp.  Unlike
Lisp, MSL uses the far more common "infix notation" for expressions
and does not rely on parenthesis as statement delimiters.   Thinking of MSL as "Lisp with a c-like syntax" is a reasonable way to start.  It isn't exactly either of those, but that is what I've had
in mind as the language evolved.

Like Lisp, MSL is heavily organized around the concept of a *symbol*.  A symbol is a unique
name and can be associated with a parameter value or a function.  A symbol associated with a function
can be called with arguments and a symbol associated with a parameter can be bound to a value.

Symbols may be used within arithmetic or logical *expressions*.  Expressions may be included in
*statements* and statements may be collected into *blocks*.

The syntax for expressions and function calls is however more like C.  Where in Lisp you would do this

     (+ 1 2)

In MSL you would write

     1 + 2

In Lisp calling a function looks like this:

    (foo a b)

In MSL it looks like this

    foo(a b)


Lisp assignments look like this:

    (setq a b)

In Msl assignments are:

    a=b

MSL has the usual keywords for conditional statements: if, then, else, while

Like Lisp all statements return a value.  So whereas in C you would do this:

    (x == y) ? 1 : 2;

In MSL you would do:

    if x == y 1 2

MSL tries to make keywords and punctuation optional when it can.  The previous statement
could also be written in the following ways:

    if (x == y) then 1 else 2

    if (x == y) {
        1
    }
    else {
        2
    }

