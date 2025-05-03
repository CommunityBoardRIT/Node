NAME        := lumen
LUMEN_DIR	:= lumen

FLAGS += -I$(SRC_DIR)/$(LUMEN_DIR)

# Main file (separated to not interfere with CPPUTEST)
MAIN_CPP    := main.cpp

# C sources
SRCS        :=

# C++ sources
LUMEN_CPP   :=

# C++ test sources
LUMEN_UTEST :=

# Object reformatting
LUMEN_OBJS		:= $(DRIVER_OBJS)
LUMEN_OBJS 		+= $(SRCS:%.c=$(OBJ_DIR)/$(LUMEN_DIR)/%.o)
LUMEN_OBJS 		+= $(LUMEN_CPP:%.cpp=$(OBJ_DIR)/$(LUMEN_DIR)/%.o)
LUMEN_MAIN 		:= $(MAIN_CPP:%.cpp=$(OBJ_DIR)/$(LUMEN_DIR)/%.o)
LUMEN_UTEST_OBJS	:= $(LUMEN_UTEST:%.cpp=$(OBJ_DIR)/$(LUMEN_DIR)/%.o)

RM          := rm -rf
# MAKEFLAGS   += --no-print-directory
DIR_DUP      = mkdir -p $(@D)
PHONIES 	+= all lumen_re clean fclean



# Executable
lumen: $(COMMON_OBJS) $(LUMEN_OBJS) $(LUMEN_MAIN)
	@mkdir -p $(BIN_DIR)
	$(CC) $^ $(FLAGS) -o $(BIN_DIR)/$@
	@echo "    Target    $@"

lumen_test: lumen_re $(COMMON_OBJS) $(COMMON_UTEST_OBJS) $(LUMEN_OBJS) $(LUMEN_UTEST_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(filter-out $(PHONIES),$^) $(FLAGS) $(UTEST_LIB) -o $(BIN_DIR)/$@
	$(BIN_DIR)/$@

lumen_re: fclean lumen

#------------------------------------------------#
#   SPEC                                         #
#------------------------------------------------#

.PHONY: clean fclean re
.SILENT:

####################################### END_3 ####