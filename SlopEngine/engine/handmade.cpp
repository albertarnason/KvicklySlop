#include "handmade.h"

internal void GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz){

	
	int16 ToneVolume = 3000;
	int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;
	int16 *SampleOut = SoundBuffer->Samples;

	for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
		{
//sound flag
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
	uint32 color = 0xFFFFFFFF;
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

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	//todo lets see what the optimizer does
	// Casting to make sure the pointer arithmetic doesnt get multiplied by C 
	uint8 *Row = (uint8 *)Buffer->Memory;

	for(int Y = 0; Y < Buffer->Height;++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(int X = 0; X <Buffer->Width;++X)
		{
			/*
				
				LITTLE ENDIAN ARCHITECTURE--------------V
				Bytes           =  0  1  2  3			V
				Pixel in memory = RR GG BB xx, -> 0x xxBBGGRR
				Bunch of windows order swapping and whatnot
				So it ends up being this:
				Pixel in memory = BB GG RR xx
			*/
			//Blue
			uint8 Blue = (uint8)(X + BlueOffset);
			uint8 Green= (uint8)(Y + GreenOffset);
			
			// *Pixel = ;, writes value to left of = into Pixel by dereferencing with *
			// *Pixel++, the ++ is post increment operator, so after expression add 1
			//Also C is doing a 1*(sizeof uint32) aka = 4 so the expression adds 4
			//Shifting green value 8bits (2 bytes) left and OR'ing with blue
			*Pixel++ = ((Green << 16) | Blue);
			//memory = BB GG RR xx
			//Register = xx RR GG BB 
		}
		Row += Buffer->Pitch;
	}
}

//extern "C" is to avoid c++ name mangling for DLL purposes
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender){
	
	//pointer arithmetic to make sure game_button_sate Buttons[] == game_button_state
	Assert((&Input->Controllers[0].Error - &Input->Controllers[0].Buttons[0]) == (ArrayCount(Input->Controllers[0].Buttons)));
	//game breaks right here in debugger if false
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize); 
	
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	if(!Memory->IsInitialized){

	
		
		//for debug purposes always reading whole files at once until multithreading
		//roundtrip system for training wheels version
		char *Filename = __FILE__;
/*
		uint64 FileSize = GetFileSize(FileName);
		void *BitmapMemory = ReserveMemory(Memory, FileSize);
		ReadEntireFileIntoMemory(FileName, BitmapMemory);
*/

		debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
		if(File.Contents)
		{
			//works
			Memory->DEBUGPlatformWriteEntireFile("C:/Users/walla/src/Handmadehero/Handmade/Handmade/Debug/test.out", File.ContentsSize, File.Contents);
			Memory->DEBUGPlatformFreeFileMemory(File.Contents);
		}

		GameState->ToneHz = 256;
		GameState->tSine  = 0.0f;
		Memory->IsInitialized = true;
		GameState->PlayerX = 100;
		GameState->PlayerY = 100;
	
	};

	//For loop for multiple controller inputs hmm
	for(int ControllerIndex = 0; ControllerIndex <ArrayCount(Input->Controllers); ++ControllerIndex){
		game_controller_input *Controller = GetController(Input, ControllerIndex);
		if(Controller->Analog){
			GameState->YOffset += 	    (int)(	4.0f * Controller->StickAverageX);
			GameState->ToneHz 	= 256 + (int)(128.0f * Controller->StickAverageY);
		} 
		else {
			//Keyboard movement
			/*
			if(Controller->ActionLeft .EndedDown){GameState->XOffset -= 1;}
			if(Controller->ActionRight.EndedDown){GameState->XOffset += 1;}
			if(Controller->ActionUp   .EndedDown){GameState->YOffset -= 1;}
			if(Controller->ActionDown .EndedDown){GameState->YOffset += 1;}
			*/
			if(Controller->MoveLeft   .EndedDown){GameState->PlayerX -= 10;}
			if(Controller->MoveRight  .EndedDown){GameState->PlayerX += 10;}
			if(Controller->MoveUp     .EndedDown){GameState->PlayerY -= 10;}
			if(Controller->MoveDown   .EndedDown){GameState->PlayerY += 10;}

			//bad jump code
			if(GameState->jumptimer > 0){
				GameState->PlayerY += (int)(3.0f*sinf(0.5f*Pi32*GameState->jumptimer));
			}
			if(Controller->ActionUp.EndedDown){
				GameState->jumptimer = 4.0f;
			}
			
			GameState->jumptimer -=0.033f;

		
			
		}

	}

    RenderWeirdGradient(Buffer, GameState->XOffset, GameState->YOffset);
	RenderPlayer(Buffer, GameState->PlayerX, GameState->PlayerY);
}

//has to be a fast function, no more than 1ms!
//todo reduce pressure on function performance by measuring it or asking about it
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples){
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}

