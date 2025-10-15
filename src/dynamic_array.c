#include "../include/dynamic_array.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Initial capacity for the dynamic array
#define INIT_CAPACITY 16

extern void clean_exit(int return_code);


// Helper to handle realloc failure
void da_error_exit(const char *msg) {
    perror(msg);
    clean_exit(EXIT_FAILURE);
}

void da_resize(DynamicArray *da)
{
    size_t new_capacity = da->capacity * 2;
    // Check for potential overflow (unlikely for typical shell use, but good practice)
    if (new_capacity < da->capacity) {
        da_error_exit("realloc (capacity overflow)");
    }
    
    char **new_data = (char **)realloc(da->data, new_capacity * sizeof(char *));

    if (!new_data) {
        da_error_exit("realloc");
    }

    da->data = new_data;
    da->capacity = new_capacity;
}

/**
 * @Brief Create a new DynamicArray with given initial capacity
 *
 * @param init_capacity The initial capacity for the array
 * @return DynamicArray* A pointer to the newly created DynamicArray
 */
DynamicArray* da_create(size_t init_capacity)
{
    // If 0 is passed, use the default
    if (init_capacity == 0) {
        init_capacity = INIT_CAPACITY;
    }

    DynamicArray *da = (DynamicArray *)malloc(sizeof(DynamicArray));
    if (!da) {
        da_error_exit("malloc");
    }

    da->data = (char **)malloc(init_capacity * sizeof(char *));
    if (!da->data) {
        free(da); 
        da_error_exit("malloc");
    }

    da->size = 0;
    da->capacity = init_capacity;

    return da;
}

/**
 * @Brief Add element to Dynamic Array at the end. Handles resizing if necessary.
 * Copies the string value.
 *
 * @param da The DynamicArray to add to
 * @param val The string value to copy and store
 */
void da_put(DynamicArray *da, const char* val)
{
    if (da->size >= da->capacity) {
        da_resize(da); 
    }

    // Allocate memory for the string value and copy it
    char *new_val = strdup(val);
    if (!new_val) {
        da_error_exit("strdup");
    }

    da->data[da->size] = new_val;
    da->size++;
}

/**
 * @Brief Get element at an index (NULL if index is out of bounds)
 *
 * @param da The DynamicArray
 * @param ind The index of the element
 * @return char* The element string, or NULL if index is out of bounds
 */
char *da_get(DynamicArray *da, const size_t ind)
{
    if (ind >= da->size) {
        return NULL; // Out of bounds
    }
    return da->data[ind];
}

/**
 * @Brief Delete Element at an index (handles packing by shifting subsequent elements)
 *
 * @param da The DynamicArray
 * @param ind The index of the element to delete
 */
void da_delete(DynamicArray *da, const size_t ind)
{
    if (ind >= da->size) {
        return; // Index out of bounds, do nothing
    }

    // Free the memory for the string being deleted
    free(da->data[ind]); 

    // Shift elements to fill the gap
    for (size_t i = ind; i < da->size - 1; i++) {
        da->data[i] = da->data[i + 1];
    }

    da->size--;
}

/**
 * @Brief Print Elements line after line
 *
 * @param da The DynamicArray
 */
void da_print(DynamicArray *da)
{
    /*
    ignore the history command itself when we type 'history',
    so iterate only till 2nd last entry.
    */ 
    for (size_t i = 0; i < da->size-1; i++) {
        fprintf(stdout, "%s", da->data[i]);
    }
    fflush(stdout);
       
}

/**
 * @Brief Free whole DynamicArray
 * Frees all stored strings and the array itself.
 *
 * @param da The DynamicArray to free
 */
void da_free(DynamicArray *da)
{
    if (da == NULL) return;

    // free mem.
    for (size_t i = 0; i < da->size; i++) {
        if (da->data[i] != NULL) {
            free(da->data[i]);
        }
    }
    free(da->data);
    free(da);
}
