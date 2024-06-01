/*
 * Copyright (c) 2024 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Utility for fmatting XML text with nice indentation.
 * Might be something better by now in the libraries.
 * Used by Mobius for configuration files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// a little juce::String support
#include <JuceHeader.h>

#include "Util.h"

#include "XmlBuffer.h"

XmlBuffer::XmlBuffer()
{
	mIndent = 0;
	mPrefix = nullptr;
	mNamespace = nullptr;
	mNamespaceDeclared = false;
	mAttributeNewline = false;
}

XmlBuffer::~XmlBuffer()
{
	delete mPrefix;
	delete mNamespace;
}

void XmlBuffer::setPrefix(const char *s)
{
	delete mPrefix;
	mPrefix = CopyString(s);
}

void XmlBuffer::setNamespace(const char *s)
{
	delete mNamespace;
	mNamespace = CopyString(s);
}

void XmlBuffer::setAttributeNewline(bool b)
{
	mAttributeNewline = b;
}

void XmlBuffer::addNamespace(const char *name, const char *url)
{
    (void)name;
    (void)url;
#if 0
	if (name != nullptr && url != nullptr) {
	    if (mNamespaces == nullptr)
		  mNamespaces = new HashMap();
	    mNamespaces.put(name, url);
	}
#endif
}

void XmlBuffer::incIndent(int i) {
	mIndent += i;
}

void XmlBuffer::incIndent() {
	mIndent += 2;
}

void XmlBuffer::decIndent(int i) {
	mIndent -= i;
	if (mIndent < 0)
	  mIndent = 0;
}

void XmlBuffer::decIndent() {
	mIndent -= 2;
	if (mIndent < 0)
	  mIndent = 0;
}

/**
 * Adds an attribute name and value to the buffer.
 * null values are suppressed.  We don't distinguish between null
 * and unspecified.
 * 
 * Performs any necessary escaping on the value.  This should
 * be used when you're building the XML for something, and
 * its possible for an attribute value to have any of the characters
 * &, ', or "
 */
void XmlBuffer::addAttribute(const char *name, 
									const char *prefix, 
									const char *value)
{
	size_t max = (value != nullptr) ? strlen(value) : 0;
	int i;

	if (max > 0) {

		if (mAttributeNewline) {
			add("\n");
			addIndent(mIndent + 2);
		}
		else
		  add(" ");

	    add(name);
	    add("=");

	    int doubles = 0;
	    int singles = 0;
	    char delim   = '\'';

	    // look for quotes
	    for (i = 0 ; i < max ; i++) {
			char ch = value[i];
			if (ch == '"')
			  doubles++;
			else if (ch == '\'')
			  singles++;
	    }

	    // if single quotes were detected, switch to double delimiters
	    if (singles > 0)
	      delim = '"';

	    addChar(delim);

		// assume that the prefix doesn't need to be escaped
		if (prefix != nullptr)
		  add(prefix);

	    // add the value, converting embedded quotes if necessary
	    for (i = 0 ; i < max ; i++) {
			char ch = value[i];
			if (ch == '&')
			  add("&amp;");
			else if (ch == '<')
			  add("&lt;");
			else if (ch < ' ') {
				// binary, ignore though could escape
			}
			else if (ch != delim)
			  addChar(ch);
			else if (delim == '\'')
			  add("&#39;");	     		// convert '
			else
			  add("&#34;");	     		// convert "
	    }

	    addChar(delim);
	}
}

void XmlBuffer::addAttribute(const char *name, const char* value) {

	addAttribute(name, nullptr, value);
}

void XmlBuffer::addAttribute(const char *name, juce::String value) {

	addAttribute(name, nullptr, value.toUTF8());
}

/**
 * Adds a boolean attribute to the buffer.
 *
 * If the value is false, the attribute is suppressed.
 */
void XmlBuffer::addAttribute(const char* name, bool value) {
	if (value)
	  addAttribute(name, "true");
}

/**
 * Adds an integer attribute to the buffer.
 */
void XmlBuffer::addAttribute(const char* name, int value) {
	char buf[64];
	snprintf(buf, sizeof(buf), "%d", value);
	addAttribute(name, buf);
}

void XmlBuffer::addAttribute(const char* name, long value) {
	char buf[64];
	snprintf(buf, sizeof(buf), "%ld", value);
	addAttribute(name, buf);
}

