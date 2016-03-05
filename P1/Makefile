LEVEL = ../..

vpath %.y $(SRC_DIR)
vpath %.lex $(SRC_DIR)

TOOLNAME=p1

SOURCES := main.c parser.c scanner.c

LINK_COMPONENTS := bitreader bitwriter analysis 

include $(LEVEL)/Makefile.common

LIBS += -ll

parser.c: parser.y
	bison -d -o $@ $<

parser.h: parser.c

scanner.c: scanner.lex parser.h
	flex -o$@ $<

p1clean:
	@rm -f p1 parser.h scanner.c parser.c
