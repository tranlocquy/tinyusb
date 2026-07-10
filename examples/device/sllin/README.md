# slLIN


[![License](https://img.shields.io/badge/license-MIT-brightgreen.svg)](https://opensource.org/licenses/MIT)

## What is this?

This is project slLIN. A LIN protocol inspired by Lawicel's serial line CAN protocol.

## How do I use it?

### Linux

Configure for master mode, 19200 baud.

```
sudo slcand -o -F -b 4b00 /dev/ttyACM1
```

Configure for slave mode, 19200 baud.

```
sudo slcand -l -F -b 4b00 /dev/ttyACM1
```

#### Configure Response

Configure response for ID 'b' using classic checksum. Note, the `-x` suppresses echoing of the CAN frame.

```
cangen slcan1 -x -e -L 4 -n 1 -D deadbeef -I 31000b
```

Configure response for ID '23' using enhanced checksum.

```
cangen slcan1 -x -e -L 8 -n 1 -D f00f00AA55AA55AA -I 320017
```


#### Frame Request

Send header for ID 'b'. Again, the `-x` suppresses the local echo of the RTR frame.

```
cangen slcan0  -x -R -I b -n 1
```

#### Master Transmission

To send a master request frame, first configure the frame data as you would for a response, then send the header.


#### AUTOSAR E2E

AUTOSAR E2E protection can be configured for each frame individually. Prior to slave transmission, counter and E2E CRC are updated in the payload and the CRC is recomputed.

E2E is disabled by default. Find the details on AUTOSAR E2E projection [here](https://www.autosar.org/fileadmin/standards/R20-11/FO/AUTOSAR_PRS_E2EProtocol.pdf).

_NOTE: the commands given below must be send over the serial line. See section on command side channel if you are using slcan._

##### Enable Profile 11

Enable profile 11 protection for LIN ID `0x0b`, E2E CRC offset (bits) 0, counter offset (bits) 8, data nibble offset (bits) 12 with mode 'both' and data ID `0xdead`:

```text
A E2E B P11 0 8 C BOTH DEAD\r
```

Same but for mode 'nibble':

```text
A E2E B P11 0 8 C NIBBLE DEAD\r
```

##### Enable Profile 2

Enable profile 2 protection for LIN ID `0x0b`, E2E CRC offset (bits) 0 and data IDs 0-15:

```text
A E2E B P22 0 0 1 2 3 4 5 6 7 8 9 A B C D E F\r
```


##### Disable AUTOSAR E2E

Disable AUTOSAR E2E for LIN ID `0x0b`. This is the default setting for each frame.

```text
A E2E B NONE\r
```

## Serial Line CAN (slcan) Protocol

The protocol for CAN is decribed [here](http://www.can232.com/docs/canusb_manual.pdf).

## Serial Line LIN (slLIN) Protocol

slLIN mostly follows Lawicel's protocol with a few modifications for LIN:

| Command&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;                                | Direction&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; |  Description                                   |
|----------------------------------------|----------------|------------------------------------------------|
| `L\r`                                  | host to device | Open device in slave mode |
| `O\r`                                  | host to device | Open device in master mode |
| `C\r`                                  | host to device | Close device |
| `TIIIIIIIILDD...\r`                       | both | See next section for CAN-ID decoding |
| `rIIIL\r`                              | host to device | Only valid in master mode. Send LIN header |
| `n\r`                                  | both           | Request device name                            |
| `Yn\r`</br>(where n is 0 or 1)         | host to device | Enable / disable periodic time stamp transmission.</br>If enabled, the device sends the current time stamp approx. once per second as `YTTTT\r`.</br>The format of the time stamp follows the Lawicel format, c.f. documentation of `Zn\r` command. |
| `shhhh\r`</br>(hex values)             | host to device | Set device baud rate, e.g  `4b00` sets 19200 Baud/s |
| `Sn\r`                                 | host to device | Set sleep timeout in seconds, i.e. `S0` sets 4 seconds, `S1` 5 seconds... |
| `v\r`                                  | both | Query firmware version |
| `A E2E ID NONE\r`                         | host to device | Disable AUTOSAR E2E for LIN ID `ID` (hex) |
| `A E2E ID P11 CRC COUNTER NIBBLE [NIBBLE\|BOTH] DATAID\r`                         | host to device | Enable AUTOSAR E2E profile 11 for LIN ID. Numbers in hex w/o prefix, offsets in bits. |
| `A E2E ID P2 CRC DATAID0 ... DATAID15 \r`                         | host to device | Enable AUTOSAR E2E profile 22 for LIN ID. Numbers in hex w/o prefix, offsets in bits. |
| `t000LDD...\r`                         | both | Command side channel over CAN. See own section for details. |


## Frame Coding

### LIN frames

| Start Bit | Length | Description |
|:----------|:-------|:------------|
| `0`       | `6`    | LIN-ID |
| `8`       | `8`    | CRC |

#### Device to Host Frames

A length of `0` implies an unanswered request.

| Bitmask      | Description |
|:-------------|:------------|
| `0x00010000` | bad sync field (not 0x55). This is typically the only flag |
| `0x00020000` | bad PID received, LIN-ID carries recovered ID |
| `0x00040000` | bit error in some fixed part of the frame e.g. start bit wasn't 0, stop bit wasn't 1, ... |
| `0x00080000` | more than 9 bytes on bus |
| `0x00100000` | request was not answered by this node |


#### Host to Device Frames

Length and data of the carrying CAN frame must be valid.

| Bitmask      | Description |
|:-------------|:------------|
| `0x00100000` | Enables frame response if set, else disables. |
| `0x00200000` | store LIN frame data. The CAN frames length and data must be valid |


| Start Bit | Length | Description |
|-----------|--------|-------------|
| `16`      | `2`    | CRC computation |

##### CRC Computation

| Value  | Description |
|:-------|:------------|
| `0x00` | take CRC field as is |
| `0x01` | ignore CRC field, compute classic CRC |
| `0x02` | ignore CRC field, compute enhanced CRC |


### Bus Error and State Frame

These are always from from device to host and do not carry any LIN frame data.

| Bitmask      | Description |
|:-------------|:------------|
| `0x10000000` | bus status bits are valid |
| `0x08000000` | bus error bits are valid |


| Start Bit | Length | Description |
|:----------|:-------|:------------|
| `0`       | `2`    | Bus State |
| `2`       | `2`    | Bus Error |

#### Bus State

| Value  | Description |
|:-------|:------------|
| `0x00` | bus is asleep |
| `0x01` | bus is awake |
| `0x02` | bus is in error state |

#### Bus Error

| Value  | Description |
|:-------|:------------|
| `0x00` | no error |
| `0x01` | bus shorted to GND |
| `0x02` | bus shorted to VBAT |

### Master Transmission

To put a LIN header on the bus, send  a remote request (RTR) CAN frame. Encode the LIN-ID as CAN-ID.
The protected identifier is computed by the device. The length part of the RTR frame is ignored.

To send only the break BREAK field only send a RTR frame with an ID of `0x100`.

To send a master request frame, store the target payload as response on the device, then send a header with the master request ID.

## Command Side Channel (slcan on Linux)

Because of the limitations imposed by Linux slcan, some commands may not be through the serial line interface. To access those command from Linux, use the CAN side channel (here `can0`). Note the device must already be registered as slcan CAN device and _up_.

### Examples

*AUTOSAR E2E*

```sh
# Enable AUTOSAR E2E profile 11 protection for LIN frame id `0x0b`
gen-can-side-channel-commands.py "A E2E b P11 0 8 C NIBBLE dead"
```

Output should be something like this:

```text
cangen -x -n 1 -I 0 -L 8 -D 4120453245206220 can0
cangen -x -n 1 -I 0 -L 8 -D 5031312030203820 can0
cangen -x -n 1 -I 0 -L 8 -D 43204e4942424c45 can0
cangen -x -n 1 -I 0 -L 5 -D 2064656164 can0
```

Then run those commands in the shell to activate the protection. The device will response with `\r` or `\b` over the CAN side channel:

```text
  can0  000   [1]  0D # side channel command suceeded
  or
  can0  000   [1]  07 # side channel command failed
```

*Firmware Version*

Generate

```sh
gen-can-side-channel-commands.py "v"
```

Send

```sh
cangen -x -n 1 -I 0 -L 1 -D 76 can0
```

Receive

```
# v0.6.0
can0  000   [7]  76 30 2E 36 2E 30 0D
```
