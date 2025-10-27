# Arena Allocator

A fast, simple, single-header arena/region allocator for C.

## What is an Arena Allocator?

An arena allocator is a memory management strategy where you allocate a large block of memory upfront, then quickly hand out pieces of it by simply incrementing a pointer. When you're done, you free everything at once.

### Visual Representation

```
Initial State:
┌─────────────────────────────────────┐
│ Arena (4096 bytes)                  │
│ ┌─────────────────────────────────┐ │
│ │         Empty Memory            │ │
│ └─────────────────────────────────┘ │
│ offset: 0                           │
└─────────────────────────────────────┘

After Allocations:
┌─────────────────────────────────────┐
│ Arena (4096 bytes)                  │
│ ┌────┬─────┬────────┬──────────────┐ │
│ │ A  │  B  │   C    │    Empty     │ │
│ └────┴─────┴────────┴──────────────┘ │
│ offset: 940                         │
└─────────────────────────────────────┘
```

## API Pattern

This arena uses a **self-referential API pattern**. Each arena contains a `self` pointer that references itself, and you call methods using:

```c
arena->function(arena->self, parameters...)
```

### Visual Explanation

```
Arena* arena = Arena_create(4096);
       ↓
   ┌─────────────────────────┐
   │ Arena struct            │
   │  self ──┐               │
   │  alloc  │  ← function   │
   │  destroy│     pointers  │
   └─────────┼───────────────┘
             │
             └─→ Points to itself
```

### Usage Examples

```c
int* data = arena->alloc(arena->self, sizeof(int) * 100);
arena->destroy(arena->self);
size_t mark = arena->get_mark(arena->self);
arena->reset_to_mark(arena->self, mark);
```

The pattern is: `arena->method_name(arena->self, other_params...)`

## Why Use an Arena?

### Traditional malloc/free (SLOW)

```c
int* a = malloc(sizeof(int) * 10);     // System call
char* b = malloc(100);                  // System call
double* c = malloc(sizeof(double) * 5); // System call

free(a);  // Track each allocation
free(b);  // Easy to leak memory
free(c);  // Manual bookkeeping
```

### Arena (FAST)

```c
Arena* arena = Arena_create(4096);  // One allocation

int* a = arena->alloc(arena->self, sizeof(int) * 10);     // Pointer math
char* b = arena->alloc(arena->self, 100);                  // Pointer math
double* c = arena->alloc(arena->self, sizeof(double) * 5); // Pointer math

arena->destroy(arena->self);  // Free everything at once
```

### Performance Comparison

| Operation          | Arena           | malloc/free          |
| ------------------ | --------------- | -------------------- |
| Allocation Speed   | O(1) 2-3 cycles | O(log n) 100+ cycles |
| Per-alloc Overhead | 0 bytes         | 8-16 bytes           |
| Batch Free         | O(1)            | O(n)                 |
| Fragmentation      | None            | Possible             |
| Cache Performance  | Sequential      | Random               |

## Installation

Copy `arena.h` to your project. Done.

## Usage

### Step 1: Include the Header

In ONE .c file:

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"
```

In all other files:

```c
#include "arena.h"
```

### Step 2: Create an Arena

```c
Arena* arena = Arena_create(4096);
```

### Step 3: Allocate Memory

```c
int* numbers = (int*)arena->alloc(arena->self, sizeof(int) * 100);
char* message = (char*)arena->alloc(arena->self, 256);
```

### Step 4: Use the Memory

```c
for (int i = 0; i < 100; i++) {
    numbers[i] = i * i;
}
sprintf(message, "Arena allocator works!");
```

### Step 5: Clean Up

```c
arena->destroy(arena->self);
```

## Complete Example

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"

int main(void) {
    Arena* arena = Arena_create(4096);

    int* data = (int*)arena->alloc(arena->self, sizeof(int) * 100);
    for (int i = 0; i < 100; i++) {
        data[i] = i * i;
    }

    printf("First: %d, Last: %d\n", data[0], data[99]);

    arena->destroy(arena->self);
    return 0;
}
```

Compile:

```bash
gcc your_file.c -o program
./program
```

## API Reference

### Creation and Destruction

```c
Arena* Arena_create(size_t size);
```

Creates a new arena with the specified size in bytes.

```c
arena->destroy(arena->self);
```

Destroys the arena and frees all memory.

### Allocation Functions

```c
void* ptr = arena->alloc(arena->self, size_t size);
```

Allocates `size` bytes from the arena. Returns pointer or NULL on failure.

Example: `int* data = (int*)arena->alloc(arena->self, sizeof(int) * 100);`

```c
void* ptr = arena->alloc_aligned(arena->self, size_t size, size_t alignment);
```

Allocates `size` bytes aligned to `alignment` boundary (must be power of 2).

Example: `float* data = (float*)arena->alloc_aligned(arena->self, 1024, 16);`

```c
void* new_ptr = arena->realloc(arena->self, void* ptr, size_t old_size, size_t new_size);
```

