/**
 * The session defines a large number of property names, most of which will
 * have a formal Symbol/ParameterProperties definition, but a few don't.
 *
 * For those that don't, keep name constants here to avoid fragile string literals
 * in the code.
 *
 * This is a hack to get new things added quickly, it is likely that most things
 * here actually do need formal Parameter definitions.
 *
 */

static const char* SessionBeatsPerBar = "beatsPerBar";
static const char* SessionHostOverrideTimeSignature = "hostOverrideTimeSignature";

static const char* SessionTransportMidiEnable = "transportMidiEnable";
static const char* SessionTransportClocksWhenStopped = "transportMidiClocksWhenStopped";
