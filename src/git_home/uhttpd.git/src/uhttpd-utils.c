/*
 * uhttpd - Tiny single-threaded httpd - Utility functions
 *
 *   Copyright (C) 2010 Jo-Philipp Wich <xm@subsignal.org>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#define _XOPEN_SOURCE 500	/* crypt() */
#define _BSD_SOURCE			/* strcasecmp(), strncasecmp() */

#include "uhttpd.h"
#include "uhttpd-file.h"
#include "uhttpd-utils.h"
#include "string.h"
#include "stdio.h"
#include <sys/sysinfo.h>

#ifdef HAVE_TLS
#include "uhttpd-tls.h"
#endif

enum {
        AUTH_OK=1,
        AUTH_TIMEOUT,
        AUTH_MULTI,
	AUTH_MULTI_GUEST
};
void arp_mac(char *ipaddr, char *mac, char *dev);
char *cat_file(char *name);
void echo_set(char *value, char *file);

static char *uh_index_files[] = {
	"start.html",
	"start.htm",
	"default.html",
	"default.htm"
};

void __nprintf(const char *fmt, ...)
{
	va_list ap;
	static FILE *filp;

	if ((filp == NULL) && (filp = fopen("/dev/console", "a")) == NULL)
		return;

	va_start(ap, fmt);
	vfprintf(filp, fmt, ap);
	fputs("\n", filp);
	va_end(ap);
}

void debug_printf(char *cfgname, const char *fmt, ...)
{
        if(config_match(cfgname, "1"))
        {
                va_list ap;
                static FILE *filp;

                if ((filp == NULL) && (filp = fopen("/dev/console", "a")) == NULL)
                        return;

                va_start(ap, fmt);
                vfprintf(filp, fmt, ap);
                fputs("\n", filp);
                va_end(ap);
        }
}

const char * sa_straddr(void *sa)
{
	static char str[INET6_ADDRSTRLEN];
	struct sockaddr_in *v4 = (struct sockaddr_in *)sa;
	struct sockaddr_in6 *v6 = (struct sockaddr_in6 *)sa;

	if( v4->sin_family == AF_INET )
		return inet_ntop(AF_INET, &(v4->sin_addr), str, sizeof(str));
	else
		return inet_ntop(AF_INET6, &(v6->sin6_addr), str, sizeof(str));
}

const char * sa_strport(void *sa)
{
	static char str[6];
	snprintf(str, sizeof(str), "%i", sa_port(sa));
	return str;
}

int sa_port(void *sa)
{
	return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
}

int sa_rfc1918(void *sa)
{
	struct sockaddr_in *v4 = (struct sockaddr_in *)sa;
	unsigned long a = htonl(v4->sin_addr.s_addr);

	if( v4->sin_family == AF_INET )
	{
		return ((a >= 0x0A000000) && (a <= 0x0AFFFFFF)) ||
		       ((a >= 0xAC100000) && (a <= 0xAC1FFFFF)) ||
		       ((a >= 0xC0A80000) && (a <= 0xC0A8FFFF));
	}

	return 0;
}

/* Simple strstr() like function that takes len arguments for both haystack and needle. */
char *strfind(char *haystack, int hslen, const char *needle, int ndlen)
{
	int match = 0;
	int i, j;

	for( i = 0; i < hslen; i++ )
	{
		if( haystack[i] == needle[0] )
		{
			match = ((ndlen == 1) || ((i + ndlen) <= hslen));

			for( j = 1; (j < ndlen) && ((i + j) < hslen); j++ )
			{
				if( haystack[i+j] != needle[j] )
				{
					match = 0;
					break;
				}
			}

			if( match )
				return &haystack[i];
		}
	}

	return NULL;
}

/* interruptable select() */
int select_intr(int n, fd_set *r, fd_set *w, fd_set *e, struct timeval *t)
{
	int rv;
	sigset_t ssn, sso;

	/* unblock SIGCHLD */
	sigemptyset(&ssn);
	sigaddset(&ssn, SIGCHLD);
	sigaddset(&ssn, SIGPIPE);
	sigprocmask(SIG_UNBLOCK, &ssn, &sso);

	rv = select(n, r, w, e, t);

	/* restore signal mask */
	sigprocmask(SIG_SETMASK, &sso, NULL);

	return rv;
}


int uh_tcp_send_lowlevel(struct client *cl, const char *buf, int len)
{
	fd_set writer;
	struct timeval timeout;

	FD_ZERO(&writer);
	FD_SET(cl->socket, &writer);

	timeout.tv_sec = cl->server->conf->network_timeout;
	timeout.tv_usec = 0;

	if (select(cl->socket + 1, NULL, &writer, NULL, &timeout) > 0)
		return send(cl->socket, buf, len, 0);

	return -1;
}

int uh_tcp_send(struct client *cl, const char *buf, int len)
{
#ifdef HAVE_TLS
	if (cl->tls)
		return cl->server->conf->tls_send(cl, (void *)buf, len);
	else
#endif
		return uh_tcp_send_lowlevel(cl, buf, len);
}

int uh_tcp_peek(struct client *cl, char *buf, int len)
{
	/* sanity check, prevent overflowing peek buffer */
	if (len > sizeof(cl->peekbuf))
		return -1;

	int sz = uh_tcp_recv(cl, buf, len);

	/* store received data in peek buffer */
	if( sz > 0 )
	{
		cl->peeklen = sz;
		memcpy(cl->peekbuf, buf, sz);
	}

	return sz;
}

