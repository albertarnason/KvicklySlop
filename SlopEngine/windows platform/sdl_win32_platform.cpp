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
	int    InputRecordingIndex;
	int    InputPlayingIndex;

	char  EXEFileName[PLATFORM_STATE_FILE_NAME_COUNT];
	char *OnePastLastEXEFileNameSlash;
};

struct platform_game_code
{
	HMODULE GameCodeDLL;
	FILETIME LastWriteTimeDLL;
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	bool32 IsValid;
};

global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable int64 GlobalPerfCountFrequency; // PORT: replace usage sites with SDL_GetPerformanceFrequency() - kept here only if you want a cached copy


// ---------------------------------------------------------------------------
// exe path / file name helpers
// PORT: StringConcat / StringLength / Win32GetEXEFileName / Win32BuildExePathFileName
//       body over almost unchanged, just renamed Platform*
// ---------------------------------------------------------------------------

internal void
StringConcat(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB,
			 size_t DestCount, char *Dest)
{
	// PORT: unchanged from Win32 version
}

internal int
StringLength(char *String)
{
	// PORT: unchanged from Win32 version
	return 0;
}

internal void
Win32GetEXEFileName(platform_state *State)
{
	// PORT: GetModuleFileNameA(0, State->EXEFileName, sizeof(State->EXEFileName));
	// then scan for last '\\' same as before.
	// NOTE: on the Linux side this becomes readlink("/proc/self/exe", ...)
	// and does NOT null-terminate - that's Linux-only, doesn't affect this file.
}

internal void
Win32BuildEXEPathFileName(platform_state *State, char *FileName, int DestCount, char *Dest)
{
	// PORT: unchanged from Win32 version (StringConcat call)
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
	// PORT: GetFileAttributesExA(FileName, GetFileExInfoStandard, &Data) -> LastWriteTime
	return LastWriteTime;
}

internal platform_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName)
{
	platform_game_code Result = {};
	// PORT: CopyFile + LoadLibraryA + GetProcAddress("GameUpdateAndRender")
	//       + GetProcAddress("GameGetSoundSamples"), same as Win32LoadGameCode.
	return Result;
}

internal void
Win32UnloadGameCode(platform_game_code *GameCode)
{
	// PORT: FreeLibrary + zero out function pointers
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
	// PORT: return VirtualAlloc(BaseAddress, (size_t)Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	return 0;
}

internal void
PlatformDeallocateMemory(void *Memory)
{
	// PORT: if (Memory) VirtualFree(Memory, 0, MEM_RELEASE);
}


// ---------------------------------------------------------------------------
// Debug read/write entire file
// PORT: DEBUGPlatformReadEntireFile / DEBUGPlatformWriteEntireFile /
//       DEBUGPlatformFreeFileMemory bodies unchanged - CreateFileA/ReadFile/
//       WriteFile/GetFileSizeEx are all still valid, SDL doesn't touch this.
// ---------------------------------------------------------------------------

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
	// PORT: VirtualFree(Memory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
	debug_read_file_result Result = {};
	// PORT: unchanged body from win32_handmade.cpp
	return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
	bool32 Result = false;
	// PORT: unchanged body from win32_handmade.cpp
	return Result;
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
Win32GetInputFileLocation(platform_state *State, bool32 InputStream, int SlotIndex,
						   int DestCount, char *Dest)
{
	// PORT: wsprintfA(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	//       then Win32BuildEXEPathFileName
}

internal platform_replay_buffer *
Win32GetReplayBuffer(platform_state *State, unsigned int Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	return &State->ReplayBuffers[Index];
}

internal void
Win32BeginRecordingInput(platform_state *State, int InputRecordingIndex)
{
	// PORT: unchanged body
}

internal void
Win32EndRecordingInput(platform_state *State)
{
	// PORT: unchanged body
}

internal void
Win32BeginInputPlayback(platform_state *State, int InputPlayingIndex)
{
	// PORT: unchanged body
}

internal void
Win32EndInputPlayback(platform_state *State)
{
	// PORT: unchanged body
}

