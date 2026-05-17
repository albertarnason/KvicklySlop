/*TODO this is not a final platform layer!!

	- saved game locations
	- handle to own executable file
	- asset loading path
	- threading (launch a thread)
	- raw input (support multiple keyboards)
	- sleep/timebeginperiod
	- clipCursor() (multimonitor support)
	- fullscreen support
	- WM_SETCURSOR (control cursor visibility)
	- querycancelautoplay (old stuff)
	- WM_ACTIVATEAPP (when we are not the active application)
	- Blit speed improvements (BitBit)
	- Hardware acceleration (OpenGL or Direct 3D or both)
	- GetKeyBoardLayout (french keyboards, international WASD support)

*/

//Todo Swap, Min, Max macros

#include "handmade.h"

#include "handmade.cpp"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

struct win32_window_dimension 
{
	int Width;
	int Height;
};

struct win32_sound_output
{
	int SamplesPerSecond;
	uint32 RunningSampleIndex;
	int BytesPerSample;
	DWORD SecondaryBufferSize;
	real32 tSine;
	int LatencySampleCount;
	//todo math gets simpler if we add "BytesPerSecond"
	//should runningsampleindex be in bytes aswell?
};

struct win32_debug_time_marker 
{
	DWORD PlayCursor;
	DWORD WriteCursor;
};


global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;


//VirtualProtect great for free Catching use-after-free, Buffer overruns, accidental writes to read-only memory

//Cant return 2 values with a C function, so u bundle them into structs
//Dont want to bundle types if possible, only when the values HAVE to go together


//Calling windows function directly something something, getting around cases where users dont have gamepad specific software installed/available
//So the program can run without those and thus making Gamepad/xbox360controller support for users OPTIONAL instead of REQUIRED
//x_input_get_State *Foo is legal, can declare a pointer to these functions
//typedef DWORD WINAPI x_input_get_state(DWORD dwUserIndex, XINPUT_STATE* pState );
//typedef DWORD WINAPI x_input_set_state(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration );
//MACRO defines a function of this form, to make stubs
//XInputGetState

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub; //static global variable value is 0
#define XInputGetState XInputGetState_

//XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub; //static global variable value is 0
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name)HRESULT WINAPI name(LPCGUID pcGuidDevice,LPDIRECTSOUND *ppDS,LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

//only for debugging!
//NOT for shipping! Blocking and write doesnt protect against lost data!
internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename){
debug_read_file_result Result = {};
HANDLE FileHandle = CreateFileA(Filename,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
if(FileHandle != INVALID_HANDLE_VALUE)
{
	LARGE_INTEGER FileSize;
	if(GetFileSizeEx(FileHandle, &FileSize))
	{
		uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);Result.Contents = VirtualAlloc(0,FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		if(Result.Contents)
		{
			DWORD BytesRead;
			if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
			{
				Result.ContentsSize = FileSize32;
			}
			else
			{
				DEBUGPlatformFreeFileMemory(Result.Contents);Result.Contents = 0;
			}
		}
		else
		{
		}
	}
	else
	{
	}
	CloseHandle(FileHandle);
}
else
{
}
return(Result);
}

internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory){
bool32 Result = false;
HANDLE FileHandle = CreateFileA(Filename,GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);
if(FileHandle != INVALID_HANDLE_VALUE)
{
	DWORD BytesWritten;
	if(WriteFile(FileHandle, Memory,MemorySize, &BytesWritten, 0))
		{
			Result =(BytesWritten == MemorySize);
		}
	else
	{
	}
	CloseHandle(FileHandle);
}
else
{
}
return(Result);
}

internal void DEBUGPlatformFreeFileMemory(void *Memory){
if(Memory){
	VirtualFree(Memory, 0, MEM_RELEASE);
}
}

