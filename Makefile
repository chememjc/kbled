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

CFLAGS = -D$(TYPE) -Wall -Wextra -O3 #add/remove -g to toggle gdb debugging information

# Target executable
TARGET1 = kbled
TARGET2 = kbledclient
TARGET3 = semsnoop
TARGET4 = kbledpsmon
TARGET5 = kbledcylon

# Other configuration files
INITSCRIPT = kbled.service

# Utility script installation targets
UTILDIR = utils
UTILSCRIPT1 = kbledcolorpicker

# Source files
SRC1 = daemon.c it829x.c keymap.c kbstatus.c sharedmem.c
SRC2 = client.c sharedmem.c
SRC3 = semsnoop.c
SRC4 = psmon.c sharedmem.c
SRC5 = cylon.c sharedmem.c

# Object files
OBJ1 = $(SRC1:.c=.o)
OBJ2 = $(SRC2:.c=.o)
OBJ3 = $(SRC3:.c=.o)
OBJ4 = $(SRC4:.c=.o)
OBJ5 = $(SRC5:.c=.o)

# Libraries to link
LIBS1 = -lhidapi-libusb -lsystemd $(XTRALIBS)
LIBS2 = 
LIBS3 = 
LIBS4 = 
LIBS5 = 

# Define the installation directories
INIT_DIR = /etc/systemd/system
BIN_DIR = /usr/bin

# Define variables for the .deb file creation
VERSION=$(shell cat release)
CURRENT_DATE=$(shell date +%-Y%02m%02d)
VERSION_DATE=$(VERSION).$(CURRENT_DATE)

# Default target
all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5)

# Check if running as root or with sudo
check-root:
	@if [ $$(id -u) -ne 0 ]; then \
		echo "You must be root to run this. Try running with sudo."; \
		exit 1; \
	fi

# Installation target
install: check-root
	# Copy the kbled.conf script to /etc/init/
	install -m 644 $(INITSCRIPT) $(INIT_DIR)/$(INITSCRIPT)
	# Copy the executables to /usr/bin
	install -m 755 $(TARGET1) $(BIN_DIR)/$(TARGET1)
	install -m 755 $(TARGET2) $(BIN_DIR)/$(TARGET2)
	install -m 755 $(TARGET3) $(BIN_DIR)/$(TARGET3)
	install -m 755 $(TARGET4) $(BIN_DIR)/$(TARGET4)
	install -m 755 $(TARGET5) $(BIN_DIR)/$(TARGET5)
	install -m 755 $(UTILDIR)/$(UTILSCRIPT1).sh $(BIN_DIR)/$(UTILSCRIPT1)
	@echo 
	@echo "To enable on startup run:  sudo systemctl enable kbled"
	@echo "To start kbled now run:    sudo systemctl start kbled"

uninstall: check-root
	#stop the service if it is running and disable it
	@systemctl is-active --quiet kbled && systemctl stop kbled || true
	@systemctl is-enabled --quiet kbled && systemctl disable kbled || true
	# remove the kbled.conf script from /etc/systemd/system
	rm -f $(INIT_DIR)/$(INITSCRIPT)
	# remove the executables from /usr/bin
	rm -f $(BIN_DIR)/$(TARGET1)
	rm -f $(BIN_DIR)/$(TARGET2)
	rm -f $(BIN_DIR)/$(TARGET3)
	rm -f $(BIN_DIR)/$(TARGET4)
	rm -f $(BIN_DIR)/$(TARGET5)
	rm -f $(BIN_DIR)/$(UTILSCRIPT1)

# Rule to build the TARGET1 executable
$(TARGET1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS1)
	
# Rule to build the TARGET2 executable
$(TARGET2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS2)
	
# Rule to build the TARGET2 executable
$(TARGET3): $(OBJ3)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS3)

$(TARGET4): $(OBJ4)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS4)

$(TARGET5): $(OBJ5)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS5)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5) *.o *.deb *.tar.gz
	./pkg/makepkg.sh clean

# Distribution target to create .deb package
distribution: all
	./pkg/makepkg.sh

.PHONY: all clean install uninstall kbled kbledclient semsnoop kbledpsmon kbledcylon distribution