int uh_tcp_recv_lowlevel(struct client *cl, char *buf, int len)
{
	fd_set reader;
	struct timeval timeout;

	FD_ZERO(&reader);
	FD_SET(cl->socket, &reader);

	timeout.tv_sec  = cl->server->conf->network_timeout;
	timeout.tv_usec = 0;

	if (select(cl->socket + 1, &reader, NULL, NULL, &timeout) > 0)
		return recv(cl->socket, buf, len, 0);

	return -1;
}

int uh_tcp_recv(struct client *cl, char *buf, int len)
{
	int sz = 0;
	int rsz = 0;

	/* first serve data from peek buffer */
	if (cl->peeklen > 0)
	{
		sz = min(cl->peeklen, len);
		len -= sz; cl->peeklen -= sz;
		memcpy(buf, cl->peekbuf, sz);
		memmove(cl->peekbuf, &cl->peekbuf[sz], cl->peeklen);
	}

	/* caller wants more */
	if (len > 0)
	{
#ifdef HAVE_TLS
		if (cl->tls)
			rsz = cl->server->conf->tls_recv(cl, (void *)&buf[sz], len);
		else
#endif
			rsz = uh_tcp_recv_lowlevel(cl, (void *)&buf[sz], len);

		if (rsz < 0)
			return rsz;

		sz += rsz;
	}

	return sz;
}


int uh_http_sendhf(struct client *cl, int code, const char *summary, const char *fmt, ...)
{
	va_list ap;

	char buffer[UH_LIMIT_MSGHEAD];
	int len;

	len = snprintf(buffer, sizeof(buffer),
		"HTTP/1.1 %03i %s\r\n"
		"Connection: close\r\n"
		"Content-Type: text/plain\r\n"
		"Transfer-Encoding: chunked\r\n\r\n",
			code, summary
	);

	ensure_ret(uh_tcp_send(cl, buffer, len));

	va_start(ap, fmt);
	len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	ensure_ret(uh_http_sendc(cl, buffer, len));
	ensure_ret(uh_http_sendc(cl, NULL, 0));

	return 0;
}


int uh_http_sendc(struct client *cl, const char *data, int len)
{
	char chunk[8];
	int clen;

	if( len == -1 )
		len = strlen(data);

	if( len > 0 )
	{
		clen = snprintf(chunk, sizeof(chunk), "%X\r\n", len);
		ensure_ret(uh_tcp_send(cl, chunk, clen));
		ensure_ret(uh_tcp_send(cl, data, len));
		ensure_ret(uh_tcp_send(cl, "\r\n", 2));
	}
	else
	{
		ensure_ret(uh_tcp_send(cl, "0\r\n\r\n", 5));
	}

	return 0;
}

int uh_http_sendf(
	struct client *cl, struct http_request *req, const char *fmt, ...
) {
	va_list ap;
	char buffer[UH_LIMIT_MSGHEAD];
	int len;

	va_start(ap, fmt);
	len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	if( (req != NULL) && (req->version > 1.0) )
		ensure_ret(uh_http_sendc(cl, buffer, len));
	else if( len > 0 )
		ensure_ret(uh_tcp_send(cl, buffer, len));

	return 0;
}

int uh_http_send(
	struct client *cl, struct http_request *req, const char *buf, int len
) {
	if( len < 0 )
		len = strlen(buf);

	if( (req != NULL) && (req->version > 1.0) )
		ensure_ret(uh_http_sendc(cl, buf, len));
	else if( len > 0 )
		ensure_ret(uh_tcp_send(cl, buf, len));

	return 0;
}


int uh_urldecode(char *buf, int blen, const char *src, int slen)
{
	int i;
	int len = 0;

#define hex(x) \
	(((x) <= '9') ? ((x) - '0') : \
		(((x) <= 'F') ? ((x) - 'A' + 10) : \
			((x) - 'a' + 10)))

	for( i = 0; (i <= slen) && (i <= blen); i++ )
	{
		if( src[i] == '%' )
		{
			if( ((i+2) <= slen) && isxdigit(src[i+1]) && isxdigit(src[i+2]) )
			{
				buf[len++] = (char)(16 * hex(src[i+1]) + hex(src[i+2]));
				i += 2;
			}
			else
			{
				buf[len++] = '%';
			}
		}
		else
		{
			buf[len++] = src[i];
		}
	}

	return len;
}

int uh_urlencode(char *buf, int blen, const char *src, int slen)
{
	int i;
	int len = 0;
	const char hex[] = "0123456789abcdef";

	for( i = 0; (i <= slen) && (i <= blen); i++ )
	{
		if( isalnum(src[i]) || (src[i] == '-') || (src[i] == '_') ||
		    (src[i] == '.') || (src[i] == '~') )
		{
			buf[len++] = src[i];
		}
		else if( (len+3) <= blen )
		{
			buf[len++] = '%';
			buf[len++] = hex[(src[i] >> 4) & 15];
			buf[len++] = hex[(src[i] & 15) & 15];
		}
		else
		{
			break;
		}
	}

	return len;
}

int uh_b64decode(char *buf, int blen, const unsigned char *src, int slen)
{
	int i = 0;
	int len = 0;

	unsigned int cin  = 0;
	unsigned int cout = 0;


	for( i = 0; (i <= slen) && (src[i] != 0); i++ )
	{
		cin = src[i];

		if( (cin >= '0') && (cin <= '9') )
			cin = cin - '0' + 52;
		else if( (cin >= 'A') && (cin <= 'Z') )
			cin = cin - 'A';
		else if( (cin >= 'a') && (cin <= 'z') )
			cin = cin - 'a' + 26;
		else if( cin == '+' )
			cin = 62;
		else if( cin == '/' )
			cin = 63;
		else if( cin == '=' )
			cin = 0;
		else
			continue;

		cout = (cout << 6) | cin;

		if( (i % 4) == 3 )
		{
			if( (len + 3) < blen )
			{
				buf[len++] = (char)(cout >> 16);
				buf[len++] = (char)(cout >> 8);
				buf[len++] = (char)(cout);
			}
			else
			{
				break;
			}
		}
	}

	buf[len++] = 0;
	return len;
}

