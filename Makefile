CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I$(INCDIR)

SRCDIR = src
INCDIR = include
OBJDIR = obj

SRC_FILES = Channel Errors Logger main Message Parser Server User

SRC = $(addprefix $(SRCDIR)/, $(addsuffix .cpp, $(SRC_FILES)))
OBJFILES = $(addprefix $(OBJDIR)/, $(addsuffix .o, $(SRC_FILES)))

NAME = ircserv

all: $(NAME)

$(NAME): $(OBJFILES)
	$(CXX) $(CXXFLAGS) $(OBJFILES) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re 
