FROM ghcr.io/wiiu-env/devkitppc:20260225

COPY --from=ghcr.io/wiiu-env/libmocha:20260126  /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20260207  /artifacts $DEVKITPRO

WORKDIR project
