#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "board.h"

//reads the file and returns the file content
char *read_file(int fd);

//checks if the file is a level file
int is_lvl_file(char *file);

//returns an array of all level files in a directory
char **get_lvl_files(char *inputdir, int *count);

//manages level files
void read_lvl_file(int fd, board_t *board, char *path);

//manages pacman files
void read_pac_file(int fd, board_t *board);



#endif