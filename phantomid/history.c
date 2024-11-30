#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "history.h"


void phantom_history_init(PhantomHistory* history, bool enable) {
    history->enabled = enable;
    history->entries = NULL;
    history->size = 0;
    history->capacity = 0;
    
    if (enable) {
        history->entries = malloc(sizeof(char*) * 10);
        if (history->entries != NULL) {
            history->capacity = 10;
            pthread_mutex_init(&history->lock, NULL);
        }
    }
}

void phantom_history_add(PhantomHistory* history, const char* entry) {
    if (!history->enabled || entry == NULL) return;
    
    pthread_mutex_lock(&history->lock);
    
    if (history->size >= history->capacity) {
        size_t new_capacity = history->capacity * 2;
        char** new_entries = realloc(history->entries, sizeof(char*) * new_capacity);
        if (new_entries != NULL) {
            history->entries = new_entries;
            history->capacity = new_capacity;
        } else {
            // Handle allocation failure
            pthread_mutex_unlock(&history->lock);
            return;
        }
    }
    
    char* entry_copy = strdup(entry);
    if (entry_copy != NULL) {
        history->entries[history->size++] = entry_copy;
    }
    
    pthread_mutex_unlock(&history->lock);
}

void phantom_history_clear(PhantomHistory* history) {
    if (!history->enabled) return;
    
    pthread_mutex_lock(&history->lock);
    
    for (size_t i = 0; i < history->size; i++) {
        free(history->entries[i]);
    }
    free(history->entries);
    history->entries = NULL;
    history->size = 0;
    history->capacity = 0;
    
    pthread_mutex_unlock(&history->lock);
    pthread_mutex_destroy(&history->lock);
}

void phantom_notify_users(const char* message) {
    printf("NOTIFY: %s\n", message);
}

void phantom_user_enter(PhantomHistory* history, const char* user_id) {
    if (!history->enabled || user_id == NULL) return;
    
    char message[128];
    snprintf(message, sizeof(message), "User %s has entered.", user_id);
    
    phantom_notify_users(message);
    phantom_history_add(history, message);
}

void phantom_user_exit(PhantomHistory* history, const char* user_id) {
    if (!history->enabled || user_id == NULL) return;
    
    char message[128];
    snprintf(message, sizeof(message), "User %s has left.", user_id);
    
    phantom_notify_users(message);
    phantom_history_add(history, message);
}