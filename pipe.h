#pragma once

void pipe_init(const char *path);
void pipe_destroy(const char *path);
int pipe_open_write(const char *path);
int pipe_open_read(const char *path);
void pipe_close(int fd);
