Ivy Engine REST Client
======================
This library is designed to provide a simple interface for issuing commands to
a Ivy Engine Server using a REST API. It communicates using the python requests
HTTP library.


Requirements
============
This library requires the use of python 3.4 or later and the third-party
library "requests".

Ivy version 2.02.00 is required.


Installation
============
::

 $ python setup.py install


Documentation
=============

https://github.com/Hitachi-Data-Systems/ivy/docs


Tests
=====
From the root directory of the rest-client
::

 $ PYTHONPATH=$(pwd):$PYTHONPATH py.test test/*.py


Files
=====
* ivyrest/ -- Contains library code.
* docs/ -- Contains API documentation, Makefile and conf.py.
* CHANGES.txt -- Library change log.
* LICENSE.txt -- "Apache License Version 2.0".
* README.txt -- This document.
