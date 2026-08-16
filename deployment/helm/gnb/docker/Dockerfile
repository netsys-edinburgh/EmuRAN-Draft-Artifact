FROM docker.io/ujjwal1997/phobos_gnb:latest

RUN apt update
RUN apt install -y iproute2 iputils-ping nano

COPY inaddr_any.patch /inaddr_any.patch

WORKDIR /openairinterface-nfapi-netsys

# Apply the patch
RUN git apply /inaddr_any.patch

# Build the gNB with the patch
RUN bash -c "source oaienv && cd cmake_targets && ./build_oai --gNB"

# Install bc
RUN apt install -y bc