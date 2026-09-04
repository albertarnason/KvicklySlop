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
    int steps = 100;
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
	// fully opaque - skip the blend math entirely
    if(source_alpha >= 1.0f){return source_color & 0x00FFFFFF;}

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
    

	// Limits rendering space to within window size + 200 pixel edge buffer,
	real32 pixel_edge_buffer = 200;
	if(point_a.x < -pixel_edge_buffer || point_a.x >= (real32)buffer->Width  + pixel_edge_buffer) { return; }
    if(point_a.y < -pixel_edge_buffer || point_a.y >= (real32)buffer->Height + pixel_edge_buffer) { return; }

	if(point_b.x < -pixel_edge_buffer || point_b.x >= (real32)buffer->Width  + pixel_edge_buffer) { return; }
    if(point_b.y < -pixel_edge_buffer || point_b.y >= (real32)buffer->Height + pixel_edge_buffer) { return; }
	
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

	entity *new_entity        				= &GameState->Entities[GameState->EntityCount++];
	new_entity->entity_transform.position	= spawn_position;
	new_entity->entity_mesh   				= entity_mesh;
	new_entity->color         				= entity_color;
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

internal int32 resolve_obj_index(int32 obj_index, uint32 count)
{
	int32 resolved_index;

	if(obj_index > 0){
		resolved_index = obj_index - 1;
	}
	else if(obj_index < 0){
		resolved_index = (int32)count + obj_index;
	}
	else{
		Assert(!"OBJ index cannot be zero");
		return -1;
	}

	Assert(resolved_index >= 0);
	Assert(resolved_index < (int32)count);

	return resolved_index;
}


internal obj_face_vertex parse_face_vertex_reference(
	char **current_position_pointer,
	char *line_end,
	uint32 vertex_count,
	uint32 texture_count,
	uint32 normal_count)
{
	char *current_position = *current_position_pointer;

	obj_face_vertex result = {};

	result.vertex_index  = -1;
	result.texture_index = -1;
	result.normal_index  = -1;

	// vertex index
	int32 obj_vertex_index = string_to_integer(&current_position, line_end);
	result.vertex_index    = resolve_obj_index(obj_vertex_index, vertex_count);

	if(current_position < line_end && *current_position == '/')
	{
		++current_position;

		// texture index
		if(current_position < line_end &&*current_position != '/'){
			int32 obj_texture_index = string_to_integer(&current_position, line_end);
			result.texture_index    = resolve_obj_index(obj_texture_index,texture_count);
		}

		// normal index
		if(current_position < line_end &&*current_position == '/'){
			++current_position;

			if(current_position < line_end && !character_is_whitespace(*current_position)){
				int32 obj_normal_index = string_to_integer(&current_position, line_end);
				result.normal_index    = resolve_obj_index(obj_normal_index, normal_count);
			}
		}
	}

	while(current_position < line_end && !character_is_whitespace(*current_position))
	{
		++current_position;
	}

	*current_position_pointer = current_position;

	return result;
}

internal void count_obj_lines(char *file_data, size_t file_data_size, uint32 *out_vertex_count, uint32 *out_face_count, uint32 *out_normal_count, uint32 *out_texture_count)
{
	uint32 vertex_count  = 0;
	uint32 face_count    = 0;
	uint32 normal_count  = 0;
	uint32 texture_count = 0;

	char *current_position = file_data;
	char *file_end         = file_data + file_data_size;

	while(current_position < file_end)
	{
		char *line_start = current_position;

		while(current_position < file_end && *current_position != '\n')
		{
			++current_position;
		}

		uint64 line_length = (uint64)(current_position - line_start);

		if(line_length >= 2)
		{
			if(line_start[0] == 'v' && line_start[1] == ' ')
			{
				++vertex_count;
			}
			else if(line_start[0] == 'f' && line_start[1] == ' ')
			{
				++face_count;
			}
			else if(line_length >= 3 && line_start[0] == 'v' && line_start[1] == 'n' && line_start[2] == ' ')
			{
				++normal_count;
			}
			else if(line_length >= 3 && line_start[0] == 'v' && line_start[1] == 't' && line_start[2] == ' ')
			{
				++texture_count;
			}
			// NOTE: 'vn ' and 'vt ' lines are naturally excluded above
			// since their second character is not a space.
		}

			if(current_position < file_end)
		{
			++current_position;
		}
	}

	*out_vertex_count  =  vertex_count;
	*out_face_count    =    face_count;
	*out_normal_count  =  normal_count;
	*out_texture_count = texture_count;
}

