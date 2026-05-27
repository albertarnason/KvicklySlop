#include "handmade.h"
#include <stdio.h>


internal void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz){

	
	int16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;
	int16 *SampleOut = SoundBuffer->Samples;

	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex){
//sound flag for debug sound
#if 0	
			real32 SineValue = sinf(GameState->tSine);
			int16 SampleValue = (int16)(SineValue * ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			
			GameState->tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
			if(GameState->tSine > 2.0f*Pi32){
				GameState->tSine -= 2.0f*Pi32;
			}
			
#endif

	}

}

internal void RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY){
	uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch*Buffer->Height;
	uint32 color = 0xFF0000FF;
	int top = PlayerY;
	int bottom = PlayerY+100;
	for(int X = PlayerX; X < PlayerX+100; ++X){
		uint8 *pixel = ((uint8 *)Buffer->Memory + X*Buffer->BytesPerPixel + top*Buffer->Pitch);
		for (int Y = top; Y < bottom; ++Y){
			if((pixel >= Buffer->Memory) && ((pixel+4) <= EndOfBuffer)){
				*(uint32 *)pixel = color;
			}
			pixel += Buffer->Pitch;
		}
	}
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;

	for(int Y = 0; Y < Buffer->Height;++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; X <Buffer->Width;++X)
		{
			uint8 Blue = (uint8)(X + BlueOffset);
			uint8 Green= (uint8)(Y + GreenOffset);
			*Pixel++ = ((Green << 16) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

//default C casting will truncate instead of rounding
internal int32 RoundReal32ToInt32 (real32 Real32){
	int32 Result = (int32)(Real32 + 0.5f);
	return Result;
}


//10->20 20->30, first number inclusive, second number exclusive
internal void DrawRectangle(game_offscreen_buffer *Buffer, real32 real_min_X, real32 real_min_Y, real32 real_max_X, real32 real_max_Y, uint32 color){

	int32 min_X = RoundReal32ToInt32(real_min_X); 
	int32 min_Y = RoundReal32ToInt32(real_min_Y); 
	int32 max_X = RoundReal32ToInt32(real_max_X);
	int32 max_Y = RoundReal32ToInt32(real_max_Y);
	if (min_X < 0){min_X = 0;}
	if (min_Y < 0){min_Y = 0;}
	if (max_X > Buffer->Width) {max_Y = Buffer->Width ;}
	if (max_Y > Buffer->Height){max_Y = Buffer->Height;}
	


	uint8 *row = ((uint8 *)Buffer->Memory +min_X*Buffer->BytesPerPixel + min_Y*Buffer->Pitch);
	for (int Y = min_Y; Y < max_Y; ++Y){
		uint32 *pixel = (uint32 *)row;
		for(int X = min_X; X < max_X; ++X){
			*pixel++ = color;
		}
		row += Buffer->Pitch;
	}
}

internal void SpawnEntity(game_state *GameState, real32 X, real32 Y, uint32 color){
		entity *E = &GameState->Entities[GameState->EntityCount++];
		E->X         = X;
		E->Y         = Y;
		E->Width     = 50.0f;
		E->Height    = 50.0f;
		E->Color     = color;
		E->VelocityX = 100.0f;
		E->VelocityY = 50.0f;
		E->IsActive  = true;
}

internal temporary_memory TemporaryMemoryNew (memory_arena *Arena){
		temporary_memory result;
		result.Arena = Arena;
		result.Used  = Arena->Used;
		return result;
}


internal void EndTemporaryMemory(temporary_memory TempMem){
    TempMem.Arena->Used = TempMem.Used;  // rewind back to the snapshot
}

internal void *ArenaPush(memory_arena *Arena, size_t Size)
{
    Assert(Arena->Used + Size <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;
    return Result;
}

internal void *ArenaPushZero(memory_arena *Arena, size_t Size)
{
    void *Result = ArenaPush(Arena, Size);
    // zero the memory
    uint8 *Byte = (uint8 *)Result;
    while(Size--){ *Byte++ = 0; }
    return Result;
}

//extern "C" is to avoid c++ name mangling for DLL purposes
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender){
	
	//pointer arithmetic to make sure game_button_sate Buttons[] == game_button_state
	Assert((&Input->Controllers[0].Error - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	//game breaks right here in debugger if false
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize); 
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if(!Memory->IsInitialized){
		// arena allocation
		
		GameState->Arena.Size = Memory->TransientStorageSize;
		GameState->Arena.Base = (uint8 *)Memory->TransientStorage;
		GameState->Arena.Used = 0;
				
		SpawnEntity(GameState, (real32)10, (real32)10, 0xFFFFFFFF);

		Memory->IsInitialized = true;
		
	};
	temporary_memory temp_memory = TemporaryMemoryNew(&GameState->Arena);
	

	//For loop for multiple controller inputs hmm
	for(int ControllerIndex = 0; ControllerIndex <ArrayCount(Input->Controllers); ++ControllerIndex){
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if(Controller->Analog){
		} 
		else {
			if(Controller->MoveLeft   .EndedDown){GameState->PlayerX -= 10;}
			if(Controller->MoveRight  .EndedDown){GameState->PlayerX += 10;}
			if(Controller->MoveUp     .EndedDown){GameState->PlayerY -= 10;}
			if(Controller->MoveDown   .EndedDown){GameState->PlayerY += 10;}
			
			
			
		}
		
	}
	real32 timedelta = Input->dtForFrame;
	printf("%f\n", timedelta);


//screen clear call
DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 0x4173BFFF);
uint32 color = 0xFFFFFFFF;
#if 0 //Entity spawning and moving
for (int i = 0; i < MAX_ENTITIES/10; ++i){
	if (i > 0){
		color = color * i;
	}
 SpawnEntity(GameState, (real32)10+(i*4),(real32)10+(i*4), color);
}

for(int i = 0; i < GameState->EntityCount; ++i)
{
	
    entity *Entity = &GameState->Entities[i];

	
    if(!Entity->IsActive) continue;

    Entity->X += Entity->VelocityX * Input->dtForFrame;
    Entity->Y += Entity->VelocityY * Input->dtForFrame;

    DrawRectangle(Buffer, Entity->X, Entity->Y, 
                  Entity->X + Entity->Width, 
                  Entity->Y + Entity->Height, 
                  Entity->Color);
}
#endif




GameState->timer += timedelta;
real32 move_cyan_block_value = 0.0f;
move_cyan_block_value = GameState->timer*3;
DrawRectangle(Buffer, 10.0f*move_cyan_block_value, 10.0f*move_cyan_block_value, 300.0f+10.0f*move_cyan_block_value, 300.0f+10.0f*move_cyan_block_value, 0xBF4E41FF);
RenderPlayer(Buffer, Input->MouseX, Input->MouseY);
   /*old render + mouse input showcase code*/
#if 0
    RenderWeirdGradient(Buffer, GameState->XOffset, GameState->YOffset);
	RenderPlayer(Buffer, Input->MouseX, Input->MouseY);
	
	for(int button_index = 0; button_index < ArrayCount(Input->MouseButtons); ++button_index){
		if(Input->MouseButtons[button_index].EndedDown){
			RenderPlayer(Buffer, 10 + 20*button_index, 10);
		}
	}
#endif
	

EndTemporaryMemory(temp_memory);
}

//has to be a fast function, no more than 1ms!
//todo reduce pressure on function performance by measuring it or asking about it
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples){
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 256);
}

