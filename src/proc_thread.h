/*
 * Copyright (C) 2022 abb128
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _APRIL_PROC_THREAD
#define _APRIL_PROC_THREAD

#include "common.h"
#include <stdbool.h>

#define PT_FLAG_KILL 1
#define PT_FLAG_AUDIO 2
#define PT_FLAG_FLUSH 4
#define PT_FLAG_OVERFLOW 8

struct ProcThread_i;

typedef struct ProcThread_i * ProcThread;
typedef void(*ProcThreadCallback)(void*, int);

ProcThread pt_create(ProcThreadCallback callback, void *userdata);
void pt_raise(ProcThread thread, int flag);
// Clears and returns the pending flags in mask, for use from the callback
int pt_take_flags(ProcThread thread, int mask);
bool pt_wait_idle(ProcThread thread);
void pt_free(ProcThread thread);

#endif
