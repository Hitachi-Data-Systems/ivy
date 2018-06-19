#!/bin/bash

cp -p /scripts/ivy/install_scripts/ivy_etc_profile_d.sh /etc/profile.d

if ! echo ${PATH} | /bin/grep -q /scripts/ivy/bin ; then
   PATH=${PATH}:/scripts/ivy/bin/latest
   export PATH
fi

