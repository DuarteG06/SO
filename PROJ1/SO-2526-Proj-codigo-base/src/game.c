#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int play_board(board_t * game_board) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    if (pacman->n_moves == 0) { // if is user input
        command_t c; 
        c.command = get_input();

        if(c.command == '\0')
            return CONTINUE_PLAY;

        c.turns = 1;
        play = &c;
    }
    else { // else if the moves are pre-defined in the file
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the pacman
        play = &pacman->moves[pacman->current_move%pacman->n_moves];
    }

    debug("KEY %c\n", play->command);

    if (play->command == 'Q') {
        return QUIT_GAME;
    }

    int result = move_pacman(game_board, 0, play);
    if (result == REACHED_PORTAL) {
        // Next level
        return NEXT_LEVEL;
    }

    if(result == DEAD_PACMAN) {
        return QUIT_GAME;
    }
    
    for (int i = 0; i < game_board->n_ghosts; i++) {
        ghost_t* ghost = &game_board->ghosts[i];
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the ghost
        move_ghost(game_board, i, &ghost->moves[ghost->current_move%ghost->n_moves]);
    }

    if (!game_board->pacmans[0].alive) {
        return QUIT_GAME;
    }      

    return CONTINUE_PLAY;  
}


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

void read_lvl_file(int fd, board_t *board){
    char *start = read_file(fd);
    char *end;

    while ((end = strchr(start, '\n')) != NULL) {
        *end = '\0';  
        
        if(start[0] != '#'){
            if(strncmp(start, "DIM ", 4) ==0){
                char *rest = start + 4;
                int width, height;
                sscanf(rest, "%d %d", &width, &height);
                board->width = width;
                board->height = height;
                
                
            }else if(strncmp(start, "PAC ", 4) ==0){
                char *rest = start + 4;
                int fd = open(rest, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    return;
                }
                read_pac_file(fd, board);




                continue;
            }else if(strncmp(start, "MON ", 4) ==0){
                continue;
            }else if(strncmp(start, "TEMPO ", 6) ==0){
                char *rest = start + 6;
                int tempo;
                sscanf(rest, "%d", &tempo);
                board->tempo =tempo;
            }
            
            //printf("Line: %s\n", start);
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
            printf("Line: %s\n", start);
        }
        start = end + 1; // move to the next line
    }
}


int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
        // TODO receive inputs
    }
    int count;
    char **lvl_files = get_lvl_files(argv[1], &count);
    if(lvl_files ==  NULL){
        return 1;
    }
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", argv[1], lvl_files[0]);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    

    //TODO ler ficheiro e tratar dos dados

    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();
    
    
    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board;

    read_lvl_file(fd, &game_board);
    while (!end_game) {
        
        load_level(&game_board, accumulated_points);
        draw_board(&game_board, DRAW_MENU);
        refresh_screen();

        while(true) {
            int result = play_board(&game_board); 

            if(result == NEXT_LEVEL) {
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(game_board.tempo);
                break;
            }

            if(result == QUIT_GAME) {
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                end_game = true;
                break;
            }
    
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;      
        }
        print_board(&game_board);
        unload_level(&game_board);
    }    

    terminal_cleanup();

    close_debug_file();


    free(lvl_files);
    return 0;
}
