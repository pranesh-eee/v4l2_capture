#Compiler gcc for C program
include rules.mk
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

SRC_V4L2 = v4l2_main.c v4l2_capture.c
OBJ_V4L2 = $(addprefix obj/, $(SRC_V4L2:.c=.o))
#Compiler flags:
# -g adds debugging information to the exec file
# -Wall turns on most, but no all, compiler warnings
CFLAGS = -g -ggdb -Wall -Wextra

#the build target exe:
app: bin/v4l2_capture

bin/v4l2_capture: $(OBJ_V4L2)
	$(Q)$(CC) -o $@ $(OBJ_V4L2)
