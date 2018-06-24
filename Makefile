
SUBDIRS = src test

subdirs:
	for dir in $(SUBDIRS); do	\
		$(MAKE) all -C $$dir;	\
	done

clean:
	for dir in $(SUBDIRS); do	\
		$(MAKE) clean -C $$dir;	\
	done

test: src
