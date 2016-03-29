/*     _________  ___
 _____ \_   /\  \/  / OS X/CoreAudioOutputPlayer.m
|  |  |_/  /__>    <  Copyright © 2014-2015 Manuel Sainz de Baranda y Goñi.
|   ____________/\__\ Released under the GNU General Public License v3.
|_*/

#import "CoreAudioOutputPlayer.h"
#import <AudioToolbox/AudioToolbox.h>
#import <Z/types/base.h>
#import <Z/functions/buffering/ZRingBuffer.h>
#import "system.h"


@implementation CoreAudioOutputPlayer


	static OSStatus FillBuffer(
		CoreAudioOutputPlayer*	    player,
		AudioUnitRenderActionFlags* ioActionFlags,
		const AudioTimeStamp*	    inTimeStamp,
		UInt32 			    inBusNumber,
		UInt32 			    inNumberFrames,
		AudioBufferList*	    ioData
	)
		{
		if (player->_buffer.fill_count < 2)
			{
			ioData->mBuffers[0].mData = z_ring_buffer_consumption_buffer(&player->_buffer);
			return noErr;
			}

		while (player->_buffer.fill_count > 3)
			z_ring_buffer_try_consume(&player->_buffer);

		ioData->mBuffers[0].mData = z_ring_buffer_consumption_buffer(&player->_buffer);
		z_ring_buffer_try_consume(&player->_buffer);
		return noErr;
		}


	- (id) init
		{
		if ((self = [super init]))
			{
			_sampleRate = 44100;
			_bufferFrameCount = 2;

			void *buffer = calloc(1, Z_INT16_SIZE * 882 * 4);
			z_ring_buffer_initialize(&_buffer, buffer, Z_INT16_SIZE * 882, 4);

			//---------------------------------------------------------------------------.
			// Configure the search parameters to find the default playback output unit. |
			//---------------------------------------------------------------------------'
			AudioComponentDescription outputDescription = {
				.componentType		= kAudioUnitType_Output,
				.componentSubType	= kAudioUnitSubType_DefaultOutput,
				.componentManufacturer	= kAudioUnitManufacturer_Apple,
				.componentFlags		= 0,
				.componentFlagsMask	= 0
			};

			//---------------------------------------.
			// Get the default playback output unit. |
			//---------------------------------------'
			AudioComponent output = AudioComponentFindNext(NULL, &outputDescription);
			NSAssert(output, @"Can't find default output");

			//------------------------------------------------------------.
			// Create a new unit based on this that we'll use for output. |
			//------------------------------------------------------------'
			OSErr error = AudioComponentInstanceNew(output, &_audioUnit);
		//	NSAssert1(_toneUnit, @"Error creating unit: %hd", error);

			// Set our tone rendering function on the unit
			AURenderCallbackStruct input;
			input.inputProc = (void *)FillBuffer;
			input.inputProcRefCon = self;

			error = AudioUnitSetProperty
				(_audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input,
				 0, &input, sizeof(input));

			NSAssert1(error == noErr, @"Error setting callback: %hd", error);

			UInt32 frames = 882;
			// Set the max frames

			error = AudioUnitSetProperty
				(_audioUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global,
				 0, &frames, sizeof(frames));

			NSAssert1(error == noErr, @"Error setting maximum frames per slice: %hd", error);

			// Set the buffer size

			error = AudioUnitSetProperty
				(_audioUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global,
				 0, &frames, sizeof(frames));

			NSAssert1(error == noErr, @"Error setting buffer frame size: %hd", error);

			//----------------------------------------------------.
			// Set the format to 16-bit little-endian lineal PCM. |
			//----------------------------------------------------'
			AudioStreamBasicDescription streamFormat = {
				.mSampleRate       = _sampleRate,
				.mFormatID	   = kAudioFormatLinearPCM,
				.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
				.mBytesPerPacket   = 2,
				.mFramesPerPacket  = 1,
				.mBytesPerFrame    = 2,
				.mChannelsPerFrame = 1,
				.mBitsPerChannel   = 16,
				.mReserved	   = 0
			};

			error = AudioUnitSetProperty
				(_audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input,
				 0, &streamFormat, sizeof(AudioStreamBasicDescription));

			NSAssert1(error == noErr, @"Error setting stream format: %hd", error);

			error = AudioUnitInitialize(_audioUnit);
			NSAssert1(error == noErr, @"Error initializing unit: %hd", error);
			}

		return self;
		}



	- (ZRingBuffer *) buffer {return &_buffer;}


	- (void) dealloc
		{
		_playing = NO;
		AudioOutputUnitStop(_audioUnit);
		AudioUnitUninitialize(_audioUnit);
		[super dealloc];
		}


	- (void) start
		{
		// Start playback
		_playing = YES;
		OSErr error = AudioOutputUnitStart(_audioUnit);

		NSAssert1(error == noErr, @"Error starting unit: %hd", error);
		//NSLog(@"play: isMainThread => %s", [NSThread isMainThread] ? "YES" : "NO");
		}


	- (void) stop
		{
		AudioOutputUnitStop(_audioUnit);
		}


@end