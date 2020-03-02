#Compiler gcc for C program
include rules.mk
CC = gcc

SRC_SER = v4l2_main.c v4l2_capture.c
OBJ_SER = $(addprefix obj/, $(SRC_SER:.c=.o))
#Compiler flags:
# -g adds debugging information to the exec file
# -Wall turns on most, but no all, compiler warnings
CFLAGS = -g -ggdb -Wall -Wextra

#the build target exe:
app: bin/v4l2_capture

bin/v4l2_capture: $(OBJ_SER)
	$(Q)$(CC) -o $@ $(OBJ_SER)
