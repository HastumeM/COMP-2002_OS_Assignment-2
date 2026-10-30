# Zishi Wang a1924840

CC = gcc

# -O2 to increase build performance
# -Wextra for extra warnings before handing anything in
CFLAGS = -O2 -Wall -Wextra -Wpointer-arith -Wstrict-prototypes -std=gnu89 -fPIC -MMD -MP -pthread

LDLIBS = -pthread

all: test-mergesort

# Changed $? to $^. $? only passes the .o files that are newer than the target, $^ always passes both
test-mergesort: test-mergesort.o mergesort.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# CFLAGS has -MMD so gcc writes .d files, but nothing ever read them, this line fixes that
-include test-mergesort.d mergesort.d

# Added "all" so a file called "all" can't shadow the default target.
.PHONY: all clean

# simpler clean
clean:
	rm -f *.o *.d test-mergesort
