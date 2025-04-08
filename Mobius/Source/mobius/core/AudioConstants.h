/**
 * Various old constants that used to live in other places.
 * 
 * Some of these are defined in more than one place, need to
 * clean this up and find a good home for these.
 */

#pragma once

/**
 * The maximum number of frames for an audio interface buffer.
 * This should as large as the higest expected ASIO buffer size.
 * 1024 is probably enough, but be safe.  
 * UPDATE: auval uses up to 4096, be consistent with 
 *
 * This is used by FadeTail, Layer, Segment, and Stream

 */
#define AUDIO_MAX_FRAMES_PER_BUFFER	(4096)

/**
 * The maximum number of channels we will have to deal with.
 * Since this is used for working buffer sizing, don't raise this until
 * we're ready to deal with surround channels everywhere.
 *
 * Used by Audio, AudioFile, FadeTail, Layer, Resampler, Segment, StreamPlugin, Stream
 */
#define AUDIO_MAX_CHANNELS 2 

/**
 * Next to Pi, the most popular universal constant.
 *
 * Used by Audio, ParameterSource, Layer, Script, Slip, Window, TrackMslHandler, MobiusConfig
 *
 */
#define CD_SAMPLE_RATE 		(44100)
