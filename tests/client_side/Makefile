NAME = tcpclient

cc = c++

CFLAGS = -Wall -Wextra -Werror

C2FLAGS = -Wall -Wextra -Werror -lc++ -std=c++98

SRC = tcpclient.cpp

OBJ = $(SRC:.cpp=.o)

all :$(NAME)

$(NAME): $(OBJ)
	$(CC) $(C2FLAGS) $(OBJ) -o $(NAME)


.cpp.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all re clean fclean