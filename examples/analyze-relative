#!/bin/sh
# This script takes in the output of relative_times report and provides
# you with summary information.
# ./flood examples/round-robin.xml > report.out
# ./examples/analyze-relative report.out
# This script requires gawk.
if [ ! -f $1 ]; then
    exit -1;
fi
echo "Slowest pages on average (worst 5):"
echo "   Average times (sec)"
echo "connect\twrite\tread\tclose\thits\tURL"
tail +1 $1 | gawk '
/OK.*https?/ {
    split($8, urlarray, /\?/); url = urlarray[1]; ht[url] = url;
    co[url] += ($2/1000000); wr[url] += ($3/1000000); re[url] += ($4/1000000);
    cl[url] += ($5/1000000); cou[url]++; }
END {
    for (i in ht) {
        printf("%.4f\t%.4f\t%.4f\t%.4f\t%d\t%s\n", co[i]/cou[i], wr[i]/cou[i], re[i]/cou[i], cl[i]/cou[i], cou[i], i)
    }
}' - | sort -rn +3 | head -5
#echo "Most frequently hit pages (top 5):"
#tail +1 $1 | gawk '/OK.*https?/ {
#    split($8, urlarray, /\?/); url = urlarray[1]; ht[url] = url;
#    co[url] += ($2/1000000); wr[url] += ($3/1000000); re[url] += ($4/1000000);
#    cl[url] += ($5/1000000); cou[url]++; }
#END {
#    for (i in ht) {
#        printf("%.4f\t%.4f\t%.4f\t%.4f\t%d\t%s\n", co[i]/cou[i], wr[i]/cou[i], re[i]/cou[i], cl[i]/cou[i], cou[i], i)
#    }
#}' - | sort -rn +4 | head -5
# This gives a summary report.
grep OK $1 | gawk '{ a[$7] += ($5) / 1000000.00; b[$7]++ } END { for (i in a) if (a[i] != 0) { c[0] += b[i]; c[1] += a[i]; c[2] += b[i]/a[i]; c[3]++; }; printf "Requests: %d Time: %.2f Req/Sec: %.2f\n", c[0], c[1]/c[3], c[2] }' - 
