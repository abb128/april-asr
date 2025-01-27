/*
 * Copyright (C) 2025 abb128
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

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "april_api.h"
#include "fbank.h"
#include "log.h"

int g_client_version = 0;
LogLevel g_loglevel = LEVEL_WARNING;

void aam_api_init(int version){
    g_client_version = version;

    char *log_env = getenv("APRIL_LOG_LEVEL");
    if(log_env){
        for(int i=0; i<=LEVEL_COUNT; i++){
            if(strcmp(log_env, LogLevelStrings[i]) == 0) {
                g_loglevel = (LogLevel)i;
            }
        }
        LOG_DEBUG("Using LogLevel %d", g_loglevel);
    }
}