static char * canonpath(const char *path, char *path_resolved)
{
	char path_copy[PATH_MAX];
	char *path_cpy = path_copy;
	char *path_res = path_resolved;

	struct stat s;


	/* relative -> absolute */
	if( *path != '/' )
	{
		getcwd(path_copy, PATH_MAX);
		strncat(path_copy, "/", PATH_MAX - strlen(path_copy));
		strncat(path_copy, path, PATH_MAX - strlen(path_copy));
	}
	else
	{
		strncpy(path_copy, path, PATH_MAX);
	}

	/* normalize */
	while( (*path_cpy != '\0') && (path_cpy < (path_copy + PATH_MAX - 2)) )
	{
		if( *path_cpy == '/' )
		{
			/* skip repeating / */
			if( path_cpy[1] == '/' )
			{
				path_cpy++;
				continue;
			}

			/* /./ or /../ */
			else if( path_cpy[1] == '.' )
			{
				/* skip /./ */
				if( (path_cpy[2] == '/') || (path_cpy[2] == '\0') )
				{
					path_cpy += 2;
					continue;
				}

				/* collapse /x/../ */
				else if( (path_cpy[2] == '.') &&
				         ((path_cpy[3] == '/') || (path_cpy[3] == '\0'))
				) {
					while( (path_res > path_resolved) && (*--path_res != '/') )
						;

					path_cpy += 3;
					continue;
				}
			}
		}

		*path_res++ = *path_cpy++;
	}

	/* remove trailing slash if not root / */
	if( (path_res > (path_resolved+1)) && (path_res[-1] == '/') )
		path_res--;
	else if( path_res == path_resolved )
		*path_res++ = '/';

	*path_res = '\0';

	/* test access */
	if( !stat(path_resolved, &s) && (s.st_mode & S_IROTH) )
		return path_resolved;

	return NULL;
}


#if 1
/*
* Useful for debugging
*/
void print(char *value)
{
	FILE *fp;

	fp = fopen("/tmp/uhttpd_debug", "a");
	if (fp == NULL)
		return;
	fputs(value, fp);
	fclose(fp);
}
#endif

static char* generate_dumaos_path( const char* package, const char* type, const char* apppath, struct path_info *p ){
	static char dst[PATH_MAX];
	sprintf( dst, "/dumaos/apps/%s/%s/frontend/%s", type, package, apppath );
	if( stat( dst, &p->stat) || ( p->stat.st_mode & S_IFDIR ) ){
		sprintf( dst, "/dumaos/apps/%s/%s/frontend/%s/index.html", type, package, apppath );
		if( stat( dst, &p->stat) )
			return NULL;
	}

	return dst;
}

/*
* Apply DumaOS URL routing when url has "/apps" prefix. Treat as file
* unless suffix is "rpc/".
*/
static int dumaos_url_routing( const char* url, struct path_info *p ){
	size_t n = strlen( url );
	const char* szapp = "/www/apps/";
	const char* szrpc = "/rpc/";
	const int szapp_count = strlen( szapp );
	const int szrpc_count = strlen( szrpc );
	const int sz_total = szapp_count + szrpc_count;

	if( n > ( sz_total + 1 ) && strstr( url, szapp ) == url ){
		if( !strncmp( url + ( n - szrpc_count), szrpc, szrpc_count ) ){
			return 0;
		} else {
			char package[PATH_MAX];
			char *start = url + szapp_count;
			char *sptr = start, *dptr = package;
			char *apppath, *out;
			while( *sptr != '/' )
				*dptr++ = *sptr++;
			*dptr = '\0';
			apppath = sptr + 1;

			if( !( out = generate_dumaos_path( package, "system", apppath, p ) ) ){
				if( !( out = generate_dumaos_path( package, "usr", apppath, p ) ) ){
					return 0;
				}
			}

			p->phys = out;
			p->name = out;
			return 1;
		}
	}

	return 0;
}

struct path_info * uh_path_lookup(struct client *cl, const char *url)
{
	static char path_phys[PATH_MAX];
	static char path_info[PATH_MAX];
	static struct path_info p;

	char buffer[UH_LIMIT_MSGHEAD];
	char *docroot = cl->server->conf->docroot;
	char *pathptr = NULL;

	int slash = 0;
	int no_sym = cl->server->conf->no_symlinks;
	int i = 0;
	struct stat s;
	size_t buffer_szlen;

	/* back out early if url is undefined */
	if ( url == NULL )
		return NULL;

	memset(path_phys, 0, sizeof(path_phys));
	memset(path_info, 0, sizeof(path_info));
	memset(buffer, 0, sizeof(buffer));
	memset(&p, 0, sizeof(p));

	/* copy docroot */
	memcpy(buffer, docroot,
		min(strlen(docroot), sizeof(buffer) - 1));

	/* separate query string from url */
	if( (pathptr = strchr(url, '?')) != NULL )
	{
		p.query = pathptr[1] ? pathptr + 1 : NULL;

		/* urldecode component w/o query */
		if( pathptr > url )
			uh_urldecode(
				&buffer[strlen(docroot)],
				sizeof(buffer) - strlen(docroot) - 1,
				url, (int)(pathptr - url) - 1
			);
	}

