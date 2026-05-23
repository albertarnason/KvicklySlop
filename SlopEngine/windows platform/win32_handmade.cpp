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

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>
#include <winioctl.h>

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
	int    SamplesPerSecond;
	int    BytesPerSample;
	DWORD  SecondaryBufferSize;
	DWORD  SafetyBytes;
	uint32 RunningSampleIndex;
	real32 tSine;
	//todo math gets simpler if we add "BytesPerSecond"
	//should runningsampleindex be in bytes aswell?
};

struct win32_debug_time_marker 
{
	DWORD OutputPlayCursor;
	DWORD OutputWriteCursor;
	DWORD OutputLocation;
	DWORD OutputByteCount;
	DWORD FlipPlayCursor;
	DWORD FlipWriteCursor;
	DWORD ExpectedFlipPlayCursor;
};


#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
	HANDLE Filehandle;
	HANDLE MemoryMap;
	char Filename[WIN32_STATE_FILE_NAME_COUNT];
	void *MemoryBlock;
};

struct win32_state
{
	uint64 TotalSize;
	void *GameMemoryBlock;
	win32_replay_buffer ReplayBuffers[4]; //how many replays we can have at once, can probably increase

	HANDLE RecordingHandle;
	HANDLE PlaybackHandle;
	int input_recording_index;
	int input_playing_index;


	char exe_file_name[WIN32_STATE_FILE_NAME_COUNT];
	char *one_past_last_exe_file_name_slash;
};

struct win32_game_code
{
	HMODULE GameCodeDLL;
	FILETIME LastWriteTimeDLL;
	//function pointers, either of the callback can be 0! use if(game.UpdateAndRender){game.UpdateAndRender()} when calling 
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	bool32 game_code_is_loaded;
};


