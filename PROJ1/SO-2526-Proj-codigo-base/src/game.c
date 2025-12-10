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
#include <pthread.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define WON_GAME 5


typedef struct{
    board_t *board;
    int ghost_index;
} monster_thread_args;


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
    command_t c; 
    if (pacman->n_moves == 0) { // if is user input
        
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

    pthread_mutex_lock(&game_board->lock);
    int result = move_pacman(game_board, 0, play);
    pthread_mutex_unlock(&game_board->lock);
    if (result == REACHED_PORTAL) {
        // Next level
        return NEXT_LEVEL;
    }

    if(result == DEAD_PACMAN) {
        return QUIT_GAME;
    }
    
    //vai passar para 
    // for (int i = 0; i < game_board->n_ghosts; i++) {
    //     ghost_t* ghost = &game_board->ghosts[i];
    //     // avoid buffer overflow wrapping around with modulo of n_moves
    //     // this ensures that we always access a valid move for the ghost
    //     move_ghost(game_board, i, &ghost->moves[ghost->current_move%ghost->n_moves]);
    // }

    if (!game_board->pacmans[0].alive) {
        return QUIT_GAME;
    }      

    return CONTINUE_PLAY;  
}

void *monster_thread(void *arg){
    monster_thread_args *monster = (monster_thread_args *)arg;

    board_t *board = monster->board;
    int ghost_index = monster->ghost_index;

    ghost_t* ghost = &board->ghosts[ghost_index];
    while(board->threads_live == 1){



         //ver o screenrefresh
        pthread_mutex_lock(&board->lock);
        move_ghost(board, ghost_index, &ghost->moves[ghost->current_move%ghost->n_moves]);
        screen_refresh(board, DRAW_MENU); 

        pthread_mutex_unlock(&board->lock);
        if (!board->pacmans[0].alive) {
            board->threads_live =0;
            break;
        } 

        sleep_ms(board->tempo);
        
    }
    free(monster);

    return NULL;

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
    pthread_mutex_init(&game_board.lock, NULL);
    int current_level =0;

    while (!end_game) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", argv[1], lvl_files[current_level]);

        //strcpy(lvl_files[i], game_board.level_name);
        //loads the level name
        snprintf(game_board.level_name, sizeof(game_board.level_name), "%s", lvl_files[current_level]);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return 1;
        }
        current_level++;
        

        //read_lvl_file(fd, &game_board, argv[1], accumulated_points);

        pthread_mutex_lock(&game_board.lock);
        load_level(&game_board, accumulated_points, fd, argv[1]);
        pthread_mutex_unlock(&game_board.lock);
        
        game_board.threads_live =1;
        
        pthread_t tid[game_board.n_ghosts];
        for(int i =0; i <game_board.n_ghosts; i++){
            monster_thread_args *args = malloc(sizeof(monster_thread_args));
            args->board = &game_board;
            args->ghost_index = i;
            if (pthread_create(&tid[i], NULL, monster_thread, args) != 0) {
                fprintf(stderr, "error creating thread.\n");
                return -1;
            }

        }
        

        

        pthread_mutex_lock(&game_board.lock);
        draw_board(&game_board, DRAW_MENU);
        refresh_screen();
        pthread_mutex_unlock(&game_board.lock);

        while(true) {
            int result = play_board(&game_board); 

            if(result == NEXT_LEVEL) {
                if(current_level>=count){
                    end_game = true;
                    game_board.threads_live =0;


                    //thread wait

                    pthread_mutex_lock(&game_board.lock);
                    screen_refresh(&game_board, DRAW_WIN);
                    pthread_mutex_unlock(&game_board.lock);

                    sleep_ms(game_board.tempo);
                    if(game_board.on_save ==1){
                        exit(WON_GAME);
                    }
                    
                }else{
                    game_board.threads_live =0;
                }
                // screen_refresh(&game_board, DRAW_WIN);
                // sleep_ms(game_board.tempo);
                break;
            }

            if(result == QUIT_GAME) {
                //suposto mostrar o game over ou voltar logo

                game_board.threads_live =0;

                //thread wait

                if(game_board.on_save ==1){


                    


                    exit(0);
                }

                pthread_mutex_lock(&game_board.lock);
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                pthread_mutex_unlock(&game_board.lock);
                sleep_ms(game_board.tempo);
                

                end_game = true;
                break;
            }

            if(result == CREATE_BACKUP){
                //criar fork
                if(game_board.on_save ==0 ){
                    game_board.on_save =1;

                    game_board.threads_live =0;

                    //thread wait



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
            if(game_board.pacmans[0].alive ==0){
                end_game =1;
                break;
            }
            pthread_mutex_lock(&game_board.lock);
            screen_refresh(&game_board, DRAW_MENU); 
            pthread_mutex_unlock(&game_board.lock);

            accumulated_points = game_board.pacmans[0].points;      
        }
        pthread_mutex_lock(&game_board.lock);
        print_board(&game_board);
        unload_level(&game_board);
        pthread_mutex_unlock(&game_board.lock);

        
    }    

    terminal_cleanup();

    close_debug_file();


    free(lvl_files);
    return 0;
}
