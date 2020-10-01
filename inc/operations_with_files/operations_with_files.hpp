#ifndef MYCAT_OPERATIONS_WITH_FILES_HPP
#define MYCAT_OPERATIONS_WITH_FILES_HPP

#include <iostream>

int writebuffer(int fd, char *buffer, ssize_t size, int *status);

int readbuffer(int fd, char *buffer, ssize_t size, int *status);

int openfile(const char *file, int flag, int *status);

int closefile(int fd, int *status);

#endif //MYCAT_OPERATIONS_WITH_FILES_HPP
