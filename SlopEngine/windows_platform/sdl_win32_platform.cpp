/*
	sdl_win32_platform.cpp

	SDL3 platform layer - WINDOWS specific compilation unit.

	This is a SKELETON. Function bodies marked with PORT: are where you move
	logic over from win32_handmade.cpp. Function bodies marked with NEW: are
	new for the SDL model and have no 1:1 Win32 equivalent - write these from
	scratch (guidance in comments).

	Build alongside sdl_platform_shared.cpp (event loop / main loop, shared
	between win32 and linux) and sdl_linux_platform.cpp (not this file).
	This file should only contain things SDL does NOT abstract:
		- exe path
		- virtual memory reservation
		- memory-mapped replay files
		- raw file IO
		- file last-write-time (DLL hot reload)
		- DLL load/unload
		- scheduler granularity (timeBeginPeriod)
		- win32 console allocation for debug builds

	TODO before this compiles:
	- confirm SDL3 is actually the API you're linking (SDL_Gamepad, not
	  SDL_GameController - SDL3 renamed the controller API)
	- fill in handmade.h include path
	- decide int types (uint8/16/32/64, real32/64) - assumed same as your
	  existing handmade.h typedefs
*/

#define SDL__MAIN_USE_CALLBACKS 0
#include <SDL3/SDL.h>
#include "handmade.h"

#include <windows.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Platform-specific state (Windows side of the split)
// ---------------------------------------------------------------------------

#define PLATFORM_STATE_FILE_NAME_COUNT MAX_PATH

struct platform_replay_buffer
{
	HANDLE FileHandle;
	HANDLE MemoryMap;
	char   Filename[PLATFORM_STATE_FILE_NAME_COUNT];
	void  *MemoryBlock;
};

struct platform_state
{
	uint64 TotalSize;
	void  *GameMemoryBlock;
	platform_replay_buffer ReplayBuffers[4];

	HANDLE RecordingHandle;
	HANDLE PlaybackHandle;
	int input_recording_index;
	int input_playing_index;

	char exe_file_name[PLATFORM_STATE_FILE_NAME_COUNT];
	char *one_past_last_exe_file_name_slash;
};

struct platform_game_code
{
	HMODULE GameCodeDLL;
	FILETIME LastWriteTimeDLL;
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	bool32 game_code_is_loaded;
};

global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable int64 GlobalPerfCountFrequency; // PORT: replace usage sites with SDL_GetPerformanceFrequency() - kept here only if you want a cached copy

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

internal void
Win32GetEXEFileName(platform_state *State)
{

	//260 characters, never use max path can lead to bad results! might return truncated filepath
	DWORD size_of_file_name = GetModuleFileNameA(0, State->exe_file_name, sizeof(State->exe_file_name));
	State->one_past_last_exe_file_name_slash = State->exe_file_name;
	//file truncation
	for(char *Scan = State->exe_file_name; *Scan; ++Scan){
		if (*Scan == '\\'){
			State->one_past_last_exe_file_name_slash = Scan + 1;
		}
	}

	// NOTE: on the Linux side this becomes readlink("/proc/self/exe", ...)
	// and does NOT null-terminate - that's Linux-only, doesn't affect this file.
}

internal void
Win32BuildEXEPathFileName(platform_state *State, char *FileName, int DestCount, char *Dest)
{
		StringConcat(State->one_past_last_exe_file_name_slash - State->exe_file_name,State->exe_file_name, StringLength(FileName),  FileName, DestCount, Dest);
}


// ---------------------------------------------------------------------------
// File last-write-time + DLL hot reload
// PORT: Win32GetLastFileWriteTime unchanged.
// PORT: Win32LoadGameCode / Win32UnloadGameCode unchanged (LoadLibraryA/
//       GetProcAddress/FreeLibrary all still valid Win32 calls under SDL).
//       Optionally replace with SDL_LoadObject/SDL_LoadFunction/SDL_UnloadObject
//       if you want fewer #ifdefs later - functionally equivalent here.
// ---------------------------------------------------------------------------

inline FILETIME
Win32GetLastFileWriteTime(char *FileName)
{
	FILETIME LastWriteTime = {};
		WIN32_FILE_ATTRIBUTE_DATA Data;
	if(GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data)){
		LastWriteTime = Data.ftLastWriteTime;
	}
	return LastWriteTime;
}

internal platform_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	platform_game_code Result = {};
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