	/* no query string, decode all of url */
	else
	{
		uh_urldecode(
			&buffer[strlen(docroot)],
			sizeof(buffer) - strlen(docroot) - 1,
			url, strlen(url)
		);
	}

	/* do url routing in uhttpd much faster & data processing competent than lua handler */
	if( dumaos_url_routing( buffer, &p ) )
		return &p;

	/* create canon path */
	for( i = strlen(buffer), slash = (buffer[max(0, i-1)] == '/'); i >= 0; i-- )
	{
		if( (buffer[i] == 0) || (buffer[i] == '/') )
		{
			memset(path_info, 0, sizeof(path_info));
			memcpy(path_info, buffer, min(i + 1, sizeof(path_info) - 1));

			if( no_sym ? realpath(path_info, path_phys)
			           : canonpath(path_info, path_phys)
			) {
				memset(path_info, 0, sizeof(path_info));
				memcpy(path_info, &buffer[i],
					min(strlen(buffer) - i, sizeof(path_info) - 1));

				break;
			}
		}
	}

	/* check whether found path is within docroot */
	if( strncmp(path_phys, docroot, strlen(docroot)) ||
	    ((path_phys[strlen(docroot)] != 0) &&
		 (path_phys[strlen(docroot)] != '/'))
	) {
		return NULL;
	}

	/* test current path */
	if( ! stat(path_phys, &p.stat) )
	{
		/* is a regular file */
		if( p.stat.st_mode & S_IFREG )
		{
			p.root = docroot;
			p.phys = path_phys;
			p.name = &path_phys[strlen(docroot)];
			p.info = path_info[0] ? path_info : NULL;
		}

		/* is a directory */
		else if( (p.stat.st_mode & S_IFDIR) && !strlen(path_info) )
		{
			/* ensure trailing slash */
			if( path_phys[strlen(path_phys)-1] != '/' )
				path_phys[strlen(path_phys)] = '/';

			/* try to locate index file */
			memset(buffer, 0, sizeof(buffer));
			memcpy(buffer, path_phys, sizeof(buffer));
			pathptr = &buffer[strlen(buffer)];

			/* if requested url resolves to a directory and a trailing slash
			   is missing in the request url, redirect the client to the same
			   url with trailing slash appended */
			if( !slash )
			{
				uh_http_sendf(cl, NULL,
					"HTTP/1.1 302 Found\r\n"
					"Location: %s%s%s\r\n"
					"Connection: close\r\n\r\n",
						&path_phys[strlen(docroot)],
						p.query ? "?" : "",
						p.query ? p.query : ""
				);

				p.redirected = 1;
			}
			else if( cl->server->conf->index_file )
			{
				strncat(buffer, cl->server->conf->index_file, sizeof(buffer));

				if( !stat(buffer, &s) && (s.st_mode & S_IFREG) )
				{
					memcpy(path_phys, buffer, sizeof(path_phys));
					memcpy(&p.stat, &s, sizeof(p.stat));
				}
			}
			else
			{
				for( i = 0; i < array_size(uh_index_files); i++ )
				{
					strncat(buffer, uh_index_files[i], sizeof(buffer));

					if( !stat(buffer, &s) && (s.st_mode & S_IFREG) )
					{
						memcpy(path_phys, buffer, sizeof(path_phys));
						memcpy(&p.stat, &s, sizeof(p.stat));
						break;
					}

					*pathptr = 0;
				}
			}

			p.root = docroot;
			p.phys = path_phys;
			p.name = &path_phys[strlen(docroot)];
		}
#ifdef SUPPORT_SOAP
		else if( strstr(path_info, "/soap/") || strstr(path_info, "/HNAP1/" ) )
		{
			p.root = docroot;
			p.phys = path_info;
			p.info = path_info;
			p.name = path_info;
		}
#endif
		/* 1.Fixed Bug28439:[USB] can't access the share folder with HTTP and HTTP(via internet)
		 * handle the special request of USB shares folder via HTTP method.
		 * 2.Fixed Bug29838:[Enable Password Recovery]the function of "Enable Password Recovery" is bad.
		 */
		else if( strstr(path_info, "/shares") || strstr(path_info, "/passwordrecovered.cgi") || strstr(path_info, "/restapi/"))
		{
			p.root = docroot;
			p.phys = path_info;
			p.info = path_info;
			p.name = path_info;
		}
	}

	return p.phys ? &p : NULL;
}


static struct auth_realm *uh_realms = NULL;

struct auth_realm * uh_auth_add(char *path, char *user, char *pass)
{
	struct auth_realm *new = NULL;
	struct passwd *pwd;

#ifdef HAVE_SHADOW
	struct spwd *spwd;
#endif

	if((new = (struct auth_realm *)malloc(sizeof(struct auth_realm))) != NULL)
	{
		memset(new, 0, sizeof(struct auth_realm));

		memcpy(new->path, path,
			min(strlen(path), sizeof(new->path) - 1));

		memcpy(new->user, user,
			min(strlen(user), sizeof(new->user) - 1));

		/* given password refers to a passwd entry */
		if( (strlen(pass) > 3) && !strncmp(pass, "$p$", 3) )
		{
#ifdef HAVE_SHADOW
			/* try to resolve shadow entry */
			if( ((spwd = getspnam(&pass[3])) != NULL) && spwd->sp_pwdp )
			{
				memcpy(new->pass, spwd->sp_pwdp,
					min(strlen(spwd->sp_pwdp), sizeof(new->pass) - 1));
			}

			else
#endif

			/* try to resolve passwd entry */
			if( ((pwd = getpwnam(&pass[3])) != NULL) && pwd->pw_passwd &&
				(pwd->pw_passwd[0] != '!') && (pwd->pw_passwd[0] != 0)
			) {
				memcpy(new->pass, pwd->pw_passwd,
					min(strlen(pwd->pw_passwd), sizeof(new->pass) - 1));
			}
		}

		/* ordinary pwd */
		else
		{
			memcpy(new->pass, pass,
				min(strlen(pass), sizeof(new->pass) - 1));
		}

		if( new->pass[0] )
		{
			new->next = uh_realms;
			uh_realms = new;

			return new;
		}

		free(new);
	}

