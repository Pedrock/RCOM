CC = gcc
CFLAGS = -Wall
SRCS = application.c protocol.c utilities.c
OBJS = $(SRCS:.c=.o)
MAIN = application

all:    $(MAIN)
		@echo  Success

$(MAIN): $(OBJS) 
		$(CC) $(CFLAGS) -o $(MAIN) $(OBJS)


.c.o:
		$(CC) $(CFLAGS) -c $<  -o $@

clean:
		$(RM) *.o *~ $(MAIN)