internal void parse_obj_into_mesh(memory_arena *arena, char *file_data, size_t file_data_size, mesh *output_mesh)
{

	Assert(file_data);
	Assert(file_data_size > 0);
	// ---- PASS 1: count vertices, faces, textures, normals, faces so the arena allocation is exact ----
	uint32 total_vertex_count = 0;
	uint32 total_texture_coordinate_count = 0;
	uint32 total_normal_count = 0;
	uint32 total_face_count = 0;
	count_obj_lines(file_data, file_data_size, &total_vertex_count, &total_face_count, &total_normal_count, &total_texture_coordinate_count);

	output_mesh->vertices   		 = (coordinate*)ArenaPush(arena, sizeof(coordinate) * total_vertex_count);
	output_mesh->faces        		 = (mesh_face *)ArenaPush(arena, sizeof(mesh_face ) * total_face_count  );
	output_mesh->normals	 		 = (coordinate*)ArenaPush(arena, sizeof(coordinate) * total_normal_count);
	output_mesh->texture_coordinates = (texture_coordinate*)ArenaPush(arena, sizeof(texture_coordinate) * total_texture_coordinate_count);
	output_mesh->vertex_count = 0;
	output_mesh->face_count   = 0;
	output_mesh->normal_count = 0;
	output_mesh->texture_count= 0;

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
		if(current_position < file_end)
		{
			++current_position;
		}

		uint64 line_length = (uint64)(line_end - line_start);
		if(line_length == 0)
		{
			continue;
		}

		if(line_length >= 2 && line_start[0] == 'v' && line_start[1] == ' ')
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
		else if(line_length >= 2 && line_start[0] == 'f' && line_start[1] == ' ')
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

				obj_face_vertex reference = parse_face_vertex_reference(&parse_position, line_end, output_mesh->vertex_count, output_mesh->texture_count, output_mesh->normal_count);
				uint32 vertex = destination_face->vertex_count++;
				destination_face->vertex_index[vertex]  = reference.vertex_index;
				destination_face->normal_index[vertex]  = reference.normal_index;
				destination_face->texture_index[vertex] = reference.texture_index;
			}
		}
		else if(line_length >= 3 && line_start[0] == 'v' && line_start[1] == 'n' && line_start[2] == ' ')
		{
			char *parse_position = line_start + 3;

			real32 normal_x = string_to_float(&parse_position, line_end);
			real32 normal_y = string_to_float(&parse_position, line_end);
			real32 normal_z = string_to_float(&parse_position, line_end);

			coordinate *destination = &output_mesh->normals[output_mesh->normal_count++];

			destination->x = normal_x;
			destination->y = normal_y;
			destination->z = normal_z;
		}
		else if(line_length >= 3 && line_start[0] == 'v' && line_start[1] == 't' && line_start[2] == ' ')
		{
			char *parse_position = line_start + 3;

			real32 texture_u = string_to_float(&parse_position, line_end);
			real32 texture_v = string_to_float(&parse_position, line_end);
			real32 texture_w = 0.0f;
			// W is optional in OBJ.
			if(parse_position < line_end){
				texture_w = string_to_float(&parse_position, line_end);
			}

			texture_coordinate *destination =
				&output_mesh->texture_coordinates[
					output_mesh->texture_count++];

			destination->u = texture_u;
			destination->v = texture_v;
			destination->w = texture_w;
		}
	}

	Assert(output_mesh->vertex_count == total_vertex_count);
	Assert(output_mesh->face_count   == total_face_count);
	Assert(output_mesh->normal_count == total_normal_count);
	Assert(output_mesh->texture_count == total_texture_coordinate_count);
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

	//TODO: Parse QUADS into Triangles
	center_mesh_on_origin(destination_mesh);
	printf("OBJ parsed: %d vertices, %d faces, %d normals, %d textures\n", destination_mesh->vertex_count, destination_mesh->face_count, destination_mesh->normal_count, destination_mesh->texture_count);
	Assert(destination_mesh->vertex_count > 0); // catch a file that loaded but parsed to nothing
	Assert(destination_mesh->face_count > 0);

	free_obj_file_memory(Memory, Thread, loaded_obj);
	return destination_mesh;
}

