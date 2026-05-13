FROM debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        clang-format \
        cmake \
        curl \
        g++ \
        lld \
        make \
        mtools \
        nasm \
        ninja-build \
        tar \
        xorriso \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
