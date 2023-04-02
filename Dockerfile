FROM ghcr.io/wiiu-env/devkitppc:20230402

COPY --from=ghcr.io/wiiu-env/libmocha:20220919 /artifacts $DEVKITPRO

WORKDIR project