	return NULL;
}

int have_invalid_char(char *str)
{
	char *p;

	p = str;

	if(p)
	{
		if(strstr(p, "/bin/") || 
			strstr(p, "/sbin/") ||
			strstr(p, "/etc/") ||
			strstr(p, "/../")
			)
			return 1;

		while (*p)
		{
			if(*p == '\'' || *p == '`')
				return 1;

			p++;
		}	
	}

	return 0;
}

int is_invalid_param(char *file, char *host, char *remote_addr, char *auth, char *cookie)
{
	if( have_invalid_char(file) ||
			have_invalid_char(host) ||
			have_invalid_char(remote_addr) ||
			have_invalid_char(auth) ||
			have_invalid_char(cookie)
	  )
		return 1;
	else
		return 0;
}

int uh_cgi_auth_check(
		struct client *cl, struct http_request *req, struct path_info *pi,
		int ignorepath, int islua
		) {
	int i, plen, rlen, ret;
	char buffer[UH_LIMIT_MSGHEAD];
	char *user = NULL;
	char *pass = NULL;
	char *file = NULL;
	char *remote_addr;
	char *auth, *host, *cookie;
	char command[1024*4], result[512];
	char mac[32], dev[32];
	FILE *fp;
	char *token=NULL;
	static const char* skip_files[] = {
		".js",
		".css",	".scss",
		".gif",	".jpg",	".png",	".svg",
		".ico",	".ttf", ".woff", ".woff2",
		".svg"
	};

	ret = AUTH_TIMEOUT;
	plen = pi ? strlen(pi->name) : 0;

	if (config_invmatch("hijack_process", "3"))
	{//if hijack_process != 3, skip auth check
		goto skipauth;
	}

	if( pi ){
		for( i = 0; i < ( sizeof( skip_files ) / sizeof( char* ) ); i++ ){
			if( strstr(pi->name, skip_files[i]) ){
				goto skipauth;
			}
		}
	}

	rlen = strlen(cl->server->conf->cgi_prefix);

	remote_addr = sa_straddr(&cl->peeraddr);

	if( ( pi && (plen >= rlen) && !strncasecmp(pi->name, cl->server->conf->cgi_prefix, rlen) ) || ignorepath )
	{
		auth=NULL;
		host=NULL;
		cookie=NULL;

		/* get user and pass*/
		foreach_header(i, req->headers)
		{
			if( !strcasecmp(req->headers[i], "Authorization")
			  ) {
				memset(buffer, 0, sizeof(buffer));
				auth = req->headers[i+1];
				uh_b64decode(buffer, sizeof(buffer) - 1,
						(unsigned char *) &req->headers[i+1][6],
						strlen(req->headers[i+1]) - 6);

				if( (pass = strchr(buffer, ':')) != NULL )
				{
					user = buffer;
					*pass++ = 0;
				}

			}
			else if( ! strcasecmp(req->headers[i], "Cookie") )
				cookie = req->headers[i+1];
			else if( ! strcasecmp(req->headers[i], "Host") )
				host = req->headers[i+1];

		}

		if (pi)
			file = pi->phys;
		else
			file = req->url;

		
		if( is_invalid_param(file, host, remote_addr, auth, cookie))
		{
			ret = AUTH_TIMEOUT;
			goto resp;
		}
		snprintf(command, sizeof(command), "net-cgi -a -f '%s' -n '%s' -d '%s' -p '%s' -k'%s'", file, host?:"", remote_addr, auth?:"", cookie?:"");

		debug_printf("authdebug", "uhttpd-----------%s\n", command);	
		fp = popen(command, "r");
		if(fp)
		{
			fgets(result, sizeof(result), fp);
			
			ret = atoi(result);

			if( ret == 0 )
				ret = AUTH_TIMEOUT;

			pclose(fp);
		}
		else
			ret = AUTH_TIMEOUT;

resp:		
		if( ret == AUTH_OK )
			return 1;
		else if( ret == AUTH_MULTI )
		{
			if( !islua ){
				uh_http_sendf(cl, NULL,
						"HTTP/%.1f 200 OK\r\n"
						"Server: uhttpd/1.0.0\r\n"
						"Date: %s\r\n"
						"Content-Type: text/html; charset=\"UTF-8\"\r\n"
						"Connection: close\r\n\r\n",
						req->version, uh_file_unix2date(time(NULL))); 

				uh_http_sendf(cl,NULL, "<html><head>\n"
						"<script>\n"
						"top.location.href=\"/multi_login.html\";\n"
						"</script>\n"
						"</head>\n"
						"<body bgcolor=\"#ffffff\">\n"
						"</body>\n"
						"</html>\n");
			} else {
				uh_http_sendf(cl, NULL,
						"HTTP/%.1f 418 Unauthorized\r\n"
						"WWW-Authenticate: Basic realm=\"NETGEAR %s\" %s\r\n"
						"Content-Type: text/plain\r\n"
						"Content-Length: 23\r\n"
						"Connection: close\r\n\r\n"
						"Authorization Required\n",
						req->version,
						cat_file("/module_name"),
						"Im a teapot");
			}

			return 0;
		}
		else if( ret == AUTH_MULTI_GUEST)
		{
			if( !islua ){
				uh_http_sendf(cl, NULL,
						"HTTP/%.1f 200 OK\r\n"
						"Server: uhttpd/1.0.0\r\n"
						"Date: %s\r\n"
						"Content-Type: text/html; charset=\"UTF-8\"\r\n"
						"Connection: close\r\n\r\n",
						req->version, uh_file_unix2date(time(NULL)));

				uh_http_sendf(cl,NULL, "<html><head>\n"
						"<script>\n"
						"top.location.href=\"/multi_guestlogin.html\";\n"
						"</script>\n"
						"</head>\n"
						"<body bgcolor=\"#ffffff\">\n"
						"</body>\n"
						"</html>\n");
			} else {
				uh_http_sendf(cl, NULL,
						"HTTP/%.1f 419 Unauthorized\r\n"
						"WWW-Authenticate: Basic realm=\"NETGEAR %s\" %s\r\n"
						"Content-Type: text/plain\r\n"
						"Content-Length: 23\r\n"
						"Connection: close\r\n\r\n"
						"Authorization Required\n",
						req->version,
						cat_file("/module_name"),
						"Im a teapot");
			}

			return 0;

		}
		else
		{
			arp_mac(remote_addr, mac, dev);

			snprintf(command, sizeof(command), "net-cgi -t %s", mac);

			fp = popen(command, "r");
			if(fp)
			{
				fgets(result, sizeof(result), fp);
				token = result;
				pclose(fp);
			}

			if(!token)
				token = "get token fail";

			#define LOGIN_ERROR    "/tmp/AUTH_login_error"         // value = "eee" will show unauth.cgi; else pop up login dialog
			char retry[4];
			char *recovery = "<html><head>\r\n"
				"<meta http-equiv='Pragma' content='no-cache'>\r\n"
				"<meta http-equiv='Cache-Control' content='no-cache'>\r\n"
				"<title> 401 Authorization</title>\r\n"
				"<script language=\"javascript\" type=\"text/javascript\">\r\n"
				"function cancelevent()\r\n"
				"{\r\n"
				"       top/*can not remove*/.location.href='/unauth.cgi';\r\n"
				"}\r\n"
				"</script>\r\n"
				"</head><body onload=cancelevent()></body></html>";
			int clen = strlen(recovery);

			strlcpy(retry,cat_file(LOGIN_ERROR),sizeof(retry));
			if(strcmp(retry, "eee") == 0)
			{
				echo_set("", LOGIN_ERROR);
				uh_http_sendf(cl, NULL,
						"HTTP/%.1f 200 OK\r\n"
						"Content-Type: text/html\r\n"
						"Content-Length: %d\r\n"
						"Connection: close\r\n"
						"\r\n"
						"%s",
						req->version,
						clen,
						recovery);

			}else{
				strcat(retry, "e");
				echo_set(retry, LOGIN_ERROR);
			

			uh_http_sendf(cl, NULL,
					"HTTP/%.1f %d Unauthorized\r\n"
					"Set-Cookie: auth_token=%s; Path=/;\r\n"
					"WWW-Authenticate: Basic realm=\"NETGEAR %s\"\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: %d\r\n"
					"Connection: close\r\n"
					"\r\n"
					"%s",
					req->version,
					401,
					token,
					cat_file("/module_name"),
					clen, 
					recovery);
			}
			return 0;
		}
	}

skipauth:
	return 1;
}

