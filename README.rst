============
vmod_perl
============

-----------------------
Perl Module for Varnish
-----------------------

:Author: Lubomir Rintel
:Date: 2011-12-09
:Version: 1.0
:Manual section: 3

SYNOPSIS
========

import perl;

DESCRIPTION
===========

This module makes it possible to extend Varnish with Perl code.
It makes it possible to evaluate Perl statements and call Perl subroutines.

FUNCTIONS
=========

Function VOID eval(PRIV_VCL, STRING)
Function STRING string(PRIV_VCL, STRING, STRING_LIST)
Function BOOL bool(PRIV_VCL, STRING, STRING_LIST)
Function INT num(PRIV_VCL, STRING, STRING_LIST)


eval
----

Prototype
        ::

                eval(STRING S)
Return value
	VOID
Description
	Evaluates given code in Perl interpreter.
Example
        ::

                perl.eval("warn 'Hello, World!'");

string
------

Prototype
        ::

                string(STRING FN, STRING_LIST S)
Return value
	STRING
Description
	Call Perl subroutine FN with given arguments S, return a string.
Example
        ::

                set req.header.Foo perl.string("MyModule::makeheader", "Foo", req.http.uri);

num
---

Prototype
        ::

                num(STRING FN, STRING_LIST S)
Return value
	INT
Description
	Call Perl subroutine FN with given arguments S, return an integer.

bool
----

Prototype
        ::

                num(STRING FN, STRING_LIST S)
Return value
	BOOL
Description
	Call Perl subroutine FN with given arguments S, return a logical value.
Example
        ::

                if (perl.bool("MyModule::baduri", req.http.uri)) {
                        return(error);
                }

INSTALLATION
============

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the varnishtest tool.

Usage::

 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``

In your VCL you could then use this vmod along the following lines::
        
        import perl;

        sub vcl_deliver {
                perl.eval("use MyModule");
                set resp.http.hello = perl.call("MyModule::hello", "World");
        }

COPYRIGHT
=========

This document is licensed under the same license as the
Varnish project. See LICENSE for details.

* Copyright (c) 2011 Varnish Software
* Copyright (c) 2011 Lubomir Rintel
