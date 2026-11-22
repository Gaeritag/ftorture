NAME = push_swap
CFLAGS = -Wall -Wextra -Werror
all: $(NAME)
$(NAME): push_swap.o
	cc $(CFLAGS) push_swap.o -o $@
push_swap.o: push_swap.c push_swap.h
	cc $(CFLAGS) -c push_swap.c -o $@
bonus: checker
checker: checker_bonus.o
	cc $(CFLAGS) checker_bonus.o -o $@
checker_bonus.o: checker_bonus.c
	cc $(CFLAGS) -c checker_bonus.c -o $@
clean:
	rm -f push_swap.o checker_bonus.o
fclean: clean
	rm -f $(NAME) checker
re: fclean all
.PHONY: all bonus clean fclean re
