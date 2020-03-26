//#define _XOPEN_SOURCE 500
#define _POSIX_C_SOURCE 200809L
#include "file_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "errors.h"

#define DAY (3600*24)
#define DELIMITER ","

int parse_file(const char *path,int (*parse_line)(const char*))
{
		char *line=NULL;
		size_t lineBuf=0;
		FILE *input=fopen(path,"r");

		if(input == NULL) {
				return -1;
		}
		int i=0;

		while(getline(&line,&lineBuf,input) > 0) {
				(*parse_line)(strtok(line,"\n"));
				line=NULL;
				lineBuf=0;
		}
		fclose(input);
		return 0;
}

float range_query(const char *path,const char *date_from,const char *date_to,float (*parse_line)(char*))
{
		char *line=NULL;
		size_t line_buf=0;
		ssize_t line_size=0;
		float result=0;
		FILE *input=fopen(path,"r");

		ASSERT(NULL != input,"Error opening range query file");

		time_t date_1 = mktime(getdate(date_from));
		time_t date_2 = mktime(getdate(date_to));

		while((line_size=getline(&line,&line_buf,input)) > 0) {

				// Strip newline
				line[line_size-1] = '\0';

				ASSERT(date_2 > date_1,"`From` date after `to` date!");

				char *_line = strdup(line);
				time_t date_3 = mktime(getdate(strtok(line,DELIMITER)));

				if(date_1 <= date_3 && date_3 <= date_2) {
					result += (*parse_line)(_line);
				}

				free(line);
				free(_line);
				line=NULL;
				line_buf=0;
		}

		fclose(input);
		return result;
}

float category_range_query(const char *path,const char *date_from,const char *date_to,const char *categ_name,float (*parse_line)(char*))
{
		char *line=NULL;
		size_t line_buf=0;
		ssize_t line_size=0;
		float result=0;
		FILE *input=fopen(path,"r");

		ASSERT(NULL != input,"Error opening range query file");

		time_t date_1 = mktime(getdate(date_from));
		time_t date_2 = mktime(getdate(date_to));

		while((line_size=getline(&line,&line_buf,input)) > 0) {

				// Strip newline
				line[line_size-1] = '\0';

				ASSERT(date_2 > date_1,"`From` date after `to` date!");

				time_t date_3 = mktime(getdate(strtok(line,DELIMITER)));

				if(date_1 <= date_3 && date_3 <= date_2 && !strcmp) {
					result += (*parse_line)(line);
				}

				free(line);
				line=NULL;
				line_buf=0;
		}

		fclose(input);
		return result;
}

const char* append_to_file(const char *fpath, const char* string)
{
		FILE *output=fopen(fpath,"a");

		ASSERT(NULL != output,"Error opening append file!");
		ASSERT(EOF != fputs(string,output),"Appening to file failed!");

		fclose(output);

		return string;
}