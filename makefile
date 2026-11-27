CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude -Isrc -MMD -MP
SRC = $(wildcard src/*.c)
HEADERS = $(wildcard src/*.h) $(wildcard include/*.h)
OBJ = $(SRC:src/%.c=obj/%.o)
DEP = $(OBJ:.o=.d)
TARGET = treasure

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)
	@echo "--->>> DEVELOPED BY VOID : GH - @Est-Void <<<---"

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

fix_format:
	clang-format -i $(SRC) $(HEADERS)

check_format:
	clang-format -n $(SRC) $(HEADERS)

clean:
	rm -rf obj $(TARGET)

-include $(DEP)

.PHONY: clean fix_format check_format
