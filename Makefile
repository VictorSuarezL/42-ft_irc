CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I$(INCDIR)
DEPFLAGS = -MMD -MP

SRCDIR = src
INCDIR = include
OBJDIR = obj

SRC_FILES = Channel Errors Logger main Message Parser Server User

SRC = $(addprefix $(SRCDIR)/, $(addsuffix .cpp, $(SRC_FILES)))
OBJFILES = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(SRC_FILES)))
DEPFILES = $(OBJFILES:.o=.d)

NAME = ircserv

all: $(NAME)

$(NAME): $(OBJFILES)
	$(CXX) $(CXXFLAGS) $(OBJFILES) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(DEPFILES)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

test: all
	python3 -B tests/run_tests.py

.PHONY: all clean fclean re test