Reallocates memory. Tries to expand in place if possible.

Example: `str = (char*)arena->realloc(arena->self, str, 10, 50);`

### Memory Management

```c
arena->reset(arena->self);
```

Resets all allocations. Memory becomes available again but is not freed.

```c
size_t mark = arena->get_mark(arena->self);
```

Returns current allocation offset (checkpoint).

Example: `size_t checkpoint = arena->get_mark(arena->self);`

```c
arena->reset_to_mark(arena->self, size_t mark);
```

Restores arena to a previously saved checkpoint.

Example: `arena->reset_to_mark(arena->self, checkpoint);`

```c
bool success = arena->resize(arena->self, size_t new_size);
```

Manually resizes the current arena chunk.

Example: `if (arena->resize(arena->self, 4096)) { ... }`

### Diagnostics

```c
arena->print_stats(arena->self);
```

Prints detailed memory usage statistics.

## How It Works

### Bump Allocation

The arena uses "bump allocation" - the fastest allocation strategy:

```c
void* ptr = arena->memory + arena->offset;
arena->offset += size;
return ptr;
```

That's it. Just 2-3 CPU cycles.

### Visual: Bump Allocation

```
Step 1: alloc(40 bytes)
┌─────────────────────────────────────┐
│ ┌──────┬────────────────────────────┐ │
│ │ 40B  │      Empty Memory          │ │
│ └──────┴────────────────────────────┘ │
│ offset: 40 ←                        │
└─────────────────────────────────────┘

Step 2: alloc(100 bytes)
┌─────────────────────────────────────┐
│ ┌──────┬────────┬───────────────────┐ │
│ │ 40B  │ 100B   │   Empty Memory    │ │
│ └──────┴────────┴───────────────────┘ │
│ offset: 140 ←                       │
└─────────────────────────────────────┘

Step 3: alloc(200 bytes)
┌─────────────────────────────────────┐
│ ┌──────┬────────┬────────┬─────────┐ │
│ │ 40B  │ 100B   │ 200B   │  Empty  │ │
│ └──────┴────────┴────────┴─────────┘ │
│ offset: 340 ←                       │
└─────────────────────────────────────┘
```

### Automatic Growth

When the arena runs out of space, it automatically creates a new chunk:

```
Arena Full:
┌─────────────────────────────────────┐
│ Chunk 1 (4096 bytes) - FULL         │
│ ┌─────────────────────────────────┐ │
│ │ ████████████████████████████████│ │
│ └─────────────────────────────────┘ │
└─────────────────────────────────────┘
         ↓ Need more space
┌─────────────────────────────────────┐     ┌─────────────────────────────────────┐
│ Chunk 1 (4096 bytes) - FULL         │ ──→ │ Chunk 2 (8192 bytes)                │
│ ┌─────────────────────────────────┐ │     │ ┌────────┬──────────────────────┐ │
│ │ ████████████████████████████████│ │     │ │ Active │      Empty           │ │
│ └─────────────────────────────────┘ │     │ └────────┴──────────────────────┘ │
└─────────────────────────────────────┘     └─────────────────────────────────────┘
```

### Checkpoint and Restore

Save your position, do temporary work, then restore. Marks work across multiple chunks automatically.

```
State 1: Save Checkpoint
┌─────────────────────────────────────┐
│ ┌─────────┬─────────────────────────┐ │
│ │ Used    │  Empty                  │ │
│ └─────────┴─────────────────────────┘ │
│ checkpoint = 500 ←                  │
└─────────────────────────────────────┘

State 2: Temporary Allocations (Multi-Chunk)
┌─────────────────────────────────────┐     ┌─────────────────────────────────────┐
│ Chunk 1                             │ ──→ │ Chunk 2                             │
│ ┌─────────┬─────────────────────────┐ │     │ ┌──────────┬─────────────────────┐ │
│ │ Used    │  Temp in Chunk 1        │ │     │ │ Temp 2   │  Empty              │ │
│ └─────────┴─────────────────────────┘ │     │ └──────────┴─────────────────────┘ │
└─────────────────────────────────────┘     └─────────────────────────────────────┘

State 3: Restore to Checkpoint (Resets Both Chunks)
┌─────────────────────────────────────┐     ┌─────────────────────────────────────┐
│ Chunk 1                             │ ──→ │ Chunk 2                             │
│ ┌─────────┬─────────────────────────┐ │     │ ┌─────────────────────────────────┐ │
│ │ Used    │  Available Again!       │ │     │ │  Reset to 0                     │ │
│ └─────────┴─────────────────────────┘ │     │ └─────────────────────────────────┘ │
│ offset: 500 ← restored              │     │ offset: 0                           │
└─────────────────────────────────────┘     └─────────────────────────────────────┘
```

Note: Marks track absolute position across all chunks, so you can save a checkpoint in one chunk and reset from another chunk safely.

## Common Patterns

### Pattern 1: Game Frame Allocations

