
# xsltproc location
xsltproc = /usr/bin/xsltproc

# docbook stylesheets location
docbook = /usr/share/sgml/docbook/xsl-stylesheets

xhtml: flood.xml
	${xsltproc} -o ../manual/ ${docbook}/xhtml/chunk.xsl flood.xml

clean:
	rm -f ../manual/*.html
