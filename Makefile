# Compiler and flags
CC = gcc

########uncomment for X11 based caps lock, num lock and scroll lock determination
#TYPE = X11
#XTRALIBS = -lX11 -lxkbfile # for X11 this should be -lX11 -lxkbfile
######## uncomment for terminal based caps lock, num lock and scroll lock determination
#TYPE = IOCTL
#XTRALIBS =  # for IOCTL this should be blank
######## uncomment for /dev/input/eventsX based caps lock, num lock and scroll lock determination
TYPE = EVENT
XTRALIBS = -levdev # for EVENT this should be -levdev

CFLAGS = -D$(TYPE) -Wall -Wextra -O3 -g  #add/remove -g to toggle gdb debugging information

# Target executable
TARGET1 = kbled
TARGET2 = kbledclient

# Source files
SRC1 = main.c it829x.c keymap.c kbstatus.c sharedmem.c
SRC2 = userspace.c

# Object files
OBJ1 = $(SRC1:.c=.o)
OBJ2 = $(SRC2:.c=.o)

# Libraries to link
LIBS1 = -lhidapi-libusb $(XTRALIBS)
LIBS2 = 

# Default target
all: $(TARGET1) $(TARGET2)

# Rule to build the TARGET1 executable
$(TARGET1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS1)
	
# Rule to build the TARGET2 executable
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS2)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET1) $(TARGET2) *.o

.PHONY: all clean
