# The context is produced by package-runtime.py from tested ELF binaries.
# No mutable base, downloads, package resolution or rebuild in this step.
FROM scratch AS runtime
ARG REVISION
ARG CODE_SHA256
ARG BUILD_IMAGE_ID
LABEL org.opencontainers.image.title="Open5GS dynamic DNN laboratory" \
      org.opencontainers.image.version="2.8.0" \
      org.opencontainers.image.revision="${REVISION}" \
      org.open5gs.source.code-sha256="${CODE_SHA256}" \
      org.open5gs.source.dirty="true" \
      org.open5gs.toolchain.image="${BUILD_IMAGE_ID}" \
      org.open5gs.protocol.profile="TS 29.244 V19.5.0 (scoped MVP)"
COPY rootfs/ /
ENV LD_LIBRARY_PATH=/opt/open5gs/lib

FROM runtime AS smf
LABEL org.open5gs.role="smf"
ENTRYPOINT ["/opt/open5gs/bin/open5gs-smfd"]
CMD ["-c", "/etc/open5gs/smf.yaml"]

FROM runtime AS upf
LABEL org.open5gs.role="upf"
ENTRYPOINT ["/opt/open5gs/bin/open5gs-upfd"]
CMD ["-c", "/etc/open5gs/upf.yaml"]
