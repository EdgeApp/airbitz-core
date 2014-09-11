DIR=~/tmp/ABC2
L=william1
P=william1
QAS="q1\nq2\nq3\n"
RAS="a1\na2\nqa3\n"

#./create-account $DIR $L $P
#./sign-in $DIR $L $P
./set-question-choices ${DIR} ${L} ${P} ${QAS} ${RAS}

# gdb ./build/set-question-choices.bin
# run ~/tmp/ABC2 william1 william1 "q1\nq2\nq3\n" "a1\na2\nqa3\n"