internal transform camera_get_transform(game_state *GameState){
	return GameState->camera_transform;
}

internal coordinate camera_get_forward(transform *camera_transform)
{
	real32 sine_angle   = -sinf(camera_transform->rotation_yaw);
	real32 cosine_angle =  cosf(camera_transform->rotation_yaw);

	coordinate forward;
	forward.x = sine_angle;
	forward.y = 0.0f;
	forward.z = cosine_angle;
	return forward;
}

internal coordinate model_to_world(coordinate model_point, transform *entity_transform){
	coordinate result = {};
	real32 sine_angle   = sinf(entity_transform->rotation_yaw);
	real32 cosine_angle = cosf(entity_transform->rotation_yaw);
	result.x = (model_point.x * cosine_angle) - (model_point.z * sine_angle) + entity_transform->position.x;
	result.y =  model_point.y + entity_transform->position.y;
	result.z = (model_point.x * sine_angle) + (model_point.z * cosine_angle) + entity_transform->position.z;
	return result;
}

internal coordinate world_to_camera(coordinate world_point, transform *camera_transform){
	coordinate translated;
	translated.x = world_point.x - camera_transform->position.x;
	translated.y = world_point.y - camera_transform->position.y;
	translated.z = world_point.z - camera_transform->position.z;

	coordinate result = {};
	real32 sine_angle   = sinf(-camera_transform->rotation_yaw);
	real32 cosine_angle = cosf(-camera_transform->rotation_yaw);
	result.x = (translated.x * cosine_angle) - (translated.z * sine_angle);
	result.y =  translated.y;
	result.z = (translated.x * sine_angle) + (translated.z * cosine_angle);
	return result;
}



internal coordinate camera_to_ndc(coordinate camera_point){
	coordinate projection = {};
	projection.x = camera_point.x/camera_point.z;
	projection.y = camera_point.y/camera_point.z;
	return projection;
}

internal coordinate ndc_to_screen(coordinate ndc_point, uint32 width, uint32 height){
	coordinate normalised = {};
 	normalised.x = 	   ((ndc_point.x + 1)/2)*(real32)width  - 0.5f;
	normalised.y = (1 - (ndc_point.y + 1)/2)*(real32)height - 0.5f;
	normalised.z = ndc_point.z;
	return normalised;
}

internal real32 slope_from_coordinates(coordinate A, coordinate B){
	if (B.x - A.x == 0.0f) {
        return 0.0f; //avoiding divide by 0 error
    }
	
	return (A.y - B.y) / (A.x - B.x);
}

internal real32
x_at_y(coordinate A, coordinate B, real32 slope, real32 current_y){
	if (A.x == B.x){
		return A.x; // vertical edge: x is constant regardless of y
	}
	return ((current_y - A.y) / slope) + A.x;
}

