#define SDL__MAIN_USE_CALLBACKS 0
#include <SDL3/SDL.h>
#include "handmade.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

#include <stdio.h>

//max path
#define PLATFORM_STATE_FILE_NAME_COUNT 260

struct platform_replay_buffer
{
	void* FileHandle;
	void* MemoryMap;
	char   Filename[PLATFORM_STATE_FILE_NAME_COUNT];
	void  *MemoryBlock;
};

struct platform_state
{
	uint64 TotalSize;
	void  *GameMemoryBlock;
	platform_replay_buffer ReplayBuffers[4];

	SDL_IOStream *RecordingHandle;
	SDL_IOStream *PlaybackHandle;
	int input_recording_index;
	int input_playing_index;

	char exe_file_name[PLATFORM_STATE_FILE_NAME_COUNT];
	char *one_past_last_exe_file_name_slash;
};

struct platform_game_code
{
	SDL_SharedObject *GameCodeDLL;
	SDL_Time LastWriteTimeDLL;
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	bool32 game_code_is_loaded;
};

global_variable bool GlobalRunning;
global_variable bool GlobalPause;
global_variable uint64 GlobalPerfCountFrequency;

//ghetto string concatenation
internal void StringConcat(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest){
	for (size_t index = 0; index < SourceACount; ++index){
		*Dest++ = *SourceA++;
	}
	for (size_t index = 0; index < SourceBCount; ++index){
		*Dest++ = *SourceB++;
	}
	//TODO dest bounds checking
	//cc strings end with null terminator
	*Dest++ = 0;
}
internal size_t StringLength(const char *String){
	size_t CharCount = 0;
	//if *String != 0 count, remember C strings are null terminated!
	while(*String++){
		++CharCount;
	}
	return CharCount;
}

internal void
StringCopy(size_t SourceCount, const char *Source, size_t DestCount, char *Dest)
{
	// Assert instead of silently truncating
	// so oversized paths get caught immediately in debug builds
	Assert(SourceCount < DestCount);

	for(size_t Index = 0; Index < SourceCount; ++Index)
	{
		Dest[Index] = Source[Index];
	}

	Dest[SourceCount] = 0;
}



internal void
SDLGetEXEFileName(platform_state *State){
	const char *base_path = SDL_GetBasePath();
	
	StringCopy(StringLength(base_path), base_path, sizeof(State->exe_file_name), State->exe_file_name);

		State->one_past_last_exe_file_name_slash = State->exe_file_name + StringLength(base_path);

}

internal void
SDLBuildEXEPathFileName(platform_state *State, char *FileName, int DestCount, char *Dest)
{
		StringConcat((size_t)(State->one_past_last_exe_file_name_slash - State->exe_file_name),State->exe_file_name, StringLength(FileName),  FileName, (size_t)DestCount, Dest);
}

internal SDL_Time
SDLGetLastFileWriteTime(char *FileName)
{
	SDL_Time LastWriteTime = 0;

	SDL_PathInfo PathInfo;
	if(SDL_GetPathInfo(FileName, &PathInfo))
	{
		LastWriteTime = PathInfo.modify_time;
	}

	return LastWriteTime;
}

internal platform_game_code SDLLoadGameCode (char *SourceDLLName, char *TempDLLName){
	platform_game_code Result = {};
	Result.LastWriteTimeDLL = SDLGetLastFileWriteTime(SourceDLLName);
		SDL_CopyFile(SourceDLLName, TempDLLName);

		Result.GameCodeDLL = SDL_LoadObject(TempDLLName);
	if(Result.GameCodeDLL){
		Result.GetSoundSamples = (game_get_sound_samples *)SDL_LoadFunction(Result.GameCodeDLL, "GameGetSoundSamples");
		Result.UpdateAndRender = (game_update_and_render *)SDL_LoadFunction(Result.GameCodeDLL, "GameUpdateAndRender");
	
		Result.game_code_is_loaded = (Result.UpdateAndRender && Result.GetSoundSamples);
	}
		if(!Result.game_code_is_loaded)
	{
		Result.GetSoundSamples = 0;
		Result.UpdateAndRender = 0;
	}

	return Result;
}

