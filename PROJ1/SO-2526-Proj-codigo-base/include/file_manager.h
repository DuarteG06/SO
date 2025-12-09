#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "board.h"

//reads the file and returns the file content
char *read_file(int fd);

//checks if the file is a level file
int is_lvl_file(char *file);

//frees the level files
void free_lvl_files(char **lvl_files, int n);

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

//manages level files
void read_lvl_file(int fd, board_t *board, char *path, int points);

//manages pacman files
void read_pac_file(int fd, board_t *board, int points);

//manages monster files
void read_mon_file(int fd, board_t *board, int ghost_index);


#endif