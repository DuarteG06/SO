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

void free_lvl_files(char **lvl_files, int n){
    for (int i = 0; i < n; i++) free(lvl_files[i]);
    free(lvl_files);
}

int lvl_comparator(const void *a, const void *b) {
    char *fa = *(char **)a;
    char *fb = *(char **)b;

    // extract numbers
    int na = atoi(fa);
    int nb = atoi(fb);

    return (na - nb);
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
                    free_lvl_files(files, n);
                    closedir(dir);
                    return NULL;
                }
            }
            files[n] = strdup(entry->d_name);
            if (!files[n]) {
                perror("strdup");
                free_lvl_files(files, n);
                closedir(dir);
                return NULL;
            }
            n++;
        }
    }
    closedir(dir);
    *count = n;
    qsort(files, n, sizeof(char *), lvl_comparator);
    return files;
}


void set_board_dim(char *dim, board_t *board){
    int width, height;
    sscanf(dim, "%d %d", &width, &height);
    board->width = width;
    board->height = height;
    board->board = calloc(board->width * board->height, sizeof(board_pos_t));
}

void prepare_and_read_pac_file(board_t *board, char *line, int points, char *dirpath){
    snprintf(board->pacman_file, sizeof(board->pacman_file), "%s", line);
    int pacman_count =1;
    board->n_pacmans = pacman_count;
    board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
    char pac_path[128];
    snprintf(pac_path, sizeof(pac_path), "%s/%s", dirpath, line);
    int fd = open(pac_path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }
    load_pacman(board, fd, points);
    close(fd);
}

void set_memory_for_ghosts(board_t *board, char *mon_files){
    int ghost_count = 1;
    for (int j = 0; mon_files[j] != '\0'; j++) {
        if (mon_files[j] == ' '){
            ghost_count++;
        }
    }
    board->n_ghosts = ghost_count;
    board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));
}

void prepare_and_read_mon_file(board_t *board, char *mon_files, char *dirpath){
    int ghost_index =0;
    char *mon_file = strtok(mon_files, " ");

    while(mon_file != NULL){
        snprintf(board->ghosts_files[ghost_index], sizeof(board->ghosts_files[ghost_index]), "%s", mon_file);
        char mon_path[128];
        snprintf(mon_path, sizeof(mon_path), "%s/%s", dirpath, mon_file);
        int fd = open(mon_path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return;
        }
        load_ghost(board, fd, ghost_index);
        mon_file = strtok(NULL, " ");
        close(fd);
        ghost_index++;
    }
}

void store_game_board(board_t *board, char *line, int line_number){
    char c;
    for (int i = 0; line[i] != '\0'; i++) {
        c = line[i];
        char current =board->board[board->width*line_number + i].content;
        if(current != 'P' && current != 'M'){
            if(c =='X'){
                board->board[board->width*line_number + i].content = 'W';
            }else if(c == 'o'){
                board->board[board->width*line_number + i].content = ' ';
                board->board[board->width*line_number + i].has_dot = 1;
            }else if(c == '@'){
                board->board[board->width*line_number + i].content = ' ';
                board->board[board->width*line_number + i].has_portal = 1;
            }
        }else{
            board->board[board->width*line_number + i].has_dot = 1;
        }   
    }
}

void load_pacman_for_player(board_t *board, int points){
    board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
    int pacman_count =1;
    board->n_pacmans = pacman_count;
    board->pacmans[0].n_moves = 0;
    board->pacmans[0].alive =1;
    board->pacmans[0].points = points;
    for(int i =0; i<board->width * board->height; i++){
        if(board->board[i].content == ' ' && board->board[i].has_portal !=1){
            int pos_y = i / board->width;
            int pos_x = i % board->width;
            board->pacmans[0].pos_x = pos_x;
            board->pacmans[0].pos_y = pos_y;
            board->board[pos_y * board->width + pos_x].content = 'P';
            break;
        }
    }
}


void store_pac_pos(board_t *board, char *linePos){
    int X, Y;
    sscanf(linePos, "%d %d", &X, &Y);
    board->pacmans[0].pos_x = X;
    board->pacmans[0].pos_y = Y;
    board->board[Y * board->width + X].content = 'P';
}

void store_pac_passo(board_t *board, char *linePasso){
    int passo;
    sscanf(linePasso, "%d", &passo);
    board->pacmans[0].passo = passo;
    board->pacmans[0].waiting = passo; //not sure
}

void store_pac_moves(board_t *board, char *command, int move_index){
    int turns_left;
    char move;
    sscanf(command, "%c %d", &move, &turns_left);
    switch (move)
    {
    case 'T':
        board->pacmans[0].moves[move_index].command = move;
        board->pacmans[0].moves[move_index].turns_left = turns_left;
        board->pacmans[0].moves[move_index].turns = 1;
        break;
    
    default:
        board->pacmans[0].moves[move_index].command = move;
        board->pacmans[0].moves[move_index].turns_left = 1;
        board->pacmans[0].moves[move_index].turns = 1;
        break;
    }
}


void store_mon_pos(board_t *board, int ghost_index, char *linePos){
    int X, Y;
    sscanf(linePos, "%d %d", &X, &Y);
    board->ghosts[ghost_index].pos_x = X;
    board->ghosts[ghost_index].pos_y = Y;
    board->board[Y * board->width + X].content = 'M'; // Monster
}

void store_mon_passo(board_t *board, int ghost_index, char *linePasso){
    int passo;
    sscanf(linePasso, "%d", &passo);
    board->ghosts[ghost_index].passo = passo;
    board->ghosts[ghost_index].waiting = passo; //not sure
}

void store_mon_moves(board_t *board, int ghost_index, char *moveInput, int move_index){
    int turns_left;
    char move;
    sscanf(moveInput, "%c %d", &move, &turns_left);
    switch (move)
    {
    case 'T':
        board->ghosts[ghost_index].moves[move_index].command = move;
        board->ghosts[ghost_index].moves[move_index].turns_left = turns_left;
        board->ghosts[ghost_index].moves[move_index].turns = 1;
        break;
    
    default:
        board->ghosts[ghost_index].moves[move_index].command = move;
        board->ghosts[ghost_index].moves[move_index].turns_left = 1;
        board->ghosts[ghost_index].moves[move_index].turns = 1;
        break;
    }
}
