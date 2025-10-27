#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct Arena Arena;

typedef struct Arena {
    Arena* self;
    void* memory;
    size_t size;
    size_t offset;
    size_t peak_usage;
    Arena* next;
    Arena* head;
    size_t allocation_count;
    size_t total_allocated;

    void* (*alloc)(Arena* self, size_t size);
    void* (*alloc_aligned)(Arena* self, size_t size, size_t alignment);
    void* (*realloc)(Arena* self, void* ptr, size_t old_size, size_t new_size);
    void (*reset)(Arena* self);
    void (*reset_to_mark)(Arena* self, size_t mark);
    size_t (*get_mark)(Arena* self);
    void (*destroy)(Arena* self);
    void (*print_stats)(Arena* self);
    bool (*resize)(Arena* self, size_t new_size);
} Arena;

Arena* Arena_create(size_t size);

#ifdef ARENA_IMPLEMENTATION

static void* arena_alloc(Arena* self, size_t size);
static void* arena_alloc_aligned(Arena* self, size_t size, size_t alignment);
static void* arena_realloc(Arena* self, void* ptr, size_t old_size, size_t new_size);
static void arena_reset(Arena* self);
static void arena_reset_to_mark(Arena* self, size_t mark);
static size_t arena_get_mark(Arena* self);
static void arena_destroy(Arena* self);
static void arena_print_stats(Arena* self);
static bool arena_resize(Arena* self, size_t new_size);

static inline size_t align_forward(size_t ptr, size_t alignment) {
    size_t modulo = ptr & (alignment - 1);
    if (modulo != 0) {
        ptr += alignment - modulo;
    }
    return ptr;
}

