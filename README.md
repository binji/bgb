# binjgb

A simple GB emulator.

## Building

Requires [CMake](https://cmake.org) and [SDL 1.2](https://www.libsdl.org/download-1.2.php).
If you run `make`, it will run CMake for you and put the output in the `out/`
directory.

## Running

```
$ out/binjgb <filename>
```

Keys:

| Action | Key |
| --- | --- |
| DPAD-UP | up |
| DPAD-DOWN | down |
| DPAD-LEFT | left |
| DPAD-RIGHT | right |
| B | z |
| A | x |
| START | enter |
| SELECT | backspace |
| Quit | escape |
| Disable audio channel 1-4 | 1-4 |
| Disable BG layer | b |
| Disable Window layer | w |
| Disable OBJ (sprites) | o |

## Test status

[Blargg's tests](http://gbdev.gg8.se/wiki/articles/Test_ROMs):

| Test | Result |
| --- | --- |
| cpu\_instrs | :ok: |
| dmg\_sound-2 | :ok: |
| instr\_timing | :ok: |
| mem\_timing-2 | :ok: |
| oam\_bug-2 | :x: |

[Gekkio's mooneye-gb tests](https://github.com/Gekkio/mooneye-gb):

| Test | Result |
| --- | --- |
| acceptance/add\_sp\_e\_timing | :ok: |
| acceptance/boot\_hwio-G | :ok: |
| acceptance/boot\_regs-dmg.gb | :ok: |
| acceptance/call\_cc\_timing2 | :ok: |
| acceptance/call\_cc\_timing | :ok: |
| acceptance/call\_timing2 | :ok: |
| acceptance/call\_timing | :ok: |
| acceptance/di\_timing-GS | :ok: |
| acceptance/div\_timing | :ok: |
| acceptance/ei\_timing | :ok: |
| acceptance/halt\_ime0\_ei | :ok: |
| acceptance/halt\_ime0\_nointr\_timing | :ok: |
| acceptance/halt\_ime1\_timing2-GS | :ok: |
| acceptance/halt\_ime1\_timing | :ok: |
| acceptance/if\_ie\_registers | :ok: |
| acceptance/intr\_timing | :ok: |
| acceptance/jp\_cc\_timing | :ok: |
| acceptance/jp\_timing | :ok: |
| acceptance/ld\_hl\_sp\_e\_timing | :ok: |
| acceptance/oam\_dma\_restart | :ok: |
| acceptance/oam\_dma\_start | :ok: |
| acceptance/oam\_dma\_timing | :ok: |
| acceptance/pop\_timing | :ok: |
| acceptance/push\_timing | :ok: |
| acceptance/rapid\_di\_ei | :ok: |
| acceptance/ret\_cc\_timing | :ok: |
| acceptance/reti\_intr\_timing | :ok: |
| acceptance/reti\_timing | :ok: |
| acceptance/ret\_timing | :ok: |
| acceptance/rst\_timing | :ok: |
| acceptance/bits/mem\_oam | :ok: |
| acceptance/bits/reg\_f | :ok: |
| acceptance/bits/unused\_hwio-GS | :ok: |
| acceptance/gpu/hblank\_ly\_scx\_timing-GS | :x: |
| acceptance/gpu/intr\_1\_2\_timing-GS | :ok: |
| acceptance/gpu/intr\_2\_0\_timing | :ok: |
| acceptance/gpu/intr\_2\_mode0\_timing | :ok: |
| acceptance/gpu/intr\_2\_mode0\_timing\_sprites | :x: |
| acceptance/gpu/intr\_2\_mode3\_timing | :ok: |
| acceptance/gpu/intr\_2\_oam\_ok\_timing | :ok: |
| acceptance/gpu/stat\_irq\_blocking | :ok: |
| acceptance/gpu/vblank\_stat\_intr-GS | :ok: |
| acceptance/timer/div\_write | :ok: |
| acceptance/timer/rapid\_toggle | :ok: |
| acceptance/timer/tim00\_div\_trigger | :ok: |
| acceptance/timer/tim00 | :ok: |
| acceptance/timer/tim01\_div\_trigger | :ok: |
| acceptance/timer/tim01 | :ok: |
| acceptance/timer/tim10\_div\_trigger | :ok: |
| acceptance/timer/tim10 | :ok: |
| acceptance/timer/tim11\_div\_trigger | :ok: |
| acceptance/timer/tim11 | :ok: |
| acceptance/timer/tima\_reload | :ok: |
| acceptance/timer/tima\_write\_reloading | :x: |
| acceptance/timer/tma\_write\_reloading | :x: |
| emulator-only/mbc1\_rom\_4banks | :ok: |
| manual-only/sprite\_priority | :ok: |