FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake jq linux-libc-dev ninja-build perl pkg-config unzip zip \
    && rm -rf /var/lib/apt/lists/*

ENV VCPKG_ROOT=/home/vscode/vcpkg
COPY vcpkg.json /tmp/slascone-vcpkg.json
RUN baseline="$(jq -er '."builtin-baseline"' /tmp/slascone-vcpkg.json)" \
    && git init "$VCPKG_ROOT" \
    && git -C "$VCPKG_ROOT" remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C "$VCPKG_ROOT" fetch --depth 1 origin "$baseline" \
    && git -C "$VCPKG_ROOT" checkout --detach FETCH_HEAD \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics \
    && chown -R vscode:vscode "$VCPKG_ROOT" \
    && rm /tmp/slascone-vcpkg.json
