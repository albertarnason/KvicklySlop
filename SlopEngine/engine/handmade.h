#if !defined(HANDMADE_H)
#include <stdint.h>

// implement sine ourselves
#include <math.h>
#define internal static
#define local_persist static
#define global_variable static 
#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))
//be careful with macros, put extra parenthesis if some inputs will mess with functionality
//such as passing foo+bar next to [0] for bar[0]


//max path
#define FILE_NAME_COUNT 260

/*
HANDMADE_INTERNAL:
0 - build for public
1 - build for dev

HANDMADE_SLOW:
0 - build for fast
1 - build for slow
*/


#if HANDMADE_SLOW
#define Assert(Expression) if(!(Expression)){*(int*)0 = 0;}
#else
#define Assert(Expression)
#endif

//should these all be 64bit?
#define Kilobytes(Value) (Value * 1024LL)
#define Megabytes(Value) (Value * 1024LL * 1024)
#define Gigabytes(Value) (Value * 1024LL * 1024 * 1024)
#define Terabytes(Value) (Value * 1024LL * 1024 * 1024 * 1024)

#define Pi32 3.14159265359f
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;


typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef float real32;
typedef double real64;

inline uint32 SafeTruncateUInt64(uint64 Value){
	//todo defines for max values UInt32Max
	Assert(Value <= 0xFFFFFFFF);
	uint32 Result = (uint32)Value;
	return(Result);
}

//for porting reasons, not necessary on win32, and something something multithreadding
struct thread_context{
    int placeholder;
};

//NOT for shipping! Blocking and write doesnt protect against lost data!
struct debug_read_file_result
{   
    uint32 ContentsSize;
    void *Contents;
};


/*
old definitions of debug functions
debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
void DEBUGPlatformFreeFileMemory(void *Memory);
bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
*/
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name (thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name (thread_context *Thread, char *Filename, uint32 MemorySize, void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);


#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name (thread_context *Thread, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);


struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct game_sound_output_buffer{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

struct game_button_state{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input{

    bool32 IsConnected;
    bool32 Analog;
    real32 StickAverageX;
    real32 StickAverageY;
    
    union
    {
        game_button_state Buttons[12];
        struct
         {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Start;
            game_button_state Back;

            //weird button for assertions to check if Buttons[] is equal to count of game_button_states, for workaround due to anonymous struct
            game_button_state Error;


            
        };
    };
};
struct game_input{
    game_button_state MouseButtons[5];
    int32 MouseX;
    int32 MouseY;
    int32 MouseZ; //idk why mousez should be a thing
    real32 dtForFrame;
    game_controller_input Controllers[5];
};
inline game_controller_input *GetController(game_input *Input, uint32 ControllerIndex){
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}


struct game_memory{
    bool32 IsInitialized;
    uint64 PermanentStorageSize;
    void *PermanentStorage; //REQUIRED to be cleared to 0
    uint64 TransientStorageSize;
    void *TransientStorage;  //REQUIRED to be cleared to 0
    char DataPath[FILE_NAME_COUNT]; 

    //function pointers to debug functions
    //c++ vtable dispatch like
    debug_platform_free_file_memory   *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file   *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file  *DEBUGPlatformWriteEntireFile;
};

//function macro definitions to allow DLL to work
//equivalent to void GameUpdateAndRender (game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer);
//No stubs, have to check for null when calling!
extern "C" void GameUpdateAndRender(thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer);
#define GAME_UPDATE_AND_RENDER(name) void name (thread_context *Thread, game_memory *Memory, game_input *Input, game_offscreen_buffer *Buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

extern "C" void GameGetSoundSamples(thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer);
#define GAME_GET_SOUND_SAMPLES(name) void name (thread_context *Thread, game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

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

struct entity{
    real32 X;
    real32 Y;
    real32 Z;
    real32 Width;
    real32 Height;
    uint32 Color;
    real32 VelocityX;
    real32 VelocityY;
    bool32 IsActive;
};


struct memory_arena{
    size_t Size;
    uint8 *Base;
    size_t Used;
};

struct temporary_memory{
    memory_arena *Arena;
    size_t Used;
};

struct coordinate{
	real32 x;
	real32 y;
    real32 z;
};


#define MAX_ENTITIES 1024

struct game_state{
    int EntityCount;
    entity Entities[MAX_ENTITIES];

    real32 timer;
    coordinate coordinates;

    //only used for sound example
    real32 tSine;


    //arena is important
    memory_arena Arena;
};


//only for gamelayer atm
#define PushArray(arena, type, count) (type *)ArenaPush((arena), sizeof(type)*(count))
#define PushArrayZero(arena, type, count) (type *)ArenaPushZero((arena), sizeof(type)*(count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

internal void *ArenaPush(memory_arena *Arena, size_t Size);

internal void *ArenaPushZero(memory_arena *Arena, size_t Size);

internal size_t StringLength(const char *String);

internal void StringConcat(size_t SourceACount, char *SourceA, size_t SourceBCount, char *SourceB, size_t DestCount, char *Dest);

#define HANDMADE_H
#endif