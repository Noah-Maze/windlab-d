pkglib_LTLIBRARIES = tape.la
tape_la_SOURCES =            \
	collector.c               \
	file.c        file.h      \
	histogram.cpp histogram.h \
	group_recorder.cpp	group_recorder.h	\
	loadshape.cpp loadshape.h \
	main.cpp                  \
	memory.c      memory.h    \
	odbc.c        odbc.h      \
	player.c                  \
	recorder.c                \
	multi_recorder.c          \
	shaper.c                  \
	tape.c        tape.h
tape_la_LDFLAGS = -module -no-undefined -avoid-version -version-info 0:0:0 -ldl

uninstall-hook:
	-rmdir $(DESTDIR)$(pkglibdir)

