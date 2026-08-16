FROM ubuntu:jammy

RUN apt update
RUN apt install -y build-essential libsctp-dev lksctp-tools zlib1g-dev
RUN apt install -y iproute2 iputils-ping nano

COPY phobos-5g /phobos-5g

WORKDIR /phobos-5g

# build the proxy
RUN make

# build the global slot checker
RUN gcc -O3 -o global_slotchecker src/timeout_handling_sc_global.c