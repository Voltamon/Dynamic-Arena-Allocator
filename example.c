#define ARENA_IMPLEMENTATION
#include "arena.h"

void basic_usage(void) {
    printf("=== Basic Usage ===\n");
    Arena* arena = Arena_create(4096);

    int* numbers = (int*)arena->alloc(arena->self, sizeof(int) * 10);
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * i;
    }

    char* message = (char*)arena->alloc(arena->self, 256);
    sprintf(message, "Allocated %d integers", 10);
    printf("%s: ", message);
    for (int i = 0; i < 5; i++) {
        printf("%d ", numbers[i]);
    }
    printf("...\n\n");

    arena->destroy(arena->self);
}

void aligned_allocations(void) {
    printf("=== Aligned Allocations ===\n");
    Arena* arena = Arena_create(8192);

    float* simd_data = (float*)arena->alloc_aligned(arena->self, sizeof(float) * 16, 16);
    printf("16-byte aligned address: %p\n", (void*)simd_data);
    printf("Is 16-byte aligned: %s\n", ((size_t)simd_data % 16 == 0) ? "Yes" : "No");

    double* cache_line = (double*)arena->alloc_aligned(arena->self, sizeof(double) * 8, 64);
    printf("64-byte aligned address: %p\n", (void*)cache_line);
    printf("Is 64-byte aligned: %s\n\n", ((size_t)cache_line % 64 == 0) ? "Yes" : "No");

    arena->destroy(arena->self);
}

void automatic_growth(void) {
    printf("=== Automatic Growth ===\n");
    Arena* arena = Arena_create(1024);

    printf("Initial arena size: 1024 bytes\n");

    char* small = (char*)arena->alloc(arena->self, 500);
    printf("Allocated 500 bytes\n");

    char* large = (char*)arena->alloc(arena->self, 2048);
    printf("Allocated 2048 bytes (triggers growth)\n");

    arena->print_stats(arena->self);

    arena->destroy(arena->self);
}

void checkpoint_restore(void) {
    printf("=== Checkpoint and Restore ===\n");
    Arena* arena = Arena_create(4096);

    int* data = (int*)arena->alloc(arena->self, sizeof(int) * 10);
    for (int i = 0; i < 10; i++) {
        data[i] = i;
    }
    printf("Allocated permanent data\n");

    size_t checkpoint = arena->get_mark(arena->self);
    printf("Saved checkpoint at offset: %zu\n", checkpoint);

    char* temp = (char*)arena->alloc(arena->self, 1000);
    sprintf(temp, "Temporary allocation");
    printf("Allocated temporary data: %s\n", temp);
    printf("Current offset: %zu\n", arena->get_mark(arena->self));

    arena->reset_to_mark(arena->self, checkpoint);
    printf("Restored to checkpoint\n");
    printf("Offset after restore: %zu\n", checkpoint);
    printf("Permanent data still valid: %d %d %d...\n\n", data[0], data[1], data[2]);

    arena->destroy(arena->self);
}

void full_reset(void) {
    printf("=== Full Reset ===\n");
    Arena* arena = Arena_create(4096);

    for (int i = 0; i < 5; i++) {
        arena->alloc(arena->self, 100);
    }
    printf("Made 5 allocations\n");
    printf("Offset before reset: %zu\n", arena->get_mark(arena->self));

    arena->reset(arena->self);
    printf("Arena reset\n");
    printf("Offset after reset: %zu\n", arena->get_mark(arena->self));

    char* new_data = (char*)arena->alloc(arena->self, 50);
    sprintf(new_data, "Fresh start");
    printf("New allocation after reset: %s\n\n", new_data);

    arena->destroy(arena->self);
}

void reallocation(void) {
    printf("=== Reallocation ===\n");
    Arena* arena = Arena_create(4096);

    char* str = (char*)arena->alloc(arena->self, 10);
    sprintf(str, "Hello");
    printf("Original: %s (size: 10)\n", str);

    str = (char*)arena->realloc(arena->self, str, 10, 50);
    strcat(str, ", World! This is longer.");
    printf("After realloc: %s (size: 50)\n\n", str);

    arena->destroy(arena->self);
}

void statistics(void) {
    printf("=== Statistics ===\n");
    Arena* arena = Arena_create(2048);

    arena->alloc(arena->self, 100);
    arena->alloc(arena->self, 200);
    arena->alloc(arena->self, 300);
    arena->alloc_aligned(arena->self, 500, 16);

    arena->print_stats(arena->self);

    arena->destroy(arena->self);
}

void game_frame_pattern(void) {
    printf("=== Game Frame Pattern ===\n");
    Arena* frame_arena = Arena_create(1024 * 64);

    for (int frame = 0; frame < 3; frame++) {
        printf("Frame %d:\n", frame);

        int entity_count = 100 + frame * 50;
        int* entities = (int*)frame_arena->alloc(frame_arena->self, sizeof(int) * entity_count);

        float* particles = (float*)frame_arena->alloc(frame_arena->self, sizeof(float) * 500);

        char* debug_str = (char*)frame_arena->alloc(frame_arena->self, 128);
        sprintf(debug_str, "Frame %d rendered with %d entities", frame, entity_count);

        printf("  %s\n", debug_str);
        printf("  Arena usage: %zu bytes\n", frame_arena->get_mark(frame_arena->self));

        frame_arena->reset(frame_arena->self);
    }
    printf("\n");

    frame_arena->destroy(frame_arena->self);
}

char* build_path(Arena* a, const char* dir, const char* file) {
    size_t len = strlen(dir) + strlen(file) + 2;
    char* path = (char*)a->alloc(a->self, len);
    sprintf(path, "%s/%s", dir, file);
    return path;
}

void string_builder_pattern(void) {
    printf("=== String Builder Pattern ===\n");
    Arena* str_arena = Arena_create(4096);

    char* path1 = build_path(str_arena, "/usr/local", "bin");
    char* path2 = build_path(str_arena, "/home/user", "documents");
    char* path3 = build_path(str_arena, "/var/log", "system.log");

    printf("Built paths:\n");
    printf("  %s\n", path1);
    printf("  %s\n", path2);
    printf("  %s\n\n", path3);

    str_arena->destroy(str_arena->self);
}

void resize_arena(void) {
    printf("=== Manual Resize ===\n");
    Arena* arena = Arena_create(1024);

    printf("Initial size: %zu bytes\n", arena->self->size);

    arena->alloc(arena->self, 500);
    printf("Allocated 500 bytes\n");

    if (arena->resize(arena->self, 4096)) {
        printf("Resized to 4096 bytes\n");
        printf("New size: %zu bytes\n\n", arena->self->size);
    }

    arena->destroy(arena->self);
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   Arena Allocator - Full Examples     ║\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("\n");

    basic_usage();
    aligned_allocations();
    automatic_growth();
    checkpoint_restore();
    full_reset();
    reallocation();
    statistics();
    game_frame_pattern();
    string_builder_pattern();
    resize_arena();

    printf("╔════════════════════════════════════════╗\n");
    printf("║          All Examples Complete         ║\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("\n");

    return 0;
}
