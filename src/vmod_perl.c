#include <bin/varnishd/cache.h>
#include <stdio.h>
#include <vrt.h>
#include <EXTERN.h>
#include <perl.h>
#include <errno.h>
#include <stdarg.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <link.h>

#include "vcc_if.h"

/* Generated into perlxsi.c */
extern void xs_init(pTHX);

static struct lock perl_mtx;

static
void
teardown (priv)
	void *priv;
{
	PerlInterpreter *my_perl = priv;
	PERL_SET_CONTEXT(my_perl);
	if (my_perl) {
		perl_destruct(my_perl);
		perl_free(my_perl);
	}
	PERL_SYS_TERM();
}

/* Rebind symbols with RTLD_GLOBAL from an already loaded library,
 * so that they're accessible for us even if the library was originally
 * loaded with RTLD_LOCAL. */
int
reloadlib (info, size, data)
	struct dl_phdr_info *info;
	size_t size;
	void *data;
{
	const char *name = data;
	char *substr;

	/* Check if this is the desired library */
	substr = strstr (info->dlpi_name, name);

	/* Not found at all */
	if (substr == NULL)
		return 0;
	/* Not following / */
	if (substr != info->dlpi_name && *(substr-1) != '/')
		return 0;
	/* Not at the end of the full path */
	if (info->dlpi_name + strlen (info->dlpi_name)
		- strlen (name) != substr)
		return 0;

	/* Reload it */
	if (dlopen (info->dlpi_name, RTLD_NOW | RTLD_GLOBAL) == NULL) {
		fprintf (stderr, "Loading %s failed: %s", info->dlpi_name,
			dlerror ());
		return 0;
	}

	return 1;
}

int
init_function (priv, conf)
	struct vmod_priv *priv;
	const struct VCL_conf *conf;
{
	char **argv = { NULL };
	int argc = 0;
	int ret;
	static char *embedding[] = { "varnish" };
	PerlInterpreter *my_perl;

	Lck_New(&perl_mtx, lck_sessmem);

	/* Somewhat related technical discussion here:
	 * http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=416266 */
	if (!dl_iterate_phdr (reloadlib, "libperl.so")) {
		fprintf (stderr, "Could not reload Perl to bind required "
			"symbols. Dynamically linked extensions might not "
			"work, if they are not linked against libperl (if "
			"perl binary is statically linked to libperl).\n");
	}


	PERL_SYS_INIT(&argc, &argv);
	my_perl = (void *)perl_alloc();
	if (my_perl == NULL) {
		perror ("Could not allocate Perl interpreter");
		exit (1);
	}

	perl_construct (my_perl);
	ret = perl_parse (my_perl, xs_init,
		sizeof(embedding) / sizeof(embedding[0]),
		embedding, NULL);
	if (ret != 0) {
		fprintf (stderr, "Could not initialize Perl.\n");
		exit (ret);
	};

	priv->priv = my_perl;
	priv->free = teardown;
	return 0;
}

enum {
	WANTINT,
	WANTSTRING,
	WANTBOOL,
	WANTVOID
};

union callr {
	char *s;
	unsigned u;
	int i;
};

union callr
perl_call (type, session, priv, fn, param, ap)
	int type;
	struct sess *session;
	struct vmod_priv *priv;
	const char *fn;
	const char *param;
	va_list ap;
{
	int retn;
        unsigned allocd, len;
	char *perlret;
	union callr r;
	PerlInterpreter *my_perl = priv->priv;
	dSP;

	Lck_Lock (&perl_mtx);
	PERL_SET_CONTEXT(my_perl);

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	/* Construct stack from string arguments */
	while (param != vrt_magic_string_end) {
		XPUSHs(sv_2mortal(newSVpv(param, 0)));
		param = va_arg(ap, const char *);
	}

	/* Call Perl */
	PUTBACK;
	retn = call_pv (fn, G_SCALAR);
	SPAGAIN;
	if (retn != 1)
		croak ("Perl call did not return anything");

	switch (type) {
	case WANTINT:
		r.i = POPi;
		break;
	case WANTBOOL:
		r.u = POPi;
		break;
	case WANTSTRING:
		/* Copy result into Varnish workspace */
		allocd = WS_Reserve(session->wrk->ws, 0);
		r.s = session->wrk->ws->f;
		perlret = POPpx;
		len = strlen (perlret) + 1;
		if (len > allocd) {
			WS_Release(session->wrk->ws, 0);
			croak ("No space to copy Perl result into");
		}
		strcpy(r.s, perlret);
		WS_Release (session->wrk->ws, len);
		break;
	case WANTVOID:
		break;
	default:
		croak ("Bad type");
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	Lck_Unlock (&perl_mtx);
	return r;
}

void
vmod_call (session, priv, fn, param /* ... */)
	struct sess *session;
	struct vmod_priv *priv;
	const char *fn;
	const char *param;
{
	va_list ap;

	va_start(ap, param);
	perl_call (WANTVOID, session, priv, fn, param, ap);
	va_end(ap);
}

unsigned
vmod_bool (session, priv, fn, param /* ... */)
	struct sess *session;
	struct vmod_priv *priv;
	const char *fn;
	const char *param;
{
	va_list ap;
	union callr ret;

	va_start(ap, param);
	ret = perl_call (WANTBOOL, session, priv, fn, param, ap);
	va_end(ap);

	return ret.u;
}

int
vmod_num (session, priv, fn, param /* ... */)
	struct sess *session;
	struct vmod_priv *priv;
	const char *fn;
	const char *param;
{
	va_list ap;
	union callr ret;

	va_start(ap, param);
	ret = perl_call (WANTINT, session, priv, fn, param, ap);
	va_end(ap);

	return ret.i;
}


const char *
vmod_string (session, priv, fn, param /* ... */)
	struct sess *session;
	struct vmod_priv *priv;
	const char *fn;
	const char *param;
{
	va_list ap;
	union callr ret;

	va_start(ap, param);
	ret = perl_call (WANTSTRING, session, priv, fn, param, ap);
	va_end(ap);

	return ret.s;
}

void
vmod_eval (session, priv, eval)
	struct sess *session;
	struct vmod_priv *priv;
	const char *eval;
{
	PerlInterpreter *my_perl = priv->priv;
	Lck_Lock (&perl_mtx);
	eval_pv (eval, 0);
	Lck_Unlock (&perl_mtx);
}
