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

#----#

host    = "_rs.angara.net"
port    = 80
uri     = "/dat?"
timeout = 30    # seconds

b_fix   = 30    # wind bearing correction value

log_file = "/tmp/wmr100.log"
log_size = 100*1000
cycle_file = "/tmp/wmr100.cycle"

SEND_INTERVAL = 90

#----#


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

hwid    = os.environ.get("HWID") or get_hwid()
psw     = os.environ.get("PSW")  or ""


def calc_b(rhc):
    if not rhc:
        return None

    print("rhc:", rhc)

    m = 1
    p = -1
    for i,r in enumerate(rhc):
        if r >= m:
            m = r
            p = i
    #-
    if p < 0:
        return None

    m  = float(m)
    m0 = float(rhc[p-1])
    m1 = float(rhc[(p+1) % len(rhc)])

    return int((p-(m0/m*0.5)+(m1/m*0.5))*22.5)
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
        try:
            if collected_data:
                d = collected_data.copy()
                collected_data.clear()

                logf = open(log_file, "a")

                print(time.strftime("%Y-%m-%d %H:%M:%S")," - ", cycle, file=logf)

                qs = "hwid="+hwid+"&cycle="+str(cycle)

                t = t0 = "???"
                x = make_avg(d.get('t1'))
                if x is not None: 
                    qs += "&t="+x
                    t = x
                x = make_avg(d.get('t0'))
                if x is not None: 
                    qs += "&t0="+x
                    t0 = x
                print("  t:", str(t), str(t0), file=logf)

                h = h0 = "???"
                x = make_avg(d.get('h1'))
                if x is not None: 
                    qs += "&h="+x
                    h = x
                x = make_avg(d.get('h0'))
                if x is not None: 
                    qs += "&h0="+x
                    h0 = x
                print("  h:", str(h), str(h0), file=logf)

                dp = dp0 = "???" 
                x = make_avg(d.get('d1'))
                if x is not None: 
                    qs += "&d="+x
                    dp = x
                x = make_avg(d.get('d0'))
                if x is not None: 
                    qs += "&d0="+x
                    dp0 = x
                print("  d:", str(dp), str(dp0), file=logf)

                p = "???"
                x = make_avg(d.get('p'))
                if x is not None: 
                    qs += "&p="+x
                    p = x
                print("  p:", p, file=logf)

                w = g = b = "???"
                x = make_avg(d.get('w'))
                if x is not None: 
                    qs += "&w="+x
                    w = x
                x = d.get('w')
                if x is not None:
                    g = x.get('max')
                    if g is not None: 
                        qs += "&g="+"{:.1f}".format(float(g))
                        g = "{:.1f}".format(float(g))
                #
                x = calc_b(d.get("rhc"))
                if x is not None: 
                    qs += "&b="+str(x+b_fix)
                    b = str(x+b_fix)
                print( "  w:", w, g, b, file=logf)
                print( "  rhc:", d.get("rhc","-"), file=logf)

                x = d.get("rf")
                if x: qs += "&rf="+x
                x = d.get("pwr")
                if x: qs += "&pwr="+x
                print( "  pwr/rf:", d.get("pwr","-"), d.get("rf","-"), file=logf)

                sha1 = hashlib.sha1()
                sha1.update(qs+psw)
                qs += "&_hkey="+sha1.hexdigest()

                conn = httplib.HTTPConnection(host, port, timeout=timeout)
                conn.request("GET", uri+qs)
                resp = conn.getresponse()

                if resp and resp.status == 200:
                    print("  http: ok", file=logf)
                    cf = open(cycle_file, "w")
                    print(cycle, file=cf)
                    cf.close()
                else:
                    perr("resp:"+str(resp))
                    print("  http:", str(resp), file=logf)

                logf.close()

                if os.stat(log_file).st_size > log_size:
                    os.rename(log_file, log_file+".old")
            #
        except Exception as ex:
            perr("error: "+str(ex))
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

# global data

RH_NUM = 16

cycle = 0
data = {}


ct = Thread(target=sender, args=(data,))
ct.start()

while True:
    try:
        s = sys.stdin.readline()
        print(s)
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

            # print(data)
        #
    except KeyboardInterrupt:
        perr("sigterm.")
        import os
        os._exit(0)
    except Exception as e:
        perr("error: "+str(e))
#-

#.
