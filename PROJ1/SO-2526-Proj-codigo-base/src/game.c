#include "board.h"
#include "display.h"
#include "file_manager.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define WON_GAME 5

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
    else if(play->command == 'G'){
        return CREATE_BACKUP;
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
    
    

    //TODO ler ficheiro e tratar dos dados

    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();
    
    
    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board;

    int i =0;

    while (!end_game) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", argv[1], lvl_files[i]);

        //strcpy(lvl_files[i], game_board.level_name);
        //loads the level name
        snprintf(game_board.level_name, sizeof(game_board.level_name), "%s", lvl_files[i]);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return 1;
        }
        i++;
        

        //read_lvl_file(fd, &game_board, argv[1], accumulated_points);


        load_level(&game_board, accumulated_points, fd, argv[1]);
        
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
                //suposto mostrar o game over ou voltar logo
                if(game_board.on_save ==1){
                    exit(0);
                }
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                

                end_game = true;
                break;
            }

            if(result == CREATE_BACKUP){
                //criar fork
                if(game_board.on_save ==0 ){
                    game_board.on_save =1;
                    pid_t pid = fork();
                    if (pid < 0) {
                        perror("fork failed");
                        exit(1);
                    }
                    if(pid != 0){
                        int status;
                        pid_t waiting = waitpid(pid, &status, 0);
                        if (waiting == -1) {
                            perror("waitpid");
                            exit(1);
                        }
                        if(WIFEXITED(status)){
                            if(WEXITSTATUS(status)==WON_GAME){
                                end_game = true;
                                break;
                            }
                        }
                        game_board.on_save =0;
                    }

                }
                
            }
    
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;      
        }
        print_board(&game_board);
        unload_level(&game_board);

        if(i>=count){
            end_game = true;
            if(game_board.on_save ==1){
                exit(WON_GAME);
            }
        }
    }    

    terminal_cleanup();

    close_debug_file();


    free(lvl_files);
    return 0;
}
