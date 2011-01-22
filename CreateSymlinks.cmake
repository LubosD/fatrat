if (DESTDIR)
	EXECUTE_PROCESS(COMMAND ln -s fatrat ${DESTDIR}/bin/fatrat-nogui)
else (DESTDIR)
	EXECUTE_PROCESS(COMMAND ln -s fatrat ${CMAKE_INSTALL_PREFIX}/bin/fatrat-nogui)
endif (DESTDIR)

