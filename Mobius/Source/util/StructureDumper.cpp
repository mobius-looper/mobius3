/**
 * Quick little tool to assist in debugging dumps of Mobius
 * internal structures.  I'm sure there are lots of ways to do this
 * but juce::String is simple and convenenient.
 */

#include <JuceHeader.h>
#include "StructureDumper.h"

StructureDumper::StructureDumper()
{
}

StructureDumper::~StructureDumper()
{
}

juce::String StructureDumper::getText()
{
    return buffer;
}

bool StructureDumper::hasText()
{
    return (buffer.length() > 0);
}

void StructureDumper::clearVisited()
{
    visited.clear();
}

void StructureDumper::visit(int i)
{
    visited.add(i);
}

bool StructureDumper::isVisited(int i)
{
    return visited.contains(i);
}

void StructureDumper::clear()
{
    buffer.clear();
}

void StructureDumper::inc()
{
    indent++;
}

void StructureDumper::dec()
{
    indent--;
}

void StructureDumper::noIndent()
{
    indent = 0;
}

void StructureDumper::start(juce::String s)
{
    for (int i = 0 ; i < indent ; i++) {
        buffer << "  ";
    }
    buffer << s;
}

void StructureDumper::add(juce::String s)
{
    buffer << " " << s;
}

void StructureDumper::newline()
{
    buffer << "\n";
}

void StructureDumper::line(juce::String s)
{
    start(s);
    if (!s.endsWithChar('\n'))
      buffer << "\n";
}

void StructureDumper::line(juce::String name, juce::String value)
{
    start("");
    add(name, value);
    newline();
}

void StructureDumper::line(juce::String name, int value)
{
    start("");
    add(name, value);
    newline();
}

void StructureDumper::add(juce::String name, juce::String value)
{
    buffer << " " << name << "=" << value;
}

void StructureDumper::add(juce::String name, int value)
{
    add(name, juce::String(value));
}

void StructureDumper::addb(juce::String name, bool value)
{
    if (value)
      add(name);
}

void StructureDumper::setRoot(juce::File r)
{
    root = r;
}

void StructureDumper::write(juce::String filename)
{
    if (root == juce::File()) {
        write(juce::File(filename));
    }
    else {
        juce::File full = root.getChildFile(filename);
        write(full);
    }
}

void StructureDumper::write(juce::File file)
{
    file.replaceWithText(buffer);
}
            
void StructureDumper::test()
{
    start("Mobius");
    add("something", "xyzzy");
    add("something", 42);
    newline();
    inc();
    line("Track");
    inc();
    line("foo");
    line("bar");
    dec();
    line("Track");
    inc();
    line("baz");

    write("dump.txt");
}
