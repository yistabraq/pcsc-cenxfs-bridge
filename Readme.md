PC/SC-CEN/XFS-bridge
====================
This project implements a bridge between [PC/SC][1] protocol (protocol for smart card communication)
and [CEN/XFS][2] protocol, used by ATM software to access devices, including chip card readers.

License
-------
MIT License. In short, free use is allowed in both commercial and open source projects. The license text
is available in both Russian and English in the LICENSE.md file. Only the English version is legally binding.

Dependencies
-----------
1. Boost (environment variable `%BOOST_ROOT%` must be set, pointing to the root directory of boost,
   i.e., the directory containing folders libs, doc, stage, etc. It is assumed that boost is built in
   the default directory, which is stage)
    1. `boost.chrono`
    2. `boost.thread`
    3. `boost.date_time` (dependency of `boost.thread`)
2. [XFS SDK][1]
    1. Can be obtained from the [freexfs][4] project. Documentation is also available there.
    2. Can also be obtained from the official FTP site, but it's quite difficult to find. Links to
       documentation could not be found on the official CEN/XFS group website, but fortunately, user
       **winner13** on the [bankomatchik.ru][6] forum somehow found the [FTP link][7].
3. PC/SC subsystem is part of the Windows SDK.

Build
-----
Prepare dependencies: download boost and build the required libraries. Build can be performed as follows:

### Method 1: Using CMake (Recommended)

Prerequisites:
1. CMake 3.10 or higher
2. Visual Studio 2019 or higher
3. Boost (see above)
4. XFS SDK (see above)

Required environment variables:
- `BOOST_ROOT`: Path to Boost installation
- `XFS_SDK`: Path to XFS SDK

To compile:
```batch
build.bat
```

The script will:
1. Check prerequisites
2. Create a `build` directory
3. Configure the project with CMake
4. Build the project

The resulting DLL will be in `build/Release/PCSCspi.dll`

### Method 2: Old method (make.bat)

Launch Visual Studio command prompt and run:
```
make
```
Or open a regular command prompt and run the commands, first replacing `%VC_INSTALL_DIR%`
with the path to installed MSVC. Since Kalignite is built for x86, we will build the library for
this architecture. Naturally, when a 64-bit version appears, it will be necessary to use `x86_amd64`:
```
"%VC_INSTALL_DIR%\VC\vcvarsall.bat" x86
make
```

Uses C++03. Build has been tested with the following compilers:

1. MSVC 2005
2. MSVC 2008
3. MSVC 2013

Architecture
-----------
When the dynamic library is loaded, a global `Manager` object is created, in whose constructor
the PC/SC subsystem is initialized and a thread is started to monitor device changes and new device
connections. In the global object's destructor, the connection to the PC/SC subsystem is closed, this
happens automatically when the XFS manager unloads the library.

Although it might seem that we could directly map the service provider handle (`HSERVICE`) to the PC/SC
context handle (`SCARDCONTEXT`), this is not done because the `SCardListReaders` function is blocking,
and it requires a context handle. Thus, if each service provider had its own PC/SC context, we would
need to create a separate thread for each one to monitor device changes.

The `Manager` class contains a list of card reading tasks, which are created when the `WFPExecute` method
is called, and a list of services representing XFS manager opened services (via `WFPOpen`).

When a PC/SC event occurs, a separate thread first notifies all subscribed windows (via
`WFPRegister`) about changes (naturally, the event is translated from PC/SC to XFS format), and
then notifies all tasks about all changes that have occurred. This implements the XFS requirement
that all events must be emitted before the `WFS_xxx_COMPLETE` event occurs.

If a task considers the change interesting, it generates a `WFS_xxx_COMPLETE` event and its `match`
method returns `true`, as a result of which it is removed from the task list.

Completion events are sent using the Windows PostMessage function, which places them in the message queue
of the thread that handles completion events. It is the responsibility of the end application to provide
window handles that exist in a single thread, otherwise the `WFS_xxx_COMPLETE` event may arrive before
other types of events. Moreover, if the application processes events in a different thread than the
asynchronous service provider calls, then events may arrive before the asynchronous call that initiated
them completes. It is the application's responsibility to work correctly in such cases and not lose
notifications. This is a feature of the XFS API, which imposes very strict requirements on the application.

Settings
--------
Most settings are intended to work around issues discovered during testing, but some
control the standard functionality of the service provider. All settings are made in the registry under
the logical service provider branch (`HKEY_LOCAL_MACHINE\SOFTWARE\XFS\SERVICE_PROVIDERS\<provider>`).
All `DWORD` values in the table are logical flags, with value `0` -- cleared, any other value -- set:

Name           |Type     |Purpose
---------------|---------|--------
ReaderName     |`REG_SZ` |PC/SC reader name that this provider should work with. If the parameter is empty or missing, all connected readers are monitored and the first one into which a card is inserted is used (this is done each time, i.e., if the card is removed from the first reader and inserted into the second, work will proceed with the second reader). If not empty, then card insertion events will only be processed from the specified reader
Exclusive      |`DWORD`  |If the flag is set, the reader will use the card in exclusive mode (`SCARD_SHARE_EXCLUSIVE`), i.e., no one except the service provider will be able to communicate with the card simultaneously. If cleared or missing, the card is opened in shared mode (`SCARD_SHARE_SHARED`)
**Workarounds**|         |Subsection -- bug workarounds
CorrectChipIO  |`DWORD`  |Analyze the length of commands sent to the chip and correct it according to what is transmitted in the command header. Kalignite may transmit extra bytes in the read command, which causes an error in the `SCardTransmit` function. If cleared or missing, no analysis is performed
CanEject       |`DWORD`  |Report that the device can eject cards in device capabilities and accept the card ejection command (`WFS_CMD_IDC_EJECT_CARD`). Nothing is actually done. If cleared or missing, report in capabilities that the command is not supported, and when receiving this command return error **unsupported command** (`WFS_ERR_UNSUPP_COMMAND`). Kalignite tries to eject the card even if this capability is not supported, and does not expect the command to fail, crashing with Fatal Error if the response code is not success
**Track2**     |         |Subsection of **Workarounds** -- track 2 settings
_(default)_    |`REG_SZ` |Value of track 2 reported by the provider, without start and end separators, as will be given to the application. The value is only reported if the `Report` flag is set
Report         |`DWORD`  |Report the ability to read magnetic track 2. If the flag is set and the track value is empty, reading returns error code **data missing** (`WFS_IDC_DATAMISSING`). If the flag is cleared, device capabilities report that reading track 2 is not supported. Kalignite requires track 2 to be read even if reading conditions specify not to read track 2 (at the time of reading everything is fine, but later when running the scenario it crashes with Fatal Error due to missing track 2)

Tested Readers
-------------
All bug workarounds were activated for working with Kalignite.

1. [OMNIKEY 3121][5] (pcsc_scan knows it as OMNIKEY AG Smart Card Reader USB 0) -- works successfully
2. [ACR38u][8] -- works successfully

[1]: http://www.pcscworkgroup.com/
[2]: http://www.cen.eu/work/areas/ict/ebusiness/pages/ws-xfs.aspx
[3]: https://code.google.com/p/freexfs/downloads/detail?name=XFS%20SDK3.0.rar&can=2&q=
[4]: https://code.google.com/p/freexfs/
[5]: http://www.hidglobal.com/products/readers/omnikey/3121
[6]: http://bankomatchik.ru/forums/topic/4654#p65827
[7]: ftp://ftp.cenorm.be/PUBLIC/CWAs/other/WS-XFS/SDK%20XFS3/sdk303.zip
[8]: http://www.acr38u.com/