int uh_auth_check(
	struct client *cl, struct http_request *req, struct path_info *pi
) {
	int i, plen, rlen, protected;
	char buffer[UH_LIMIT_MSGHEAD];
	char *user = NULL;
	char *pass = NULL;

	struct auth_realm *realm = NULL;

	plen = strlen(pi->name);
	protected = 0;

	/* check whether at least one realm covers the requested url */
	for( realm = uh_realms; realm; realm = realm->next )
	{
		rlen = strlen(realm->path);

		if( (plen >= rlen) && !strncasecmp(pi->name, realm->path, rlen) )
		{
			req->realm = realm;
			protected = 1;
			break;
		}
	}

	/* requested resource is covered by a realm */
	if( protected )
	{
		/* try to get client auth info */
		foreach_header(i, req->headers)
		{
			if( !strcasecmp(req->headers[i], "Authorization") &&
				(strlen(req->headers[i+1]) > 6) &&
				!strncasecmp(req->headers[i+1], "Basic ", 6)
			) {
				memset(buffer, 0, sizeof(buffer));
				uh_b64decode(buffer, sizeof(buffer) - 1,
					(unsigned char *) &req->headers[i+1][6],
					strlen(req->headers[i+1]) - 6);

				if( (pass = strchr(buffer, ':')) != NULL )
				{
					user = buffer;
					*pass++ = 0;
				}

				break;
			}
		}

		/* have client auth */
		if( user && pass )
		{
			/* find matching realm */
			for( realm = uh_realms; realm; realm = realm->next )
			{
				rlen = strlen(realm->path);

				if( (plen >= rlen) &&
				    !strncasecmp(pi->name, realm->path, rlen) &&
				    !strcmp(user, realm->user)
				) {
					req->realm = realm;
					break;
				}
			}

			/* found a realm matching the username */
			if( realm )
			{
				/* check user pass */
				if (!strcmp(pass, realm->pass) ||
				    !strcmp(crypt(pass, realm->pass), realm->pass))
					return 1;
			}
		}

		/* 401 */
		uh_http_sendf(cl, NULL,
			"HTTP/%.1f 401 Unauthorized\r\n"
			"WWW-Authenticate: Basic realm=\"NETGEAR %s\"\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: 23\r\n"
			"Connection: close\r\n\r\n"
			"Authorization Required\n",
				req->version, cat_file("/module_name"));

		return 0;
	}

	return 1;
}


