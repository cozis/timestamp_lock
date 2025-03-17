
ifeq ($(OS),Windows_NT)
	PLATFORM := Windows
	LFLAGS = -lsynchronization
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		PLATFORM := Linux
	else
		PLATFORM := Unknown
	endif
endif

.PHONY: all tsan clean

all:
	gcc test.c timestamp_lock.c -o test -Wall -Wextra -ggdb $(LFLAGS)

tsan:
	gcc test.c timestamp_lock.c -o test -Wall -Wextra -fsanitize=thread,undefined -ggdb $(LFLAGS)

clean:
	rm test test.exe