# C++ Qt Wsm library

This repository contains a fully open-source C++ library for communicating
with a [Wireless SpeedoMeter](https://github.com/kmzbrnoI/wsm-pcb).

It connects to the (virtual) serial port, which is associated with Bluetooth
SPP profile.

## Requirements

This library uses [Qt](https://www.qt.io/)'s
[SerialPort](http://doc.qt.io/qt-5/qtserialport-index.html) which creates a
very good cross-platform abstraction of serial port interface. Thus, the
library uses Qt's mechanisms like slots and signals.

It is **not** usable without Qt.

There are no other requirements.

## Usage

You may use this library in two major ways:

 * Simply include `wsm.h` header file into your project and use instance of
   `Wsm` class.
 * Compile this project using `qmake` and use compiled object file.

## Basic information

 * See `wsm.h` for API specification.
 * To change the version of this library, update both constants at `wsm.pro`
   file and `wsm.h` file. This is needed for proper behavior as a standalone-lib
   and plain header too.

## Authors

This library was created by:

 * Jan Horacek ([jan.horacek@kmz-brno.cz](mailto:jan.horacek@kmz-brno.cz))

Do not hesitate to contact author in case of any troubles!

## License

This application is released under the [Apache License v2.0
](https://www.apache.org/licenses/LICENSE-2.0).
