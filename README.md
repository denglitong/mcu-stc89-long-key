# K51 Caculator

## How to build

```bash
/usr/local/bin/sdcc --model-large src/main.c && \
stcgal -P stc89 -p /dev/tty.wchusbserial14220 main.ihx
```