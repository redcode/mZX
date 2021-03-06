/*	_________  ___
  _____ \_   /\  \/  /	iOS/MachineViewController.h
 |  |  |_/  /__>    <	Copyright © 2014-2015 Manuel Sainz de Baranda y Goñi.
 |   ____________/\__\	Released under the terms of the GNU General Public License v3.
 |_*/

#import "Machine.hpp"
#import "GLVideoView.h"
#import "CoreAudioOutputPlayer.hpp"
#import <Z/hardware/machine/platform/computer/ZX Spectrum.h>
#import <Z/hardware/machine/model/computer/ZX Spectrum/ZX Spectrum.h>

@interface MachineViewController : UIViewController {


	//-----------.
	// Emulation |
	//-----------'
	Machine* _machine;
	MachineABI *_abi;
	//----------.
	// Geometry |
	//----------'
	Zeta::Value2D<Zeta::Real> _minimumWindowSize;

	//-------------.
	// I/O Buffers |
	//-------------'
	//QTripleBuffer*	_videoOutputBuffer;
	//QRingBuffer*		_audioOutputBuffer;
	Zeta::TripleBuffer*	_keyboardBuffer;
	Z64Bit*			_keyboard;
	Z64Bit			_keyboardState;

	BOOL			_smooth;
	IBOutlet GLVideoView*	_videoOutputView;
	CoreAudioOutputPlayer*	_audioOutputPlayer;
	BOOL			_keyboardInput;
	Zeta::RingBuffer	_audioInputBuffer;

	//--------.
	// Panels |
	//--------'
	//KeyboardWindowController*     _keyboardWindowController;
	//DebuggerWindowController*     _debuggerWindowController;
	//TapeRecorderWindowController* _tapeRecorderWindowController;

	struct {BOOL isFullScreen	 :1;
		BOOL ignoreKeyboardInput :1;
	} _flags;

	// Temporal
	Zeta::Size   _tapeSampleCount;
	Zeta::UInt8* _tapeSamples;
	BOOL	     _attachInputBuffer;
}

	- (id) initWithMachineABI: (MachineABI *) machineABI;

@end
