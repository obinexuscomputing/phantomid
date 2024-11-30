#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <pthread.h>

typedef struct PhantomHistory PhantomHistory;

void phantom_history_init(PhantomHistory* history, bool enable);
void phantom_history_add(PhantomHistory* history, const char* entry);
void phantom_history_clear(PhantomHistory* history);
void phantom_notify_users(const char* message);
void phantom_user_enter(PhantomHistory* history, const char* user_id);
void phantom_user_exit(PhantomHistory* history, const char* user_id);

#endif // HISTORY_H