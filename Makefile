.PHONY: all tsan clean

all:
	gcc test.c timestamp_lock.c -o test -Wall -Wextra -ggdb -lsynchronization

tsan:
	gcc test.c timestamp_lock.c -o test -Wall -Wextra -fsanitize=thread,undefined -ggdb -lsynchronization

clean:
	rm test test.exe