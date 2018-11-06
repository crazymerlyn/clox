SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

default: clox


clox: $(OBJECTS) $(SOURCES)
	gcc $(OBJECTS) -o $@

clean:
	rm *.o clox
