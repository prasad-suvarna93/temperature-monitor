# Temperature monitor

Firmware for a temperature monitor with three lamps. The sensor is read through an ADC every 100 us with low jitter. Configuration lives in an I2C EEPROM. Two board revisions carry sensors of different resolution.

There are two versions of the same design. One in C and one in C++. Both build and run on a PC with the hardware mocked.

```
G  normal    below 85 degC
Y  warning   85 degC and above
R  critical  105 degC and above or below 5 degC
```

## Build and run

You need make and gcc/g++.

```
cd c
make test
make demo

cd ../cpp
make test
make demo
```

## Layout

```
c/                       cpp/
  app/    application      app/    application
  hal/    interfaces       hal/    interfaces
  port/   host and board   port/   host and board
  test/                    test/
```

hal is four narrow interfaces. Exactly one port is linked into a binary. Everything above the HAL is the same code on the board and in the tests. The PC build only swaps the port.

## Design

The whole design comes from one requirement to sample every 100 us with very low jitter. So a timer triggers the ADC in hardware and DMA fills a ring. The CPU sees one interrupt per block of 64 samples and never sits in the timing path. However busy it gets it cannot move when a sample was taken.

Everything above that seam is plain portable code. One calibration table scales both board revisions. A median rejects a single stuck sample. The overlapping conditions and the noisy threshold are handled by assumptions that live in the config. A device that cannot measure blinks red and never shows green.
