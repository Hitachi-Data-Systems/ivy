Ivy Engine REST Client
======================
This library is designed to provide a simple interface for issuing commands to
a Ivy Engine Server using a REST API. It communicates using the python requests
HTTP library.


Requirements
============
This library requires the use of python 3.6 or later and the third-party
Python library/modules, such as:  "json", "requests", "jsonschema", "uuid", "sys"

Ivy version 2.01.01 is required.

By default Ivy REST API is accessible at port 9000.

Installation
============
::

$ export PATH=/opt/rh/rh-python36/root/bin:$PATH

$ python --version
Python 3.6.3

(as needed)
$ pip install requests
$ pip install json
$ pip install jsonschema
$ pip install uuid
$ pip install sys

$ mkdir ivy_rest_client
$ cd ivy_rest_client
$ tar xzf ivy_python_rest_client.tgz
$ cd python_rest_client

$ python setup.py install

(some sample python ivy scripts)
$ cd ../samples

$ unset http_proxy
$ unset https_proxy

(To try modify sample code - with correct host name and serial number)

Documentation
=============

https://github.com/Hitachi-Data-Systems/ivy/docs


Tests
=====
From the root directory of the rest-client
::


Files
=====
* ivyrest/ -- Contains library code.
* docs/ -- Contains API documentation, Makefile and conf.py.
* CHANGES.txt -- Library change log.
* LICENSE.txt -- "Apache License Version 2.0".
* README.txt -- This document.