internal void Win32LoadXInpuT()
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		//no input error
	}

	if(XInputLibrary)
	{
		//no idea why this is duplicated
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if(!XInputGetState){XInputGetState = XInputGetStateStub;}
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if(!XInputSetState){XInputSetState = XInputSetStateStub;}
			else
		{
			//todo logging
		}
	}
	else
	{
		//todo logging
	}
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	//Load library
	//Get DirectSound object - cooperative mode (Component Object Model aaaaaa)
	//Create primary buffer
	//Create Secondary buffer (write to this)
	//start it playing

	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if(DSoundLibrary)
	{
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
		
		// directsound 8 or 7
		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{	
			//int16 [LEFT RIGHT] 4 bytes LEFT RIGHT SAMPLES IN BUFFER
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) /8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize;

			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription ={};
				BufferDescription.dwSize =sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				//DSBCAPS_GLOBALFOCUS?
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if(SUCCEEDED(Error))
					{
						//primary buffer set
						OutputDebugStringA("Primary buffer format was set\n");
					}
					else
					{
						//todo logging
					}
				}
				else
				{
					//todo logging
				}
			}
			else
			{
			//todo logging
			}
			//DSBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription  = {};

			BufferDescription.dwSize        = sizeof(BufferDescription);
			BufferDescription.dwFlags       = DSBCAPS_GETCURRENTPOSITION2;
			BufferDescription.dwFlags       = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat   = &WaveFormat;
			//DSBCAPS_GLOBALFOCUS?

			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if(SUCCEEDED(Error))
			{
				//Secondary buffer
				OutputDebugStringA("Secondary buffer format was set\n");
			}
			else
			{
					//todo logging
			}		
		}
		else
		{
			//todo logging
		}
	}
	else
	{
		//to do logging
	}



}



internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window,&ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
};


internal void Win32ReSizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if(Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; //negative height makes the rows go from topleft -> down, if it was positive it would be bottomleft -> up
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize =(Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;

	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	Buffer->Pitch = Width*Buffer->BytesPerPixel;
}


internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,HDC DeviceContext, int WindowWidth, int WindowHeight)
{
	//TODO aspect ratio correction
	StretchDIBits(
	DeviceContext,
	0,0, WindowWidth, WindowHeight,
	0,0, Buffer->Width, Buffer->Height,
	Buffer->Memory,
	&Buffer->Info,
	DIB_RGB_COLORS, SRCCOPY);
}



LRESULT CALLBACK Win32MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM WParam,
  LPARAM LParam)
  {

	LRESULT Result = 0;

	switch(Message)
	{
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{Assert(!"Keyboard input came in through a non-dispatch message!")} break;
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;
		case WM_DESTROY:
		{
			//handle this as an error - recreate window? 
			GlobalRunning = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		
		case WM_CLOSE:
		{
			//handle this with a message to the user?
			GlobalRunning = false;
			OutputDebugStringA("WM_CLOSE\n");
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
			//beginpaint endpaint is important for windows so it knows when to start and stop painting, it keeps its own record
			//and if u dont have it itll flood message queue
			EndPaint(Window, &Paint);
		} break;
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam); 
		} break;
	}

	return(Result);
}



internal void Win32ClearBuffer(win32_sound_output *SoundOutput){
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0,
		SoundOutput->SecondaryBufferSize,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
		{
			uint8 *DestSample = (uint8 *)Region1;
			for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
			{
				*DestSample++ = 0;
			}
			DestSample = (uint8 *)Region2;
			for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
			{
				*DestSample++ = 0;
			}
			GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
		}
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{

	// More testing
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(
		ByteToLock,
		BytesToWrite,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{
		// Assert that Region1Size/Region2Size is valid
		
		//Collapse these two loops
		DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}

}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state *OldState, game_button_state *NewState, DWORD ButtonBit){
	NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
	NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;

}

internal void Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown){
	Assert(NewState->EndedDown != IsDown);
	NewState->EndedDown = IsDown;
	++NewState->HalfTransitionCount;

}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold){
	real32 Result = 0;
	if 		 	(Value < -DeadZoneThreshold){Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);} 
		else if (Value > DeadZoneThreshold) {Result = (real32)(Value - DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);}
	
	return Result;
}

