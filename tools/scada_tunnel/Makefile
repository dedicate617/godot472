# Makefile for MindSCADA Transparent Tunnel Gateway (C / Mongoose)

CC ?= gcc
CFLAGS ?= -std=gnu99 -O2 -Wall -Wextra
TARGET = scada_tunnel

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    TARGET := $(TARGET).exe
else
    LDFLAGS += -lpthread
endif

SRCS = main.c mongoose.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -I. $(SRCS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f *.o $(TARGET) scada_tunnel scada_tunnel.exe

.PHONY: all clean
