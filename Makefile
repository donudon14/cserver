CC = gcc
CFLAGS = -Wall -Werror -g $(EXTRA_CFLAGS)
LDFLAGS = $(EXTRA_LDFLAGS)
COMPILE = $(CC) $(CFLAGS)
LINK = $(CC) $(LDFLAGS)
LIBS = -lpthread

targets = t-cserver app

all: $(targets)

%.o: %.c
	$(COMPILE) -c $<

app: util.o cserver.o app.o
	$(LINK) $^ -o $@ $(LIBS)

t-cserver: t-cserver.o cserver.o util.o
	$(LINK) $^ -o $@ $(LIBS)

check: t-cserver
	./t-cserver

coverage:
	@$(MAKE) clean
	@echo initial
	@echo Running tests...
	@$(MAKE) EXTRA_CFLAGS="--coverage" EXTRA_LDFLAGS="--coverage" check || echo "tests failed"

	@lcov --capture --directory . \
	  --output-file tests.info

	@lcov --remove tests.info 't-cserver.c' --output tests_filtered.info
	@mkdir coverage &&  \
	     cd coverage && genhtml --prefix `dirname $$PWD` ../tests_filtered.info

clean:
	rm -f $(targets) *.o *.gcno *.gcda *.info core
	rm -rf coverage

package: clean
	COPYFILE_DISABLE=1 tar zcvf cserver.tar.gz *.c *.h *.py Makefile README REPORT.txt

.PHONY: clean coverage
