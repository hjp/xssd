/*
    xssd - extremely simple sudo


    This is a replacement for sudo. It is intended to be extremely
    simple and therefore easy to verify for correctness.

    Usage: xssd user command arguments

    xssd then looks up the configuration file 
    /etc/xssd/<user>/<command>, which contains lines in 
    Keyword: Value format, or comments starting with "#".
    
    Currently defined keywords are:

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

#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>


char xssd_c_cvs_version[] = 
    "$Id: xssd.c,v 1.6 2002-04-26 15:17:26 hjp Exp $";


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

static void clean_fds(void) {
    /*
     * Make sure stdin/out/err are open.
     * On Linux and some BSDs the kernel already ensures that for setuid
     * programs, but on most other UNIXes don't.
     *
     * xssd itself isn't exploitable (at worst, an attacker could
     * redirect stderr to the syslog), but the scripts started by xssd
     * might be.
     *
     * We could also close all fds > 2 here, but that might prevent some
     * legitimate uses, so we delegate that responsibilitly to the
     * called programs.
     */

    int fd;

    do {
	fd = open("/dev/null", O_RDONLY);
	if (fd == -1) exit(1); /* tough luck :-) */
    } while (fd <= 2);
    close(fd);
}

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
    
    clean_fds();

    cmnd = argv[0];

    if (argc < 3) usage();

    user = argv[1];

    openlog("xssd", LOG_PID | LOG_PERROR, LOG_AUTH);

    /* a / in either the username or the command name could be used to 
     * break out of /etc/xssd. Of course the user would actually have to 
     * exist to do harm, but better safe than sorry.
     */
    if (strchr(user, '/')) {
	syslog(LOG_ERR, "invalid username %s. [Ruid: %d]",
	       user, getuid());
	exit(1);
    }
    if (strchr(argv[2], '/')) {
	syslog(LOG_ERR, "invalid command name %s. [Ruid: %d]",
	       argv[2], getuid());
	exit(1);
    }

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
	 

	/* Check for keywords. 
	 */
	if (*line == '#') {
	    continue;
	}
	if (strncmp(line, "Command:", strlen("Command:") ) == 0) {
	    int lp;

	    skip_ws(line, lp, strlen("Command:"));
	    command = strdup(line + lp);
	    continue;
	}
	if (strncmp(line, "User:", strlen("User:") ) == 0) {
	    int lp;
	    struct passwd *pw;
	    skip_ws(line, lp, strlen("User:"));

	    pw = getpwnam(line + lp);
	    if (pw && pw->pw_uid == getuid()) {
		grant = 1;
	    }
	    continue;
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
	    continue;
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
	    continue;
	}
	/* if we get here, we have an unknown keyword or mistyped or ...
	 */
	fprintf(stderr, "Do not know what to do with \"%s\"\n", line);
	exit(1);
    }
    fclose(fp);
    if (env_i >= env_a) {
	env_a = env_a * 3 / 2 + 10;
	if ((env = realloc(env, env_a * sizeof(*env))) == NULL) {
	    syslog(LOG_ERR, "config file %s[%d]: cannot realloc env to %ld strings: %s. [Ruid: %d]",
		   cfgfile, linenr, (long)env_a, strerror(errno), getuid());
	    exit(1);
	}
    }
    env[env_i] = NULL;

    if (grant) {
	syslog(LOG_INFO, "%s: access granted. [Ruid: %d]", cfgfile, getuid());
    } else {
	syslog(LOG_NOTICE, "%s: access denied. [Ruid: %d]", cfgfile, getuid());
	exit(1);
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
	exit(1);
    }
    if (setgid(pw->pw_gid) == -1) {
	syslog(LOG_ERR, "%s: setgid(%d) failed: %s. [Ruid: %d]",
	       cfgfile, (int)pw->pw_gid, strerror(errno), getuid());
	exit(1);
    }
    if (setuid(pw->pw_uid) == -1) {
	syslog(LOG_ERR, "%s: setuid(%d) failed: %s. [Ruid: %d]",
	       cfgfile, (int)pw->pw_uid, strerror(errno), getuid());
	exit(1);
    }
    syslog(LOG_INFO, "%s: execing %s. [Ruid: %d]",
	   cfgfile, command, getuid());
    execve(command, argv + 2, env);
    syslog(LOG_ERR, "%s: execve(%s) failed: %s. [Ruid: %d]",
	   cfgfile, command, strerror(errno), getuid());

    return 1;
}

/* 
    $Log: xssd.c,v $
    Revision 1.6  2002-04-26 15:17:26  hjp
    Close config file after use.
    Make sure stdin/out/err are open.

    Revision 1.5  2002/01/26 23:20:25  hjp
    Don't allow / in command and user name to prevent /../ attack.
    Exit on failure to set user or group ids.
    (Thanks to Günther Leber for reporting these problems)

    Fixed log level on successful execution.
    CVS ----------------------------------------------------------------------

    Revision 1.4  2001/11/19 08:23:06  hjp
    Croak on unknown keywords. Made Comments explicit.
    Thanks to Bernd Petrovitsch for the patch.

    Revision 1.3  2001/11/12 20:39:58  hjp
    Outch. We should really deny access not only say that we do.

    Revision 1.2  2001/11/12 20:22:55  hjp
    Removed wrong argument from syslog (Shouldn't gcc know that syslog
    uses a printf-like format string?)

    Revision 1.1  2001/11/12 10:24:55  hjp
    Pre-Release


 */
