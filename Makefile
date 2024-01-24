INC     = -I ./

TARGET  = libm17.so

CFLAGS  = $(INC) -fPIC -Wall -Wextra
LDFLAGS = -shared -lm
SRCS    = $(filter-out unit_tests/unit_tests.c, $(wildcard */*.c))
OBJS    = $(SRCS:.c=.o)

all: $(TARGET) clean

clean:
	rm -f $(OBJS)

fclean:
	rm -f $(TARGET)

test:
	$(CC) $(CFLAGS) unit_tests/unit_tests.c -o unit_tests/unit_tests -lm -lunity -lm17

testrun:
	./unit_tests/unit_tests

install:
	sudo install $(TARGET) /usr/local/lib

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

.PHONY: all clean fclean