internal void Win32ProcessPendingMessages(game_controller_input *KeyboardController){

MSG Message;
//has to process the message queue from windows
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch (Message.message)
		{
			case WM_QUIT: {GlobalRunning = false;} break;
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				// VKCode tells which key it is
				// VKCode == 'W' = w key
				uint32 VKCode  = (uint32)Message.wParam;
				bool WasDown   = ((Message.lParam & (1 << 30)) != 0);
				bool IsDown    = ((Message.lParam & (1 << 31)) == 0);

				if (WasDown != IsDown)
				{
					if      (VKCode == 'W')        { Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, 			 IsDown);}
					else if (VKCode == 'A')        { Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, 		 IsDown);}
					else if (VKCode == 'S')        { Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, 		 IsDown);}
					else if (VKCode == 'D')        { Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, 		 IsDown);}
					else if (VKCode == 'Q')        { Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, 	 IsDown);}
					else if (VKCode == 'E')        { Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder,	 IsDown);}
					else if (VKCode == VK_UP)      { Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, 		 IsDown);}
					else if (VKCode == VK_DOWN)    { Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, 		 IsDown);}
					else if (VKCode == VK_LEFT)    { Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, 	     IsDown);}
					else if (VKCode == VK_RIGHT)   { Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, 	     IsDown);}
					else if (VKCode == VK_SPACE)   { Win32ProcessKeyboardMessage(&KeyboardController->Back, 	     	 IsDown);}
					else if (VKCode == VK_ESCAPE)  { Win32ProcessKeyboardMessage(&KeyboardController->Start, 	    	 IsDown);}
				}
				bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
				if ((VKCode == VK_F4) && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			} break;
			default:
			{
			TranslateMessage(&Message);
			DispatchMessageA(&Message);
			} break;
		}
	}
}


inline LARGE_INTEGER Win32GetWallClock(void){
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end){
	real32 result = ((real32)end.QuadPart - start.QuadPart) / (real32)GlobalPerfCountFrequency;
	return result;
}

internal void Win32DebugDrawVertical(win32_offscreen_buffer *GlobalBackBuffer, int X, int top, int bottom, uint32 color){
	uint8 *pixel = ((uint8 *)GlobalBackBuffer->Memory + X*GlobalBackBuffer->BytesPerPixel + top*GlobalBackBuffer->Pitch);
	for (int Y = top; Y < bottom; ++Y){
		*(uint32 *)pixel = color;
		pixel += GlobalBackBuffer->Pitch;
	}

}

inline void Win32DrawSoundBufferMarker (win32_offscreen_buffer *BackBuffer, win32_sound_output *SoundOutput, real32 C, int pad_x, int top, int bottom, DWORD value_to_draw, uint32 color) {

	Assert(value_to_draw < SoundOutput->SecondaryBufferSize);
	real32 XReal32 = (C * (real32)value_to_draw);
	int X =  pad_x + (int)XReal32;
	Win32DebugDrawVertical(BackBuffer, X, top, bottom, color);
}

//debugging for sound syncing
internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer, int marker_count, win32_debug_time_marker *markers, win32_sound_output *SoundOutput, real32 target_seconds_elapsed_per_frame){
	
	int pad_x = 16;
	int pad_y = 16;
	int top = pad_y;
	int bottom = BackBuffer->Height - pad_y;
	real32 C = (real32)(BackBuffer->Width -2 * pad_x) / (real32)SoundOutput->SecondaryBufferSize;
	for(int marker_index = 0; marker_index < marker_count; ++marker_index){
		
		win32_debug_time_marker *this_marker = &markers[marker_index];

		Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->PlayCursor,  0xFFFFFFFF);
		Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->WriteCursor,  0xFFFF0000);
	}
	
	return;
}



int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CommandLine,
	int       ShowCode){
	
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	//Set the windows scheduler granularity to 1ms
	//so that sleep() can be more granular
	UINT desired_scheduler_ms = 1;
	bool32 sleep_is_granular  = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);
	
	Win32LoadXInpuT();

	WNDCLASSA WindowClass = {};
	Win32ReSizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
 	// WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	//todo how to query this on windows
	//hz = cycles per sec == frames per second

