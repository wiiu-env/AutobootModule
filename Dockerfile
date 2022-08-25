FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libiosuhax:20220523 /artifacts $DEVKITPRO

WORKDIR project