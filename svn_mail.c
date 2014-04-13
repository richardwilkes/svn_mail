/*
 * Copyright (c) 2005 by Richard A. Wilkes. All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License,
 * version 2.0. If a copy of the MPL was not distributed with this file, You
 * can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as defined
 * by the Mozilla Public License, version 2.0.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

typedef struct Criteria Criteria;

struct Criteria {
	struct Criteria *	next;
	char *				dir_prefix;
	char *				subject_prefix;
	char *				email;
};

typedef struct DirList DirList;

struct DirList {
	struct DirList *	next;
	char *				dir;
};

static char *		gRepository;
static char *		gRevision;
static char *		gFrom;
static char *		gBuffer;
static int	 		gBufferUsed;
static int	 		gBufferAvailable;
static Criteria *	gCriteria;

static void init_buffer() {
	gBufferUsed			= 0;
	gBufferAvailable	= 1024;
	gBuffer				= (char *)malloc(gBufferAvailable);
}

static void reset_buffer() {
	gBufferAvailable	+= gBufferUsed;
	gBufferUsed			= 0;
}

static void add_to_buffer(int amt,char *buffer) {
	if (gBufferAvailable >= amt) {
		memcpy(&gBuffer[gBufferUsed],buffer,amt);
		gBufferUsed			+= amt;
		gBufferAvailable	-= amt;
	} else {
		int more = (gBufferUsed + gBufferAvailable) * 2;

		if (more < gBufferUsed + amt) {
			more = gBufferUsed + amt;
		}
		gBuffer				= realloc(gBuffer,more);
		gBufferAvailable	= more - gBufferUsed;
		memcpy(&gBuffer[gBufferUsed],buffer,amt);
		gBufferUsed			+= amt;
		gBufferAvailable	-= amt;
	}
}

static void add_string_to_buffer(char *buffer) {
	add_to_buffer(strlen(buffer),buffer);
}

static void run_svnlook(char *command) {
	static char *svnlook = "/usr/local/bin/svnlook";
	static char *svnlook2 = "/usr/bin/svnlook";
	int pid;
	int pipe_fd[2];

	reset_buffer();

	if (pipe(pipe_fd) < 0) {
		fprintf(stderr,"pipe() failed\n");
		exit(errno);
	}

	pid = fork();
	if (pid < 0) {
		fprintf(stderr,"fork() failed\n");
		exit(errno);
	}
	if (pid) {
		char buffer[1024];
		int amt;

		// Parent
		close(pipe_fd[STDOUT_FILENO]);
		amt = read(pipe_fd[STDIN_FILENO],buffer,1024);
		while (amt > 0) {
			add_to_buffer(amt,buffer);
			amt = read(pipe_fd[STDIN_FILENO],buffer,1024);
		}
		if (gBufferUsed > 0 && gBuffer[gBufferUsed - 1] == '\n') {
			gBuffer[gBufferUsed - 1] = 0;
		} else {
			buffer[0] = 0;
			add_to_buffer(1,buffer);
		}
		close(pipe_fd[STDIN_FILENO]);
	} else {
		FILE *fp = fopen(svnlook, "rb");
		char *cmd;
		if (fp) {
			fclose(fp);
			cmd = svnlook;
		} else {
			cmd = svnlook2;
		}
		// Child
		close(pipe_fd[STDIN_FILENO]);
		dup2(pipe_fd[STDOUT_FILENO],STDOUT_FILENO);
		close(pipe_fd[STDOUT_FILENO]);
		execl(cmd,cmd,command,gRepository,"-r",gRevision,NULL);
	}
}

static DirList *create_dir_list(char *buffer) {
	DirList *top = NULL;

	buffer = strtok(buffer,"\n");
	while (buffer) {
		DirList *one = (DirList *)calloc(1,sizeof(DirList));

		one->dir	= strdup(buffer);
		one->next	= top;
		top			= one;
		buffer		= strtok(NULL,"\n");
	}
	return top;
}

static void mail(char *to,char *subject,char *msg) {
	static char *sendmail = "/usr/sbin/sendmail";
	int pid;
	int pipe_fd[2];

	if (pipe(pipe_fd) < 0) {
		fprintf(stderr,"pipe() failed\n");
		exit(errno);
	}

	pid = fork();
	if (pid < 0) {
		fprintf(stderr,"fork() failed\n");
		exit(errno);
	}
	if (pid) {
		FILE *fp;

		// Parent
		close(pipe_fd[STDIN_FILENO]);
		fp = fdopen(pipe_fd[STDOUT_FILENO],"w");
		fprintf(fp,"To: %s\n",to);
		fprintf(fp,"From: %s\n",gFrom);
		fprintf(fp,"Subject: %s\n",subject);
		fprintf(fp,"Content-Type: text/plain; charset=UTF-8\n");
		fprintf(fp,"Content-Transfer-Encoding: 8bit\n");
		fprintf(fp,"\n");
		fprintf(fp,"%s\n",msg);
		fclose(fp);
	} else {
		// Child
		close(pipe_fd[STDOUT_FILENO]);
		dup2(pipe_fd[STDIN_FILENO],STDIN_FILENO);
		close(pipe_fd[STDIN_FILENO]);
		execl(sendmail,sendmail,"-i","-f",gFrom,to,NULL);
	}
}

static void process() {
	Criteria *criteria = gCriteria;
	DirList *dirs;
	char *author;
	char *date;
	char *log;
	char *changed;

	run_svnlook("author");
	author = strdup(gBuffer);
	run_svnlook("date");
	date = strdup(gBuffer);
	run_svnlook("log");
	log = strdup(gBuffer);
	run_svnlook("changed");
	changed = strdup(gBuffer);
	run_svnlook("dirs-changed");
	dirs = create_dir_list(gBuffer);

	reset_buffer();

	add_string_to_buffer("Author: ");
	add_string_to_buffer(author);
	add_string_to_buffer("\n");
	
	add_string_to_buffer("Date: ");
	add_string_to_buffer(date);
	add_string_to_buffer("\n");

	add_string_to_buffer("New Revision: ");
	add_string_to_buffer(gRevision);
	add_string_to_buffer("\n\n");

	add_string_to_buffer("Log:\n");
	add_string_to_buffer(log);
	add_string_to_buffer("\n\n");

	add_string_to_buffer("Change List:\n");
	add_string_to_buffer(changed);
	add_string_to_buffer("\n");

	while (criteria) {
		int match = 0;

		if (!strcmp(criteria->dir_prefix,"-")) {
			match = 1;
		} else {
			DirList *one = dirs;
			char *prefix = criteria->dir_prefix;
			int prefix_length = strlen(prefix);

			while (one) {
				if (!strncmp(prefix,one->dir,prefix_length)) {
					match = 1;
					break;
				}
				one = one->next;
			}
		}

		if (match) {
			char *subject = (char *)malloc(14 + strlen(criteria->subject_prefix) +
											strlen(gRevision) + strlen(author));

			sprintf(subject,"%s Revision %s (%s)",criteria->subject_prefix,gRevision,author);
			mail(criteria->email,subject,gBuffer);
			free(subject);
			break;
		}
		criteria = criteria->next;
	}
}

static void usage() {
	fprintf(stderr,"svn_mail\n");
	fprintf(stderr,"Copyright (c) 2005 by Richard A. Wilkes\n");
	fprintf(stderr,"All rights reserved worldwide\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Usage: svn_mail <svn repository path> <revision> <from email> <dir prefix=subject prefix=to email>...\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"There must be one or more <dir prefix=subject prefix=to email> specifications.\n");
	fprintf(stderr,"If 'dir prefix' is '-', it matches all directories. The first one to match wins.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Example:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"svn_mail /svn 1 no_reply@sample.com '-=[svn:mine]=svn_users@sample.com'\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"would send mail to svn_users@sample.com for all check-ins, and would prefix\n");
	fprintf(stderr,"the mail with '[svn:mine]'.\n");
	exit(1);
}

int main(int argc,char **argv) {
	Criteria *	current	= NULL;
	int			i;

	if (argc < 5) {
		usage();
	}

	gRepository	= argv[1];
	gRevision	= argv[2];
	gFrom		= argv[3];

	gCriteria = NULL;
	for (i = 4; i < argc; i++) {
		Criteria *criteria	= (Criteria *)calloc(1,sizeof(Criteria));

		criteria->dir_prefix = strtok(argv[i],"=");
		if (!criteria->dir_prefix) {
			usage();
		}
		criteria->subject_prefix = strtok(NULL,"=");
		if (!criteria->subject_prefix) {
			usage();
		}
		criteria->email = strtok(NULL,"=");
		if (!criteria->email) {
			usage();
		}
		if (current) {
			current->next = criteria;
		} else {
			gCriteria = criteria;
		}
		current = criteria;
	}

	init_buffer();
	process();
	return 0;
}
