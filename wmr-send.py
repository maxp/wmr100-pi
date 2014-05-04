#!/usr/bin/env python

#   read wmr100-pi data from stdin and send it to rs.angara.net

#   on.environ {HWID,PSW}


from __future__ import print_function

import time
from threading import Thread
import subprocess
import sys
import re
import os


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


def sender(data):
    """ collects data from stdin, updates data dict: min, max, total, count """

    print("started")

    s = sys.stdin.readline()
    while s:
        print("s:", s)
        if s[0] == '*':
            print(s[1:])


        #-
        s = sys.stdin.readline()
    #-
#--


# main

cycle = 0
hwid  = os.environ.get("HWID") or get_hwid()
psw   = os.environ.get("PSW")  

data = {}

# ct = Thread(target=collector, args=(data,))
# ct.start()

while True:
    s = sys.stdin.readline()
    if s[0] == '*':
        print(s[1:])

    # cycle += 1
    # print("cycle:", cycle)

    # if cycle >= 20: break
    # time.sleep(1)

#-

#.





# psw="secret"
# url='http://example.com/dat?'
# wget='wget -q -O - '
# cycle_file='/tmp/cycle'    



#   qs="hwid=${hwid}&cycle=${cycle}&t=${val}"
#   hk = sha1sum(qs+psw)
#   qs="${qs}&_hkey=${hk}"
