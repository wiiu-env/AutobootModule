FROM wiiuenv/devkitppc:20211229

COPY --from=wiiuenv/libiosuhax:20211008 /artifacts $DEVKITPRO

WORKDIR project