static struct listener *uh_listeners = NULL;
static struct client *uh_clients = NULL;

struct listener * uh_listener_add(int sock, struct config *conf)
{
	struct listener *new = NULL;
	socklen_t sl;

	if( (new = (struct listener *)malloc(sizeof(struct listener))) != NULL )
	{
		memset(new, 0, sizeof(struct listener));

		new->socket = sock;
		new->conf   = conf;

		/* get local endpoint addr */
		sl = sizeof(struct sockaddr_in6);
		memset(&(new->addr), 0, sl);
		getsockname(sock, (struct sockaddr *) &(new->addr), &sl);

		new->next = uh_listeners;
		uh_listeners = new;

		return new;
	}

	return NULL;
}

struct listener * uh_listener_lookup(int sock)
{
	struct listener *cur = NULL;

	for( cur = uh_listeners; cur; cur = cur->next )
		if( cur->socket == sock )
			return cur;

	return NULL;
}

void close_all_listeners(void)
{
	struct listener *cur = NULL;

	for( cur = uh_listeners; cur; cur = cur->next ){
		close( cur->socket );
	}
}

struct client * uh_client_add(int sock, struct listener *serv)
{
	struct client *new = NULL;
	socklen_t sl;

	if( (new = (struct client *)malloc(sizeof(struct client))) != NULL )
	{
		memset(new, 0, sizeof(struct client));

		new->socket = sock;
		new->server = serv;

		/* get remote endpoint addr */
		sl = sizeof(struct sockaddr_in6);
		memset(&(new->peeraddr), 0, sl);
		getpeername(sock, (struct sockaddr *) &(new->peeraddr), &sl);

		/* get local endpoint addr */
		sl = sizeof(struct sockaddr_in6);
		memset(&(new->servaddr), 0, sl);
		getsockname(sock, (struct sockaddr *) &(new->servaddr), &sl);

		new->next = uh_clients;
		uh_clients = new;
	}

	return new;
}

struct client * uh_client_lookup(int sock)
{
	struct client *cur = NULL;

	for( cur = uh_clients; cur; cur = cur->next )
		if( cur->socket == sock )
			return cur;

	return NULL;
}

void uh_client_remove(int sock)
{
	struct client *cur = NULL;
	struct client *prv = NULL;

	for( cur = uh_clients; cur; prv = cur, cur = cur->next )
	{
		if( cur->socket == sock )
		{
			if( prv )
				prv->next = cur->next;
			else
				uh_clients = cur->next;

			free(cur);
			break;
		}
	}
}


#ifdef HAVE_CGI
static struct interpreter *uh_interpreters = NULL;

struct interpreter * uh_interpreter_add(const char *extn, const char *path)
{
	struct interpreter *new = NULL;

	if( (new = (struct interpreter *)
			malloc(sizeof(struct interpreter))) != NULL )
	{
		memset(new, 0, sizeof(struct interpreter));

		memcpy(new->extn, extn, min(strlen(extn), sizeof(new->extn)-1));
		memcpy(new->path, path, min(strlen(path), sizeof(new->path)-1));

		new->next = uh_interpreters;
		uh_interpreters = new;

		return new;
	}

	return NULL;
}

struct interpreter * uh_interpreter_lookup(const char *path)
{
	struct interpreter *cur = NULL;
	const char *e;

	for( cur = uh_interpreters; cur; cur = cur->next )
	{
		e = &path[max(strlen(path) - strlen(cur->extn), 0)];

		if( !strcmp(e, cur->extn) )
			return cur;
	}

	return NULL;
}
#endif

int is_domain_name(char *host)
{
	char *p;
	char hostname[128];
	int dot_number=0;
	snprintf(hostname, sizeof(hostname),"%s", host);

	p=hostname;

	while(*p != '\0')
	{
		if( *p == '.')
		{
			 p++;
			 dot_number++;
		}
		else if( *p >= '0' && *p <= '9')
		{
			p++;
			continue;
		}
		else if( dot_number == 3 && *p == ':')
		{
			return 0;

		}
		else
			return 1;       // if hostname no only have number and '.', it auth have char, then it is a domain name
		if( dot_number == 4 )   // if hostname have four '.', the host name is not IP.
			return 1;
	}
	return 0;
}

int is_router_domain_addr(char *httphost)
{
	if ( httphost == NULL )
		return 0;
        else if(strcasecmp(httphost, "routerlogin.net")==0 || strcasecmp(httphost, "www.routerlogin.net")==0)
                return 0;
        else if(strcasecmp(httphost, "www.routerlogin.com")==0 || strcasecmp(httphost, "routerlogin.com")==0)
                return 0;
        else if(strcasecmp(httphost, "readyshare.routerlogin.net")==0 || strcasecmp(httphost, "readyshare.routerlogin.com")==0)
                return 0;
        else if(strcasecmp(httphost, "readyshare")==0)
                return 0;
        else if(strcasecmp(httphost, "mywifiext.net")==0 || strcasecmp(httphost, "mywifiext.com")==0 || strcasecmp(httphost, "www.mywifiext.com")==0 || strcasecmp(httphost, "www.mywifiext.net")==0)
		return 0;
	else if(strcasecmp(httphost, config_get("lan_ipaddr"))==0)
                return 0;
	else if(config_match("endis_ddns","1") && strncasecmp(httphost,config_get("sysDNSHost"),strlen(config_get("sysDNSHost")))==0)
		return 0;
        else
                return 1;
}

