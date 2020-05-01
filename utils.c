/*
 * =================================================================================
 * Arm-BwBench - A bandwidth benchmarking suite for Armv8
 * 
 * Copyright (C) 2020 Key Liao (Liao Qiucheng)
 * 
 * This program is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software 
 * Foundation, either version 3 of the License, or (at your option) any later 
 * version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY 
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with 
 * this program. If not, see https://www.gnu.org/licenses/.
 * 
 * =================================================================================
 * utils.c
 * Description: some accessory functions. 
 * Author: Key Liao
 * Modified: Apr. 18th, 2020
 * Email: keyliaohpc@gmail.com
 * =================================================================================
 */


#include <string.h>
#include <limits.h>     /* PATH_MAX */
#include <sys/stat.h>   /* mkdir(2) */
#include <errno.h>

int make_dir(const char *path)
{
    const size_t len = strlen(path);
    char _path[PATH_MAX];
    char *p; 

    errno = 0;

    // Copy string so its mutable
    if(len > sizeof(_path)-1) {
        errno = ENAMETOOLONG;
        return -1; 
    }   
    strcpy(_path, path);

    // Iterate the string
    for(p = _path + 1; *p; p++) {
        if (*p == '/') {
            // Temporarily truncate 
            *p = '\0';

            if(mkdir(_path, S_IRWXU) != 0) {
                if(errno != EEXIST)
                    return -1; 
            }

            *p = '/';
        }
    }   

    if(mkdir(_path, S_IRWXU) != 0){
        if(errno != EEXIST)
            return -1; 
    }   
    return 0;
}