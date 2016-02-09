PROGRAMS = assemble simulate
CFLAGS = -Wall -Wextra -std=gnu99
CC = gcc

# Already proving their worth!
all: $(PROGRAMS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

% : %.o
	$(CC) -o $@ $^

clean:
	rm -f *.o $(PROGRAMS) tmp *.mc output*


