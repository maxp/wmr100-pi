
### Completely refactored by [m@penzin.ru](http://github.com/maxp)

Verbose messages removed, some errors fixed.


Oregon Scientific WMR100 client for Linux/BSD/etc.
==================================================

Supports
--------

* Oregon Scientific WMR100
* Oregon Scientific WMR200
* Oregon Scientific RMS300A

Introduction
------------

The WMR100/200 use a proprietary protocol for their USB connection.

It's very useful for enthusiasts running a headless Linux box to collect and
analyze data from this link, but unfortunately the protocol isn't openly
documented, nor are clients provided for this platform.

This simple C program handles the USB protocol of the WMR100, and translates to
a JSON format, easy for parsing/analysing.

You can output to:
- stdout
- a file
- a zeromq socket

You'll need to setup the udev rules (see udev/README) if you want to run this
not as root. This is due to how libhid accesses the USB ports.

Requisites
----------

Packages:
- libhid-dev
- pkg-config
- libusb-dev
- libzmq-dev

Building
--------

Run 'make'.

To install, copy wmr100 to your path.

*One time install for osx*

To keep the default HIDManager from taking the wmr100, run this once:

    make setup_osx

If you want to use different software to read the wmr100 device, you should undo
this by running `make unsetup_osx` and then reboot so that the HIDManager will
take control of the device again.

Raspberry pi
------------

To install on raspberry pi use this script:

https://github.com/think-free/pi-scripts/raw/master/InstallWmr100OnPi.sh


    sudo apt-get install libusb-dev
    # libzmq-dev
    wget --no-check-certificate https://github.com/think-free/pi-packages/raw/master/libhid-0.2.16-rpi.tar.gz
    tar xvf libhid-0.2.16-rpi.tar.gz
    cd libhid-0.2.16
    # ./configure --prefix=/
    ./configure --prefix=/usr
    make
    sudo make install
    cd ..

    # git clone https://github.com/barnybug/wmr100.git
    cd wmr100
    make


    gcc wmr100.c -l hid

    apt-get install fping

    # /etc/crontab
    #
    # */20 *  * *  *  root  fping -q 8.8.8.8 || /sbin/reboot
    #
    #.

Usage
-----

Run:
    ./wmr100

This will dump data to stdout. Each data lines begins with '*'.
