/**
 * An interface for something that can provide application-wide utility objects.
 * This is implemented by Supervisor.
 *
 * Things that need things from Supervisor should reference this interface instead
 * so the definition of Supervisor.h can change without recompiling the whole damn world.
 *
 */

#pragma once

class Provider
{
  public:

    virtual ~Provider() {}

    virtual class MobiusConfig* getMobiusConfig() = 0;
    virtual class MainConfig* getMainConfig() = 0;
    virtual class SymbolTable* getSymbols() = 0;
    virtual class MobiusMidiTransport* getMidiTransport() = 0;
    virtual class MidiRealizer* getMidiRealizer() = 0;
    virtual class MidiManager* getMidiManager() = 0;
    virtual int getSampleRate() = 0;
};
