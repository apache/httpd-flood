# Gawk script that takes the output of a relative_times report test
# that was generated with a startcount and startdelay flood scenario
# and breaks down the times to show how the performance during each
# startdelay period for which there is data.
#
# This should serve as an advanced example of how to process data from
# flood with awk.  And, probably as an example of how Justin codes
# at 12AM falling asleep on his keyboard.

BEGIN {
    currentperiod = 0
    # Should be passed in now
    #users = 5
    #delay = 180
    duration = delay * 1000000
    periods = 0
}

/OK.*https?/ {
    split($8, urlarray, /\?/)
    if (currentperiod == 0)
    {
        currentperiod = $1 + duration
    }
    else if (currentperiod < $1)
    {
        currentperiod += duration
        periods++
    }
    url = urlarray[1]
    ht[url, periods] = url
    co[url, periods] += ($2/1000000)
    wr[url, periods] += ($3/1000000)
    re[url, periods] += ($4/1000000)
    cl[url, periods] += ($5/1000000)
    # These two are used to refer back...
    pd[url, periods] = periods
    urls[url] = url
    cou[url, periods]++
}

END {
    for (p = 0; p < periods; p++) {
        printf("   Average times (sec) - Period %d (%d users)\n", p, (p+1)*users)
        printf("connect\twrite\tread\tclose\thits\tURL\n")
        for (u in urls) {
            if (cou[u, p] != 0) {
                printf("%.4f\t%.4f\t%.4f\t%.4f\t%d\t%s\n", co[u, p]/cou[u, p], wr[u,p]/cou[u,p], re[u,p]/cou[u,p], cl[u,p]/cou[u,p], cou[u,p], ht[u,p]) | "sort -rn +3 | head -5"
            }
        }
        close("sort -rn +3 | head -5")
    }
}