internal void ScanFill(game_offscreen_buffer *Buffer, coordinate a, coordinate b, coordinate c, uint32 color){
	coordinate bound_y_min;
	coordinate bound_y_mid;
	coordinate bound_y_max;
 
	//Sorts the 3 2d-coordinates into 3 positions, smallest to largest
	//just be happy I didnt use nested : ? ternary operators

	if ((a.y <= b.y) && (a.y <= c.y) ){bound_y_min = a;if (b.y <= c.y){
			bound_y_mid = b;
			bound_y_max = c;
		}
		else {
			bound_y_mid = c;
			bound_y_max = b;
		}
	} 
	else if (b.y <= c.y){ 
		bound_y_min = b; 
		if (a.y <= c.y){
			bound_y_mid = a;
			bound_y_max = c;
		}
		else {
			bound_y_mid = c;
			bound_y_max = a;
		}
	} 
	else { 
		bound_y_min = c; 
		if (a.y <= b.y){
			bound_y_mid = a;
			bound_y_max = b;
		}
		else{
			bound_y_mid = b;
			bound_y_max = a;
		}
	}

	real32 slope_long      = slope_from_coordinates(bound_y_max, bound_y_min);
	real32 slope_short_top = slope_from_coordinates(bound_y_max, bound_y_mid);
	real32 slope_short_bot = slope_from_coordinates(bound_y_mid, bound_y_min);

	real32 x_pos_left;
	real32 x_pos_right;
	int32 y_top = RoundReal32ToInt32(bound_y_max.y);
	int32 y_mid = RoundReal32ToInt32(bound_y_mid.y);
	int32 y_bot = RoundReal32ToInt32(bound_y_min.y);

	//Splits A triangle into two parts, top part (from y_top to y_mid), fills pixels within it
	for(uint32 scan_index = 0; scan_index < uint32(y_top - y_mid); ++scan_index){
		real32 current_y = bound_y_max.y - scan_index;

		real32 x_pos_1 = x_at_y(bound_y_mid, bound_y_max, slope_short_top, current_y);
		real32 x_pos_2 = x_at_y(bound_y_min, bound_y_max, slope_long, current_y);

		if (x_pos_2 >= x_pos_1){
			x_pos_right = x_pos_2; 
			x_pos_left  = x_pos_1;
		}
		else{
			x_pos_right = x_pos_1;
			x_pos_left  = x_pos_2;
		}
		uint32 pixel_counter = 0;
		while ( pixel_counter <= (uint32)RoundReal32ToInt32(x_pos_right - x_pos_left)){
			DrawPixel(Buffer, RoundReal32ToInt32(x_pos_left)+ pixel_counter, RoundReal32ToInt32(current_y), color);
			pixel_counter++;
		}
	}	

	//2nd triangle part from y_mid to y_min
	for(uint32 scan_index = 0; scan_index < uint32(y_mid - y_bot); ++scan_index){
		real32 current_y = bound_y_mid.y - scan_index;

		real32 x_pos_1 = x_at_y(bound_y_mid, bound_y_min, slope_short_bot, current_y);
		real32 x_pos_2 = x_at_y(bound_y_min, bound_y_max, slope_long, current_y);

		if (x_pos_2 >= x_pos_1){
			x_pos_right = x_pos_2; 
			x_pos_left  = x_pos_1;
		}
		else{
			x_pos_right = x_pos_1;
			x_pos_left  = x_pos_2;
		}
		uint32 pixel_counter = 0;
		while ( pixel_counter <= (uint32)RoundReal32ToInt32(x_pos_right - x_pos_left)){
			DrawPixel(Buffer, RoundReal32ToInt32(x_pos_left)+ pixel_counter, RoundReal32ToInt32(current_y), color);
			pixel_counter++;
		}
	}	

}

internal bool32 clip_edge_to_near_plane(coordinate *camera_point_a, coordinate *camera_point_b, real32 near_plane){

	if (camera_point_a->z < near_plane && camera_point_b->z < near_plane){
		return false;
	}

	if (camera_point_a->z < near_plane){
		real32 t = (near_plane - camera_point_a->z) / (camera_point_b->z - camera_point_a->z);
		camera_point_a->x = camera_point_a->x + t * (camera_point_b->x - camera_point_a->x);
		camera_point_a->y = camera_point_a->y + t * (camera_point_b->y - camera_point_a->y);
		camera_point_a->z = near_plane;
	}

	if (camera_point_b->z < near_plane){
		real32 t = (near_plane - camera_point_b->z) / (camera_point_a->z - camera_point_b->z);
		camera_point_b->x = camera_point_b->x + t * (camera_point_a->x - camera_point_b->x);
		camera_point_b->y = camera_point_b->y + t * (camera_point_a->y - camera_point_b->y);
		camera_point_b->z = near_plane;
	}

	return true;
}
// returns false if the edge should be discarded entirely (both points behind)
// otherwise adjusts whichever endpoint is behind the near plane in-place

internal coordinate edge_intersect_near_plane (coordinate from, coordinate to, real32 near_plane){
	real32 t = (near_plane - from.z) /(to.z - from.z);
	coordinate result;
	result.x = from.x + t * (to.x - from.x);
	result.y = from.y + t * (to.y - from.y);
	result.z = near_plane;
	return result;
}

