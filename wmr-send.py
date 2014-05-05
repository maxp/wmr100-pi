#!/usr/bin/env python

#   read wmr100-pi data from stdin and send it to rs.angara.net

#   on.environ {HWID,PSW}


from __future__ import print_function

import hashlib
import httplib
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


def calc_b(rhumbs):
    if not r:
        return None

    # find maximum
    m = 1
    p = -1
    for i,r in enumerate(rhumbs):
        if r >= m:
            m = r
            p = i
    #-
    if p < 0:
        return None

    m  = float(m)
    m0 = float(r[p-1])
    m1 = float(r[(p+1) % len(r)])

    return int((m-(m0/m*0.5)+(m1/m*0.5))*22.5)
#-

def make_avg(d):
    if not d:
        return None
    c = d.get('count')
    if not c:
        return None
    v = d.get('sum')
    if v is None:
        return None
    s = "{:.1f}".format(float(v) / c)
    if s.endswith(".0"): return s[0:-2]
    else: return s
#--    

def sender(collected_data):
    global cycle
    while True:
        cycle += 1
        if collected_data:
            d = collected_data.copy()
            collected_data.clear()

            qs = "hwid="+hwid+"&cycle="+str(cycle)

            x = make_avg(d.get('t1'))
            if x is not None: qs += "&t="+x

            x = make_avg(d.get('h1'))
            if x is not None: qs += "&h="+x

            x = make_avg(d.get('d1'))
            if x is not None: qs += "&d="+x

            x = make_avg(d.get('t0'))
            if x is not None: qs += "&t0="+x

            x = make_avg(d.get('h0'))
            if x is not None: qs += "&h0="+x

            x = make_avg(d.get('d0'))
            if x is not None: qs += "&d0="+x

            x = make_avg(d.get('p'))
            if x is not None: qs += "&p="+x

            x = make_avg(d.get('w'))
            if x is not None: qs += "&w="+x

            x = d.get('w')
            if x is not None:
                g = x.get('max')
                if g is not None: 
                    qs += "&g="+"{:.1f}".format(float(g))
            #

            x = calc_b(d.get("r"))
            if x is not None: qs += "&b="+str(x+b_fix)

            x = d.get("rf")
            if x: qs += "&rf="+x

            x = d.get("pwr")
            if x: qs += "&pwr="+x


            sha1 = hashlib.sha1()
            sha1.update(qs+psw)
            qs += "&_hkey="+sha1.hexdigest()

            print()
            print("*** qs:", qs)
            print()


            conn = httplib.HTTPConnection(host, port, timeout=timeout)
            conn.request("GET", uri+qs)
            resp = conn.getresponse()

            if resp and resp.status == 200:
                print("sent_ok")
            else:
                perr("resp:"+str(resp))

            # update cycle file if sent_ok
            # cycle_file='/tmp/cycle'    
        #
        time.sleep(SEND_INTERVAL)    
    #
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

host    = "rs.angara.net"
port    = 80
uri     = "/dat?"
timeout = 30

hwid    = os.environ.get("HWID") or get_hwid()
psw     = os.environ.get("PSW")  or ""
b_fix   = 30    # dergrees

SEND_INTERVAL = 100
RH_NUM = 16

# global data

cycle = 0
data = {}

ct = Thread(target=sender, args=(data,))
ct.start()

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
                    rhc = data.get('rhc')
                    if not rhc:
                        rhc = [0 for j in range(RH_NUM)]
                        data['rhc'] = rhc
                    #-
                    rhc[int(v)] += 1
                elif k in 'pw':  update_data(data, k, v)
                elif k in 'thd': update_data(data, k+sn, v)
                #
            #
            if pwr: data['pwr'+sn] = pwr
            if rf:  data['rf']  = rf

            print(data)
        #
    except KeyboardInterrupt:
        perr("SIGTERM")
        import os
        os._exit(0)
    except Exception as e:
        perr("error: "+str(e))
#-

#.
