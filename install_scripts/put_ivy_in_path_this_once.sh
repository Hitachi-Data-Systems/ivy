#!/bin/bash

if ! echo ${PATH} | /bin/grep -q /scripts/ivy/bin ; then
   PATH=${PATH}:/scripts/ivy/bin/latest
   export PATH
fi

