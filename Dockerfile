FROM debian:jessie
MAINTENER Vincent Giersch <vincent@flat.io>

RUN \
  apt-get update && \
  apt-get install -y build-essential libssl-dev llvm libncurses5-dev git && \
  rm -rf /var/lib/apt/lists/*

RUN mkdir /faust
WORKDIR /faust
COPY . /faust

RUN make && make install
ENTRYPOINT ["/usr/local/bin/faust"]
