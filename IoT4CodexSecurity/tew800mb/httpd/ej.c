/*
 * Tiny Embedded JavaScript parser
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: ej.c 241182 2011-02-17 21:50:03Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <httpd.h>

#include "../shared/bcmconfig.h"

static char * get_arg(char *args, char **next);
static void call(char *func, FILE *stream);

#if defined(__CONFIG_PROJECT_TEW800MB__) || defined(__CONFIG_PROJECT_TEW811DRU__) || defined(__CONFIG_PROJECT_TEW812DRUV2__)
//Tom.Hung 2012-5-11, Multi-language support
#define tagLen 32
#define transStringLen 4096
	
struct transLang {
	char tag[tagLen];
//	char transString[transStringLen];
	int offset;
}; 

struct transLang *currentLang;
char *csb = NULL; // Compact String Block
int langCounter = 0;
char multiLangInit = '0';

void lang_free()
{
	if (multiLangInit != '0')
	{
		//printf("lang_free: Start to release multi-lang mapping.\n");
		langCounter = 0;
		if (currentLang != NULL)
		{
			free(currentLang);
			currentLang = NULL;
		}
		if (csb != NULL)
		{
			free(csb);
			csb = NULL;
		}
		multiLangInit = '0';
		//printf("lang_free: End of release multi-lang mapping.\n");
	}
	//else
		//printf("lang_free: Don't need release multi-lang mapping again.\n");
}

int lang_get(char *tag, char **getString)
{
	int loopCount = 0;

	if (multiLangInit == '0')
		return 0;

	if (currentLang == NULL || csb == NULL)
		return 0;

	for (loopCount = 0; loopCount < langCounter; loopCount++)
	{
		if (!strcmp(currentLang[loopCount].tag, tag))
		{
			if (currentLang[loopCount].offset >= 0)
			{
				*getString = (csb + currentLang[loopCount].offset);
				return 1;
			}
			else
			{
				return 0;
			}
		}
	}

	return 0;
}

int lang_init()
{
	FILE *fp;
	struct stat fileStat;
	char fileName[64], *language, readBuf[4096], tagTmp[32];
	char *fileContent, *p, *begin, *end;
	int fileSize, offset = 0;
	int rowCounter = 0, loopCount = 0, resultLen = 0, checkCount = 0, saveCount = 0;

	lang_free();

	language = nvram_get("Language");
	memset(fileName, '\0', sizeof(fileName));
	snprintf(fileName, sizeof(fileName), "/www/lang/STRINGS_%s.txt", language);

	if ((fp = fopen(fileName, "r")) != NULL)
	{
		fstat(fileno(fp), &fileStat);
		fileSize = fileStat.st_size;

		while (fgets(readBuf, sizeof(readBuf), fp) != NULL)
		{
			rowCounter++;
		}
		langCounter = rowCounter;
		fclose(fp);

		if (multiLangInit == '0')
		{
			currentLang = (struct transLang *)malloc(rowCounter * sizeof(struct transLang));
			if (currentLang != NULL)
				memset(currentLang, '\0', rowCounter * sizeof(struct transLang));
			csb = (char *)malloc(fileSize * sizeof(char));
			if (csb != NULL)
				memset(csb, '\0', fileSize * sizeof(char));
		}

		if (currentLang != NULL && csb != NULL)
		{
			fp = fopen(fileName, "r");
			memset(readBuf, '\0', sizeof(readBuf));
			while (fgets(readBuf, sizeof(readBuf), fp) != NULL)
			{
				if ((p = strchr(readBuf, '\n')) != 0)
					*p = '\0';
				begin = readBuf;
				end = strchr(readBuf, ',');
				resultLen = end - begin;
				if (end)
				{
					*end = '\0';
					end += 1; // Shift to translate string head
					if (resultLen >= tagLen)
					{
						printf("lang_init: Tag '%s' too long! Skip it!\n", readBuf);
						memset(readBuf, '\0', sizeof(readBuf));
						continue;
					}

					memset(currentLang[loopCount].tag, '\0', tagLen);
					strncpy(currentLang[loopCount].tag, readBuf, resultLen);

					if (*end != '\0')
					{
						strcpy(csb + offset, end);
						currentLang[loopCount].offset = offset;
						offset = offset + strlen(end) + 1; // Move offset to point to next string head
					}
					else
					{
						printf("lang_init: Tag '%s' does not found translate string.\n", currentLang[loopCount].tag);
						currentLang[loopCount].offset = -1; // Means tag does not have corresponding translate string
					}

					loopCount++;
				}
				else
				{
					printf("lang_init: Division fail! Skip line '%s'.\n", readBuf);
					memset(readBuf, '\0', sizeof(readBuf));
					continue;
				}

				memset(readBuf, '\0', sizeof(readBuf));
			}

			fclose(fp);

			if (loopCount > 0)
				multiLangInit = '1';

			if (loopCount != rowCounter)
			{
				printf("lang_init: Please check mem content and original string file, that seems lose some row content!\n");
				printf("           The file row counter is %d and memory content counter is %d.\n", rowCounter, loopCount);
			}
		}
		else
		{
			printf("lang_init: Error!! Allocate memory to multi-lang fail!\n");
			if (currentLang != NULL)
			{
				free(currentLang);
				currentLang = NULL;
			}
			if (csb != NULL)
			{
				free(csb);
				csb = NULL;
			}

			return 0;
		}

		//printf("lang_init: Initial multi-language success! Current language is %s.\n", language);
		return 1;
	}
	else
	{
		//printf("lang_init: File %s open fail!\n", fileName);

		// Just clear all tag and value, we don't want use free to release it.
		if (multiLangInit == '1')
		{
			for (loopCount = 0; loopCount < langCounter; loopCount++)
			{
				memset(currentLang[loopCount].tag, '\0', tagLen);
				currentLang[loopCount].offset = 0;
			}
		}

		langCounter = 0;
		return 0;
	}
}

char *transString(char* tag, char* orgString)
{
	char *language = nvram_get("Language");
	char *transString;
	if (lang_get(tag, &transString) == 1 && strcmp(language, "EN")) {
		return transString;
	} else {
		return orgString;
	}
}
//Tom.Hung 2012-5-11 end
#endif // __CONFIG_PROJECT_TEW812DRU__

/* Look for unquoted character within a string */
static char *
unqstrstr(char *haystack, char *needle)
{
	char *cur;
	int q;

	for (cur = haystack, q = 0;
	     cur < &haystack[strlen(haystack)] && !(!q && !strncmp(needle, cur, strlen(needle)));
	     cur++) {
		if (*cur == '"')
			q ? q-- : q++;
	}
	return (cur < &haystack[strlen(haystack)]) ? cur : NULL;
}