internal void
Win32UnloadGameCode(platform_game_code *GameCode)
{
	if(GameCode->GameCodeDLL){
		FreeLibrary(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->game_code_is_loaded = false;
	GameCode->GetSoundSamples     = 0;
	GameCode->UpdateAndRender     = 0;
}


// ---------------------------------------------------------------------------
// Virtual memory reservation
// PORT: VirtualAlloc/VirtualFree call sites for GameMemoryBlock and any
//       debug base-address (Terabytes(2)) trick carry over unchanged - this
//       is Windows-only code, no SDL equivalent exists.
// ---------------------------------------------------------------------------

internal void *
PlatformAllocateMemory(void *BaseAddress, uint64 Size)
{
	return VirtualAlloc(BaseAddress, (size_t)Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void
PlatformDeallocateMemory(void *Memory)
{
 	if (Memory) VirtualFree(Memory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory){
if(Memory){
	VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

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


// ---------------------------------------------------------------------------
// Memory-mapped replay/loop-recording buffers
// PORT: Win32GetInputFileLocation / Win32GetReplayBuffer unchanged (just
//       renamed win32_state -> platform_state, win32_replay_buffer ->
//       platform_replay_buffer).
// PORT: replay buffer setup (CreateFileMappingA / MapViewOfFile) unchanged -
//       this is the exact code that had the max_size_high/low bug, so
//       double check that HighPart/LowPart assignment when you port it.
// ---------------------------------------------------------------------------

internal void
Win32GetInputFileLocation(platform_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest)
{
		char Temp[64];
	wsprintfA(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	Win32BuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal platform_replay_buffer *
Win32GetReplayBuffer(platform_state *State, unsigned int Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	return &State->ReplayBuffers[Index];
}

internal void Win32BeginRecordingInput(platform_state *State, int input_recording_index){
	platform_replay_buffer *replay_buffer = Win32GetReplayBuffer(State, input_recording_index);
	if(replay_buffer->MemoryBlock){
		State->input_recording_index = input_recording_index;

		char filename[PLATFORM_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, input_recording_index, sizeof(filename), filename);
		State->RecordingHandle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
		LARGE_INTEGER file_position;
		file_position.QuadPart = State->TotalSize;
		SetFilePointerEx(State->RecordingHandle, file_position, 0, FILE_BEGIN);
#endif
		
		CopyMemory(replay_buffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}


internal void Win32EndRecordingInput(platform_state *State){
	CloseHandle(State->RecordingHandle);
	State->input_recording_index = 0;
}

internal void Win32BeginInputPlayback(platform_state *State, int input_playing_index){
	platform_replay_buffer *replay_buffer = Win32GetReplayBuffer(State, input_playing_index);
	if(replay_buffer->MemoryBlock){
		State->input_playing_index =  input_playing_index;
				
		char filename[PLATFORM_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(State, true, input_playing_index, sizeof(filename), filename);
		State->PlaybackHandle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 1
		LARGE_INTEGER file_position;
		file_position.QuadPart = State->TotalSize;
		SetFilePointerEx(State->PlaybackHandle, file_position, 0, FILE_BEGIN);
#endif
		CopyMemory(State->GameMemoryBlock, replay_buffer->MemoryBlock, State->TotalSize);
	}
}

internal void Win32EndInputPlayback(platform_state *State){
	CloseHandle(State->PlaybackHandle);
	State->input_playing_index = 0;
}

internal void Win32RecordInput(platform_state *State, game_input *input_to_record){
	DWORD BytesWritten;
	WriteFile(State->RecordingHandle, input_to_record, sizeof(*input_to_record), &BytesWritten, 0);
}

internal void Win32PlaybackInput(platform_state *State, game_input *input_to_playback){
	DWORD BytesRead = 0;

	if(ReadFile(State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback), &BytesRead, 0)){
		if(BytesRead == 0)
		{
			//hit end of stream, go back to beginning
			int playing_index = State->input_playing_index;
			Win32EndInputPlayback(State);
			Win32BeginInputPlayback(State, playing_index);
			ReadFile(State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback), &BytesRead, 0);
		}
	}
}



// Scheduler granularity (Windows-only, no Linux equivalent needed)
internal bool32
Win32SetSchedulerGranularity(void)
{
	UINT DesiredSchedulerMS = 1;
	bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
	return SleepIsGranular;
}

internal void
Win32ClearSchedulerGranularity(void)
{
	timeEndPeriod(1);
}


// Scheduler granularity (Windows-only, no Linux equivalent needed)
#if HANDMADE_INTERNAL
internal void
Win32AllocDebugConsole(void)
{
	AllocConsole();
	FILE *File;
	freopen_s(&File, "CONOUT$", "w", stdout);
}
#endif


internal SDL_Window *
PlatformCreateWindow(const char *Title, int Width, int Height)
{
	return SDL_CreateWindow(Title, Width, Height, SDL_WINDOW_RESIZABLE);
}

	// NEW: replaces Win32ProcessPendingMessages. Loop SDL_PollEvent, switch on
	// event.type:
	//   SDL_EVENT_QUIT                -> GlobalRunning = false;
	//   SDL_EVENT_KEY_DOWN / KEY_UP   -> use event.key.scancode (SDL_Scancode,
	//                                    not keysym.sym - SDL3 flattened this),
	//                                    call your existing
	//                                    Win32ProcessKeyboardMessage(&Controller->X, IsDown)
	//                                    per bound scancode.
	//                                    event.key.repeat tells you WasDown-equivalent
	//                                    if you need to suppress repeats.
	//   SDL_EVENT_WINDOW_RESIZED      -> trigger backbuffer/texture recreation
	//   SDL_EVENT_WINDOW_FOCUS_GAINED/LOST -> WM_ACTIVATEAPP equivalent
	//
	// Your 'L' loop-recording toggle and internal-build 'P' pause toggle
	// (VKCode == 'L' / 'P' in the original) port over as
	// SDL_SCANCODE_L / SDL_SCANCODE_P checks in this same switch.

internal void
PlatformProcessPendingEvents(platform_state *State, game_controller_input *KeyboardController)
{
	SDL_Event Event;
	while (SDL_PollEvent(&Event))
	{
		switch (Event.type)
		{
			case SDL_EVENT_QUIT:
			{
				GlobalRunning = false;
			} break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			{
				bool32 IsDown = (Event.type == SDL_EVENT_KEY_DOWN);
				bool32 WasDown = (Event.key.repeat != 0);

				// TODO: map Event.key.scancode to KeyboardController buttons,
				// e.g.:
				// if (Event.key.scancode == SDL_SCANCODE_W)
				//     Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);

				if (Event.key.scancode == SDL_SCANCODE_ESCAPE && IsDown)
				{
					GlobalRunning = false;
				}
			} break;

			case SDL_EVENT_WINDOW_RESIZED:
			{
				// TODO: reallocate BackBufferMemory / recreate BackBufferTexture
			} break;
		}
	}
}



// ---------------------------------------------------------------------------
// Main entry point
// PORT: this replaces WinMain. Keep it as a normal main() (SDL3 does not
// require WinMain, and SDL_main handles the Windows subsystem entry point
// for you if you #include <SDL3/SDL_main.h> and link SDL3::SDL3-static or
// use the SDL_MAIN_USE_CALLBACKS approach - pick one, don't mix).
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	platform_state State = {};
	Win32GetEXEFileName(&State);

	char SourceGameCodeDLLFullPath[PLATFORM_STATE_FILE_NAME_COUNT];
	char SourceGameCodeDLLFileName[] = "handmade.dll";
	Win32BuildEXEPathFileName(&State, SourceGameCodeDLLFileName,
							   sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

	char TempGameCodeDLLFullPath[PLATFORM_STATE_FILE_NAME_COUNT];
	char TempGameCodeDLLFileName[] = "handmade_temp.dll";
	Win32BuildEXEPathFileName(&State, TempGameCodeDLLFileName,
							   sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

#if HANDMADE_INTERNAL
	Win32AllocDebugConsole();
#endif

	GlobalPerfCountFrequency = SDL_GetPerformanceFrequency();
	bool32 SleepIsGranular = Win32SetSchedulerGranularity();

	// NEW: SDL_Init replaces WNDCLASSA registration + XInput/DirectSound loading.
	// Use SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD.

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD);
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
	{
		// TODO: log SDL_GetError()
		return 1;
	}

	int window_width  = 1280;
	int window_height =  720;
	SDL_Window *Window = PlatformCreateWindow("HandmadeHero", window_width, window_height);
	if (!Window)
	{
		// TODO: log SDL_GetError()
		return 1;
	}

	// NEW: renderer + streaming texture replaces win32_offscreen_buffer +
	// StretchDIBits. Create once here, recreate texture on resize.
	SDL_Renderer *Renderer = SDL_CreateRenderer(Window, 0);
	SDL_Texture *BackBufferTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
	// Keep your existing game_offscreen_buffer.Memory as a plain malloc'd
	// buffer that your renderer writes into untouched, then each frame:


	// NEW: audio device setup replaces Win32InitDSound. SDL3 audio is stream-
	// based: SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
	// &spec, callback, userdata) or SDL_PutAudioStreamData if you want to push
	// samples explicitly each frame like your current DirectSound loop does.
	// This does NOT port 1:1 - the PlayCursor/WriteCursor prediction math in
	// your current WinMain (audio_card_is_low_latency etc.) has no equivalent
	// concept in SDL's audio stream model and should be redesigned, not moved.

	// NEW: gamepad open replaces Win32LoadXInpuT. Enumerate with
	// SDL_GetGamepads() / SDL_OpenGamepad() instead of a fixed XUSER_MAX_COUNT
	// loop over XInputGetState.

	game_memory GameMemory = {};
	GameMemory.PermanentStorageSize = Megabytes(64);
	GameMemory.TransientStorageSize = Gigabytes((uint64)1);		
	GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
	GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
	GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

	State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
#if HANDMADE_INTERNAL
LPVOID BaseAddress = (void *)Terabytes((uint64)2);
#else
LPVOID BaseAdress = 0;
#endif
	State.GameMemoryBlock = PlatformAllocateMemory(BaseAddress, State.TotalSize);
	GameMemory.PermanentStorage = State.GameMemoryBlock;
	GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

	// TODO: replay buffer setup loop (CreateFileMappingA/MapViewOfFile),
	// ported from the ArrayCount(State.ReplayBuffers) loop in WinMain.

	platform_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

	game_input Input[2] = {};
	game_input *NewInput = &Input[0];
	game_input *OldInput = &Input[1];

	real32 GameUpdateHz = 30.0f;
	real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

	uint64 LastCounter = SDL_GetPerformanceCounter();

	int BytesPerPixel = 4;
	void *BackBufferMemory = PlatformAllocateMemory(0, (uint64)(window_width * window_height * BytesPerPixel));
	//TODO: Needs to be reallocated if SDL_EVENT_WINDOW_RESIZED activates (resizing of window)
	//extract into function that can be called on event

	game_offscreen_buffer Buffer = {};
	Buffer.Memory        = BackBufferMemory;
	Buffer.Width         = window_width;
	Buffer.Height        = window_height;
	Buffer.Pitch         = window_width * BytesPerPixel;
	Buffer.BytesPerPixel = BytesPerPixel;
	
	GlobalRunning = true;
	while (GlobalRunning)
	{
		// PORT: DLL hot-reload check (CompareFileTime) - unchanged logic

		game_controller_input *OldKeyboardController = GetController(OldInput, 0);
		game_controller_input *NewKeyboardController = GetController(NewInput, 0);
		*NewKeyboardController = {};
		NewKeyboardController->IsConnected = true;
		for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
		{
			NewKeyboardController->Buttons[ButtonIndex].EndedDown =
				OldKeyboardController->Buttons[ButtonIndex].EndedDown;
		}

		PlatformProcessPendingEvents(&State, NewKeyboardController);

		if (!GlobalPause)
		{
			// NEW: mouse state via SDL_GetMouseState(&x, &y) - already window-
			// relative, no ScreenToClient step needed.
			float MouseX, MouseY;
			SDL_MouseButtonFlags MouseButtons = SDL_GetMouseState(&MouseX, &MouseY);
			NewInput->MouseX = (int)MouseX;
			NewInput->MouseY = (int)MouseY;
			NewInput->MouseZ = 0;
			// TODO: Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0], MouseButtons & SDL_BUTTON_LMASK); etc.

			// TODO: gamepad polling loop using SDL_GetGamepadAxis /
			// SDL_GetGamepadButton, feeding your existing
			// Win32ProcessXInputStickValue / Win32ProcessXInputDigitalButton
			// (rename Win32Process* if you want, logic is unchanged)

			thread_context Thread = {};

			if (State.input_recording_index) { Win32RecordInput(&State, NewInput); }
			if (State.input_playing_index)   { Win32PlaybackInput(&State, NewInput); }

			NewInput->dtForFrame = TargetSecondsPerFrame;

			if (Game.UpdateAndRender)
			{
				Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
			}
			SDL_UpdateTexture(BackBufferTexture, 0, Buffer.Memory, Buffer.Pitch);
			SDL_RenderTexture(Renderer, BackBufferTexture, 0, 0);
			SDL_RenderPresent(Renderer);

			// TODO: sound - fill an SDL audio stream/queue from
			// Game.GetSoundSamples output, redesigned per audio notes above

			// TODO: frame pacing - SDL_Delay for coarse wait + spin-wait tail,
			// same two-stage approach as your current Sleep()+busy-wait loop,
			// just swap Win32GetSecondsElapsed for SDL_GetPerformanceCounter-based calc

			// TODO: present - SDL_UpdateTexture + SDL_RenderTexture + SDL_RenderPresent

			game_input *Temp = NewInput;
			NewInput = OldInput;
			OldInput = Temp;
		}
	}

	Win32ClearSchedulerGranularity();
	Win32UnloadGameCode(&Game);
	SDL_Quit();

	return 0;
}