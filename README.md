# mfrc522cli
A generic serial cli interface for writing mifare classic cards usind an ESP8266 and a mfrc522 module
![](mfrc522_cli_bb.png)
## This is a cli serial interface for mfrc522
  It allows to copy, modify or clone cards using a serial interface commands.
  It also allows to write block zero 
   
Commands: read, lka, show, lb, clean
  
  ### command: lka (sector) (key)
description: load key A for a sector. This key will be used in read or write card commands.
parameter: sector number
parameter: key
example:
```
    lka 3 a0a1a2a3a4a5 
```
load key a0a1a2a3a4a5 into internal buffer for sector 3

### command: lb (block number) (data)
description: loads to internal buffer the data for a specified block number 
parameter: block number
parameter: 16 bytes hex data in ascci mode 
example: 
```
    lb 4 fffebbccaabbcc112233445566aa1122
```

  ### command: read (card/uid)
description: read to the buffer a mifare classic card using keys. default FFFFFFFFFFF
params: card / uid
example:
```
  read card 
```
  reads a entire card and store data to write buffer except trailer sectors blocks

 ### command: write (zero) (trailer)
writes data in the buffer to the card. To see what data is going to be written "show write"
NOTE: If you want to clone a entire card, block zero included, you need to do this in two spteps.
first copy block zero: load block and key for block zero and then write it.

```
lka 0 ffffffffffff
lb 0 CAD9BF802C0804006263646566676869
write zero
```
then read card and write
```
read card
write
```
This won't copy trailer blocks, if you want to copy trailer blocks load them and write trailer:


```
lka 0 ffffffffffff
lb 3 ffffffffffffFF078069FFFFFFFFFFFF
lb 7 ffffffffffffFF078069FFFFFFFFFFFF
lb 11 ffffffffffffFF078069FFFFFFFFFFFF
lb 15 ffffffffffffFF078069FFFFFFFFFFFF
[...]
write trailer
```

IMPORTANT: When you write a trailer block you are writing a new Key A and Key B.



### command: show [write] [keys] [uid]
description: show data
parameter: write - show blocks to write (all blocks if read command or several if lb command)
parameter: keys  - show loaded keys for read or write commands
parameter: uid   - show uid card (needed to aproach it to the reader)
example:
```
    show write
    show uid
    show keys
```
### command: clear (data) (keys) [all]
description: clear stored buffer data 
example:
```
    clear data  -  clear readed or loaded data stored in buffer
    clear keys  -  clear all loaded keys to FFFFFFFFFFFF
```
Command: fix [card] (stop)
description: This command allows to fix a unresponsive magic card.
This not work with common cards only with magic cards.
Usage:
```
  fix start
```
```
  fix stop
```
 
