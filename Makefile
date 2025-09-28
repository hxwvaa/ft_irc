name = ft_irc

SRC = client.cpp main.cpp \
OBJ = $(SRC:.cpp=.o)

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 
RM = rm -f

all: $(name)

$(name): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM
re: fclean all
) $(OBJ)

fclean: clean
	$(RM) $(name)

.PHONY: all clean fclean re