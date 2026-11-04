# Context: source/ is the recorded checkout plus the two pinned Meson vendors.
# The toolchain image is exported with the local validation evidence; its ID is
# checked by build.sh. Compilation happens here, never on a Kubernetes node.
ARG TOOLCHAIN_IMAGE=open5gs-pfcp-codec-build:20261007
ARG RUNTIME_IMAGE=ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852
FROM ${TOOLCHAIN_IMAGE} AS builder
COPY source/ /src/
WORKDIR /src
RUN meson setup /build --prefix=/opt/open5gs --buildtype=debugoptimized \
        -Dwerror=false --wrap-mode=nodownload \
    && ninja -C /build -j4 \
    && meson test -C /build core crypt unit smf-selection --print-errorlogs \
    && DESTDIR=/runtime meson install -C /build --no-rebuild

FROM ${RUNTIME_IMAGE} AS runtime
ARG REVISION
ARG CODE_SHA256
LABEL org.opencontainers.image.title="Open5GS dynamic DNN - Helm runtime" \
      org.opencontainers.image.source="https://github.com/BMarujo/open5gs_core" \
      org.opencontainers.image.revision="${REVISION}" \
      org.opencontainers.image.version="2.8.0" \
      org.open5gs.source.code-sha256="${CODE_SHA256}" \
      org.open5gs.protocol.profile="TS 29.244 V19.5.0 (scoped MVP)"
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates bash tini iproute2 iptables procps iputils-ping \
        tcpdump tshark curl libtalloc2 libyaml-0-2 libsctp1 libc-ares2 \
        libgnutls30t64 libgcrypt20 libssl3t64 libmongoc-1.0-0t64 libbson-1.0-0t64 \
        libnghttp2-14 libmicrohttpd12t64 libcurl4t64 libcurl3t64-gnutls libidn12 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /runtime/opt/open5gs/ /opt/open5gs/
COPY entrypoint.sh /usr/local/bin/open5gs-entrypoint
ENV PATH=/opt/open5gs/bin:${PATH} \
    LD_LIBRARY_PATH=/opt/open5gs/lib/x86_64-linux-gnu:/opt/open5gs/lib
WORKDIR /opt/open5gs
RUN groupadd --system --gid 999 open5gs \
    && useradd --system --uid 999 --gid 999 --home-dir /opt/open5gs open5gs \
    && chown -R 999:999 /opt/open5gs \
    && chmod 0755 /usr/local/bin/open5gs-entrypoint \
    && for nf in amf smf upf nrf scp ausf udm udr pcf nssf bsf; do \
         ldd /opt/open5gs/bin/open5gs-${nf}d > /tmp/ldd-${nf}; \
         if grep -q 'not found' /tmp/ldd-${nf}; then cat /tmp/ldd-${nf}; exit 1; fi; \
       done
USER 999:999
# The Gradiant chart overrides USER for the UPF and creates its TUN/NAT in an
# init container. Do not create another TUN or inject startup sleeps here.
ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/open5gs-entrypoint"]
CMD ["/bin/bash"]
