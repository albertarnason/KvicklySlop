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
internal void DrawLine_old(game_offscreen_buffer *Buffer, coordinate a, coordinate b, uint32 color){
    int steps = 1000;
    for(int s = 0; s <= steps; ++s){
        real32 t = (real32)s / (real32)steps;
        real32 x = a.x + t * (b.x - a.x);
        real32 y = a.y + t * (b.y - a.y);
        DrawRectangle(Buffer, x, y, x + 2.0f, y + 2.0f, color);
    }
}

uint32 ColorWithAlpha(uint32 color, real32 normalized_alpha){
    // pack normalized float alpha into top byte of color
    uint8  alpha_byte = (uint8)(normalized_alpha * 255.0f);
    uint32 color_with_alpha = (color & 0x00FFFFFF) | ((uint32)alpha_byte << 24);
    return color_with_alpha;
}

internal uint32 BlendPixel(uint32 source_color, uint32 destination_color){
    // extract alpha from source as 0-1 float
    real32 source_alpha = (real32)((source_color >> 24) & 0xFF) / 255.0f;

    // extract rgb channels from source and destination
    uint8 source_red   = (source_color >> 16) & 0xFF;
    uint8 source_green = (source_color >>  8) & 0xFF;
    uint8 source_blue  = (source_color >>  0) & 0xFF;

    uint8 destination_red   = (destination_color >> 16) & 0xFF;
    uint8 destination_green = (destination_color >>  8) & 0xFF;
    uint8 destination_blue  = (destination_color >>  0) & 0xFF;

    // linear blend source over destination
    uint8 output_red   = (uint8)(source_red   * source_alpha + destination_red   * (1.0f - source_alpha));
    uint8 output_green = (uint8)(source_green * source_alpha + destination_green * (1.0f - source_alpha));
    uint8 output_blue  = (uint8)(source_blue  * source_alpha + destination_blue  * (1.0f - source_alpha));

    return (output_red << 16) | (output_green << 8) | output_blue;
}

internal void DrawPixel(game_offscreen_buffer *buffer, int32 pixel_x, int32 pixel_y, uint32 color){
    // bounds check
    if(pixel_x < 0 || pixel_x >= buffer->Width)  { return; }
    if(pixel_y < 0 || pixel_y >= buffer->Height) { return; }

    // compute pixel address and blend into framebuffer
    uint32 *destination_pixel = (uint32 *)((uint8 *)buffer->Memory + pixel_x*buffer->BytesPerPixel + pixel_y*buffer->Pitch);
    *destination_pixel = BlendPixel(color, *destination_pixel);
}

internal void DrawLine(game_offscreen_buffer *buffer, coordinate point_a, coordinate point_b, uint32 color){
    real32 start_x = point_a.x;
    real32 start_y = point_a.y;
    real32 end_x   = point_b.x;
    real32 end_y   = point_b.y;

	// degenerate case — draw single pixel and early out
    if(start_x == end_x && start_y == end_y){
        DrawPixel(buffer, RoundReal32ToInt32(start_x), RoundReal32ToInt32(start_y), ColorWithAlpha(color, 1.0f));
        return;
    }

    // select horizontal or vertical stepping based on dominant axis
    if(fabsf(end_y - start_y) < fabsf(end_x - start_x)){

        // ensure left to right ordering
        if(end_x < start_x){
            real32 temp_x = start_x; 
            real32 temp_y = start_y; 
			start_x = end_x; 
			start_y = end_y;
			end_x = temp_x;
			end_y = temp_y;
        }

        real32 delta_x = end_x - start_x;
        real32 delta_y = end_y - start_y;
        real32 slope   = delta_y / delta_x;

        // step along x, blend two pixels per column based on fractional y distance
        for(int step = 0; step < (int32)delta_x; ++step){
            real32 current_x             = start_x + (real32)step;
            real32 current_y             = start_y + (real32)step * slope;
            int32  pixel_x               = RoundReal32ToInt32(current_x);
            int32  pixel_y               = RoundReal32ToInt32(current_y);
            real32 fractional_y_distance = fabsf(current_y - (real32)pixel_y);

            // neighbor pixel direction depends on slope sign
            int32 neighbor_pixel_y;
            if(slope >= 0){ neighbor_pixel_y = pixel_y + 1; }
            else          { neighbor_pixel_y = pixel_y - 1; }

            DrawPixel(buffer, pixel_x, pixel_y,          ColorWithAlpha(color, 1.0f - fractional_y_distance));
            DrawPixel(buffer, pixel_x, neighbor_pixel_y, ColorWithAlpha(color,        fractional_y_distance));
        }
    }
    else{

        // ensure top to bottom ordering
        if(end_y < start_y){
            real32 temp_x = start_x; 
            real32 temp_y = start_y; 
			start_x = end_x; 
			start_y = end_y; 
			end_x = temp_x;
			end_y = temp_y;
        }

        real32 delta_x = end_x - start_x;
        real32 delta_y = end_y - start_y;
        real32 slope   = delta_x / delta_y;

        // step along y, blend two pixels per row based on fractional x distance
        for(int step = 0; step < (int32)delta_y; ++step){
            real32 current_x             = start_x + (real32)step * slope;
            real32 current_y             = start_y + (real32)step;
            int32  pixel_x               = RoundReal32ToInt32(current_x);
            int32  pixel_y               = RoundReal32ToInt32(current_y);
            real32 fractional_x_distance = fabsf(current_x - (real32)pixel_x);

            // neighbor pixel direction depends on slope sign
            int32 neighbor_pixel_x;
            if(slope >= 0){ neighbor_pixel_x = pixel_x + 1; }
            else          { neighbor_pixel_x = pixel_x - 1; }

            DrawPixel(buffer, pixel_x,          pixel_y, ColorWithAlpha(color, 1.0f - fractional_x_distance));
            DrawPixel(buffer, neighbor_pixel_x, pixel_y, ColorWithAlpha(color,        fractional_x_distance));
        }
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
			if(Controller->MoveLeft   .EndedDown){}
			if(Controller->MoveRight  .EndedDown){}
			if(Controller->MoveUp     .EndedDown){}
			if(Controller->MoveDown   .EndedDown){}
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