internal void
Win32RecordInput(platform_state *State, game_input *InputToRecord)
{
	// PORT: unchanged body (WriteFile)
}

internal void
Win32PlaybackInput(platform_state *State, game_input *InputToPlayback)
{
	// PORT: unchanged body (ReadFile + loop-back-to-start on 0 bytes read)
}


// ---------------------------------------------------------------------------
// Scheduler granularity (Windows-only, no Linux equivalent needed)
// ---------------------------------------------------------------------------

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


// ---------------------------------------------------------------------------
// Debug console (Windows-only, no Linux equivalent needed - terminal already
// there when launched from one)
// ---------------------------------------------------------------------------

#if HANDMADE_INTERNAL
internal void
Win32AllocDebugConsole(void)
{
	AllocConsole();
	FILE *File;
	freopen_s(&File, "CONOUT$", "w", stdout);
}
#endif


// ---------------------------------------------------------------------------
// SDL setup helpers
// NEW: these have no Win32 equivalent - the WNDCLASSA/CreateWindowExA/
//      Win32MainWindowCallback/DispatchMessageA machinery is entirely
//      replaced by these three calls.
// ---------------------------------------------------------------------------

internal SDL_Window *
PlatformCreateWindow(const char *Title, int Width, int Height)
{
	// NEW: SDL_CreateWindow(Title, Width, Height, SDL_WINDOW_RESIZABLE);
	// NOTE: SDL3 dropped the separate x/y params from SDL2's SDL_CreateWindow -
	// positioning is done via SDL_SetWindowPosition if needed after creation.
	return 0;
}

internal void
PlatformProcessPendingEvents(platform_state *State, game_controller_input *KeyboardController)
{
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
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
	{
		// TODO: log SDL_GetError()
		return 1;
	}

	SDL_Window *Window = PlatformCreateWindow("HandmadeHero", 1280, 720);
	if (!Window)
	{
		// TODO: log SDL_GetError()
		return 1;
	}

	// NEW: renderer + streaming texture replaces win32_offscreen_buffer +
	// StretchDIBits. Create once here, recreate texture on resize.
	SDL_Renderer *Renderer = SDL_CreateRenderer(Window, 0);
	// TODO: SDL_Texture *BackBufferTexture = SDL_CreateTexture(Renderer,
	//           SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, Width, Height);
	// Keep your existing game_offscreen_buffer.Memory as a plain malloc'd
	// buffer that your renderer writes into untouched, then each frame:
	//   SDL_UpdateTexture(BackBufferTexture, 0, Buffer.Memory, Buffer.Pitch);
	//   SDL_RenderTexture(Renderer, BackBufferTexture, 0, 0);
	//   SDL_RenderPresent(Renderer);

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

	uint64 TotalSize = 0; // TODO: PermanentStorageSize + TransientStorageSize, same as GameMemory setup
	void *BaseAddress = 0;
#if HANDMADE_INTERNAL
	BaseAddress = (void *)Terabytes((uint64)2);
#endif
	State.GameMemoryBlock = PlatformAllocateMemory(BaseAddress, TotalSize);
	State.TotalSize = TotalSize;

	// TODO: replay buffer setup loop (CreateFileMappingA/MapViewOfFile),
	// ported from the ArrayCount(State.ReplayBuffers) loop in WinMain.

	platform_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

	game_input Input[2] = {};
	game_input *NewInput = &Input[0];
	game_input *OldInput = &Input[1];

	real32 GameUpdateHz = 30.0f;
	real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

	uint64 LastCounter = SDL_GetPerformanceCounter();

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
			game_offscreen_buffer Buffer = {};
			// TODO: fill Buffer.Memory/Width/Height/Pitch/BytesPerPixel from
			// your own backing buffer (see texture note above)

			if (State.InputRecordingIndex) { Win32RecordInput(&State, NewInput); }
			if (State.InputPlayingIndex)   { Win32PlaybackInput(&State, NewInput); }

			NewInput->dtForFrame = TargetSecondsPerFrame;

			if (Game.UpdateAndRender)
			{
				Game.UpdateAndRender(&Thread, 0 /* TODO: &GameMemory */, NewInput, &Buffer);
			}

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