internal uint32 clip_triangle_to_near_plane(coordinate a,coordinate b,coordinate c, real32 near_plane, coordinate *out_triangles){
	coordinate vertex[3] = {a, b, c};
	bool32 behind[3] = {vertex[0].z < near_plane, vertex[1].z < near_plane, vertex[2].z < near_plane };
	uint32 behind_count = uint32((int32)behind[0] + (int32)behind[1] + (int32)behind[2]);

	if(behind_count == 0){
		out_triangles[0] = a;
		out_triangles[1] = b;
		out_triangles[2] = c;
		return 1;
	}
	
	if(behind_count == 3){
		return 0;
	}

	// Rotate winding so index 0 is the "odd one out" (the lone behind, or the lone in-front
	// for the cases where 1 or 2 vertices have z value too close to near_plane)
	uint32 rotate = 0;
	if(behind_count == 1){
		if(behind[1]){rotate = 1;}
		else if (behind[2]){rotate = 2;}
	}
	else{
		if(!behind[1]){rotate = 1;}
		else if (!behind[2]){rotate = 2;}
	}

	coordinate p0 = vertex[(rotate + 0) % 3];
	coordinate p1 = vertex[(rotate + 1) % 3];
	coordinate p2 = vertex[(rotate + 2) % 3];

	coordinate intersect_01 = edge_intersect_near_plane(p0, p1, near_plane);
	coordinate intersect_02 = edge_intersect_near_plane(p0, p2, near_plane);

	if (behind_count == 1){
		out_triangles[0] = intersect_01; 
		out_triangles[1] = p1;
		out_triangles[2] = p2;
		out_triangles[3] = intersect_01;
		out_triangles[4] = p2;
		out_triangles[5] = intersect_02;
		return 2;
	}
	else{
		out_triangles[0] = p0; 
		out_triangles[1] = intersect_01; 
		out_triangles[2] = intersect_02;
		return 1;
	}

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

		//camera loading, standard inverse transform convention
		GameState->camera_transform.position.z = -3.0f;

		//penger loading, parsing, printf, memory freeing
		mesh* penger_mesh_dest = load_obj_file(GameState, Memory, Thread, (char *)"real-penger.obj");
		mesh* bugatti_mesh_dest = load_obj_file(GameState, Memory, Thread, (char *)"Bugatti-Veyron.obj");
		coordinate bugatti_world_coordinate = {1.0f, 1.0f, 1.0f};
		coordinate penger_world_coordinate  = {1.0f, -1.0f, 0.0f};
		coordinate penger_world_coordinate2 = {0.0f, 0.0f, 0.0f};
		coordinate penger_world_coordinate3 = {2.0f, 10.0f, -1.0f};
		coordinate penger_world_coordinate4 = {4.0f, -2.0f, -1.0f};
		coordinate penger_world_coordinate5 = {5.0f, 0.0f, -1.0f};
		coordinate penger_world_coordinate6 = {6.0f, 3.0f, -1.0f};
		coordinate penger_world_coordinate7 = {0.0f, 100.0f, -1.0f};


		//spawn_entity(GameState, bugatti_mesh_dest, bugatti_world_coordinate , 0xFF90EE90);
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate2, 0xFFFF9090);
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate3, 0xFF9090FF);
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate4, 0xFF9090FF); 
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate5, 0xFF9090FF); 
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate6, 0xFF9090FF); 
		spawn_entity(GameState, penger_mesh_dest, penger_world_coordinate7, 0xFF9090FF);


		
		
		
		
		
		Memory->IsInitialized = true;
	};
	temporary_memory temp_memory = TemporaryMemoryNew(&GameState->Arena);
	real32 timedelta = Input->dtForFrame;
	GameState->timer += timedelta;
	
	
	real32 camera_speed = 30.0f;
	real32 camera_rotation_speed = 2.0f;
	//For loop for multiple controller
	for(uint32 ControllerIndex = 0; ControllerIndex <(uint32)(ArrayCount(Input->Controllers)); ++ControllerIndex){
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if(Controller->Analog){
		}
		else {
			coordinate forward = camera_get_forward(&GameState->camera_transform);

			if(Controller->MoveUp.EndedDown){
				GameState->camera_transform.position.x += forward.x * camera_speed * timedelta;
				GameState->camera_transform.position.z += forward.z * camera_speed * timedelta;
			}
			if(Controller->MoveDown.EndedDown){
				GameState->camera_transform.position.x -= forward.x * camera_speed * timedelta;
				GameState->camera_transform.position.z -= forward.z * camera_speed * timedelta;
			}
			if(Controller->MoveLeft.EndedDown){
				GameState->camera_transform.position.x -= forward.z * camera_speed * timedelta;
				GameState->camera_transform.position.z += forward.x * camera_speed * timedelta;
			}
			if(Controller->MoveRight.EndedDown){
				GameState->camera_transform.position.x += forward.z * camera_speed * timedelta;
				GameState->camera_transform.position.z -= forward.x * camera_speed * timedelta;
			}

			if(Controller->RightShoulder.EndedDown){
				GameState->camera_transform.position.y += camera_speed * timedelta;
			}
			if(Controller->LeftShoulder.EndedDown){
				GameState->camera_transform.position.y -= camera_speed * timedelta;
			}

			if(Controller->ActionRight.EndedDown){
				GameState->camera_transform.rotation_yaw += camera_rotation_speed * timedelta;
			}
			if(Controller->ActionLeft.EndedDown){
				GameState->camera_transform.rotation_yaw -= camera_rotation_speed * timedelta;
			}
		}
	}
	
	//COLOR FORMAT IS 0xAARRGGBB (something something little endian windows something something)
	uint32 background_color = 0xFF73BFFF;
	//screen clear call/background
	DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, background_color);

	uint32 box_color = 0xFF90EE90;
	real32 point_size = 20.0f;
	real32 offset = 50.0f;
	
	
	transform camera_transform = camera_get_transform(GameState);
	real32 near_plane = 0.3f;

	//quick loop for rotating by game speed
	for(uint32 entity_index = 0; entity_index < GameState->EntityCount; ++entity_index)
	{
		GameState->Entities[entity_index].entity_transform.rotation_yaw = GameState->timer;
	}

	for(uint32 entity_index = 0; entity_index < GameState->EntityCount; ++entity_index)
	{
		entity *current_entity = &GameState->Entities[entity_index];
		mesh   *current_mesh   = current_entity->entity_mesh;

		for(uint32 face_index = 0; face_index < current_mesh->face_count; ++face_index)
		{
			mesh_face *current_face = &current_mesh->faces[face_index];
	
			int32 vertex_a = current_face->vertex_index[0];
			int32 vertex_b = current_face->vertex_index[1];
			int32 vertex_c = current_face->vertex_index[2];

			// world + camera space, computed fresh per edge instead of cached per vertex
			coordinate world_a  = model_to_world(current_mesh->vertices[vertex_a], &current_entity->entity_transform);
			coordinate world_b  = model_to_world(current_mesh->vertices[vertex_b], &current_entity->entity_transform);
			coordinate world_c  = model_to_world(current_mesh->vertices[vertex_c], &current_entity->entity_transform);
			coordinate camera_a = world_to_camera(world_a, &camera_transform);
			coordinate camera_b = world_to_camera(world_b, &camera_transform);
			coordinate camera_c = world_to_camera(world_c, &camera_transform);

			coordinate clipped[6];
			uint32 clipped_triangle_count = clip_triangle_to_near_plane(camera_a, camera_b, camera_c, near_plane, clipped);

			for(uint32 clip_index = 0; clip_index < clipped_triangle_count; ++clip_index){
				coordinate tri_a = clipped[clip_index * 3 + 0];
				coordinate tri_b = clipped[clip_index * 3 + 1];
				coordinate tri_c = clipped[clip_index * 3 + 2];

				coordinate pixel_a = ndc_to_screen(camera_to_ndc(tri_a), (uint32)Buffer->Width, (uint32)Buffer->Height);
				coordinate pixel_b = ndc_to_screen(camera_to_ndc(tri_b), (uint32)Buffer->Width, (uint32)Buffer->Height);
				coordinate pixel_c = ndc_to_screen(camera_to_ndc(tri_c), (uint32)Buffer->Width, (uint32)Buffer->Height);

					// Limits rendering space to within window size + 200 pixel edge buffer,
				real32 pixel_edge_buffer = 200;
				if(pixel_a.x < -pixel_edge_buffer || pixel_a.x >= (real32)Buffer->Width  + pixel_edge_buffer) { continue; }
				if(pixel_a.y < -pixel_edge_buffer || pixel_a.y >= (real32)Buffer->Height + pixel_edge_buffer) { continue; }

				if(pixel_b.x < -pixel_edge_buffer || pixel_b.x >= (real32)Buffer->Width  + pixel_edge_buffer) { continue; }
				if(pixel_b.y < -pixel_edge_buffer || pixel_b.y >= (real32)Buffer->Height + pixel_edge_buffer) { continue; }

				if(pixel_c.x < -pixel_edge_buffer || pixel_c.x >= (real32)Buffer->Width  + pixel_edge_buffer) { continue; }
				if(pixel_c.y < -pixel_edge_buffer || pixel_c.y >= (real32)Buffer->Height + pixel_edge_buffer) { continue; }

				ScanFill(Buffer, pixel_a, pixel_b, pixel_c, current_entity->color);
			}
		}
	}
 /*
	for(uint32 entity_index = 0; entity_index < GameState->EntityCount; ++entity_index)
	{
		entity *current_entity = &GameState->Entities[entity_index];
		mesh   *current_mesh   = current_entity->entity_mesh;

		for(uint32 face_index = 0; face_index < current_mesh->face_count; ++face_index)
		{
			mesh_face *current_face = &current_mesh->faces[face_index];
			for(uint32 face_vertex_index = 0; face_vertex_index < current_face->vertex_count; ++face_vertex_index)
			{
				int32 vertex_a = current_face->vertex_index[face_vertex_index];
				int32 vertex_b = current_face->vertex_index[(face_vertex_index + 1) % current_face->vertex_count];

				// world + camera space, computed fresh per edge instead of cached per vertex
				coordinate world_a  = model_to_world(current_mesh->vertices[vertex_a], &current_entity->entity_transform);
				coordinate world_b  = model_to_world(current_mesh->vertices[vertex_b], &current_entity->entity_transform);
				coordinate camera_a = world_to_camera(world_a, &camera_transform);
				coordinate camera_b = world_to_camera(world_b, &camera_transform);

				// clip in camera space, before projection
				if(!clip_edge_to_near_plane(&camera_a, &camera_b, near_plane))
				{
					continue; // whole edge behind camera, skip drawing it
				}

				coordinate pixel_a = ndc_to_screen(camera_to_ndc(camera_a), (uint32)Buffer->Width, (uint32)Buffer->Height);
				coordinate pixel_b = ndc_to_screen(camera_to_ndc(camera_b), (uint32)Buffer->Width, (uint32)Buffer->Height);

					// Limits rendering space to within window size + 200 pixel edge buffer,
				real32 pixel_edge_buffer = 200;
				if(pixel_a.x < -pixel_edge_buffer || pixel_a.x >= (real32)Buffer->Width  + pixel_edge_buffer) { continue; }
				if(pixel_a.y < -pixel_edge_buffer || pixel_a.y >= (real32)Buffer->Height + pixel_edge_buffer) { continue; }

				if(pixel_b.x < -pixel_edge_buffer || pixel_b.x >= (real32)Buffer->Width  + pixel_edge_buffer) { continue; }
				if(pixel_b.y < -pixel_edge_buffer || pixel_b.y >= (real32)Buffer->Height + pixel_edge_buffer) { continue; }


				DrawLine(Buffer, pixel_a, pixel_b, current_entity->color);
			}
		}
	}
		*/
	//TODO:  Change face loop to use ScanFill() instead of drawline
	/*
	Model_to_screen pipeline 3 points instead of 2
	change clipping function
	 ScanFill()
	 	triangle's bounding box (min/max X and Y among its three corners).
		test whether the pixel's center lies inside the triangle 
		(a well-known test using "barycentric coordinates" or the "edge function" method — 
		checking whether the point is on the correct side of all three edges).
		If inside, color that pixel.
	Z-Buffering
	
	Normals tell which direction of face is outwards?
	What is an obj VT, u, v, w ?
	Should Model_to_screen pipeline be called on a per vertex basis?
	
	*/


	
	
	
	
	
	
	
	
	
	
	
	
	



	
	
	
	
	
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

