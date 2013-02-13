#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include "pcre.h"

int check_request(int lines, char **request, time_t request_time, pcre **regexps, int num_regexps)
{
	int *matches, i, j, matched;

	matches = malloc(sizeof(int) * num_regexps);
	memset(matches, 0, (sizeof(int) * num_regexps));

	for(i=0; i < lines; i++) { 
		for(j=0; j < num_regexps; j++) {
			int ovector[30];
			if ( matches[j] ) continue;

			matched = pcre_exec(regexps[j], NULL, request[i], strlen(request[i]), 0, 0, ovector, 30);
			if ( matched > 0 )
				matches[j] = 1;	
		}
	}

	matched = 1;
	for (j=0; j < num_regexps; j++) {
		matched &= matches[j];
	}

	free(matches);
	return matched;
}	

void print_request(int request_lines, char **request)
{
	int i, j;
	putchar('\n');

	for(i=0; i < request_lines; i++) 
		printf("%s", request[i]);

	for(j=0; j < strlen(request[request_lines - 1]) && j < 80; j++ ) 
		putchar('-');

	putchar('\n');
	fflush(stdout);
}

int main(int argc, char **argv)
{
	int i, tick;
	ssize_t line_size, allocated;
	char *buf, *date_buf;

	time_t start_time;
	time_t end_time;

	const char *error;
	int erroffset;

	char **request;
	int request_lines;

	time_t request_tv;

	pcre *request_start = NULL;
	int num_regexps = 0;
	pcre **regexps = NULL;

	if ( argc < 4 ) { 
		fprintf(stderr, "Usage: ug_guts start_time end_time regexps [... regexps]\n");
		exit(1);
	}

	start_time = atol(argv[1]);
	end_time = atol(argv[2]);

	for ( i = 3; i < argc; i++) { 
		regexps = realloc(regexps, sizeof(pcre *) * (num_regexps + 1));
		regexps[num_regexps] = pcre_compile(argv[i], 0, &error, &erroffset, NULL);
		if ( error ) { 
			fprintf(stderr, "Error compiling regexp \"%s\": %s\n", argv[i], error);
			exit;
		}
		num_regexps++;
	}	

	request_start =  pcre_compile("^Processing.*(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})", 0, &error, &erroffset, NULL);

	buf = NULL;
	request = NULL;
	request_lines = 0;

	while ( (line_size = getline(&buf, &allocated, stdin)) != -1 ) {
		int ovector[30];
		int matched = pcre_exec(request_start, NULL, buf, line_size, 0, 0, ovector, 30);

		if (line_size == 1 )
			continue; 

		if ( matched > 0 ) {
			if ( request ) { 
				if ( request_tv >= start_time && 
						check_request(request_lines, request, request_tv, regexps, num_regexps)) {
					printf("@@%lu\n", request_tv);

					print_request(request_lines, request);
				}
				for(i=0; i < request_lines; i++) 
					free(request[i]);
				free(request);
				request = NULL;
				request_lines = 0;
			}
			
			pcre_get_substring(buf, ovector, matched, 1, (const char **) &date_buf);

			struct tm request_tm;
			strptime(date_buf, "%Y-%m-%d %H:%M:%S", &request_tm);

			free(date_buf);

			request_tv = mktime(&request_tm);

			if ( tick % 100 == 0 )
				printf("@@%lu\n", request_tv);

			if ( request_tv > (end_time) ) {
				exit(0);
			}
		}
		request = realloc(request, sizeof(char *) * (request_lines + 1));
		request[request_lines] = buf;
		request_lines++;
		tick++;
		buf = NULL;
	}
}

