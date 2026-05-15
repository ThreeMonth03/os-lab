FROM debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang-19 \
        clang-format-19 \
        clang-tidy-19 \
        cmake \
        curl \
        g++ \
        libgtest-dev \
        lld-19 \
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
