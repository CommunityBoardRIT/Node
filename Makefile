MAKE_DIR	 = $(PWD)
OBJ_DIR     := build/obj
BIN_DIR		:= build/bin
SRC_DIR		:= src

USE_ACTIVEMQ = 0

# Compiler options
CC          := g++
CPP_FLAGS   := -g -Wall -Wextra -Wno-deprecated-declarations -D _DEFAULT_SOURCE
CPP_LIB 	:= -lpthread
CPP_INC     := -I./src/common
UTEST_LIB 	:= -lCppUTest -lCppUTestExt
FLAGS 		:= $(CPP_FLAGS) $(CPP_LIB) $(CPP_INC)
PHONIES := clean
.PHONY: clean

clean:
	@rm -rf build/

# C
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(DIR_DUP)
	@$(CC) $(FLAGS) -c -o $(MAKE_DIR)/$@ $<
	@echo "    CC        $@"

# C++
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(DIR_DUP)
	@$(CC) $(FLAGS) -c -o $(MAKE_DIR)/$@ $<
	@echo "    CC        $@"

include src/common/source.mk
include src/lumen/source.mk