global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub; //static global variable value is 0
#define XInputGetState XInputGetState_

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

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory){
if(Memory){
	VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

//only for debugging!
//NOT for shipping! Blocking and write doesnt protect against lost data!
DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile){
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
				DEBUGPlatformFreeFileMemory(Thread, Result.Contents);Result.Contents = 0;
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile){
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


inline FILETIME Win32GetLastFileWriteTime(char *filename){

	FILETIME LastWriteTime = {};

	WIN32_FILE_ATTRIBUTE_DATA Data;
	if(GetFileAttributesExA(filename, GetFileExInfoStandard, &Data)){
		LastWriteTime = Data.ftLastWriteTime;
	}

	return LastWriteTime;
}

//ghetto string concatenation
internal void StringConcat(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest){
	for (int index = 0; index < SourceACount; ++index){
		*Dest++ = *SourceA++;
	}
	for (int index = 0; index < SourceBCount; ++index){
		*Dest++ = *SourceB++;
	}
	//TODO dest bounds checking
	//cc strings end with null terminator
	*Dest++ = 0;
}

internal int StringLength(char *String){
	int CharCount = 0;
	//if *String != 0 count, remember C strings are null terminated!
	while(*String++){
		++CharCount;
	}
	return CharCount;
}

internal void Win32GetEXEFileName(win32_state *Win32State){

//260 characters, never use max path can lead to bad results! might return truncated filepath
	DWORD size_of_file_name = GetModuleFileNameA(0, Win32State->exe_file_name, sizeof(Win32State->exe_file_name));
	Win32State->one_past_last_exe_file_name_slash = Win32State->exe_file_name;
	//file truncation
	for(char *Scan = Win32State->exe_file_name; *Scan; ++Scan){
		if (*Scan == '\\'){
			Win32State->one_past_last_exe_file_name_slash = Scan + 1;
		}
	}

}

internal void Win32BuildExePathFileName(win32_state *Win32State, char *filename, int DestCount, char *Dest){
	StringConcat(Win32State->one_past_last_exe_file_name_slash - Win32State->exe_file_name,Win32State->exe_file_name, StringLength(filename),  filename, DestCount, Dest);
}

internal win32_game_code Win32LoadGameCode(char *SourceDLLName, char *TempDLLName){

	win32_game_code Result = {};

	Result.LastWriteTimeDLL = Win32GetLastFileWriteTime(SourceDLLName);
	CopyFile(SourceDLLName , TempDLLName, FALSE);

	Result.GameCodeDLL = LoadLibraryA(TempDLLName);
	if(Result.GameCodeDLL){
		Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
		Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
	
		Result.game_code_is_loaded = (Result.UpdateAndRender && Result.GetSoundSamples);
	}

	if(!Result.game_code_is_loaded){
		Result.GetSoundSamples = 0;
		Result.UpdateAndRender = 0;
	}
	return Result;
}

internal void Win32UnloadGameCode(win32_game_code *GameCode){
	if(GameCode->GameCodeDLL){
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->game_code_is_loaded = false;
	GameCode->GetSoundSamples     = 0;
	GameCode->UpdateAndRender     = 0;
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
	int stretch_mode_width;
	int stretch_mode_height;
	//1 = image stretches to window, 0 = image doesn't stretch
	//TODO 1, image stretched to window makes mouse inputs inprecise
#if 0
	stretch_mode_width = WindowWidth;
	stretch_mode_height = WindowHeight;
#else
	stretch_mode_width = Buffer->Width;
	stretch_mode_height = Buffer->Height;
#endif
	StretchDIBits(
	DeviceContext,
	0,0, stretch_mode_width, stretch_mode_height,
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
	if(NewState->EndedDown != IsDown){
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}

internal real32 Win32ProcessXInputStickValue(SHORT Value, SHORT DeadZoneThreshold){
	real32 Result = 0;
	if 		 	(Value < -DeadZoneThreshold){Result = (real32)(Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);} 
		else if (Value > DeadZoneThreshold) {Result = (real32)(Value - DeadZoneThreshold) / (32768.0f - DeadZoneThreshold);}
	
	return Result;
}

internal void Win32GetInputFileLocation(win32_state *Win32State, bool32 input_stream, int slot_index, int DestCount, char *Dest){
	char Temp[64];
	wsprintfA(Temp, "loop_edit_%d_%s.hmi", slot_index, input_stream ? "input" : "state");
	Win32BuildExePathFileName(Win32State, Temp, DestCount, Dest);
}

internal win32_replay_buffer * Win32GetReplayBuffer(win32_state *Win32State, int unsigned index){
	
	Assert(index < ArrayCount(Win32State->ReplayBuffers));
	win32_replay_buffer *replay_buffer = &Win32State->ReplayBuffers[index];
	return replay_buffer;
}

internal void Win32BeginRecordingInput(win32_state *Win32State, int input_recording_index){
	win32_replay_buffer *replay_buffer = Win32GetReplayBuffer(Win32State, input_recording_index);
	if(replay_buffer->MemoryBlock){
		Win32State->input_recording_index = input_recording_index;

		char filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(Win32State, true, input_recording_index, sizeof(filename), filename);
		Win32State->RecordingHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
		LARGE_INTEGER file_position;
		file_position.QuadPart = Win32State->TotalSize;
		SetFilePointerEx(Win32State->RecordingHandle, file_position, 0, FILE_BEGIN);
#endif
		
		CopyMemory(replay_buffer->MemoryBlock, Win32State->GameMemoryBlock, Win32State->TotalSize);
	}
}

internal void Win32EndRecordingInput(win32_state *Win32State){
	CloseHandle(Win32State->RecordingHandle);
	Win32State->input_recording_index = 0;
}

internal void Win32BeginInputPlayback(win32_state *Win32State, int input_playing_index){
	win32_replay_buffer *replay_buffer = Win32GetReplayBuffer(Win32State, input_playing_index);
	if(replay_buffer->MemoryBlock){
		Win32State->input_playing_index =  input_playing_index;
				
		char filename[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(Win32State, true, input_playing_index, sizeof(filename), filename);
		Win32State->PlaybackHandle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
		LARGE_INTEGER file_position;
		file_position.QuadPart = Win32State->TotalSize;
		SetFilePointerEx(Win32State->PlaybackHandle, file_position, 0, FILE_BEGIN);
#endif
		CopyMemory(Win32State->GameMemoryBlock, replay_buffer->MemoryBlock, Win32State->TotalSize);
	}
}

internal void Win32EndInputPlayback(win32_state *Win32State){
	CloseHandle(Win32State->PlaybackHandle);
	Win32State->input_playing_index = 0;
}
internal void Win32RecordInput(win32_state *Win32State, game_input *input_to_record){
	DWORD BytesWritten;
	WriteFile(Win32State->RecordingHandle, input_to_record, sizeof(*input_to_record), &BytesWritten, 0);
}

internal void Win32PlaybackInput(win32_state *Win32State, game_input *input_to_playback){
	DWORD BytesRead = 0;

	if(ReadFile(Win32State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback), &BytesRead, 0)){
		if(BytesRead == 0)
		{
			//hit end of stream, go back to beginning
			int playing_index = Win32State->input_playing_index;
			Win32EndInputPlayback(Win32State);
			Win32BeginInputPlayback(Win32State, playing_index);
			ReadFile(Win32State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback), &BytesRead, 0);
		}
	}
}


internal void Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController){

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
				//since we are comparing WasDown to IsDown we must convert these bit tests to 0/1 values (bools);
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
					//loop mode
					else if (VKCode == 'L'){
						if(IsDown){
							if(Win32State->input_playing_index == 0){

								if(Win32State->input_recording_index == 0){
									Win32BeginRecordingInput(Win32State, 1);
								}
								else{
									Win32EndRecordingInput(Win32State);
									Win32BeginInputPlayback(Win32State, 1);
								}
							}
							else{
								Win32EndInputPlayback(Win32State);
							}
						}
					}


#if HANDMADE_INTERNAL
					else if (VKCode == 'P')        { if(IsDown){GlobalPause = !GlobalPause;}  }																	
#endif 
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

internal void Win32DebugDrawVertical(win32_offscreen_buffer *BackBuffer, int X, int top, int bottom, uint32 color){
	if(top <= 0){
		top = 0;
	}
	if (bottom > BackBuffer->Height){
		bottom = BackBuffer->Height;
	}
	if ((X >= 0 )&& (X < BackBuffer->Width)){
		uint8 *pixel = ((uint8 *)BackBuffer->Memory + X*BackBuffer->BytesPerPixel + top*BackBuffer->Pitch);
		for (int Y = top; Y < bottom; ++Y){
			*(uint32 *)pixel = color;
			pixel += BackBuffer->Pitch;
		}
	}
}

inline void Win32DrawSoundBufferMarker (win32_offscreen_buffer *BackBuffer, win32_sound_output *SoundOutput, real32 C, int pad_x, int top, int bottom, DWORD value_to_draw, uint32 color) {

	real32 XReal32 = (C * (real32)value_to_draw);
	int X =  pad_x + (int)XReal32;

	Win32DebugDrawVertical(BackBuffer, X, top, bottom, color);
}

//debugging for sound syncing, see ep20 for meaning of lines
//TODO move out of platform layer
internal void Win32DebugSyncDisplay(win32_offscreen_buffer *BackBuffer, int marker_count, win32_debug_time_marker *markers, int current_marker_index, win32_sound_output *SoundOutput, real32 target_seconds_elapsed_per_frame){
	
	int pad_x = 16;
	int pad_y = 16;
	int line_height = 64;

	real32 C = (real32)(BackBuffer->Width -2 * pad_x) / (real32)SoundOutput->SecondaryBufferSize;
	for(int marker_index = 0; marker_index < marker_count; ++marker_index){
		
		DWORD play_color  = 0xFFFFFFFF;
		DWORD write_color = 0xFFFF0000;
		DWORD play_window_color = 0xFFFF00FF;
		DWORD expected_flip_play_color = 0xFFFFFF00;
		int top    = pad_y;
		int bottom = pad_y + line_height;
		win32_debug_time_marker *this_marker = &markers[marker_index];
		Assert(this_marker->OutputPlayCursor  < SoundOutput->SecondaryBufferSize);
		Assert(this_marker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
		Assert(this_marker->FlipPlayCursor    < SoundOutput->SecondaryBufferSize);
		Assert(this_marker->FlipWriteCursor   < SoundOutput->SecondaryBufferSize);
		Assert(this_marker->OutputLocation    < SoundOutput->SecondaryBufferSize);
		Assert(this_marker->OutputByteCount   < SoundOutput->SecondaryBufferSize);

		if(marker_index == current_marker_index){

			top    += line_height + pad_y;
			bottom += line_height + pad_y;
			int first_top = top;
			Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->OutputPlayCursor,   play_color);
			Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->OutputWriteCursor,  write_color);
			
			top    += line_height + pad_y;
			bottom += line_height + pad_y;
			Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->OutputLocation,   play_color);
			Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->OutputLocation + this_marker->OutputByteCount,  write_color);
			
			top    += line_height + pad_y;
			bottom += line_height + pad_y;
			Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, first_top, bottom, this_marker->ExpectedFlipPlayCursor,  expected_flip_play_color);
		}

		Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->FlipPlayCursor,  play_color);
		Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->FlipWriteCursor,  write_color);
		Win32DrawSoundBufferMarker (BackBuffer, SoundOutput, C, pad_x, top, bottom, this_marker->FlipPlayCursor + 480 * SoundOutput->BytesPerSample,  play_window_color);
	}
	return;
}


