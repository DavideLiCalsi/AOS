echo -ne prova > /dev/newtopic

gcc subscriber_test.c -o subscriber
gcc publisher_test.c -o publisher

./subscriber &
./publisher
