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

        if(c.command == '\0'){
            if(game_board->pacmans[0].alive ==0){
                return QUIT_GAME;
            }else{
                return CONTINUE_PLAY;
            }
        }

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
        if(pacman->n_moves != 0){
            pacman->current_move++;
        }
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

        pthread_mutex_lock(&board->lock);
        move_ghost(board, ghost_index, &ghost->moves[ghost->current_move%ghost->n_moves]);
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


int start_threads(pthread_t *tid, board_t *game_board){
    for(int i =0; i <game_board->n_ghosts; i++){
        monster_thread_args *args = malloc(sizeof(monster_thread_args));
        args->board = game_board;
        args->ghost_index = i;
        if (pthread_create(&tid[i], NULL, monster_thread, args) != 0) {
            fprintf(stderr, "error creating thread.\n");
            return -1;
        }
    }
    return 0;
}








int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <level_directory>\n", argv[0]);
    }
    int count; //number of levels
    char **lvl_files = get_lvl_files(argv[1], &count);
    if(lvl_files ==  NULL){
        return 1;
    }

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

        //loads the level name
        snprintf(game_board.level_name, sizeof(game_board.level_name), "%s", lvl_files[current_level]);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return 1;
        }
        current_level++;
        
        load_level(&game_board, accumulated_points, fd, argv[1]);
        close(fd);

        game_board.threads_live =1;
        
        pthread_t tid[game_board.n_ghosts];
        if(start_threads(tid, &game_board) ==-1){
            return -1; //error creating threads
        }
        

        draw_board(&game_board, DRAW_MENU);
        refresh_screen();

        while(true) {
            int result = play_board(&game_board); 
            if(result == NEXT_LEVEL) {

                game_board.threads_live =0;
                for(int i =0; i <game_board.n_ghosts; i++){
                    pthread_join(tid[i], NULL);
                }

                if(current_level>=count){
                    end_game = true;
                    screen_refresh(&game_board, DRAW_WIN);
                    sleep_ms(game_board.tempo);
                    if(game_board.on_save ==1){
                        exit(WON_GAME);
                    }
                    
                }else{
                    
                }
                break;
            }
            if(result == QUIT_GAME) {
                //wait for threads to finish
                game_board.threads_live =0;
                for(int i =0; i <game_board.n_ghosts; i++){
                   pthread_join(tid[i], NULL);
                }
                
                if(game_board.on_save ==1){
                    if(game_board.pacmans[0].alive ==1){
                        exit(QUIT_GAME);
                    }
                    else{
                        exit(0);
                    }
                }
                if(game_board.pacmans[0].alive ==1){
                    end_game = true;
                    break;
                }

                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                
                end_game = true;
                break;
            }

            if(result == CREATE_BACKUP){
                
                if(game_board.on_save ==0 ){
                    game_board.on_save =1;

                    game_board.threads_live =0;
                    for(int i =0; i <game_board.n_ghosts; i++){
                        pthread_join(tid[i], NULL);
                    }
                    
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
                            }else if(WEXITSTATUS(status)==QUIT_GAME){
                                end_game = true;
                                break;
                            }
                            else{
                                game_board.threads_live =1;
                                pthread_t tid[game_board.n_ghosts];
                                if(start_threads(tid, &game_board) ==-1){
                                    return -1; //error creating threads
                                }
                            }
                        }
                        game_board.on_save =0;
                        
                    }
                    if(pid ==0){
                        game_board.threads_live =1;
                        pthread_t tid[game_board.n_ghosts];
                        if(start_threads(tid, &game_board) ==-1){
                            return -1; //error creating threads
                        }
                    }

                }
                
            }
            
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;      
        }
        print_board(&game_board);
        unload_level(&game_board);
    }    

    terminal_cleanup();

    close_debug_file();

    free_lvl_files(lvl_files, count);
   
    return 0;
}
