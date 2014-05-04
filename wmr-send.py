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
    # psw="secret"
    # url='http://example.com/dat?'
    # wget='wget -q -O - '
    # cycle_file='/tmp/cycle'    



    #   qs="hwid=${hwid}&cycle=${cycle}&t=${val}"
    #   hk = sha1sum(qs+psw)
    #   qs="${qs}&_hkey=${hk}"

    print("send")
#--


def update_data(data, k, v):
    v = float(v)
    d = data.get(k)
    if d:
        if v < d['min']: d['min'] = v
        if v > d['max']: d['max'] = v
        d['sum'] += v
        d['count'] += 1
    else:
        data[k] = {'min':v, 'max':v, 'sum':v, 'count':1}
    #
#--


# constants

hwid  = os.environ.get("HWID") or get_hwid()
psw   = os.environ.get("PSW")  
send_interval = 240

RH_MAX = 20

# global data

cycle = 0
data = {}

# ct = Thread(target=sender, args=(data,))
# ct.start()

while True:
    try:
        s = sys.stdin.readline()
        if s[0] == '*':
            sn = '0'
            pwr = ''
            rf = ''
            for i in s[1:].split():
                [k,v] = i.split('=')
                if k == 'sn':
                    sn = v
                elif k == 'rf':
                    if v != '0': rf = v
                elif k == 'pwr':
                    if v != '0': pwr = v
                elif k == 'r':
                    rhs = data.get('rhs')
                    if rhs:
                        if len(rhs) >= RH_MAX: rhs.pop(0)
                        rhs.append(int(v))
                    else:
                        data['rhs'] = [int(v),]
                    #
                elif k in 'pw': 
                    update_data(data, k, v)
                elif k in 'thd':
                    if sn == '0':
                        update_data(data, k+'0', v)
                    else:
                        update_data(data, k, v)
                #
            #
            if pwr: data['pwr'+sn] = pwr
            if rf:  data['rf']  = rf

            print(data)
        #
    except Exception as e:
        perr("error: "+str(e))
#-

#.
