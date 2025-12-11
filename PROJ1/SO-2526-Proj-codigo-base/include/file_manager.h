#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "board.h"

//reads the file and returns the file content
char *read_file(int fd);

//checks if the file is a level file
int is_lvl_file(char *file);

//frees the level files
void free_lvl_files(char **lvl_files, int n);

//used to sort the level files read
int lvl_comparator(const void *a, const void *b);

//returns an array of all level files in a directory
char **get_lvl_files(char *inputdir, int *count);

//sets up board dim
void set_board_dim(char *dim, board_t *board);

//prepares to call read_pac_file
void prepare_and_read_pac_file(board_t *board, char *line, int points, char *path);

//sets memory for ghosts
void set_memory_for_ghosts(board_t *board, char *mon_files);

//prepares to call read_mon_file
void prepare_and_read_mon_file(board_t *board, char *mon_files, char *dirpath);

//saves the initialized board
void store_game_board(board_t *board, char *line, int line_number);

//loads pacman for player input
void load_pacman_for_player(board_t *board, int points);

//stores inicial pacman position
void store_pac_pos(board_t *board, char *linePos);

//stores pacman passo
void store_pac_passo(board_t *board, char *linePasso);

//stores pacman moves
void store_pac_moves(board_t *board, char *command, int move_index);

//stores monster inicial position
void store_mon_pos(board_t *board, int ghost_index, char *linePos);

//stores monster passo
void store_mon_passo(board_t *board, int ghost_index, char *linePasso);

//stores monster moves
void store_mon_moves(board_t *board, int ghost_index, char *moveInput, int move_index);


#endif