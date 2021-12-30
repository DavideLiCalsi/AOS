echo -ne "p1" > /dev/newtopic
echo -ne "p2" > /dev/newtopic
echo -ne "p3" > /dev/newtopic
echo -ne "p4" > /dev/newtopic
echo -ne "p5" > /dev/newtopic
echo -ne "p6" > /dev/newtopic
echo -ne "p7" > /dev/newtopic
echo -ne "p8" > /dev/newtopic

echo a > /dev/topics/p1/subscribe
echo a > /dev/topics/p2/subscribe
echo a > /dev/topics/p3/subscribe
echo a > /dev/topics/p4/subscribe
echo a > /dev/topics/p5/subscribe
echo a > /dev/topics/p6/subscribe
echo a > /dev/topics/p7/subscribe
echo a > /dev/topics/p8/subscribe

python3 -c 'print("a")' > /dev/topics/p1/subscribe
python3 -c 'print("a")' > /dev/topics/p2/subscribe
python3 -c 'print("a")' > /dev/topics/p3/subscribe
python3 -c 'print("a")' > /dev/topics/p4/subscribe
python3 -c 'print("a")' > /dev/topics/p5/subscribe
python3 -c 'print("a")' > /dev/topics/p6/subscribe
python3 -c 'print("a")' > /dev/topics/p7/subscribe
python3 -c 'print("a")' > /dev/topics/p8/subscribe

cat /dev/topics/p1/subscribers
cat /dev/topics/p2/subscribers
cat /dev/topics/p3/subscribers
cat /dev/topics/p4/subscribers
cat /dev/topics/p5/subscribers
cat /dev/topics/p6/subscribers
cat /dev/topics/p7/subscribers
cat /dev/topics/p8/subscribers

echo -ne "\x11" > /dev/topics/p1/signal_nr
cat /dev/topics/p1/signal_nr
