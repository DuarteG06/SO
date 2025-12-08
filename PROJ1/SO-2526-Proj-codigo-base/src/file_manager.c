#include "file_manager.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

char *read_file(int fd){
    int stride = 128;
    char *buffer = malloc(stride* sizeof(char));
    if (!buffer) return NULL;
    int buf_free = stride;
    int buf_total = stride;
    int buf_bytes_written =0;

    /* read the contents of the file */
    int bytes_read =1;
    int done = 0;
    while (bytes_read !=0) {
        if(buf_free < stride){
                buffer = realloc(buffer, buf_total +stride);
                buf_free += stride;
                buf_total += stride;
                if(buffer == NULL){
                    free(buffer);
                    return 0;
                }
        }

        bytes_read = read(fd, buffer + done, stride);

        if (bytes_read < 0) {
            perror("read error");
            return NULL;
        }

        /* if we read 0 bytes, we're done */
        if (bytes_read == 0) {
            break;
        }

        /**
         * it might not have managed to read all data.
         * like on open-write, if you're curious, try to find out why, in this
         * case, the program will always be able to read it all.
         */
        done += bytes_read;
        buf_free -= bytes_read;
        buf_bytes_written += bytes_read;
    }
    buffer[done] = '\0';
    return buffer;
}


int is_lvl_file(char *file){
    size_t len = strlen(file);
    if (len < 4) return 0;
    return strcmp(file + len - 4, ".lvl") == 0;
}



char **get_lvl_files(char *inputdir, int *count){
    DIR *dir = opendir(inputdir);
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }
    struct dirent *entry;
    int free_size =4;
    int n =0;
    char **files = malloc(free_size *sizeof(char*));
    if (!files) {
        perror("malloc");
        closedir(dir);
        return NULL;
    }
    while((entry =readdir(dir)) !=NULL){
        if(is_lvl_file(entry->d_name)){
            if(n>=free_size){
                free_size *=2;
                files = realloc(files, free_size * sizeof(char*));
                if(files == NULL){
                    perror("realloc lvl files");
                    for (int i = 0; i < n; i++) free(files[i]);
                    free(files);
                    closedir(dir);
                    return NULL;
                }
            }
            files[n] = strdup(entry->d_name);
            if (!files[n]) {
                perror("strdup");
                for (int i = 0; i < n; i++) free(files[i]);
                free(files);
                closedir(dir);
                return NULL;
            }
            n++;
        }
    }
    closedir(dir);
    *count = n;
    return files;
}

void read_lvl_file(int fd, board_t *board, char *path){
    char *start = read_file(fd);
    char *end;
    int line_number =0;
    while ((end = strchr(start, '\n')) != NULL) {
        *end = '\0';  
        if(start[0] != '#'){
            if(strncmp(start, "DIM ", 4) ==0){
                char *rest = start + 4;
                int width, height;
                sscanf(rest, "%d %d", &width, &height);
                board->width = width;
                board->height = height;
                board->board = calloc(board->width * board->height, sizeof(board_pos_t));
                
                
            }else if(strncmp(start, "PAC ", 4) ==0){
                char *rest = start + 4;
                char pac_path[128];
                snprintf(pac_path, sizeof(pac_path), "%s/%s", path, rest);
                int fd = open(pac_path, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    return;
                }
                read_pac_file(fd, board);

            }else if(strncmp(start, "MON ", 4) ==0){
                
            }else if(strncmp(start, "TEMPO ", 6) ==0){
                char *rest = start + 6;
                int tempo;
                sscanf(rest, "%d", &tempo);
                board->tempo =tempo;
            }else{
                char c;

                for (int i = 0; start[i] != '\0'; i++) {
                    c = start[i];
                    if(c =='X'){
                        board->board[board->width*line_number + i].content = 'W';
                    }else if(c == 'o'){
                        board->board[board->width*line_number + i].content = ' ';
                        board->board[board->width*line_number + i].has_dot = 1;
                    }else if(c == '@'){
                        board->board[board->width*line_number + i].content = ' ';
                        board->board[board->width*line_number + i].has_portal = 1;
                    }
                    
                }
                line_number++;
                
            }
            
        }
        start = end + 1; // move to the next line
    }
    // Provavelmente n será util
    if (*start != '\0') {
        printf("Line: %s\n", start);
    }
}


void read_pac_file(int fd, board_t *board){
    char *start = read_file(fd);
    char *end;

    while ((end = strchr(start, '\n')) != NULL) {
        *end = '\0';  
        
        if(start[0] != '#'){
            if(strncmp(start, "POS ", 4) ==0){
                char *rest = start + 4;
                int X, Y;
                sscanf(rest, "%d %d", &X, &Y);
                board->pacmans[0].pos_x = X;
                board->pacmans[0].pos_y = Y;
            }
        }
        start = end + 1; // move to the next line
    }
    return;
}