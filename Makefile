SOURCES := $(wildcard *.c)
HEADERS := $(wildcard *.h)
OBJECTS := $(SOURCES:.c=.o)

default: clox


clox: $(OBJECTS) $(SOURCES) $(HEADERS)
	gcc $(OBJECTS) -o $@

clean:
	rm *.o clox
