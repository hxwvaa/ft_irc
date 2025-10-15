# Makefile for IRC Server

# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Executable name
NAME = ircserv

# Source files
SRCS = main.cpp \
       Server.cpp \

# Object files
OBJS = $(SRCS:.cpp=.o)

# Header files
HDRS = Server.hpp

# Default rule
all: $(NAME)

# Rule to build the executable
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

# Rule to compile source files into object files
%.o: %.cpp $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean object files
clean:
	rm -f $(OBJS)

# Rule to clean executable and object files
fclean: clean
	rm -f $(NAME)

# Rule to recompile everything
re: fclean all

# Phony targets
.PHONY: all clean fclean re