static inline bool is_power_of_two(size_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

Arena* Arena_create(size_t size) {
    if (size == 0) {
        fprintf(stderr, "Arena: Cannot create arena with size 0\n");
        return NULL;
    }

    Arena* self = (Arena*)malloc(sizeof(Arena));
    if (!self) {
        fprintf(stderr, "Arena: Failed to allocate arena struct\n");
        return NULL;
    }

    self->memory = malloc(size);
    if (!self->memory) {
        fprintf(stderr, "Arena: Failed to allocate arena memory of size %zu\n", size);
        free(self);
        return NULL;
    }

    self->self = self;
    self->size = size;
    self->offset = 0;
    self->peak_usage = 0;
    self->next = NULL;
    self->head = self;
    self->allocation_count = 0;
    self->total_allocated = 0;
    self->alloc = arena_alloc;
    self->alloc_aligned = arena_alloc_aligned;
    self->realloc = arena_realloc;
    self->reset = arena_reset;
    self->reset_to_mark = arena_reset_to_mark;
    self->get_mark = arena_get_mark;
    self->destroy = arena_destroy;
    self->print_stats = arena_print_stats;
    self->resize = arena_resize;

    return self;
}

static void* arena_alloc(Arena* self, size_t size) {
    if (!self || size == 0) {
        return NULL;
    }

    if (self->offset + size <= self->size) {
        void* ptr = (uint8_t*)self->memory + self->offset;
        self->offset += size;
        self->allocation_count++;
        self->total_allocated += size;

        if (self->offset > self->peak_usage) {
            self->peak_usage = self->offset;
        }

        return ptr;
    }

    size_t new_arena_size = self->size * 2;
    if (new_arena_size < size) {
        new_arena_size = size * 2;
    }

    Arena* new_chunk = Arena_create(new_arena_size);
    if (!new_chunk) {
        fprintf(stderr, "Arena: Failed to grow arena\n");
        return NULL;
    }

    new_chunk->head = self->head;
    new_chunk->next = NULL;

    Arena* current = self;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_chunk;

    return new_chunk->alloc(new_chunk, size);
}

static void* arena_alloc_aligned(Arena* self, size_t size, size_t alignment) {
    if (!self || size == 0) {
        return NULL;
    }

    if (!is_power_of_two(alignment)) {
        fprintf(stderr, "Arena: Alignment must be a power of 2\n");
        return NULL;
    }

    size_t current_ptr = (size_t)self->memory + self->offset;
    size_t aligned_ptr = align_forward(current_ptr, alignment);
    size_t padding = aligned_ptr - current_ptr;

    if (self->offset + padding + size <= self->size) {
        self->offset += padding;
        void* ptr = (uint8_t*)self->memory + self->offset;
        self->offset += size;
        self->allocation_count++;
        self->total_allocated += size + padding;

        if (self->offset > self->peak_usage) {
            self->peak_usage = self->offset;
        }

        return ptr;
    }

    size_t new_arena_size = self->size * 2;
    size_t required_size = size + alignment;
    if (new_arena_size < required_size) {
        new_arena_size = required_size * 2;
    }

    Arena* new_chunk = Arena_create(new_arena_size);
    if (!new_chunk) {
        fprintf(stderr, "Arena: Failed to grow arena\n");
        return NULL;
    }

    new_chunk->head = self->head;
    new_chunk->next = NULL;

    Arena* current = self;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = new_chunk;

    return new_chunk->alloc_aligned(new_chunk, size, alignment);
}

static void* arena_realloc(Arena* self, void* ptr, size_t old_size, size_t new_size) {
    if (!self) {
        return NULL;
    }

    if (ptr == NULL) {
        return arena_alloc(self, new_size);
    }

    if (new_size == 0) {
        return NULL;
    }

    void* expected_ptr = (uint8_t*)self->memory + (self->offset - old_size);
    if (ptr == expected_ptr) {
        if (self->offset - old_size + new_size <= self->size) {
            self->offset = self->offset - old_size + new_size;
            if (self->offset > self->peak_usage) {
                self->peak_usage = self->offset;
            }
            return ptr;
        }
    }

    void* new_ptr = arena_alloc(self, new_size);
    if (new_ptr) {
        size_t copy_size = old_size < new_size ? old_size : new_size;
        memcpy(new_ptr, ptr, copy_size);
    }

    return new_ptr;
}

static void arena_reset(Arena* self) {
    if (!self) {
        return;
    }

    Arena* head = self->head;
    Arena* current = head;
    while (current != NULL) {
        current->offset = 0;
        current->allocation_count = 0;
        current = current->next;
    }
}

static void arena_reset_to_mark(Arena* self, size_t mark) {
    if (!self) {
        return;
    }

    Arena* head = self->head;
    Arena* current = head;
    size_t cumulative_size = 0;
    bool found = false;

    while (current != NULL) {
        size_t next_cumulative = cumulative_size + current->size;

        if (!found && mark <= next_cumulative) {
            current->offset = mark - cumulative_size;
            found = true;
        } else if (found) {
            current->offset = 0;
            current->allocation_count = 0;
        }

        cumulative_size = next_cumulative;
        current = current->next;
    }
}

static size_t arena_get_mark(Arena* self) {
    if (!self) {
        return 0;
    }

    Arena* head = self->head;
    Arena* current = head;
    size_t cumulative_offset = 0;

    while (current != NULL) {
        if (current == self) {
            return cumulative_offset + current->offset;
        }
        cumulative_offset += current->offset;
        current = current->next;
    }

    return cumulative_offset;
}

static bool arena_resize(Arena* self, size_t new_size) {
    if (!self || new_size == 0) {
        return false;
    }

    if (new_size < self->offset) {
        fprintf(stderr, "Arena: Cannot resize to smaller than current usage\n");
        return false;
    }

    void* new_memory = realloc(self->memory, new_size);
    if (!new_memory) {
        fprintf(stderr, "Arena: Failed to resize arena\n");
        return false;
    }

    self->memory = new_memory;
    self->size = new_size;

    return true;
}

static void arena_print_stats(Arena* self) {
    if (!self) {
        return;
    }

    Arena* head = self->head;

    printf("\n=== Arena Statistics ===\n");

    size_t total_size = 0;
    size_t total_used = 0;
    size_t total_allocations = 0;
    size_t chunk_count = 0;

    Arena* current = head;
    while (current != NULL) {
        chunk_count++;
        total_size += current->size;
        total_used += current->offset;
        total_allocations += current->allocation_count;

        printf("Chunk %zu:\n", chunk_count);
        printf("  Size: %zu bytes\n", current->size);
        printf("  Used: %zu bytes (%.2f%%)\n",
               current->offset,
               (current->offset * 100.0) / current->size);
        printf("  Peak: %zu bytes\n", current->peak_usage);
        printf("  Allocations: %zu\n", current->allocation_count);

        current = current->next;
    }

    printf("\nTotal Summary:\n");
    printf("  Chunks: %zu\n", chunk_count);
    printf("  Total Size: %zu bytes\n", total_size);
    printf("  Total Used: %zu bytes (%.2f%%)\n",
           total_used,
           total_size > 0 ? (total_used * 100.0) / total_size : 0.0);
    printf("  Total Allocations: %zu\n", total_allocations);
    printf("========================\n\n");
}

static void arena_destroy(Arena* self) {
    if (!self) {
        return;
    }

    Arena* head = self->head;
    Arena* current = head;
    while (current != NULL) {
        Arena* next = current->next;
        free(current->memory);
        free(current);
        current = next;
    }
}

#endif
#endif
