sensgw lightning
======

wlan-slovenija sensor gateway - Arduino compatible lightning detector code

See http://dev.wlan-si.net/wiki/Telemetry/sensgw/lightning for more info.

UART communication:


GET functions:

    ACOM /0 - test
    ACOM /1 - time in 100ms (beware wraps around at 65536 or once about every 109 minutes, read data more often then that)
    ACOM /2 - detection results list, entries separated by comma, each entry <time> <type> <distance> <power>
    ACOM /3 - calibration offset (range 0-10 is ok)
    ACOM /4 - noise level (under 3 is ok)
    ACOM /5 - reset system 

SET functions:

    ACOM /0 x - test set
    ACOM /1 x - led on/off
    ACOM /2 x - set gain 0-15(default)
    ACOM /3 x - set disturber 1 enable(default), 0 disable 