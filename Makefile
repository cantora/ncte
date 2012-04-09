.SECONDARY:

DEFINES			= -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1 -D_POSIX_C_SOURCE=	199309L
OUTPUT			= ncte
BUILD			= ./build
MKBUILD			:= $(shell mkdir -p $(BUILD) )
LIBVTERM		= ./libvterm/.libs/libvterm.a
LIB 			= -lutil -lrt -lncursesw $(LIBVTERM)
INCLUDES		= -iquote"./libvterm/include" -iquote"./libvterm/src" -iquote"./src"
CXX_FLAGS		= -ggdb -Wall -Wextra $(INCLUDES) $(DEFINES)
CXX_CMD			= gcc $(CXX_FLAGS)

SRCS			= $(notdir $(filter-out ./src/$(OUTPUT).c, $(wildcard ./src/*.c) ) $(BUILD)/vterm_ansi_colors.c )
OBJECTS			= $(patsubst %.c, $(BUILD)/%.o, $(SRCS) ) 

default: all

.PHONY: all
all: $(OUTPUT)

$(LIBVTERM): ./libvterm
	make -C ./libvterm

libvterm:
	bzr checkout lp:libvterm

$(BUILD)/vterm_ansi_colors.c: $(LIBVTERM)
	{ \
	echo '#include "vterm.h"'; \
	awk '/static const VTermColor ansi_colors\[\].*/, /};/' ./libvterm/src/pen.c \
		| sed 's/ansi_colors/vterm_ansi_colors/' \
		| sed 's/^static const/const/'; \
	} > $@

$(OUTPUT): $(BUILD)/$(OUTPUT).o $(OBJECTS)
	$(CXX_CMD) $+ $(LIB) -o $@

$(BUILD)/$(OUTPUT).o: ./src/$(OUTPUT).c $(LIBVTERM)
	$(CXX_CMD) -c $< -o $@

$(BUILD)/screen.o: ./src/screen.c ./src/vterm_ansi_colors.h
	$(CXX_CMD) -c $< -o $@

$(BUILD)/%.o: $(BUILD)/%.c
	$(CXX_CMD) -c $< -o $@

$(BUILD)/%.o: ./src/%.c
	$(CXX_CMD) -c $< -o $@

$(BUILD)/%.o: ./src/%.c ./src/%.h
	$(CXX_CMD) -c $< -o $@

.PHONY: clean 
clean: 
	rm -rf $(BUILD)
	rm -f $(OUTPUT)

.PHONY: libclean
libclean: clean
	rm -rf ./libvterm