#!/usr/bin/env python

#   read wmr100-pi data from stdin and send it to rs.angara.net

#   "link/ether b8:27:eb:9e:1c:c9 "

from __future__ import print_function


import subprocess
import sys
import re


def perr(msg):
    print(msg, file=sys.stderr)
#--

def get_hwid():
    try:
        s = subprocess.Popen(["ip", "link"], stdout=subprocess.PIPE).communicate()
        return re.search("link/ether (..:..:..:..:..:..) ", s[0]).group(1)
    except Exception as ex:
        perr(str(ex))
        return "???"
#--


print( get_hwid() )