/**
 * Adds a string of element content to the buffer.
 * 
 * Replaces special characters in a string with XML character entities.
 * The characters replaced are '&' and '<'.
 * This should be when building strings intended to be the
 * values of XML attributes or XML element content.
 */
void XmlBuffer::addContent(const char* s) {

	size_t max = (s != nullptr) ? strlen(s) : 0;
	for (int i = 0 ; i < max ; i++) {
	    char ch = s[i];
	    if (ch == '&')
	      add("&amp;");
	    else if (ch == '<')
	      add("&lt;");
	    else
	      addChar(ch);
	}
}

/**
 * Add indentation to the buffer.
 */
void XmlBuffer::addIndent(int indent) {
	if (indent > 0) {
	    for (int i = 0 ; i < indent ; i++)
		  add(" ");
	}
}

/**
 * Adds an open element start tag.
 */
void XmlBuffer::addOpenStartTag(const char* name) {

	addOpenStartTag(mPrefix, name);
}

void XmlBuffer::addOpenStartTag(const char* nmspace, const char* name) {

	addIndent(mIndent);
	add("<");
	if (nmspace != nullptr && strchr(name, ':') == nullptr) {
		add(nmspace);
	    add(":");
	}
	add(name);
	checkNamespace();
}

void XmlBuffer::checkNamespace() {

	if (!mNamespaceDeclared) {

	    if (mNamespace != nullptr) {
			add(" xmlns");
			if (mPrefix != nullptr) {
				add(":");
				add(mPrefix);
			}
			add("='");
			add(mNamespace);
			add("'");
	    }

#if 0
	    if (mNamespaces != nullptr) {
			Iterator it = mNamespaces.entrySet().iterator();
			while (it.hasNext()) {	
				Map.Entry entry = (Map.Entry)it.next();
				add(" xmlns:");
				add((String)entry.getKey());
				add("='");
				add((String)entry.getValue());
				add("'");
			}
	    }
#endif
		mNamespaceDeclared = true;
	}
}

/**
 * Close an open start tag.
 */
void XmlBuffer::closeStartTag() {
	closeStartTag(true);
}

/**
 * Close an element with control over trailing newline.
 */
void XmlBuffer::closeStartTag(bool newline) {
	add(">");
	if (newline)
	  add("\n");
}

/**
 * Close an empty open start tag.
 */
void XmlBuffer::closeEmptyElement() {
	add("/>\n");
}

/**
 * Adds a closed element start tag followed by a newline.
 */
void XmlBuffer::addStartTag(const char* name) {
	addStartTag(mPrefix, name, true);
}

void XmlBuffer::addStartTag(const char* nmspace, const char* name) {
	addStartTag(nmspace, name, true);
}

/**
 * Adds a closed element start tag with control over the 
 * trailing newline.
 */
void XmlBuffer::addStartTag(const char* name, bool newline) {

	addStartTag(mPrefix, name, newline);
}

void XmlBuffer::addStartTag(const char* nmspace, const char* name, bool newline) {

	addIndent(mIndent);
	add("<");
	if (nmspace != nullptr && strchr(name, ':') == nullptr) {
		add(nmspace);
	    add(":");
	}
	add(name);
	checkNamespace();
	add(">");
	if (newline)
	  add("\n");
}

/**
 * Adds an element end tag.
 */
void XmlBuffer::addEndTag(const char* name) {
	addEndTag(mPrefix, name);
}

void XmlBuffer::addEndTag(const char* nmspace, const char* name) {
	addEndTag(nmspace, name, true);
}

/**
 * Adds an element end tag, with control over indentation.
 */
void XmlBuffer::addEndTag(const char* name, bool indent) {
	addEndTag(mPrefix, name, indent);
}

void XmlBuffer::addEndTag(const char* nmspace, const char* name, bool indent) {
	if (indent)
	  addIndent(mIndent);
	add("</");
	if (nmspace != nullptr && strchr(name, ':') == nullptr) {
		add(nmspace);
	    add(":");
	}
	add(name);
	add(">\n");
}
/**
 * Adds an element with content to the buffer, being careful
 * to escape content.
 */
void XmlBuffer::addElement(const char* element, const char* content) {

	addElement(mPrefix, element, content);
}

void XmlBuffer::addElement(const char* nmspace, const char* element, 
								  const char* content) {

	if (content != nullptr) {
	    addStartTag(nmspace, element, false);
	    addContent(content);
	    addEndTag(nmspace, element, false);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