#define monitor_refresh_hz 60
#define game_update_hz (monitor_refresh_hz / 2)
	real32 target_seconds_per_frame = 1.0f / (real32)game_update_hz;


  	if(RegisterClassA(&WindowClass))
  	{
		HWND Window = CreateWindowExA(
			0, //dwExStyle
			WindowClass.lpszClassName, //lpClassName
			"HandmadeHero", //lpWindowName
			WS_OVERLAPPEDWINDOW|WS_VISIBLE, //dwStyle
			CW_USEDEFAULT, //X 
			CW_USEDEFAULT, //Y 
			CW_USEDEFAULT, //X width
			CW_USEDEFAULT, //Y height
			0,
			0,
			Instance,
			0);
		if(Window != NULL)
		{
			HDC DeviceContext = GetDC(Window);
			win32_sound_output SoundOutput = {};

			//Make this like 60 seconds (so playcursor cant wrap on us)
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			//todo get rid of latency sample count
			SoundOutput.LatencySampleCount = 3*(SoundOutput.SamplesPerSecond / game_update_hz);
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond , SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0,0, DSBPLAY_LOOPING);

			GlobalRunning = true;
#if 0
			//testing playcursor/writecursor update frequency
			//480 samples
			//audio debug info
			while(GlobalRunning){
				DWORD PlayCursor;
				DWORD WriteCursor;
				GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
				char sound_text_buffer[256];
				_snprintf_s(sound_text_buffer, sizeof(sound_text_buffer), "PC:%u WC:%u\n", PlayCursor, WriteCursor);
				OutputDebugStringA(sound_text_buffer);
			}
#endif
			int16 *Samples =(int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL 
LPVOID BaseAdress = (LPVOID)Terabytes((uint64)2);
#else
LPVOID BaseAdress = 0;
#endif

			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)4);

			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;

			GameMemory.PermanentStorage = VirtualAlloc(BaseAdress, TotalSize,  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			
			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input [2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];
				
				LARGE_INTEGER LastCounter =  Win32GetWallClock();
				
				int debug_time_marker_index = 0;
				win32_debug_time_marker debug_time_markers[game_update_hz / 2] = {0};
				
				DWORD  last_play_cursor      = 0;
				DWORD  last_write_cursor     = 0;
				DWORD  audio_latency_bytes   = 0;
				real32 audio_latency_seconds = 0;
				bool32 SoundIsValid          = false;
				
				uint64 LastCycleCount = __rdtsc();
				
				while(GlobalRunning)
				{

					//todo make zeroing macro
					//todo we cant zero everything because the up/down state will be wrong!!!!
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex){
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					
					Win32ProcessPendingMessages(NewKeyboardController);
			
					DWORD MaxControllerCount = XUSER_MAX_COUNT;
					if(MaxControllerCount > ArrayCount(NewInput->Controllers)){MaxControllerCount = ArrayCount(NewInput->Controllers);}
					for(DWORD ControllerIndex = 0;ControllerIndex < MaxControllerCount; ++ControllerIndex)
					{
						DWORD OurControllerIndex = ControllerIndex + 1;
						game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
						game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

						XINPUT_STATE ControllerState;
						if(XInputGetState(ControllerIndex,&ControllerState) == ERROR_SUCCESS)
						{
							NewController->IsConnected = true;
							//CONTROLLER IS PLUGGED IN
							// See if ControllerState.dwPacketNumber increments too rapidly
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							NewController->StickAverageX = Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
							NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

							if ((NewController->StickAverageX != 0.0f)||(NewController->StickAverageY != 0.0f)){NewController->Analog = true;}
							
							if (Pad ->wButtons & XINPUT_GAMEPAD_DPAD_UP)	{NewController->StickAverageY = -1.0f; NewController->Analog = false;};
							if (Pad ->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)	{NewController->StickAverageY =  1.0f; NewController->Analog = false;};
							if (Pad ->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)	{NewController->StickAverageY = -1.0f; NewController->Analog = false;};
							if (Pad ->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)	{NewController->StickAverageY =  1.0f; NewController->Analog = false;};
							
							real32 Threshold = 0.5f;
							Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, &OldController->MoveLeft, &NewController->MoveLeft, 1);
							Win32ProcessXInputDigitalButton((NewController->StickAverageX >  Threshold) ? 1 : 0, &OldController->MoveRight,&NewController->MoveRight,1);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, &OldController->MoveDown, &NewController->MoveDown, 1);
							Win32ProcessXInputDigitalButton((NewController->StickAverageY <  Threshold) ? 1 : 0, &OldController->MoveUp,   &NewController->MoveUp,   1);

							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->ActionDown,    &NewController->ActionDown, 	 XINPUT_GAMEPAD_A);
							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->ActionRight,   &NewController->ActionRight,  XINPUT_GAMEPAD_B);
							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->ActionLeft,    &NewController->ActionLeft, 	 XINPUT_GAMEPAD_X);
							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->ActionUp,      &NewController->ActionUp, 	 XINPUT_GAMEPAD_Y);

							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->LeftShoulder,  &NewController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER );
							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->RightShoulder, &NewController->RightShoulder,XINPUT_GAMEPAD_RIGHT_SHOULDER);

							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->Start, 		   &NewController->Start, 		 XINPUT_GAMEPAD_START);
							Win32ProcessXInputDigitalButton(Pad ->wButtons, &OldController->Back,          &NewController->Back,  		 XINPUT_GAMEPAD_BACK );
							
						}
						else
						{
							NewController->IsConnected = false;
						}
					}
					/*
						Here is how sound output computation works

						We define a safety value that is the number of samples we think our game update loop may vary by. (lets say up to 2 ms)

						When we wake up to write audio, we will look and see what the playcursor position in and we will forecast ahead where we think the play cursor will be on the next frame boundary

						we will then look to see if the write cursor is before that by atleast our safety value. If it is, the target fill position is that frame boundary plus one frame. This gives us perfect audio sync in the case of a (sound)card that has low enough latency

						If the write cursor is _after_ that safety margin, then we assume we can never sync the audio perfectly, so we will write one frame's worth of audio plus the safety margin worth of guard samples (1ms or something determined safe, whatever we think the variability of our frame computation is)


					*/
					//compute how much sound to write and where
					DWORD ByteToLock = 0;
					DWORD BytesToWrite = 0;
					DWORD TargetCursor = 0;
					if(SoundIsValid)
					{ ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;


						TargetCursor = ((last_play_cursor + SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
						
						// change this to using a lower latency offset from the playcursor
						// when we actually start having sound effects
						// TODO check this next
						if(ByteToLock > TargetCursor)
						{	
							BytesToWrite  = (SoundOutput.SecondaryBufferSize - ByteToLock);
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}
						//audio debug info
						char sound_text_buffer[256];
						_snprintf_s(sound_text_buffer, sizeof(sound_text_buffer), "PC: %u BTL: %u TC:%u BTW: %u\n", last_play_cursor, ByteToLock, TargetCursor, BytesToWrite);
						OutputDebugStringA(sound_text_buffer);

					}

					//sound is wrong because it doesnt go with new frame loop
					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount      = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples          = Samples;

					

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackBuffer.Memory;
					Buffer.Width  = GlobalBackBuffer.Width;
					Buffer.Height = GlobalBackBuffer.Height;
					Buffer.Pitch  = GlobalBackBuffer.Pitch;
					GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);
					
					//Buggy
					//DirectSound output test
					if(SoundIsValid){
#if HANDMADE_INTERNAL
						//casey just uses DWORD PlayCursor here aswell
						DWORD DebugPlayCursor;
						DWORD DebugWriteCursor;
						GlobalSecondaryBuffer->GetCurrentPosition(&DebugPlayCursor, &DebugWriteCursor);
						DWORD unwrapped_write_cursor = DebugWriteCursor;
						if(unwrapped_write_cursor < DebugPlayCursor){
							unwrapped_write_cursor += SoundOutput.SecondaryBufferSize;
						}
						audio_latency_bytes = unwrapped_write_cursor - DebugPlayCursor;
						audio_latency_seconds = (((real32)audio_latency_bytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
						
						
						char sound_text_buffer2[256];
						_snprintf_s(sound_text_buffer2, sizeof(sound_text_buffer2), "LPC: %u BTL: %u TC:%u BTW: %u PC: %u WC:%u DELTA: %u (%fs)\n", last_play_cursor, ByteToLock, TargetCursor, BytesToWrite, DebugPlayCursor, DebugWriteCursor, audio_latency_bytes, audio_latency_seconds);
						OutputDebugStringA(sound_text_buffer2);
#endif
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					//Performance  and framelocking
					
					
					LARGE_INTEGER WorkCounter = Win32GetWallClock();
					real32 work_seconds_elapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
					real32 seconds_elapsed_for_frame = work_seconds_elapsed;
					
					//cpu melting solution to cap frames at 60fps
					if (seconds_elapsed_for_frame < target_seconds_per_frame){
						if(sleep_is_granular){
							DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds_per_frame - seconds_elapsed_for_frame));
							if (sleep_ms > 0){
								Sleep(sleep_ms);
							}
						}

						real32 test_seconds_elapsed_for_frame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
						if(test_seconds_elapsed_for_frame < target_seconds_per_frame){
							//TODO Log miss here
						}
						//testing to see if we're within 33mspf budget, but we hit 34.41mspf, consistently
						while(seconds_elapsed_for_frame < target_seconds_per_frame){
							seconds_elapsed_for_frame = Win32GetSecondsElapsed(LastCounter,Win32GetWallClock());
						}
					}
					else{
						//MISSED FRAMERATE CHECK HERE
						//logging!
					}
					
					//framerate logging
					LARGE_INTEGER EndCounter = Win32GetWallClock();
					real64 MSPerFrame = 1000.0f * Win32GetSecondsElapsed(LastCounter, EndCounter);
					LastCounter = EndCounter;
					
					//displaying frame
					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
					Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(debug_time_markers), debug_time_markers, &SoundOutput, target_seconds_per_frame);
#endif
					Win32DisplayBufferInWindow(&GlobalBackBuffer,DeviceContext, Dimension.Width, Dimension.Height);
					DWORD PlayCursor;
					DWORD WriteCursor;
					if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK){
						last_write_cursor = WriteCursor;
						last_play_cursor  = PlayCursor;
						if(!SoundIsValid){
							SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
							SoundIsValid = true;
						}
					}
					else{
						SoundIsValid = false;	
					}
