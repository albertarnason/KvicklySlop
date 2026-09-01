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
#else

		int16 SampleValue = 0;
#endif

	}

}

//claude helper
internal bool32 character_is_digit(char character)
{
	return (character >= '0' && character <= '9');
}

//claude helper
internal bool32 character_is_whitespace(char character)
{
	return (character == ' ' || character == '\t' || character == '\r');
}

//claude parsing
internal real32 string_to_float(char **current_position_pointer, char *end)
{
	char *current_position = *current_position_pointer;
	while(current_position < end && character_is_whitespace(*current_position)){ ++current_position; }

	real32 sign = 1.0f;
	if(current_position < end && *current_position == '-'){ sign = -1.0f; ++current_position; }

	real32 value = 0.0f;
	while(current_position < end && character_is_digit(*current_position))
	{
		value = value * 10.0f + (real32)(*current_position - '0');
		++current_position;
	}

	if(current_position < end && *current_position == '.')
	{
		++current_position;
		real32 scale = 0.1f;
		while(current_position < end && character_is_digit(*current_position))
		{
			value += (real32)(*current_position - '0') * scale;
			scale *= 0.1f;
			++current_position;
		}
	}

	*current_position_pointer = current_position;
	return sign * value;
}

//claude parsing
internal int32 string_to_integer(char **current_position_pointer, char *end)
{
	char *current_position = *current_position_pointer;
	while(current_position < end && character_is_whitespace(*current_position)){ ++current_position; }

	int32 sign = 1;
	if(current_position < end && *current_position == '-'){ sign = -1; ++current_position; }

	int32 value = 0;
	while(current_position < end && character_is_digit(*current_position))
	{
		value = value * 10 + (*current_position - '0');
		++current_position;
	}

	*current_position_pointer = current_position;
	return sign * value;
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

internal void spawn_entity(game_state *GameState, mesh *entity_mesh, coordinate spawn_position, uint32 entity_color)
{
	Assert(GameState->EntityCount < MAX_ENTITIES);

	entity *new_entity        = &GameState->Entities[GameState->EntityCount++];
	new_entity->position      = spawn_position;
	new_entity->entity_mesh   = entity_mesh;
	new_entity->color         = entity_color;
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

// parses one "vertex_index/texture_index/normal_index" style face
// reference and returns only the position index, since mesh_face does
// not store texture or normal indices
internal int32 parse_face_vertex_reference(char **current_position_pointer, char *line_end)
{
	char *current_position = *current_position_pointer;

	int32 one_based_vertex_index = string_to_integer(&current_position, line_end);

	// skip past any "/texture_index/normal_index" that follows
	while(current_position < line_end && !character_is_whitespace(*current_position))
	{
		++current_position;
	}

	*current_position_pointer = current_position;
	return one_based_vertex_index - 1; // OBJ indices are 1-based
}

internal void count_obj_lines(char *file_data, size_t file_data_size, uint32 *out_vertex_count, uint32 *out_face_count)
{
	uint32 vertex_count = 0;
	uint32 face_count   = 0;

	char *current_position = file_data;
	char *file_end         = file_data + file_data_size;

	while(current_position < file_end)
	{
		char *line_start = current_position;

		while(current_position < file_end && *current_position != '\n')
		{
			++current_position;
		}

		if(line_start < file_end)
		{
			if(line_start[0] == 'v' && line_start[1] == ' ')
			{
				++vertex_count;
			}
			else if(line_start[0] == 'f' && line_start[1] == ' ')
			{
				++face_count;
			}
			// NOTE: 'vn ' and 'vt ' lines are naturally excluded above
			// since their second character is not a space.
		}

		++current_position; // skip the '\n'
	}

	*out_vertex_count = vertex_count;
	*out_face_count   = face_count;
}

internal void parse_obj_into_mesh(memory_arena *arena, char *file_data, size_t file_data_size, mesh *output_mesh)
{

	Assert(file_data);
	Assert(file_data_size > 0);
	// ---- PASS 1: count vertices and faces so the arena allocation is exact ----
	uint32 total_vertex_count = 0;
	uint32 total_face_count   = 0;
	count_obj_lines(file_data, file_data_size, &total_vertex_count, &total_face_count);

	output_mesh->vertices     = (coordinate *)ArenaPush(arena, sizeof(coordinate) * total_vertex_count);
	output_mesh->faces        = (mesh_face *)ArenaPush(arena, sizeof(mesh_face) * total_face_count);
	output_mesh->vertex_count = 0;
	output_mesh->face_count   = 0;

	// ---- PASS 2: fill vertices and faces ----
	char *current_position = file_data;
	char *file_end         = file_data + file_data_size;

	while(current_position < file_end)
	{
		char *line_start = current_position;

		while(current_position < file_end && *current_position != '\n')
		{
			++current_position;
		}
		char *line_end = current_position; // exclusive
		++current_position;                // skip '\n' for next iteration

		if(line_start >= line_end)
		{
			continue; // empty line
		}

		if(line_start[0] == 'v' && line_start[1] == ' ')
		{
			char *parse_position = line_start + 2; // skip "v "

			real32 vertex_x = string_to_float(&parse_position, line_end);
			real32 vertex_y = string_to_float(&parse_position, line_end);
			real32 vertex_z = string_to_float(&parse_position, line_end);

			coordinate *destination_vertex = &output_mesh->vertices[output_mesh->vertex_count++];
			destination_vertex->x = vertex_x;
			destination_vertex->y = vertex_y;
			destination_vertex->z = vertex_z;
		}
		else if(line_start[0] == 'f' && line_start[1] == ' ')
		{
			char *parse_position = line_start + 2; // skip "f "

			mesh_face *destination_face = &output_mesh->faces[output_mesh->face_count++];
			destination_face->vertex_count = 0;

			while(parse_position < line_end && destination_face->vertex_count < 4)
			{
				while(parse_position < line_end && character_is_whitespace(*parse_position))
				{
					++parse_position;
				}
				if(parse_position >= line_end)
				{
					break;
				}

				int32 vertex_index = parse_face_vertex_reference(&parse_position, line_end);
				destination_face->vertex_index[destination_face->vertex_count++] = vertex_index;
			}
		}
	}

	Assert(output_mesh->vertex_count == total_vertex_count);
	Assert(output_mesh->face_count   == total_face_count);
}

internal void center_mesh_on_origin(mesh *target_mesh)
{
	Assert(target_mesh->vertex_count > 0);

	coordinate min_bounds = target_mesh->vertices[0];
	coordinate max_bounds = target_mesh->vertices[0];

	for(uint32 vertex_index = 1; vertex_index < target_mesh->vertex_count; ++vertex_index)
	{
		coordinate *current_vertex = &target_mesh->vertices[vertex_index];

		if(current_vertex->x < min_bounds.x) { min_bounds.x = current_vertex->x; }
		if(current_vertex->y < min_bounds.y) { min_bounds.y = current_vertex->y; }
		if(current_vertex->z < min_bounds.z) { min_bounds.z = current_vertex->z; }

		if(current_vertex->x > max_bounds.x) { max_bounds.x = current_vertex->x; }
		if(current_vertex->y > max_bounds.y) { max_bounds.y = current_vertex->y; }
		if(current_vertex->z > max_bounds.z) { max_bounds.z = current_vertex->z; }
	}

	coordinate center_offset = {};
	center_offset.x = (min_bounds.x + max_bounds.x) * 0.5f;
	center_offset.y = (min_bounds.y + max_bounds.y) * 0.5f;
	center_offset.z = (min_bounds.z + max_bounds.z) * 0.5f;

	for(uint32 vertex_index = 0; vertex_index < target_mesh->vertex_count; ++vertex_index)
	{
		target_mesh->vertices[vertex_index].x -= center_offset.x;
		target_mesh->vertices[vertex_index].y -= center_offset.y;
		target_mesh->vertices[vertex_index].z -= center_offset.z;
	}
}

internal void free_obj_file_memory(game_memory *Memory, thread_context *Thread, debug_read_file_result file_result){
	Memory->DEBUGPlatformFreeFileMemory(Thread, file_result.Contents);
}


internal mesh* load_obj_file(game_state *GameState, game_memory *Memory, thread_context *Thread, char* file_name){
	char objpath[FILE_NAME_COUNT];
	StringConcat(StringLength(Memory->DataPath), Memory->DataPath, StringLength(file_name), file_name, sizeof(objpath), objpath);
	debug_read_file_result loaded_obj = Memory->DEBUGPlatformReadEntireFile(Thread, objpath);
	Assert(loaded_obj.Contents);

	mesh *destination_mesh = &GameState->Meshes[GameState->MeshCount++];
	parse_obj_into_mesh(&GameState->Arena, (char *)loaded_obj.Contents, loaded_obj.ContentsSize, destination_mesh);

	center_mesh_on_origin(destination_mesh);
	printf("OBJ parsed: %d vertices, %d faces\n", destination_mesh->vertex_count, destination_mesh->face_count);
	Assert(destination_mesh->vertex_count > 0); // catch a file that loaded but parsed to nothing
	Assert(destination_mesh->face_count > 0);

	free_obj_file_memory(Memory, Thread, loaded_obj);
	return destination_mesh;
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


		//penger loading, parsing, printf, memory freeing
		mesh* penger_mesh_dest = load_obj_file(GameState, Memory, Thread, (char *)"real-penger.obj");
		coordinate penger_world_coordinate = {0.0f, 0.0f, 0.0f};
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate, 0xFF90EE90);
	// spawn_entity(GameState, penger_mesh, 3.0f, 0.0f, 0.0f, 0xFFFF9090); // second penger, different position
	// spawn_entity(GameState, other_mesh, -3.0f, 0.0f, 0.0f, 0xFF9090FF); // different model entirely

		
		
		
		
		
		
		Memory->IsInitialized = true;
	};
	temporary_memory temp_memory = TemporaryMemoryNew(&GameState->Arena);

	//For loop for multiple controller
	for(uint32 ControllerIndex = 0; ControllerIndex <(uint32)(ArrayCount(Input->Controllers)); ++ControllerIndex){
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
	uint32 background_color = 0xFF73BFFF;
	//screen clear call/background
	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, background_color);

	uint32 box_color = 0xFF90EE90;
	real32 point_size = 20.0f;
	real32 offset = 50.0f;
	real32 camera_z = 3.0f;
	
	
	for(uint32 entity_index = 0; entity_index < GameState->EntityCount; ++entity_index)
	{
	entity *current_entity   = &GameState->Entities[entity_index];
	mesh   *current_mesh     = current_entity->entity_mesh;

	coordinate *screen_points = (coordinate *)ArenaPush(&GameState->Arena, sizeof(coordinate) * current_mesh->vertex_count);

	for(uint32 vertex_index = 0; vertex_index < current_mesh->vertex_count; ++vertex_index)
	{
		coordinate *source_vertex      = &current_mesh->vertices[vertex_index];
		coordinate rotated_vertex      = rotate(source_vertex->x, source_vertex->y, source_vertex->z, GameState->timer /*+ current_entity->rotation_yaw*/);
		coordinate projected_vertex    = project(rotated_vertex.x + current_entity->position.x, rotated_vertex.y + current_entity->position.y,
												 rotated_vertex.z + current_entity->position.z + camera_z);
		coordinate screen_space_vertex = screen(projected_vertex, Buffer->Width, Buffer->Height);

		/*screen_space_vertex.y += 200.0f; manual penger pixelbased adjustment*/
		screen_points[vertex_index] = screen_space_vertex;
	}

	for(uint32 face_index = 0; face_index < current_mesh->face_count; ++face_index)
	{
		mesh_face *current_face = &current_mesh->faces[face_index];
		for(uint32 face_vertex_index = 0; face_vertex_index < current_face->vertex_count; ++face_vertex_index)
		{
			int32 vertex_a = current_face->vertex_index[face_vertex_index];
			int32 vertex_b = current_face->vertex_index[(face_vertex_index + 1) % current_face->vertex_count];

			DrawLine(Buffer, screen_points[vertex_a], screen_points[vertex_b], current_entity->color);
		}
	}
}



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

