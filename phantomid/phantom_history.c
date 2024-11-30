#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct PhantomHistory {
    char** entries;
    size_t size;
    size_t capacity;
    pthread_mutex_t lock;
    bool enabled;  // Flag to enable/disable history
} PhantomHistory;

// Initialize history
void phantom_history_init(PhantomHistory* history, bool enable) {
    history->enabled = enable;
    if (enable) {
        history->entries = malloc(sizeof(char*) * 10);
        history->size = 0;
        history->capacity = 10;
        pthread_mutex_init(&history->lock, NULL);
    } else {
        history->entries = NULL;
        history->size = 0;
        history->capacity = 0;
    }
}

// Add an entry to history
void phantom_history_add(PhantomHistory* history, const char* entry) {
    if (!history->enabled) return;

    pthread_mutex_lock(&history->lock);

    if (history->size >= history->capacity) {
        history->capacity *= 2;
        history->entries = realloc(history->entries, sizeof(char*) * history->capacity);
    }
    history->entries[history->size++] = strdup(entry);

    pthread_mutex_unlock(&history->lock);
}

// Clear history
void phantom_history_clear(PhantomHistory* history) {
    if (!history->enabled) return;

    pthread_mutex_lock(&history->lock);

    for (size_t i = 0; i < history->size; i++) {
        free(history->entries[i]);
    }
    free(history->entries);
    history->entries = NULL;
    history->size = 0;

    pthread_mutex_unlock(&history->lock);
    pthread_mutex_destroy(&history->lock);
}

// Notify users of entry/exit
void phantom_notify_users(const char* message) {
    printf("NOTIFY: %s\n", message);
}



void phantom_user_enter(PhantomHistory* history, const char* user_id) {
    if (!history->enabled) return;

    char message[128];
    snprintf(message, sizeof(message), "User %s has entered.", user_id);

    // Notify all users
    phantom_notify_users(message);

    // Add to history
    phantom_history_add(history, message);
}

void phantom_user_exit(PhantomHistory* history, const char* user_id) {
    if (!history->enabled) return;

    char message[128];
    snprintf(message, sizeof(message), "User %s has left.", user_id);

    // Notify all users
    phantom_notify_users(message);

    // Clear history
    phantom_history_clear(history);
}
