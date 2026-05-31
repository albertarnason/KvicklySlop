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
	int bottom = PlayerY+10;
	for(int X = PlayerX; X < PlayerX+10; ++X){
		uint8 *pixel = ((uint8 *)Buffer->Memory + X*Buffer->BytesPerPixel + top*Buffer->Pitch);
		for (int Y = top; Y < bottom; ++Y){
			if((pixel >= Buffer->Memory) && ((pixel+4) <= EndOfBuffer)){
				*(uint32 *)pixel = color;
			}
			pixel += Buffer->Pitch;
		}
	}
}

/* internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
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
} */

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
	if (max_X > Buffer->Width) {max_X = Buffer->Width ;}
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

internal void DrawCenteredBox(game_offscreen_buffer *Buffer, real32 x, real32 y, real32 size, uint32 color){

real32 min_X = x - (size/2);
real32 min_Y = y - (size/2); 
real32 max_X = x + (size/2); 
real32 max_Y = y + (size/2);
	DrawRectangle(Buffer, min_X, min_Y, max_X, max_Y, color);
}

internal void DrawCenteredBoxCoordinate(game_offscreen_buffer *Buffer, coordinate p, real32 size, uint32 color){
	DrawCenteredBox(Buffer, p.x, p.y, size, color);
}

//stupid claude way to draw line
internal void DrawLine(game_offscreen_buffer *Buffer, coordinate a, coordinate b, uint32 color){
    int steps = 1000;
    for(int s = 0; s <= steps; ++s){
        real32 t = (real32)s / (real32)steps;
        real32 x = a.x + t * (b.x - a.x);
        real32 y = a.y + t * (b.y - a.y);
        DrawRectangle(Buffer, x, y, x + 2.0f, y + 2.0f, color);
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

internal coordinate screen(coordinate coordinate_not_norm, int width, int height){
 
coordinate normalised = {};
	
 	normalised.x = ((coordinate_not_norm.x + 1)/2)*(real32)width  - 0.5f;
	normalised.y = (1 - (coordinate_not_norm.y + 1)/2)*(real32)height - 0.5f;
	normalised.z = coordinate_not_norm.z;
return normalised;
}

internal coordinate project(real32 x, real32 y, real32 z){
	coordinate projection = {};
	projection.x = x/z;
	projection.y = y/z;
	return projection;
}

internal coordinate rotate(real32 x, real32 y, real32 z, real32 angle){
	coordinate result = {};
	
	sinf(angle); 
	cosf(angle); 
	result.x = (x * cosf(angle)) - (z * sinf(angle));
	result.y = y;
	result.z = (x * sinf(angle)) + (z * cosf(angle));
	return result;

}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender){
	//Void unused parameter to make compiler happy
  	(void)Thread;

	//pointer arithmetic to make sure game_button_sate Buttons[] == game_button_state
	Assert((&Input->Controllers[0].Error - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	//game breaks right here in debugger if false
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize); 
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if(!Memory->IsInitialized){
		/*Persistent across frame memory initialisation*/
		GameState->Arena.Size = Memory->TransientStorageSize;
		GameState->Arena.Base = (uint8 *)Memory->TransientStorage;
		GameState->Arena.Used = 0;		
		GameState->Entities[0].X =  0.5f; GameState->Entities[0].Y =  0.5f; GameState->Entities[0].Z =  0.5f;
		GameState->Entities[1].X = -0.5f; GameState->Entities[1].Y =  0.5f; GameState->Entities[1].Z =  0.5f;
		GameState->Entities[2].X =  0.5f; GameState->Entities[2].Y = -0.5f; GameState->Entities[2].Z =  0.5f;
		GameState->Entities[3].X = -0.5f; GameState->Entities[3].Y = -0.5f; GameState->Entities[3].Z =  0.5f;
		GameState->Entities[4].X =  0.5f; GameState->Entities[4].Y =  0.5f; GameState->Entities[4].Z = -0.5f;
		GameState->Entities[5].X = -0.5f; GameState->Entities[5].Y =  0.5f; GameState->Entities[5].Z = -0.5f;
		GameState->Entities[6].X =  0.5f; GameState->Entities[6].Y = -0.5f; GameState->Entities[6].Z = -0.5f;
		GameState->Entities[7].X = -0.5f; GameState->Entities[7].Y = -0.5f; GameState->Entities[7].Z = -0.5f;
		GameState->EntityCount = 8;

		Memory->IsInitialized = true;
	};
temporary_memory temp_memory = TemporaryMemoryNew(&GameState->Arena);

	//For loop for multiple controller
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
	GameState->timer += timedelta;
	
	//COLOR FORMAT IS 0xAARRGGBB (something something little endian windows something something)
	uint32 background_color = 0x4173BFFF;
	uint32 box_color = 0x0090EE90;
	real32 point_size = 20.0f;
	real32 offset = 50.0f;
	
	//screen clear call/background
	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, background_color);
	real32 size = 20.0f;
	coordinate p2 = {};
	coordinate p3 = {};
	real32 angle = 0;
	real32 camera_z = 3.0f;
	coordinate screen_points[8] = {};
	int screen_count = 0;

	for (int i = 0; i < GameState->EntityCount; ++i){

		angle += 2*Pi32*GameState->timer;
		coordinate rotated = rotate(GameState->Entities[i].X, GameState->Entities[i].Y, GameState->Entities[i].Z, GameState->timer);
		
		p2 = project(rotated.x, rotated.y, rotated.z + camera_z);
		p3 = screen(p2, Buffer->Width, Buffer->Height);
		
		screen_points[screen_count++] = p3;
	//	DrawCenteredBoxCoordinate(Buffer, p3, size, box_color);
	}

	// draw lines between front face (0-3) and back face (4-7)
	// front face edges
	DrawLine(Buffer, screen_points[0], screen_points[1], box_color);
	DrawLine(Buffer, screen_points[1], screen_points[3], box_color);
	DrawLine(Buffer, screen_points[3], screen_points[2], box_color);
	DrawLine(Buffer, screen_points[2], screen_points[0], box_color);
	// back face edges
	DrawLine(Buffer, screen_points[4], screen_points[5], box_color);
	DrawLine(Buffer, screen_points[5], screen_points[7], box_color);
	DrawLine(Buffer, screen_points[7], screen_points[6], box_color);
	DrawLine(Buffer, screen_points[6], screen_points[4], box_color);
	// connecting edges
	DrawLine(Buffer, screen_points[0], screen_points[4], box_color);
	DrawLine(Buffer, screen_points[1], screen_points[5], box_color);
	DrawLine(Buffer, screen_points[2], screen_points[6], box_color);
	DrawLine(Buffer, screen_points[3], screen_points[7], box_color);

	RenderPlayer(Buffer, Input->MouseX, Input->MouseY);





















EndTemporaryMemory(temp_memory);
}

//has to be a fast function, no more than 1ms!
//todo reduce pressure on function performance by measuring it or asking about it
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples){
  	//Void unused parameter to make compiler happy
  	(void)Thread;

	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, 256);
}

