NULL =

SPEC_XML_FILES = \
	xfce4-screensaver.xml  \
	dbus-interface.xml     \
	$(NULL)

if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)
htmldoc_DATA = xfce4-screensaver.html

xfce4-screensaver.html : dbus-interface.xml
	$(XMLTO) html-nochunks -m config.xsl xfce4-screensaver.xml

endif # DOCBOOK_DOCS_ENABLED

EXTRA_DIST = \
	xfce4-screensaver.xml.in  \
	config.xsl                \
	docbook.css               \
	$(SPEC_XML_FILES)
	$(NULL)

clean-local:
	rm -f *~
	rm -f xfce4-screensaver.html
