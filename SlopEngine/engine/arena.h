#if !defined(ARENA_H)

// some macro helpers that I've found nice:
/*



// create or destroy a 'stack' - an "arena"
Arena *ArenaAlloc(void);
void ArenaRelease(Arena *arena);

// push some bytes onto the 'stack' - the way to allocate
void *ArenaPush(Arena *arena, uint64 size);
void *ArenaPushZero(Arena *arena, uint64 size);


// pop some bytes off the 'stack' - the way to free
void ArenaPop(Arena *arena, uint64 size);

// get the # of bytes currently allocated.
uint64 ArenaGetPos(Arena *arena);

// also some useful popping helpers:
void ArenaSetPosBack(Arena *arena, uint64 pos);
void ArenaClear(Arena *arena);
*/
#define ARENA_H
#endif