#if HANDMADE_INTERNAL
					//debug code to find out where DirectSound think it is
					{
						Assert(debug_time_marker_index < ArrayCount(debug_time_markers))
						win32_debug_time_marker *Marker = &debug_time_markers[debug_time_marker_index++];
						
						if (debug_time_marker_index == ArrayCount(debug_time_markers)){
							debug_time_marker_index = 0;
						}
						Marker->PlayCursor = PlayCursor;
						Marker->WriteCursor = WriteCursor;
					}
#endif

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
				
					uint64 EndCycleCount = __rdtsc();
					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					LastCycleCount = EndCycleCount;

					
					real64 FPS = 0.0f;
					real64 MCPF = (real64)(CyclesElapsed / (1000.0f *1000.0f));

					char FPSBuffer[256];
					//milliseconds per frame, frames per second, (mega)cycles per frame
					//sprintf always takes 64bit floats
					_snprintf_s(FPSBuffer, sizeof(FPSBuffer), "%.02fmspf, %.02ffps, %.02fmcpf\n", MSPerFrame, FPS, MCPF);

					OutputDebugStringA(FPSBuffer);

				}
			}
			else
			{
				//todo logging
			}
		} 
		else{
			//todo logging
		}
  }
  else
  {
	//todo logging
  };
  return(0);
}
