NAME = irc_test

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = client.cpp channel.cpp parcer.cpp server.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re