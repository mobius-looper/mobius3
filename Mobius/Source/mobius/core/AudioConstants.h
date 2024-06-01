/**
 * Various constants that used to live in other places.
 * 
 * Some of these are defined in more than one place, need to
 * clean this up and find a good home for this.  Moved here
 * from AudioInterface.h while trying to get rid of that
 */

#pragma once

// some of these are defined elsewhere in Kernel, can migrate gradually

/**
 * The preferred number of frames in an audio interface buffer.
 * This is what we will request of PortAudio, and usually get, but
 * you should not assume this will be the case.  When using the VST
 * interface, the size is under control of the VST driver and it will
 * often be 512, but usually not higher.
 */
#define AUDIO_FRAMES_PER_BUFFER	(256)

/**
 * The maximum number of frames for an audio interface buffer.
 * This should as large as the higest expected ASIO buffer size.
 * 1024 is probably enough, but be safe.  
 * UPDATE: auval uses up to 4096, be consistent with 
 * MAX_HOST_BUFFER_FRAMES in HostInterface.h
 * !! don't like the duplication, move HostInterface over here?
 */
#define AUDIO_MAX_FRAMES_PER_BUFFER	(4096)

/**
 * The maximum number of channels we will have to deal with.
 * Since this is used for working buffer sizing, don't raise this until
 * we're ready to deal with surround channels everywhere.
 */
#define AUDIO_MAX_CHANNELS 2 


/**
 * The maximum size for an intermediate buffer used for transformation
 * of an audio interrupt buffer.  Used to pre-allocate buffers that
 * will always be large enough.
 */
#define AUDIO_MAX_SAMPLES_PER_BUFFER AUDIO_MAX_FRAMES_PER_BUFFER * AUDIO_MAX_CHANNELS

/**
 * Maximum number of ports (assumed to be stereo) we support
 * for a given audio device.
 */
#define AUDIO_MAX_PORTS 16

#define CD_SAMPLE_RATE 		(44100)
