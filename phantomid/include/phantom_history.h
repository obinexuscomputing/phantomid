#ifndef PHANTOM_HISTORY_H
#define PHANTOM_HISTORY_H

#include <stdbool.h>  
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Rest of the file remains unchanged

typedef struct PhantomHistory PhantomHistory;

void phantom_history_init(PhantomHistory* history, bool enable);
void phantom_history_add(PhantomHistory* history, const char* entry);
void phantom_history_clear(PhantomHistory* history);
void phantom_notify_users(const char* message);
void phantom_user_enter(PhantomHistory* history, const char* user_id);
void phantom_user_exit(PhantomHistory* history, const char* user_id);

#endif // PHANTOM_HISTORY_H
