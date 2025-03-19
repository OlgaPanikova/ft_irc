NAME = ircserv
CC = c++
CFLAGS = -std=c++98 -Wall -Wextra -Werror

OBJS_DIR = ./objs

SRCS = $(wildcard *.cpp)
HEADER = $(wildcard *.hpp)

OBJS = $(SRCS:%.cpp=$(OBJS_DIR)/%.o)

all: $(OBJS_DIR) $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJS_DIR)/%.o: %.cpp $(HEADER)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

clean:
	rm -rf $(OBJS_DIR)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re