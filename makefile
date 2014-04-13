OBJDIR		= build
EXE			= svn_mail
CFLAGS		= -Wall -O2 -MMD -MP
SOURCES		:= $(wildcard *.c)
OBJFILES	:= $(SOURCES:%.c=$(OBJDIR)/%.o)
DEPFILES	:= $(OBJFILES:.o=.d)

all: $(EXE)

$(EXE): $(OBJDIR) $(OBJFILES)
	$(CC) $(CFLAGS) $(OBJFILES) -o $@

run: $(EXE)
	./$(EXE) /svn 14

install: $(EXE)
	/usr/bin/sudo /usr/bin/install -v -s -p $(EXE) /usr/local/bin

clean:
	/bin/rm -rf $(OBJDIR) $(EXE)

$(OBJDIR):
	/bin/mkdir -p $@

$(OBJDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

-include $(DEPFILES)
