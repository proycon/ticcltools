FROM alpine:latest
#VERSION can be:
# - stable: builds latest stable versions from source (default)
# - distro: uses packages as provided by Alpine Linux (may be slightly out of date)
# - devel: latest development version (git master/main branch)
ARG VERSION="stable"
LABEL org.opencontainers.image.authors="Maarten van Gompel <proycon@anaproy.nl>"
LABEL description="Ticcltools"

RUN mkdir -p /usr/src/ticcltools /data
COPY . /usr/src/ticcltools
COPY entrypoint.sh /usr/bin/entrypoint.sh

RUN PACKAGES="icu-libs libxml2 libstdc++ libbz2 libtar" &&\
    BUILD_PACKAGES="build-base autoconf-archive autoconf automake libtool icu-dev bzip2-dev libtar-dev libxml2-dev git" &&\
    apk add $PACKAGES $BUILD_PACKAGES &&\ 
    cd /usr/src/ && ./ticcltools/build-deps.sh &&\
    cd ticcltools && sh ./bootstrap.sh && ./configure && make && make install &&\
    apk del $BUILD_PACKAGES && rm -Rf /usr/src

WORKDIR /data
VOLUME /data

ENTRYPOINT [ "/usr/bin/entrypoint.sh" ]
