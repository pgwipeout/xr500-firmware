WEB_SOURCES := $(shell find . \( -name "files" -o -name "ipkg*" \) -prune -o \( -name "*.html" -o -name "*.js" -o -name "*.css" \))
HTML_OBJECTS := $(WEB_SOURCES:%.html=%.min.html.processed)
JS_OBJECTS := $(WEB_SOURCES:%.js=%.min.js.processed)
CSS_OBJECTS := $(WEB_SOURCES:%.css=%.min.css.processed)

REPLACE_STRING:= \1\?v=$(DUMAOS_API_VERSION)\&lang=<%= lang %>

HTML_HREF_SINGLE_PATTERN = s/href='"'"'\([^\ ]*\)'"'"'/href='"'"'$(REPLACE_STRING)'"'"'/g
HTML_HREF_DOUBLE_PATTERN = s/href="\([^\ ]*\)"/href="$(REPLACE_STRING)"/g
HTML_SRC_SINGLE_PATTERN = s/src='"'"'\([^\ ]*\)'"'"'/src='"'"'$(REPLACE_STRING)'"'"'/g
HTML_SRC_DOUBLE_PATTERN = s/src="\([^\ ]*\)"/src="$(REPLACE_STRING)"/g
CSS_URL_SINGLE_PATTERN = s/url('"'"'\([^\ ]*\)'"'"')/url('"'"'$(REPLACE_STRING)'"'"')/g
CSS_URL_DOUBLE_PATTERN = s/url("\([^\ ]*\)")/url("$(REPLACE_STRING)")/g

.PHONY : all
all : $(HTML_OBJECTS) $(JS_OBJECTS) $(CSS_OBJECTS)

%.min.html.processed: %.html
	cat $< |\
	sed '$(HTML_HREF_SINGLE_PATTERN); $(HTML_HREF_DOUBLE_PATTERN); $(HTML_SRC_SINGLE_PATTERN); $(HTML_SRC_DOUBLE_PATTERN); $(EMAIL_COMMENT_PATTERN)' > $@

%.min.css.processed: %.css
	cat $< |\
	sed '$(CSS_URL_SINGLE_PATTERN); $(CSS_URL_DOUBLE_PATTERN); $(EMAIL_COMMENT_PATTERN)' > $@

%.min.js.processed: %.js
	cat $< |\
	sed '$(EMAIL_COMMENT_PATTERN);' > $@