int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR     CommandLine,
	int       ShowCode){
	
	AllocConsole();
	FILE *f;
	freopen_s(&f, "CONOUT$", "w", stdout);

	win32_state Win32State = {};

	Win32GetEXEFileName(&Win32State);
	
	//source game dll path
	char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	char SourceGameCodeDLLFilename[] = "handmade.dll";
	Win32BuildExePathFileName(&Win32State,SourceGameCodeDLLFilename, sizeof (SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

	//temp game dll path
	char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
	char TempGameCodeDLLFilename[]   = "handmade_temp.dll";
	Win32BuildExePathFileName(&Win32State,TempGameCodeDLLFilename,   sizeof (TempGameCodeDLLFullPath),   TempGameCodeDLLFullPath  );


	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	//Set the windows scheduler granularity to 1ms
	//so that sleep() can be more granular
	UINT desired_scheduler_ms = 1;
	bool32 sleep_is_granular  = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);
	
	Win32LoadXInpuT();

	WNDCLASSA WindowClass = {};
	//display size	
	Win32ReSizeDIBSection(&GlobalBackBuffer, 1280, 720);
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
 	// WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";


  	if(RegisterClassA(&WindowClass))
  	{
		HWND Window = CreateWindowExA(
//1 = Window stays on top while alt tabbing, 0 = Window minimizes when alt tabbing
#if 0
WS_EX_TOPMOST  
#else
 0
#endif
			, 
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
		if(Window)
		{
			HDC DeviceContext = GetDC(Window);
			//todo how to query this on windows, VSYNC?
			//hz = cycles per sec == frames per second
				int monitor_refresh_hz = 60;
				int Win32RefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
			//ReleaseDC(Window, DeviceContext);
			if (Win32RefreshRate > 1){
				monitor_refresh_hz = Win32RefreshRate;
			}
			//game_update_hz currently fixed to variable monitor_refresh_rate, should probably be fixed to a constant
			//(real32)(monitor_refresh_hz / 2)
			real32 game_update_hz = 30.0f;
			real32 target_seconds_per_frame = 1.0f / (real32)game_update_hz;


			win32_sound_output SoundOutput = {};
			//Make this like 60 seconds (so playcursor cant wrap on us)
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16)*2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
			//todo compute to see what lowest reasonable safetybyte value is
			SoundOutput.SafetyBytes =(DWORD)((int)((real32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) / game_update_hz / 3.0f));
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond , SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0,0, DSBPLAY_LOOPING);
		
			GlobalRunning = true;
#if 0
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


//sets memory baseaddress for allocation to be at 2TB
//this is possible because it is VIRTUAL memory address
//allows us to do looping and whatnot more consistently as pointers will allocate and point to the same places in memory, as all memory allocation will be relative to this baseaddress
//see handmade hero day 024 1:00:00 for extensive explanation on virtual memory
//also handmade hero day 024 1:12:00 for more explanation on connection to input recording
#if HANDMADE_INTERNAL 
LPVOID BaseAdress = (LPVOID)Terabytes((uint64)2);
#else
LPVOID BaseAdress = 0;
#endif

			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes((uint64)1);
			//initialising function pointers
			GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
			GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
			GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
			
			Win32State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			//todo use |MEM_LARGE_PAGES and call adjusttokenpriviledges to increase TLB efficiency, translation buffer
			Win32State.GameMemoryBlock = VirtualAlloc(BaseAdress, (size_t)Win32State.TotalSize,  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			//replay handling
			//TODO Fix first time starting recording lagspike
			for(int replay_index = 0; replay_index < ArrayCount(Win32State.ReplayBuffers); ++replay_index){

				win32_replay_buffer *replay_buffer = &Win32State.ReplayBuffers[replay_index];


			
				Win32GetInputFileLocation(&Win32State, false, replay_index, sizeof(replay_buffer->Filename), replay_buffer->Filename);

				replay_buffer->Filehandle = CreateFileA(replay_buffer->Filename, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);


				LARGE_INTEGER max_size;
				max_size.QuadPart = Win32State.TotalSize;
				replay_buffer->MemoryMap = CreateFileMappingA(replay_buffer->Filehandle, 0 , PAGE_READWRITE, max_size.HighPart, max_size.LowPart, 0);
				DWORD Error = GetLastError();

				replay_buffer->MemoryBlock = MapViewOfFile(replay_buffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);
				if(replay_buffer->MemoryBlock){
					
				}
				else{
					//TODO logging
				}
			}

			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input [2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];
				
				
				LARGE_INTEGER LastCounter   =  Win32GetWallClock();
				LARGE_INTEGER FlipWallClock =  Win32GetWallClock();
				
				int debug_time_marker_index  = 0;
				win32_debug_time_marker debug_time_markers[30] = {0};
				
				DWORD  last_play_cursor      = 0;
				DWORD  last_write_cursor     = 0;
				DWORD  audio_latency_bytes   = 0;
				real32 audio_latency_seconds = 0;
				bool32 SoundIsValid          = false;
				
				uint64 LastCycleCount = __rdtsc();
	
				win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
	
				while(GlobalRunning)
				{
					FILETIME newdllwritetime = Win32GetLastFileWriteTime(SourceGameCodeDLLFullPath);
					if(CompareFileTime (&newdllwritetime,&Game.LastWriteTimeDLL) != 0)
					{
						Win32UnloadGameCode(&Game);
						Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
					}
					//todo make zeroing macro
					//todo we cant zero everything because the up/down state will be wrong!!!!
					game_controller_input *OldKeyboardController = GetController(OldInput, 0);
					game_controller_input *NewKeyboardController = GetController(NewInput, 0);
					*NewKeyboardController = {};
					NewKeyboardController->IsConnected = true;
					for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex){
						NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
					}
					
					Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

					if (!GlobalPause)
					{	
						
						//todo make more robust mouse tracking and mousekey implementation
						POINT MouseP;
						GetCursorPos(&MouseP);
						ScreenToClient(Window, &MouseP);
						NewInput->MouseX = MouseP.x;
						NewInput->MouseY = MouseP.y;
						NewInput->MouseZ = 0;
						//todo support mousewheel
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], GetKeyState(VK_LBUTTON)  & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1], GetKeyState(VK_RBUTTON)  & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2], GetKeyState(VK_MBUTTON)  & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

						
				
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
								NewController->Analog = OldController->Analog;
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
						thread_context Thread = {};

						game_offscreen_buffer Buffer = {};
						Buffer.Memory = GlobalBackBuffer.Memory;
						Buffer.Width  = GlobalBackBuffer.Width;
						Buffer.Height = GlobalBackBuffer.Height;
						Buffer.Pitch  = GlobalBackBuffer.Pitch;
						Buffer.BytesPerPixel = GlobalBackBuffer.BytesPerPixel;

						if(Win32State.input_recording_index){
							Win32RecordInput (&Win32State, NewInput);
						}
						if(Win32State.input_playing_index){
							Win32PlaybackInput(&Win32State, NewInput);
						}

						NewInput->dtForFrame = target_seconds_per_frame;

						if(Game.UpdateAndRender){
							
							Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
						}
						
						/*
						Here is how sound output computation works

						We define a safety value that is the number of samples we think our game update loop may vary by. (lets say up to 2 ms)

						When we wake up to write audio, we will look and see what the playcursor position in and we will forecast ahead where we think the play cursor will be on the next frame boundary

						we will then look to see if the write cursor is before that by atleast our safety value. If it is, the target fill position is that frame boundary plus one frame. This gives us perfect audio sync in the case of a (sound)card that has low enough latency

						If the write cursor is _after_ that safety margin, then we assume we can never sync the audio perfectly, so we will write one frame's worth of audio plus the safety margin worth of guard samples (1ms or something determined safe, whatever we think the variability of our frame computation is)
						*/

						LARGE_INTEGER audio_wall_clock = Win32GetWallClock();
						real32 from_begin_to_audio_seconds = 1000.0f * Win32GetSecondsElapsed(FlipWallClock, audio_wall_clock);

						DWORD PlayCursor;
						DWORD WriteCursor;
						if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
						{
							if(!SoundIsValid){
								SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
								SoundIsValid = true;
							}
							DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize);
							DWORD expected_sound_bytes_per_frame = (int)((real32)(SoundOutput.SamplesPerSecond *SoundOutput.BytesPerSample) /game_update_hz);
							real32 seconds_left_until_flip       = (target_seconds_per_frame - from_begin_to_audio_seconds);
							DWORD expected_bytes_until_flip      = (DWORD)((seconds_left_until_flip / target_seconds_per_frame) 
																   * (real32)expected_sound_bytes_per_frame);
							DWORD expected_frame_boundary_byte   = PlayCursor + expected_bytes_until_flip;

							DWORD safe_write_cursor              = WriteCursor;
							if(safe_write_cursor < PlayCursor)
							{
								safe_write_cursor += SoundOutput.SecondaryBufferSize;
							}
							Assert(safe_write_cursor >= PlayCursor);

							safe_write_cursor += SoundOutput.SafetyBytes;
							bool32 audio_card_is_low_latency = (safe_write_cursor < expected_frame_boundary_byte);
							//audio_card_is_low_latency is used to check whether we can perfect audiosync or not
							DWORD TargetCursor = 0;
							if(audio_card_is_low_latency)
							{
								TargetCursor = (expected_frame_boundary_byte + expected_sound_bytes_per_frame);
							}
							else
							{
								TargetCursor = (WriteCursor + expected_sound_bytes_per_frame + SoundOutput.SafetyBytes);
							}
							TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);
							
							DWORD BytesToWrite = 0;
							if(ByteToLock > TargetCursor)
							{	
								BytesToWrite  = (SoundOutput.SecondaryBufferSize - ByteToLock);
								BytesToWrite += TargetCursor;
							}
							else
							{
								BytesToWrite = TargetCursor - ByteToLock;
							}

								
							game_sound_output_buffer SoundBuffer = {};
							SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
							SoundBuffer.SampleCount      = BytesToWrite / SoundOutput.BytesPerSample;
							SoundBuffer.Samples          = Samples;
							if(Game.GetSoundSamples){
								Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
							}
						
#if HANDMADE_INTERNAL
							
							win32_debug_time_marker *Marker = &debug_time_markers[debug_time_marker_index];
							Marker->OutputPlayCursor       = PlayCursor;
							Marker->OutputWriteCursor      = WriteCursor;
							Marker->OutputLocation         = ByteToLock;
							Marker->OutputByteCount        = BytesToWrite;
							Marker->ExpectedFlipPlayCursor = expected_frame_boundary_byte;
							DWORD unwrapped_write_cursor = WriteCursor;
							if(unwrapped_write_cursor < PlayCursor){
								unwrapped_write_cursor += SoundOutput.SecondaryBufferSize;
							}
							audio_latency_bytes = unwrapped_write_cursor - PlayCursor;
							audio_latency_seconds = (((real32)audio_latency_bytes / (real32)SoundOutput.BytesPerSample) / (real32)SoundOutput.SamplesPerSecond);
							
#if 0		
							char sound_text_buffer2[256];
							_snprintf_s(sound_text_buffer2, sizeof(sound_text_buffer2), "BTL: %u TC:%u BTW: %u PC: %u WC:%u DELTA: %u (%fs)\n", ByteToLock, TargetCursor, BytesToWrite, PlayCursor, WriteCursor, audio_latency_bytes, audio_latency_seconds);
							OutputDebugStringA(sound_text_buffer2);
#endif			
#endif
							Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
						}
						else{
							SoundIsValid = false;
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
						//TODO current is wrong on the zero'th index for debug_time_marker_index - 1
#if 0
						Win32DebugSyncDisplay(&GlobalBackBuffer, ArrayCount(debug_time_markers), debug_time_markers, debug_time_marker_index - 1, &SoundOutput, target_seconds_per_frame);
#endif
#endif
						Win32DisplayBufferInWindow(&GlobalBackBuffer,DeviceContext, Dimension.Width, Dimension.Height);
					
						FlipWallClock = Win32GetWallClock();
						
#if HANDMADE_INTERNAL
						//debug code to find out where DirectSound think it is
						DWORD DebugPlayCursor;
						DWORD DebugWriteCursor;
						if(GlobalSecondaryBuffer->GetCurrentPosition(&DebugPlayCursor, &DebugWriteCursor) == DS_OK)
						{
							Assert(debug_time_marker_index < ArrayCount(debug_time_markers))
							win32_debug_time_marker *Marker = &debug_time_markers[debug_time_marker_index];
							Marker->FlipPlayCursor = DebugPlayCursor;
							Marker->FlipWriteCursor = DebugWriteCursor;
						}
#endif

						game_input *Temp = NewInput;
						NewInput = OldInput;
						OldInput = Temp;
					
#if 0 
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
#endif
#if HANDMADE_INTERNAL
						++debug_time_marker_index;
						if (debug_time_marker_index == ArrayCount(debug_time_markers)){
							debug_time_marker_index = 0;
						}
#endif
					}

				
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
  }
  else
  {
	//todo logging
  };
  return(0);
}
