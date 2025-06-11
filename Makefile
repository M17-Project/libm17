INC     = -I ./

TARGET  = libm17.so
VERSION = 1.0.10

CFLAGS  = $(INC) -fPIC -Wall -Wextra
LDFLAGS = -shared -lm
SRCS    = m17.c $(filter-out unit_tests/unit_tests.c, $(wildcard */*.c))
OBJS    = $(SRCS:.c=.o)

VERSION_HEADER = version.h
GIT_TAG := $(shell git describe --tags --always)

$(TARGET): $(VERSION_HEADER) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(VERSION_HEADER):
	echo '#pragma once' > $(VERSION_HEADER)
	echo '#define LIBM17_VERSION		"$(VERSION)-$(GIT_TAG)"' >> $(VERSION_HEADER)

all: $(TARGET)

clean:
	rm -f $(TARGET) $(OBJS) $(VERSION_HEADER)

test:
	$(CC) $(CFLAGS) unit_tests/unit_tests.c -o unit_tests/unit_tests -lm -lunity -lm17

testrun:
	./unit_tests/unit_tests

install:
	sudo install $(TARGET) /usr/local/lib

.PHONY: all clean test testrun install