int is_ipv6_domain(char *httphost){
        char *p;
        char hostname[128];
        int colon_num = 0;
        int square_num = 1;
        int square_op_num = 1;

        snprintf(hostname, sizeof(hostname),"%s", httphost);
        p=hostname;
        while(*p != '\0'){
                if(*p == '['){
                        p++;
                        square_num--;
                }else if( *p == ':'){
                        p++;
                        colon_num++;
                }else if( *p == ']'){
                        p++;
                        square_op_num--;
                        break;
                }else{
                        p++;
                }
        }
        if(!square_num&&!square_op_num&&colon_num){
                return 1;
        }
        return 0;
}

int IsSameSubnet(char *lanip, char *lansubnet,char *wanip, char *wansubnet)
{
        unsigned int aip, bip, anet, bnet;

        aip = inet_addr(lanip);
        bip = inet_addr(wanip);

        anet = inet_addr(lansubnet);
        bnet = inet_addr(wansubnet);

        return (aip & anet) == (bip & anet) ||(aip & bnet) == (bip & bnet);
}

char *cat_file(char *name)
{
	int i;
	FILE *fp;
	static char buf[512];

	buf[0] = '\0';

	fp = fopen(name, "r");
	if (fp == NULL)
		return buf;
	flock(fileno(fp), LOCK_SH);
	fgets(buf, sizeof(buf), fp);
	flock(fileno(fp), LOCK_UN);
	fclose(fp);

	i = 0;
	while (buf[i] != '\0' && buf[i] != '\r' && buf[i] != '\n')
		i++;
	buf[i] = '\0';

	return buf;
}

/*
* Since we introduced concurrency to uhttpd this function was temperamental.
* Even after introducing  flocks it was possible for cat_file to read an empty
* string.
*
* The problem is that fopen with "w" truncates the file. If you perform the lock
* after opening the file the truncation is not guarded by the flock. Moreover
* make sure that funlock happens *after* fclose as that data must be flushed
* before the lock is released.
*
* -@NETDUMA_iain
*/
void echo_set(char *value, char *file)
{
	FILE *fp;
	int fd;

	fd = open(file, O_CREAT);
	if (fd == -1)
		return;
	flock(fd, LOCK_EX);

	fp = fopen(file, "w");
	if (fp == NULL)
		return;
	fputs(value, fp);
	fclose(fp);

	flock(fd, LOCK_UN);
	close(fd);
}


void arp_mac(char *ipaddr, char *mac, char *dev)
{
	int i;
	FILE *fp;
	char *p, line[256];
	int get_mac=0;

	*mac = *dev = '\0';

	/* ipv4 */
	fp = fopen("/proc/net/arp", "r");
	if (fp == NULL)
		goto ipv6;

	/* discard the one header lines */
	fgets(line, sizeof(line), fp);
	while (fgets(line, sizeof(line), fp))
	{
		p = line; i = 0;
		while(*p == ' ') p++;
		while(ipaddr[i] == *p) { p++; i++; }
		if (ipaddr[i] ||*p != ' ')
			continue;

		/* skip 2 columns */
		for (i = 2; i > 0 && *p != '\0'; i--)
		{
			while(*p == ' ') p++;
			while(*p != ' ' && *p) p++;
		}

		while(*p == ' ') p++;
		while(*p != ' ' && *p) *mac++ = *p++;
		*mac = '\0';

		while(*p == ' ' ||*p == '*') p++;
		while(*p != '\n' && *p) *dev++ = *p++;
		*dev = '\0';

		get_mac = 1;
		break;
	}
	fclose(fp);

	if( get_mac == 1)
		return;
ipv6:
	/* ipv6 */
	fp = popen("ip -6 neigh show", "r");
	if( fp == NULL )
		return;


	while (fgets(line, sizeof(line), fp))
	{
		p = line; i = 0;
		while(*p == ' ') p++;
		while(ipaddr[i] == *p) { p++; i++; }
		if (ipaddr[i] ||*p != ' ')
			continue;

		/* skip 1 columns */
		while(*p == ' ') p++;
		while(*p != ' ' && *p) p++;

		/* get dev */
		while(*p == ' ') p++;
		while(*p != ' ' && *p) *dev++ = *p++;
		*dev = '\0';

		/* skip 1 columns */
		while(*p == ' ') p++;
		while(*p != ' ' && *p) p++;

		/* get mac */
		while(*p == ' ') p++;
		while(*p != ' ' && *p) *mac++ = *p++;
		*mac = '\0';

		break;
	}

	pclose(fp);

}

#define LAN_IFNAME	"br0"

char *login_type(char *dev, int *local)
{
	if (strcmp(dev, LAN_IFNAME) == 0)
	{
		*local = 1;
		return "admin";
	}
	else
	{
		*local = 0;
		return "remote";
	}
}

static int rdfile(char *file, void *buf, int size)
{
	int fd, ret;

	fd = open(file, O_RDONLY, 0666);
	if (fd < 0)
		return 0;

	if (read(fd, buf,  size) != size)
		ret = 0;
	else
		ret = 1;
	close(fd);

	return ret;
}

/* read a word/integer non-zero value from `file'. */
int readw(char *file)
{
	int value;

	if (!rdfile(file, &value, sizeof(value)))
		value = 0;

	return value;
}

/* write a word/integer value to `file'. */
int writew(char *file, int value)
{
	int fd;
	int ret;

	fd = open(file, O_WRONLY |O_CREAT |O_TRUNC, 0666);
	if (fd < 0)
		return 0;

	ret = write(fd, &value, sizeof(int)) == sizeof(int);
	close(fd);

	return ret;
}