internal void
SDLUnloadGameCode(platform_game_code *GameCode)
{
	if(GameCode->GameCodeDLL){
		SDL_UnloadObject(GameCode->GameCodeDLL);
		GameCode->GameCodeDLL = 0;
	}
	GameCode->game_code_is_loaded = false;
	GameCode->GetSoundSamples     = 0;
	GameCode->UpdateAndRender     = 0;
}

internal void *
PlatformAllocateMemory(void *BaseAddress, uint64 Size)
{
#if defined(_WIN32)
    return VirtualAlloc(BaseAddress, (size_t)Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else
    void *Result = mmap(BaseAddress, (size_t)Size, PROT_READ|PROT_WRITE,
                         MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    return (Result == MAP_FAILED) ? 0 : Result;
#endif
}

internal void
PlatformDeallocateMemory(void *Memory, uint64 Size)
{
#if defined(_WIN32)
    (void)Size; // VirtualFree with MEM_RELEASE doesn't need a size
    if (Memory) VirtualFree(Memory, 0, MEM_RELEASE);
#else
    if (Memory) munmap(Memory, (size_t)Size);
#endif
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile){
	debug_read_file_result Result = {};

	size_t FileSize = 0;
	void *FileData = SDL_LoadFile(Filename, &FileSize);

	if(FileData)
	{
		Result.Contents     = FileData;
		Result.ContentsSize = SafeTruncateUInt64(FileSize);
	}

	return(Result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile){
	bool32 Result = SDL_SaveFile(Filename, Memory, MemorySize);
	return(Result);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory){
	if(Memory)
	{
		SDL_free(Memory);
	}
}

internal void
SDLGetInputFileLocation(platform_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest)
{
		char Temp[64];
	snprintf(Temp, sizeof(Temp), "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
	SDLBuildEXEPathFileName(State, Temp, DestCount, Dest);
}

internal platform_replay_buffer *
SDLGetReplayBuffer(platform_state *State, uint32 Index)
{
	Assert(Index < ArrayCount(State->ReplayBuffers));
	return &State->ReplayBuffers[Index];
}

internal void SDLBeginRecordingInput(platform_state *State, int input_recording_index){
	platform_replay_buffer *replay_buffer = SDLGetReplayBuffer(State, (uint32)input_recording_index);
	if(replay_buffer->MemoryBlock){
		State->input_recording_index = input_recording_index;

		char filename[PLATFORM_STATE_FILE_NAME_COUNT];
		SDLGetInputFileLocation(State, true, input_recording_index, sizeof(filename), filename);
		State->RecordingHandle = SDL_IOFromFile(filename, "w");
		
#if 0
		SDL_SeekIO(State->RecordingHandle, State->TotalSize, SDL_IO_SEEK_SET);
#endif
		memcpy(replay_buffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
	}
}


internal void SDLEndRecordingInput(platform_state *State){
	SDL_CloseIO(State->RecordingHandle);
	State->input_recording_index = 0;
}

internal void SDLBeginInputPlayback(platform_state *State, int input_playing_index){
	platform_replay_buffer *replay_buffer = SDLGetReplayBuffer(State, (uint32)input_playing_index);
	if(replay_buffer->MemoryBlock){
		State->input_playing_index =  input_playing_index;
				
		char filename[PLATFORM_STATE_FILE_NAME_COUNT];
		SDLGetInputFileLocation(State, true, input_playing_index, sizeof(filename), filename);
		State->PlaybackHandle = SDL_IOFromFile(filename, "r");
#if 1
		SDL_SeekIO(State->RecordingHandle, (Sint64)State->TotalSize, SDL_IO_SEEK_SET);
#endif
		memcpy(State->GameMemoryBlock, replay_buffer->MemoryBlock, State->TotalSize);
	}
}

internal void SDLEndInputPlayback(platform_state *State){
	SDL_CloseIO(State->PlaybackHandle);
	State->input_playing_index = 0;
}

internal void SDLRecordInput(platform_state *State, game_input *input_to_record){
	SDL_WriteIO(State->RecordingHandle, input_to_record, sizeof(*input_to_record));
}

internal void SDLPlaybackInput(platform_state *State, game_input *input_to_playback){
	size_t BytesRead = SDL_ReadIO(State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback));

	if(BytesRead == 0)
	{
		//hit end of stream, go back to beginning
		int playing_index = State->input_playing_index;
		SDLEndInputPlayback(State);
		SDLBeginInputPlayback(State, playing_index);
		SDL_ReadIO(State->PlaybackHandle, input_to_playback, sizeof(*input_to_playback));
	}
}

//TODO add Windows/linux console

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

internal void SDLProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown){
	if(NewState->EndedDown != IsDown){
		NewState->EndedDown = IsDown;
		++NewState->HalfTransitionCount;
	}
}

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
				if (WasDown != IsDown)
				{
					if      (Event.key.scancode == SDL_SCANCODE_W)        { SDLProcessKeyboardMessage(&KeyboardController->MoveUp, 	     IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_A)        { SDLProcessKeyboardMessage(&KeyboardController->MoveLeft, 	 IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_S)        { SDLProcessKeyboardMessage(&KeyboardController->MoveDown, 	 IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_D)        { SDLProcessKeyboardMessage(&KeyboardController->MoveRight,    IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_Q)        { SDLProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_E)        { SDLProcessKeyboardMessage(&KeyboardController->RightShoulder,IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_UP)       { SDLProcessKeyboardMessage(&KeyboardController->ActionUp,     IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_DOWN)     { SDLProcessKeyboardMessage(&KeyboardController->ActionDown,   IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_LEFT)     { SDLProcessKeyboardMessage(&KeyboardController->ActionLeft, 	 IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_RIGHT)    { SDLProcessKeyboardMessage(&KeyboardController->ActionRight,  IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_SPACE)    { SDLProcessKeyboardMessage(&KeyboardController->Back, 	     IsDown);}
					else if (Event.key.scancode == SDL_SCANCODE_ESCAPE)   { SDLProcessKeyboardMessage(&KeyboardController->Start, 	     IsDown);}
					//loop mode
					else if (Event.key.scancode == SDL_SCANCODE_L){
						if(IsDown){
							if(State->input_playing_index == 0){

								if(State->input_recording_index == 0){
									SDLBeginRecordingInput(State, 1);
								}
								else{
									SDLEndRecordingInput(State);
									SDLBeginInputPlayback(State, 1);
								}
							}
							else{
								SDLEndInputPlayback(State);
							}
						}
					}
				}
				if (Event.key.scancode == SDL_SCANCODE_P && IsDown)
				{
					if(GlobalPause){
						GlobalPause = false;
					}
					else{
						GlobalPause = true;
					}
				}
			} break;

			case SDL_EVENT_WINDOW_RESIZED:
			{
				// TODO: reallocate BackBufferMemory / recreate BackBufferTexture
			} break;
		}
	}
}


int main(int argc, char *argv[])
{
	platform_state State = {};
	SDLGetEXEFileName(&State);

	char SourceGameCodeDLLFullPath[PLATFORM_STATE_FILE_NAME_COUNT];
	char SourceGameCodeDLLFileName[] = "handmade.dll";
	SDLBuildEXEPathFileName(&State, SourceGameCodeDLLFileName,
							   sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

	char TempGameCodeDLLFullPath[PLATFORM_STATE_FILE_NAME_COUNT];
	char TempGameCodeDLLFileName[] = "handmade_temp.dll";
	SDLBuildEXEPathFileName(&State, TempGameCodeDLLFileName,
							   sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

	GlobalPerfCountFrequency = SDL_GetPerformanceFrequency();

	//SDL INITIALIZATION
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

	SDL_Renderer *Renderer = SDL_CreateRenderer(Window, 0);
	SDL_Texture *BackBufferTexture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
	SDL_SetTextureBlendMode(BackBufferTexture, SDL_BLENDMODE_NONE);


	// SDL_GetGamepads() / SDL_OpenGamepad()

	thread_context Thread = {};

	game_memory GameMemory = {};
	GameMemory.PermanentStorageSize = Megabytes(64);
	GameMemory.TransientStorageSize = Gigabytes((uint64)1);		
	GameMemory.DEBUGPlatformFreeFileMemory  = DEBUGPlatformFreeFileMemory;
	GameMemory.DEBUGPlatformReadEntireFile  = DEBUGPlatformReadEntireFile;
	GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

	State.TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
#if HANDMADE_INTERNAL
void* BaseAddress = (void *)Terabytes((uint64)2);
#else
void* BaseAddress = 0;
#endif
	State.GameMemoryBlock = PlatformAllocateMemory(BaseAddress, State.TotalSize);
	GameMemory.PermanentStorage = State.GameMemoryBlock;
	GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

	platform_game_code Game = SDLLoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);

	game_input Input[2] = {};
	game_input *NewInput = &Input[0];
	game_input *OldInput = &Input[1];

	real32 GameUpdateHz = 30.0f;
	real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;
	uint64 frame_start;
	uint64 frame_time;
	uint64 frame_delay = (uint64)(1000.0f / GameUpdateHz);

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
	
	SDL_AudioSpec spec = {
    SDL_AUDIO_S16LE, // format
    2,               // channels
    48000           // freq
	};

	SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);

	if (!audio_stream) {
    SDL_Log("Failed to create audio stream: %s", SDL_GetError());
	}

	SDL_ResumeAudioStreamDevice(audio_stream);


	game_sound_output_buffer SoundBuffer = {};
	SoundBuffer.SamplesPerSecond = spec.freq;

	real32 AudioBufferDuration = TargetSecondsPerFrame * 3.0f;

	int BytesPerSample = spec.channels * (int)sizeof(int16);
	uint32 TargetQueueBytes = (uint32)((real32)spec.freq * AudioBufferDuration * (real32)BytesPerSample);
    uint32 CurrentlyQueuedBytes = (uint32)SDL_GetAudioStreamQueued(audio_stream);
    
    int32 BytesToWrite = (int32)TargetQueueBytes - (int32)CurrentlyQueuedBytes;
	int BytesPerStereoSample = spec.channels * (int)sizeof(int16); // 2 channels * 2 bytes = 4 bytes
	int16 *AudioSampleScratchBuffer = (int16 *)SDL_malloc(TargetQueueBytes);

	GlobalRunning = true;
	while (GlobalRunning)
	{
		SDL_Time newdllwritetime = SDLGetLastFileWriteTime(SourceGameCodeDLLFullPath);
			if(newdllwritetime && (newdllwritetime > Game.LastWriteTimeDLL))
			{
				SDLUnloadGameCode(&Game);
				Game = SDLLoadGameCode(SourceGameCodeDLLFullPath, TempGameCodeDLLFullPath);
			}
		frame_start = SDL_GetTicks();


		game_controller_input *OldKeyboardController = GetController(OldInput, 0);
		game_controller_input *NewKeyboardController = GetController(NewInput, 0);
		*NewKeyboardController = {};
		NewKeyboardController->IsConnected = true;
		for (uint64 ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ++ButtonIndex)
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

			if (State.input_recording_index) { SDLRecordInput(&State, NewInput); }
			if (State.input_playing_index)   { SDLPlaybackInput(&State, NewInput); }

			NewInput->dtForFrame = TargetSecondsPerFrame;

			if (Game.UpdateAndRender)
			{
				Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &Buffer);
			}
			SDL_UpdateTexture(BackBufferTexture, 0, Buffer.Memory, Buffer.Pitch);
			SDL_RenderTexture(Renderer, BackBufferTexture, 0, 0);
			SDL_RenderPresent(Renderer);


			CurrentlyQueuedBytes = (uint32)SDL_GetAudioStreamQueued(audio_stream);
			BytesToWrite = (int32)TargetQueueBytes - (int32)CurrentlyQueuedBytes;
			 if (BytesToWrite > 0) {
				
				SoundBuffer.SampleCount = (int32)(BytesToWrite / BytesPerStereoSample);
				
				// 3. Allocate temporary memory for this frame's audio chunks
				SoundBuffer.Samples = AudioSampleScratchBuffer;
				
				//clear scratchbuffer to make sure there is no garbage sound playing
				SDL_memset(AudioSampleScratchBuffer, 0, (size_t)BytesToWrite);

				// 4. game engine call
				Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
				
				// 5. Submit the newly generated data to the SDL3 stream
				SDL_PutAudioStreamData(audio_stream, SoundBuffer.Samples, BytesToWrite);

			}
					
			frame_time = SDL_GetTicks() - frame_start;

			if (frame_delay > frame_time) {
				SDL_Delay((uint32)(frame_delay - frame_time)); // Sleep for remaining time
			}

			game_input *Temp = NewInput;
			NewInput = OldInput;
			OldInput = Temp;
		}
	}

	SDLUnloadGameCode(&Game);
	SDL_free(AudioSampleScratchBuffer);
	SDL_DestroyAudioStream(audio_stream);
	SDL_Quit();

	return 0;
}