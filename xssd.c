/*
    xssd - extremely simple sudo


    This is a replacement for sudo. It is intended to be extremely
    simple and therefore easy to verify for correctness.

    Usage: xssd user command arguments

    xssd then looks up the configuration file 
    /etc/xssd/<user>/<command>, which contains lines in 
    Keyword: Value format. Currently defined keywords are:

    Command: The command which is really executed. Must be 
    	absolute path.
    User: A user which is allowed to execute this command. 
    Group: A group which is allowed to execute this command.
    Env: An environment variable which is passed through to the
	command. All other environment variables are discarded.

    Ideas for enhancements: Configurable logging, initializing env
    variables.

*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>


char xssd_c_cvs_version[] = 
    "$Id: xssd.c,v 1.1 2001-11-12 10:24:55 hjp Exp $";


char *cmnd;


static void usage(void) {
    fprintf(stderr, "Usage: %s user command arguments\n", cmnd);
    exit(1);
}

/*
    Skip whitespace in a string.

    initial: initial offset
    line: the string to be scanned
    lp: (line pointer) Points to first non-ws character on exit.
 */
#define skip_ws(line, lp, initial) \
    for ((lp) = (initial); \
	 isspace((unsigned char)(line)[(lp)]); \
	 (lp)++);

int main(int argc, char **argv) {
    FILE *fp;
    char cfgfile[128];
    char line[1024];
    int linenr = 0;
    int grant = 0;
    char **env = NULL;
    size_t env_a = 0;
    size_t env_i = 0;
    char *command = NULL;
    char *user;
    struct passwd *pw;
    
    cmnd = argv[0];

    if (argc < 3) usage();

    user = argv[1];

    openlog("xssd", LOG_PID | LOG_PERROR, LOG_AUTH);

    snprintf(cfgfile, sizeof(cfgfile), "/etc/xssd/%s/%s",
	     user, argv[2]);					/* check for return value unnecessary, because
	     							   fopen below will fail if no config file 
								   exists
								 */
    if ((fp = fopen(cfgfile, "r")) == NULL) {
	syslog(LOG_ERR, "%s: fopen failed: %s. [Ruid: %d]",
	       cfgfile, strerror(errno), getuid());
	exit(1);
    }
    while (fgets(line, sizeof(line), fp)) {
	int lp;

	linenr++;

	/* remove trailing white space */
	lp = strlen(line)-1;
	if (line[lp] != '\n') {
	    syslog(LOG_ERR, "config file %s[%d]: Line unterminated or too long. [Ruid: %d]",
		   cfgfile, linenr, getuid());
	    exit(1);
	}
	while (lp >= 0 && isspace((unsigned char)line[lp])) {
	    line[lp--] = '\0';
	}
	 

	/* Check for keywords. If none are found, ignore the line 
	   (Automatic comments :-)
	 */
	 
	if (strncmp(line, "Command:", strlen("Command:") ) == 0) {
	    int lp;

	    skip_ws(line, lp, strlen("Command:"));
	    command = strdup(line + lp);
	}
	if (strncmp(line, "User:", strlen("User:") ) == 0) {
	    int lp;
	    struct passwd *pw;
	    skip_ws(line, lp, strlen("User:"));

	    pw = getpwnam(line + lp);
	    if (pw && pw->pw_uid == getuid()) {
		grant = 1;
	    }
	}
	if (strncmp(line, "Group:", strlen("Group:") ) == 0) {
	    int lp;
	    struct group *gr;
	    skip_ws(line, lp, strlen("Group:"));

	    gr = getgrnam(line + lp);
	    if (gr) {
		if (gr->gr_gid == getgid()) {
		    grant = 1;
		} else {
		    gid_t groups[NGROUPS_MAX] = {0};
		    int ngroups = getgroups(NGROUPS_MAX, groups);
		    int i;
		    for (i = 0; i < ngroups; i++) {
			if (gr->gr_gid == groups[i]) {
			    grant = 1;
			}
		    }
		}
	    }
	}
	if (strncmp(line, "Env:", strlen("Env:") ) == 0) {
	    char *s;
	    int lp;
	    skip_ws(line, lp, strlen("Env:"));

	    fprintf(stderr, "Env: %s\n", line + lp);
	    s = getenv(line + lp);
	    if (s) {
		if (env_i >= env_a) {
		    env_a = env_a * 3 / 2 + 10;
		    if ((env = realloc(env, env_a * sizeof(*env))) == NULL) {
			syslog(LOG_ERR, "config file %s[%d]: cannot realloc env to %ld strings: %s. [Ruid: %d]",
			       cfgfile, linenr, (long)env_a, strerror(errno), getuid());
			exit(1);
		    }
		}
		if ((env[env_i] = malloc(strlen(line + lp) + 1 + strlen(s) + 1)) == NULL) {
		    syslog(LOG_ERR, "config file %s[%d]: cannot strdup env variable %s: %s. [Ruid: %d]",
			   cfgfile, linenr, line + lp, strerror(errno), getuid());
		}
		sprintf(env[env_i], "%s=%s", line + lp, s);
		env_i++;
	    }
	}
    }
    if (env_i >= env_a) {
	env_a = env_a * 3 / 2 + 10;
	if ((env = realloc(env, env_a * sizeof(*env))) == NULL) {
	    syslog(LOG_ERR, "config file %s[%d]: cannot realloc env to %ld strings: %s. [Ruid: %d]",
		   cfgfile, linenr, (long)env_a, strerror(errno), getuid());
	    exit(1);
	}
    }
    env[env_i] = NULL;

    if (!grant) {
	syslog(LOG_NOTICE, "%s: access denied. [Ruid: %d]", cfgfile, getuid());
    }

    pw = getpwnam (user); 
    if (pw == NULL) { 
	syslog(LOG_ERR, "%s: unknown target user %s. [Ruid: %d]",
	       cfgfile, user, getuid());
	exit(1);
    }
    if (initgroups(user, pw->pw_gid) == -1) {
	syslog(LOG_ERR, "%s: initgroups(%s, %d) failed: %s. [Ruid: %d]",
	       cfgfile, user, pw->pw_gid, strerror(errno), getuid());
    }
    if (setgid(pw->pw_gid) == -1) {
	syslog(LOG_ERR, "%s: setgid(%d) failed: %s. [Ruid: %d]",
	       cfgfile, (int)pw->pw_gid, strerror(errno), getuid());
    }
    if (setuid(pw->pw_uid) == -1) {
	syslog(LOG_ERR, "%s: setuid(%d) failed: %s. [Ruid: %d]",
	       cfgfile, (int)pw->pw_uid, strerror(errno), getuid());
    }
    syslog(LOG_ERR, "%s: execing %s. [Ruid: %d]",
	   cfgfile, command, strerror(errno), getuid());
    execve(command, argv + 2, env);
    syslog(LOG_ERR, "%s: execve(%s) failed: %s. [Ruid: %d]",
	   cfgfile, command, strerror(errno), getuid());

    return 1;
}

/* 
    $Log: xssd.c,v $
    Revision 1.1  2001-11-12 10:24:55  hjp
    Pre-Release


 */