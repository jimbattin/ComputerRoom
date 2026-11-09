@ Embeds the board's preset bank (presets.toml or presets_hothouse.toml, passed by the
@ Makefile as PRESETS_TOML_FILE; libdaisy assembles .s through the C preprocessor) into
@ the firmware image. .rodata is collected into the .text output section, which
@ STM32H750IB_sram.lds places in SRAM, so the blob ships inside the same .bin the
@ bootloader loads.
    .section .rodata
    .align 2
    .global presets_toml
presets_toml:
    .incbin PRESETS_TOML_FILE
    .byte 0
presets_toml_end:
    .align 2
    .global presets_toml_len
presets_toml_len:
    .word presets_toml_end - presets_toml
