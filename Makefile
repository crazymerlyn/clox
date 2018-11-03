SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

default: clox


clox: $(OBJECTS)
	gcc $^ -o $@