```c
Arena* frame_arena = Arena_create(1024 * 1024);

while (game_running) {
    Entity* entities = frame_arena->alloc(frame_arena->self, sizeof(Entity) * count);
    Particle* particles = frame_arena->alloc(frame_arena->self, sizeof(Particle) * p_count);

    render_frame(entities, particles);

    frame_arena->reset(frame_arena->self);
}

frame_arena->destroy(frame_arena->self);
```

### Pattern 2: Temporary Scratch Memory

```c
Arena* scratch = Arena_create(4096);

void process_data(void) {
    size_t checkpoint = scratch->get_mark(scratch->self);

    char* temp_buffer = scratch->alloc(scratch->self, 2048);
    int* temp_array = scratch->alloc(scratch->self, sizeof(int) * 1000);

    scratch->reset_to_mark(scratch->self, checkpoint);
}
```

### Pattern 3: String Building

```c
Arena* string_arena = Arena_create(4096);

char* build_path(Arena* a, const char* dir, const char* file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char* path = a->alloc(a->self, len);
    sprintf(path, "%s/%s", dir, file);
    return path;
}

char* path1 = build_path(string_arena, "/usr", "bin");
char* path2 = build_path(string_arena, "/home", "user");

string_arena->destroy(string_arena->self);
```

### Pattern 4: Aligned SIMD Data

```c
Arena* simd_arena = Arena_create(8192);

float* vectors = (float*)simd_arena->alloc_aligned(simd_arena->self,
                                                     sizeof(float) * 1024,
                                                     16);

simd_arena->destroy(simd_arena->self);
```

## When to Use

### Perfect For:

- Game frame allocations
- Parsers and compilers (AST nodes)
- Loading data files
- String manipulation
- Temporary buffers
- Batch processing

### Not Ideal For:

- Long-lived objects with different lifetimes
- When you need to free individual items
- Thread-shared memory without synchronization

## Visual: Arena vs malloc/free

### malloc/free Pattern

```
malloc(40)  ──→  [Block A]  System call
malloc(100) ──→  [Block B]  System call
malloc(200) ──→  [Block C]  System call

free(A)     ──→  Track each  System call
free(B)     ──→  allocation  System call
free(C)     ──→  manually    System call
```

### Arena Pattern

```
Arena_create(4096)  ──→  [Large Block]  One system call

arena->alloc(arena->self, 40)   ──→  [A|B|C|Empty]  Just pointer math
arena->alloc(arena->self, 100)  ──→  [A|B|C|Empty]  Just pointer math
arena->alloc(arena->self, 200)  ──→  [A|B|C|Empty]  Just pointer math

arena->destroy(arena->self)     ──→  Free all        One call
```

## Multi-File Projects

### main.c

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"

int main(void) {
    Arena* arena = Arena_create(4096);
    process_data(arena);
    arena->destroy(arena->self);
    return 0;
}
```

### module.c

```c
#include "arena.h"

void process_data(Arena* arena) {
    int* data = arena->alloc(arena->self, sizeof(int) * 100);
}
```

### module.h

```c
#include "arena.h"
void process_data(Arena* arena);
```

## Thread Safety

Not thread-safe by default. Use one arena per thread:

```c
_Thread_local Arena* thread_arena = NULL;

void thread_init(void) {
    thread_arena = Arena_create(64 * 1024);
}

void thread_cleanup(void) {
    thread_arena->destroy(thread_arena->self);
}
```

## Benchmarks

Typical performance on modern hardware:

| Metric             | Arena      | malloc      |
| ------------------ | ---------- | ----------- |
| Allocation         | 2-3 cycles | 100+ cycles |
| 1000 allocations   | ~0.001ms   | ~0.1ms      |
| Overhead per alloc | 0 bytes    | 8-16 bytes  |
| Free all           | O(1)       | O(n)        |

## Tips

1. **Pre-allocate if you know the size**

   ```c
   Arena* arena = Arena_create(known_total_size);
   ```

2. **Use checkpoints for nested operations**

   ```c
   size_t mark = arena->get_mark(arena->self);
   arena->reset_to_mark(arena->self, mark);
   ```

3. **Reset instead of destroy for repeated operations**

   ```c
   for (int i = 0; i < iterations; i++) {
       arena->reset(arena->self);
   }
   ```

4. **Monitor usage during development**

   ```c
   arena->print_stats(arena->self);
   ```

5. **Use aligned allocations for SIMD**
   ```c
   float* data = arena->alloc_aligned(arena->self, size, 16);
   ```

## Troubleshooting

### "Undefined reference to Arena_create"

One .c file must have `#define ARENA_IMPLEMENTATION` before including.

### "Multiple definition of Arena_create"

Only ONE file should have `#define ARENA_IMPLEMENTATION`.

### Out of memory

Increase initial size or use reset to reuse memory.

## Example Program

See `example.c` for comprehensive examples covering all features.

Compile:

```bash
gcc example.c -o example
./example
```

Or:

```bash
clang example.c -o example
./example
```

## License

Public domain. Use however you want.
