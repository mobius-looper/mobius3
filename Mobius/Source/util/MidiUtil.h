/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Misc MIDI and/or music oriented utilities of common interest.
 *
 */

#ifndef MIDIUTIL_H
#define MIDIUTIL_H

void MidiNoteName(int note, char *buf);
int	MidiNoteNumber(const char *str);

int	MidiChecksum(unsigned char *buffer, int len);

void MidiGetName(unsigned char *src, int size, char *dest);
void MidiSetName(unsigned char *dest, int size, char *src);

#endif