static char *
get_arg(char *args, char **next)
{
	char *arg, *end;

	/* Parse out arg, ... */
	if (!(end = unqstrstr(args, ","))) {
		end = args + strlen(args);
		*next = NULL;
	} else
		*next = end + 1;

	/* Skip whitespace and quotation marks on either end of arg */
	for (arg = args; isspace((int)*arg) || *arg == '"'; arg++);
	for (*end-- = '\0'; isspace((int)*end) || *end == '"'; end--)
		*end = '\0';

	return arg;
}

static void
call(char *func, FILE *stream)
{
	char *args, *end, *next;
	int argc;
	char * argv[16];
	struct ej_handler *handler;

	/* Parse out ( args ) */
	if (!(args = strchr(func, '(')))
		return;
	if (!(end = unqstrstr(func, ")")))
		return;
	*args++ = *end = '\0';

	/* Set up argv list */
	for (argc = 0; argc < 16 && args && *args; argc++, args = next) {
		if (!(argv[argc] = get_arg(args, &next)))
			break;
	}

	/* Call handler */
	for (handler = &ej_handlers[0]; handler->pattern; handler++) {
		if (strncmp(handler->pattern, func, strlen(handler->pattern)) == 0)
			handler->output(0, stream, argc, argv);
	}
}

void
do_ej(char *path, FILE *stream)
{
	FILE *fp;
	int c;
	char pattern[1000], *asp = NULL, *func = NULL, *end = NULL;
	int len = 0;
#if defined(__CONFIG_PROJECT_TEW800MB__) || defined(__CONFIG_PROJECT_TEW811DRU__) || defined(__CONFIG_PROJECT_TEW812DRUV2__)
	char *multilang = NULL; //Tom.Hung 2012-4-10
	
#endif

	if (!(fp = fopen(path, "r")))
		return;

	while ((c = getc(fp)) != EOF) {

		/* Add to pattern space */
		pattern[len++] = c;
		pattern[len] = '\0';
		if (len == (sizeof(pattern) - 1))
			goto release;


		/* Look for <% ... */
		if (!asp && !strncmp(pattern, "<%", len)) {
			if (len == 2)
				asp = pattern + 2;
			continue;
		}

		/* Look for ... %> */
		if (asp) {
			if (unqstrstr(asp, "%>")) {
				for (func = asp; func < &pattern[len]; func = end) {
					/* Skip initial whitespace */
					for (; isspace((int)*func); func++);
					if (!(end = unqstrstr(func, ";")))
						break;
					*end++ = '\0';

					/* Call function */
					call(func, stream);
				}
				asp = NULL;
				len = 0;
			}
			continue;
		}

#if defined(__CONFIG_PROJECT_TEW800MB__) || defined(__CONFIG_PROJECT_TEW811DRU__) || defined(__CONFIG_PROJECT_TEW812DRUV2__)
		//Tom.Hung 2012-5-11, multi-language tag
		/* Look for <!--#tr */

		if (!multilang && !strncmp(pattern, "<!--#tr", len)) {
			if (len == 7) //strlen("<!--#tr")
				multilang = pattern + 7;
			continue;
		}

		/* Look for ... --> */
		if (multilang) {
			char *multilang_end;
			multilang_end=unqstrstr(multilang, "<!--#endtr -->");
			if(!multilang_end)
				multilang_end=unqstrstr(multilang, "<!--#endtr-->");
			if (multilang_end) {
				char *language, *tags, *tage, *funcs, *orgString_start, *orgString_end, *transString;
				char tag[64], tmp[64], orgString[transStringLen];
				int tag_len, tmp_len, orgString_len;
				unsigned int replace=0;

				language = nvram_get("Language");
				memset(tmp, 0, sizeof(tmp));
				memset(orgString, 0, sizeof(orgString));

				//Find end string to split multi-language tag and original string
				if(!(end = strstr(multilang, "-->")))
					continue;
				else {
					orgString_start = end+3;
					orgString_end = multilang_end;
					orgString_len = orgString_end-orgString_start;
					strncpy(orgString, orgString_start, orgString_len);
					tmp_len = end-multilang;
					strncpy(tmp, multilang, tmp_len);
					//printc("tmp=%s\n", tmp);
					//printc("orgString=%s\n", orgString);
				}

				/* Multi-language function */
				funcs=tmp;
				if(strlen(tmp) != 0) {
					if(tags=strstr(funcs, "id=\"")) {
						tags=tags+4;
						if(tage=strstr(tags, "\"")) {
							tag_len=tage-tags;

							memset(tag, 0, sizeof(tag));
							strncpy(tag, tags, tag_len);
							//printc("tag=%s\n", tag);
							replace=1;
						} else {
							printc("not found id end \".\n");
						}
					} else {
						printc("not found id tag!\n");
					}
				}
				if(!strcmp(language, "EN"))
					replace=0;
				if(replace) {
					if (lang_get(tag, &transString) == 1)	{
						websWrite(stream, "%s", transString);
					} else {
						printc("Can't find id=(%s)\n", tag);
						replace=0;
					}
				}
				if(!replace && strlen(orgString) != 0){
					websWrite(stream, "%s", orgString);
				}
				multilang = NULL;
				len = 0;
			}
			continue;
		}
		//Tom.Hung 2012-5-11 end
#endif //(__CONFIG_PROJECT_TEW800MB__) || (__CONFIG_PROJECT_TEW811DRU__) || (__CONFIG_PROJECT_TEW812DRUV2__)

	release:
		/* Release pattern space */
		fputs(pattern, stream);
		len = 0;
	}

	fclose(fp);
}

int
ejArgs(int argc, char **argv, char *fmt, ...)
{
	va_list	ap;
	int arg;
	char *c;

	if (!argv)
		return 0;

	va_start(ap, fmt);
	for (arg = 0, c = fmt; c && *c && arg < argc;) {
		if (*c++ != '%')
			continue;
		switch (*c) {
		case 'd':
			*(va_arg(ap, int *)) = atoi(argv[arg]);
			break;
		case 's':
			*(va_arg(ap, char **)) = argv[arg];
			break;
		}
		arg++;
	}
	va_end(ap);

	return arg;
}
