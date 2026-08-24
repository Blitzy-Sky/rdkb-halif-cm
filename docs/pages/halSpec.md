# CM HAL Documentation

## Version History

This table records revisions of *this document*. It is not the version of the interface, of the repository, or of the generated documentation site; those are three further identities, kept apart immediately below.

| Date | Comment | Version |
| --- | --- | --- |
| 08/24/26 | First recorded revision of this document. Brought to the canonical `HAL` specification topic set: every declared `API` is named, the four version identities are separated, the asynchronous-notification and device-management claims are corrected against `include/cm_hal.h`, and the placeholder identifiers in the sequence diagram are replaced with declared ones. | 0.1.0 |

Four version identities exist around this interface, and a reader who conflates them will draw the wrong conclusion about how mature it is:

- **Document revision** \- the `Version` column above. No revision of this document was recorded before this one, so the table begins here rather than claiming a history it cannot show.
- **Release tag** \- `1.0.1`, the nearest ancestor tag of the revision this document describes. `CHANGELOG.md` records `1.0.1` without a date, carrying a header syntax fix, and `1.0.0` on 7 June 2024, which migrated the `CM HAL` header into this repository. A later tag `1.1.0` exists in the repository, but it is not an ancestor of this revision and is absent from `CHANGELOG.md`, so no later release is claimed here.
- **Interface version** \- **none is published.** `include/cm_hal.h` declares no version macro; its complete macro set is tabulated under `Data Structures and Defines` and holds type aliases, status codes and bounds only. A caller therefore cannot test which revision of the interface it compiled against, and `Variability Management` states what governs the interface's evolution instead.
- **Generated-site version string** \- `docs/generate_docs.sh` derives `PROJECT_VERSION` from `git describe --tags`, which yields a string of the form `<tag>-<commits-since-tag>-g<abbreviated-hash>`; at the revision this document describes that is `1.0.1-5-g2e9447f`. It is a build identifier, not a released version, and must not be read as one.

## Acronyms

The expansions below cover the terms this document uses. Interface terminology follows `include/cm_hal.h`.

- `ANSC` \- the legacy `RDK` type prefix carried by the `ANSC_IPV4_ADDRESS` macro; this interface does not expand it
- `API` \- Application Programming Interface
- `BPI` \- Baseline Privacy Interface, the `DOCSIS` link-layer security protocol
- `CM` \- Cable Modem
- `CM HAL` \- Cable Modem Hardware Abstraction Layer, the interface this document specifies
- `CMTS` \- Cable Modem Termination System, the head-end a modem registers with
- `CPE` \- Customer Premises Equipment
- `DHCP` \- Dynamic Host Configuration Protocol
- `DOCSIS` \- Data Over Cable Service Interface Specification
- `DS` \- Downstream, the direction from the network toward the modem
- `DSOFDM` \- Downstream Orthogonal Frequency Division Multiplexing
- `HAL` \- Hardware Abstraction Layer
- `HTTP` \- Hypertext Transfer Protocol
- `IP` \- Internet Protocol
- `IPv4` \- Internet Protocol version 4
- `IPv6` \- Internet Protocol version 6
- `LED` \- Light Emitting Diode, the indicator the download path flashes
- `LLD` \- Low Latency DOCSIS
- `MAC` \- Media Access Control
- `MDD` \- MAC Domain Descriptor
- `OFDM` \- Orthogonal Frequency Division Multiplexing
- `OFDMA` \- Orthogonal Frequency Division Multiple Access
- `PHY` \- Physical layer
- `QoS` \- Quality of Service
- `RDK-B` \- Reference Design Kit for Broadband
- `SLA` \- Service Level Agreement
- `SNMP` \- Simple Network Management Protocol
- `SNR` \- Signal-to-Noise Ratio
- `TFTP` \- Trivial File Transfer Protocol
- `ToD` \- Time of Day
- `URL` \- Uniform Resource Locator
- `US` \- Upstream, the direction from the modem toward the network
- `USOFDMA` \- Upstream Orthogonal Frequency Division Multiple Access
- `WAN` \- Wide Area Network

## Description

The `CM HAL` abstracts a `DOCSIS` cable modem - the `CM` in every identifier below - so that `RDK-B` can drive it without depending on the modem chipset or on which `DOCSIS` generation it implements. This repository holds the interface definition only: a vendor supplies the implementation behind it, and the caller is the `RDK-B` middleware. The workspace inventory names `CcspCMAgent` as the service that consumes this interface through `cm_hal.h`, so a tool or test that drives the `HAL` directly contends with that service for the modem; on the reference platforms the services to stop first are `checkbrcmwifisupport` and `CcspPandMSsp`.

The diagram below describes a high-level software architecture of the Broadband CM HAL module stack.

```mermaid

flowchart   
    stack["CcspCMAgent\n`RDKB Stack`"] --> contract["cm_hal.h\n`RDKB Contract`"];
    contract --> vendor_library["libcm_mgnt.so\n`Vendor-Delivery`"];
    vendor_library --> Vendor_Software;
    style stack fill:#0088ff;
    style contract fill:#0088ff;
    style vendor_library fill:#00ffee;
    style Vendor_Software fill:#00ffee;
```

Every diagram in this document is fenced Mermaid. It renders as a diagram on GitHub, which is the surface a developer reads through this repository's `README.md`; in the documentation site the generator produces, the same block appears as diagram source rather than as a picture.

**What this interface declares**, taken from the 51 function prototypes and the callback typedef in `include/cm_hal.h`:

- **Initialization.** `cm_hal_InitDB()` brings up the `HAL` and its dependencies; `docsis_InitDS()` and `docsis_InitUS()` prepare the downstream and upstream `PHY` layers and direct hardware access. **No de-initialization function is declared** - see `Object Lifecycles`.
- **Status and registration.** The modem's `DOCSIS` status as a formatted string, and the registration detail behind it: scanning, ranging, `TFTP` config-file download, data registration, `ToD` synchronisation, `BPI` state and network access.
- **Channel information and statistics.** Downstream and upstream channel parameters, active channel counts, error codeword counts, and the `DOCSIS 3.1` `DSOFDM` and `USOFDMA` channel and status tables.
- **Channel and frequency control.** The upstream channel identifier, the primary downstream start frequency and the `MDD` `IP` provisioning-mode override. With the `HTTP` download settings and the `MAC` re-initialisation threshold, these are the only values this interface lets a caller write.
- **Addressing information, read-only.** `DHCP` and `IPv6` `DHCP` information and the `CPE` list are retrieved, never set. **This interface declares no setter for an `IP` address, a subnet mask or a default gateway**; a caller that must change addressing does so outside this interface.
- **Firmware download and recovery.** `HTTP` download configuration, initiation and status polling, `LED` flashing during a download, reboot readiness, reboot, firmware update with factory reset, and `MAC`-layer re-initialisation with its threshold.
- **Diagnostics and identity.** The `DOCSIS` event log, certificate file path and status, four reset counters, market region, `SNMP` v3 kickstart initialisation and `DOCSIS` energy detection.
- **Asynchronous notification.** One callback, for diplexer variation - see `Asynchronous Notification Model`.

The interface is layered rather than monolithic: a caller may stay at the level of "is the modem online" through `docsis_getCMStatus()`, or descend to per-channel `DOCSIS 3.1` statistics, without changing how it links against the `HAL`. `API Surface` is the complete index of what is available at either level.

## Optional Components

The following components are optional and it is up to the vendor's discretion whether a platform provides them. Where a facility is absent the declaration that reaches it still exists, because it is part of the interface; what varies is whether the implementation behind it does anything. `include/cm_hal.h` is the only source for this topic - each entry below quotes what that header states, and nothing is inferred from a function's name.

- **Diplexer configuration and its notification.** `cm_hal_get_DiplexerSettings()` reads the current upstream and downstream diplexer band edges, and `cm_hal_Register_DiplexerVariationCallback()` installs the handler that follows their changes. The registration function's documented `RETURN_ERR` covers the case "if not supported/implemented, or in case of errors (e.g., stub function, misconfiguration)", so a vendor may legitimately ship it as a stub. A caller must therefore treat a failed registration as "this platform does not report diplexer variation" and continue, not as an error worth retrying.
- **Low Latency DOCSIS.** `docsis_LLDgetEnableStatus()` returns `DISABLE` both when `LLD` is disabled and when the entry is missing from the modem's bootfile, which the header states explicitly. Absence of the feature and absence of its provisioning are consequently not distinguishable through this interface.
- **`DOCSIS 3.1` channel tables.** `docsis_GetDsOfdmChanTable()`, `docsis_GetUsOfdmaChanTable()` and `docsis_GetStatusOfdmaUsTable()` describe `DOCSIS 3.1` `OFDM` and `OFDMA` channels; a modem operating on an earlier `DOCSIS` generation has none to report. The header defines no distinct return code for that case, so a caller reads the entry count these functions write and treats zero entries as "none present" rather than inferring a failure.

No other facility in this interface is marked optional by `include/cm_hal.h`. In particular the header does not establish whether `cm_hal_GetMarket()`, `cm_hal_snmpv3_kickstart_initialize()` or the four reset counters are implemented on every platform, so a caller must not read a declaration's presence in the header as a guarantee that the platform behind it supports the operation.

## Component Runtime Execution Requirements

This interface is delivered as a shared library the caller links against, and its lifetime is the lifetime of the calling process. The requirements in this block are the ones a caller can rely on and a vendor implementation must meet; each states the source it is taken from, which is either `include/cm_hal.h` or this specification's own statement of policy for the `CM HAL`.

### Initialization and Startup

During initialization and startup, the Broadband CM client module is required to invoke the following APIs in sequence:

- `cm_hal_InitDB()` - initializes the `HAL` and its dependencies. Its documented failure cases are the failure to create threads or to open files.
- `docsis_InitDS()` - prepares the global `PHY`-level data structures and direct hardware access for the downstream (`DS`) direction.
- `docsis_InitUS()` - the same for the upstream (`US`) direction.

`cm_hal_InitDB()` is expected to block if the hardware is not ready. It is the one exception to the non-blocking requirement stated under `Blocking calls`, and it is the reason a caller should perform initialization on a thread whose progress nothing else depends on.

Initialization is mandatory before any other operation: the `HAL` must be initialized before a caller reads status, configures channels or registers a notification handler. Nothing in `include/cm_hal.h` returns a distinct code for "not initialized", so a caller must not attempt to detect the condition from a return value - it must enforce the order itself.

### Threading Model

The interface is not required to be thread safe.

Vendors can implement internal threading and event mechanisms for operational purposes. These mechanisms must ensure thread safety when interacting with the provided interface. Additionally, they must guarantee cleanup of resources upon closure.

The consequence for a caller is concrete: two threads must not call into this interface concurrently unless the caller serialises them itself. **This interface does not specify which thread the diplexer variation callback is invoked on**, and neither the typedef at `cm_hal.h:1209` nor its registration function at `cm_hal.h:1228` states it, so a handler must serialise its own access to caller state rather than assume it runs on the registering thread.

### Process Model

This module is expected to be called from multiple process.

The requirement is to ensure that the module can handle concurrent calls effectively. The vendor needs to implement proper synchronization and scalability measures for robust performance.

`include/cm_hal.h` states no process affinity on any declaration, so the requirement above is this specification's rather than the header's, and it is left to the implementation how the synchronization is achieved.

### Memory Model

#### Caller Responsibilities

- Callers must assume full responsibility for managing any memory explicitly given to the module functions to populate. This includes proper allocation and de-allocation to prevent memory leaks.
- All strings used in this module must be zero-terminated. This ensures that string functions can accurately determine the length of the string and prevents buffer overflows when manipulating strings.
- Where the interface fixes a minimum buffer size, `include/cm_hal.h` states it on the declaration and the caller must honour it. The three the header names are the status buffer of `docsis_getCMStatus()`, at least 40 bytes; the `HTTP` `URL` and filename buffers of `cm_hal_Set_HTTP_Download_Url()` and `cm_hal_Get_HTTP_Download_Url()`, at least 200 bytes each, where the header warns that an insufficient buffer can lead to memory corruption; and the mode string of `cm_hal_GetCPEList()`, at most 100 bytes.
- Five functions invert the usual direction and hand back memory the **caller** must release: `docsis_GetDSChannel()` and `docsis_GetUSChannel()` return a dynamically allocated structure, and `docsis_GetDsOfdmChanTable()`, `docsis_GetUsOfdmaChanTable()` and `docsis_GetStatusOfdmaUsTable()` return a dynamically allocated array whose length they report through their entry-count argument. Failing to free these is a leak in the caller, not in the `HAL`.

#### Module Responsibilities

- Modules must allocate and de-allocate memory for their internal operations, ensuring efficient resource management.
- Modules are required to release all internally allocated memory upon closure to prevent resource leaks.
- All module implementations and caller code must strictly adhere to these memory management requirements for optimal performance and system stability. Unless otherwise stated specifically in the API documentation.

That final escape clause is not decorative: it is what the five caller-frees functions above rely on, and it is why a caller reads each declaration's own documentation in `include/cm_hal.h` before assuming who owns a buffer. Where the header says the caller must provide a pre-allocated structure - as it does for `docsis_GetDOCSISInfo()`, `docsis_GetErrorCodewords()`, `cm_hal_GetDHCPInfo()` and `cm_hal_GetIPv6DHCPInfo()` - the module allocates nothing on the caller's behalf.

### Power Management Requirements

`include/cm_hal.h` declares no power-management entry point, and no declaration or comment in it states an obligation on the implementation when the device changes power state. **This interface therefore specifies no participation in power management.** A caller must not assume that the `HAL` is notified of a power-state transition, and must not treat any function here as a way to request one; the functions that come closest are recovery operations rather than power controls - `cm_hal_HTTP_Download_Reboot_Now()` reboots the device and `cm_hal_FWupdateAndFactoryReset()` updates firmware and resets to factory defaults, both described under `API Surface`.

### Asynchronous Notification Model

This interface declares exactly one asynchronous notification, and it is the diplexer variation callback. `cm_hal_DiplexerVariationCallback` (`cm_hal.h:1209`) is documented as a "callback function for receiving current Diplexer settings", "invoked when the CM Diplexer settings change", and it is installed by `cm_hal_Register_DiplexerVariationCallback()` (`cm_hal.h:1228`).

Four properties of it bind a caller, all stated by the header:

- **Registration is one-way.** "The registered callback cannot be removed and should be provided during initialization." There is no unregister function, so a caller installs the handler once, during startup, and the handler must remain valid for the lifetime of the process.
- **The handler receives the settings by value.** It is passed a `CM_DIPLEXER_SETTINGS` structure holding the upstream and downstream diplexer upper band edges in `MHz`, so there is no lifetime question about the data itself.
- **The handler returns a status.** `RETURN_OK` on successful processing of the settings, `RETURN_ERR` on error - for example a failure to handle the settings change.
- **Registration may legitimately fail.** `RETURN_ERR` covers "not supported/implemented", including a stub implementation, which is why `Optional Components` treats the whole diplexer facility as optional.

Because `Threading Model` does not establish which thread invokes the handler, and because `Blocking calls` requires the interface's callers not to suspend the calling context, a handler should do the least work necessary - record the new settings and return - and leave any lengthy reaction to the caller's own thread.

No other notification exists here: nothing in `include/cm_hal.h` registers an event handler for registration state, channel changes or firmware download progress. Download progress in particular is polled, through `cm_hal_Get_HTTP_Download_Status()`, not pushed.

### Blocking calls

The APIs are expected to work synchronously and should complete within a time period commensurate with the complexity of the operation and in accordance with any relevant Broadband CM specification. Any calls that can fail due to the lack of a response from connected device should have a timeout period in accordance with any API documentation.
This API is called from a single thread context, therefore it must not suspend.

Two departures from that rule are stated by the interface itself and are the ones a caller must plan around:

- `cm_hal_InitDB()` is expected to block if the hardware is not ready, as `Initialization and Startup` records.
- `docsis_ClearDocsisEventLog()` carries the opposite obligation in the other direction: the header states that the function "must not block or use blocking system calls", and describes the clearing as asynchronous, likely by sending a message to a driver event handler. A caller therefore has no completion signal for it beyond its return code.

**No numeric timeout is specified by this interface.** `include/cm_hal.h` states no per-function time bound, and neither does this specification; a caller that needs a bound must impose it, and a vendor implementation must not rely on an unbounded wait. The long-running operations are the ones to watch: `cm_hal_HTTP_Download()` initiates a download whose progress is read separately through `cm_hal_Get_HTTP_Download_Status()`, which is the interface's own answer to "do not block for the duration of a firmware transfer".

### Internal Error Handling

**Synchronous Error Handling:** All Broadband CM HAL APIs must return errors synchronously as a return value. This ensures immediate notification of errors to the caller.

**Internal Error Reporting:** The HAL is responsible for reporting any internal system errors (e.g., out-of-memory conditions) through the return value.

**Focus on Logging for Errors:** For system errors, the HAL should prioritize logging the error details for further investigation and resolution. Recovery attempts at the interface level are not expected to be successful in these cases.

**The vocabulary is deliberately narrow, and a caller must read the return type before reading the return value.** `include/cm_hal.h` defines `RETURN_OK` as `0` and `RETURN_ERR` as `-1`, and most declarations here report only those two, so a caller learns that an operation failed but not why. Three shapes exist and they are not interchangeable:

| Return shape | Declarations | What a caller does with it |
| --- | --- | --- |
| A status code | 46 of the 51 declarations, plus the callback | Compare against `RETURN_OK` and `RETURN_ERR`. The reason for a failure is not reported; the header's own per-function text names the usual causes - null pointers, allocation failure, retrieval error - but the code does not distinguish them. |
| A value, not a status | `docsis_GetUSChannelId()` returns the channel identifier, `docsis_GetDownFreq()` returns the frequency, and `docsis_GetDocsisEventLogItems()` returns the number of log entries it retrieved | There is no error code to test. `docsis_GetDocsisEventLogItems()` is the well-behaved case: a count of zero is a meaningful answer. For the other two the interface defines no sentinel, so a caller cannot distinguish a failed read from a genuine value and should treat the surrounding state as its only evidence. |
| Nothing at all | `docsis_SetUSChannelId()` and `docsis_SetStartFreq()` are declared `void` | The write cannot be confirmed through the call. A caller that must know whether it took effect reads the value back with `docsis_GetUSChannelId()` or `docsis_GetDownFreq()`. |

`docsis_LLDgetEnableStatus()` is the one function returning a three-way result: `ENABLE`, `DISABLE` or `RETURN_ERR`. Because `DISABLE` also covers a missing bootfile entry, only `RETURN_ERR` indicates a failed read.

### Persistence Model

There is no requirement for the HAL to persist any setting information.

That statement bounds the `HAL`, not the modem. Two consequences follow, and neither is resolved by `include/cm_hal.h`: **the interface does not state whether a value written through it survives a reboot** - the upstream channel identifier, the primary downstream start frequency, the `MAC` re-initialisation threshold and the `HTTP` download settings are all written without any documented persistence guarantee - and the values it reads from the modem's own provisioning, such as the `LLD` bootfile entry read by `docsis_LLDgetEnableStatus()`, are persisted by the provisioning system rather than by this interface. A caller that needs a setting to be durable must re-apply it after a restart.

## Non functional requirements

Following non functional requirement should be supported by the component. Each topic below states whether what it requires comes from this specification's own policy or from `include/cm_hal.h`.

### Logging and debugging requirements

The component is required to record all errors and critical informative messages to aid in identifying, debugging, and understanding the functional flow of the system. Logging should be implemented using the syslog method, as it provides robust logging capabilities suited for system-level software. The use of `printf` is discouraged unless `syslog` is not available.

All HAL components must adhere to a consistent logging process. When logging is necessary, it should be performed into the `cm_vendor_hal.log` file, which is located in the `/rdklogs/logs/` directory.

Logs must be categorized according to the following log levels, as defined by the Linux standard logging system, listed here in descending order of severity:

- **FATAL**: Critical conditions, typically indicating system crashes or severe failures that require immediate attention.
- **ERROR**: Non-fatal error conditions that nonetheless significantly impede normal operation.
- **WARNING**: Potentially harmful situations that do not yet represent errors.
- **NOTICE**: Important but not error-level events.
- **INFO**: General informational messages that highlight system operations.
- **DEBUG**: Detailed information typically useful only when diagnosing problems.
- **TRACE**: Very fine-grained logging to trace the internal flow of the system.

Each log entry should include a timestamp, the log level, and a message describing the event or condition. This standard format will facilitate easier parsing and analysis of log files across different vendors and components.

Logging carries more weight in this interface than in one with a richer error vocabulary: as `Internal Error Handling` records, most declarations report only `RETURN_OK` or `RETURN_ERR`, so the vendor log is where the reason for a failure has to be found.

### Memory and performance requirements

The component should not contributing more to memory and CPU utilization while performing normal Broadband CM operations and commensurate with the operation required.

**No memory footprint limit is specified for this interface.** Neither `include/cm_hal.h` nor this specification states a maximum resident size, a heap budget or a `CPU` share, so a vendor implementation is held to the proportionality requirement above rather than to a number. Where a caller needs a bound - on a memory-constrained platform, for instance - it must be agreed with the vendor outside this interface. The one memory obligation this interface does state precisely is ownership, under `Memory Model`.

### Quality Control

To maintain software quality, it is recommended that the CM HAL implementation is verified without any errors using third-party tools such as Coverity, Black Duck, Valgrind, etc.

Both HAL wrapper and 3rd party software implementations should prioritize robust memory management to guarantee leak-free and corruption-resistant operation.

**Keeping this document true:** every topic here names the file its content was derived from - `include/cm_hal.h` for an interface fact, `CHANGELOG.md` and the repository's tags for `Version History`, `docs/generate_docs.sh` for the generated-site version string, the workspace `README.md` for the owning service and the platform inventory. Any change to one of those files obliges a review of the topics that cite it. That makes staleness detectable from a diff rather than from a review-by date, and in particular renaming, adding or removing a declared function invalidates `API Surface`, `Data Structures and Defines` and the `Sequence Diagram` immediately.

**Who reviews it:** `.github/CODEOWNERS` names `@rdkcentral/rdkb-hal-advisory` as the owner of every path in this repository, so that team is the addressee for the review obligation above and the reviewer of any pull request that changes this document.

### Licensing

Broadband CM HAL implementation is expected to released under the Apache License 2.0.

The full licence text is in `LICENSE`, with attribution in `NOTICE` and the copyright statement in `COPYING`; all three are linked into `docs/pages/` so the generated documentation carries them.

### Build Requirements

The source code should be capable of, but not be limited to, building under the Yocto distribution environment. The recipe should deliver a shared library named as `libcm_mgnt.so`.

A caller of that library:

1. must include `cm_hal.h` to make use of Broadband `CM HAL` capabilities;
2. must include a linker dependency for `libcm_mgnt`.

`libcm_mgnt.so` and the Yocto recipe are the only build artefacts this repository names; it declares no build manifest, no toolchain version and no compiler flag, so nothing further is stated here.

### Variability Management

The role of adjusting the interface, guided by versioning, rests solely within architecture requirements. Thereafter, vendors are obliged to align their implementation with a designated version of the interface. As per Service Level Agreement (`SLA`) terms, they may transition to newer versions based on demand needs.

Each API interface will be versioned using [Semantic Versioning 2.0.0](https://semver.org/), the vendor code will comply with a specific version of the interface.

**That version is not readable from the header.** `include/cm_hal.h` publishes no version macro, so a caller cannot test at compile time or at runtime which revision of the interface it has, and must take the version from the release it consumed - see `Version History`, which separates the release tag from the three other identities it is often confused with.

### Platform or Product Customization

**This interface defines no compile-time customization flags.** The complete preprocessor surface of `include/cm_hal.h` is its include guard, the `extern "C"` guard, and the `#ifndef`-guarded macros tabulated under `Data Structures and Defines`; those guards exist so that a caller which already defines `CHAR`, `INT`, `RETURN_OK` and the rest keeps its own definitions, not to select a variant of the interface. Nothing here is conditionally declared, so there is no `CM HAL` equivalent of the build-variability flags other `RDK-B` HALs use, and no function or structure appears or disappears with a build option.

Product variation is instead expressed at runtime, and a caller reads it rather than compiling for it:

- `cm_hal_GetMarket()` reports the market region the modem is built for, for example `EURO` for Europe or `US` for the United States - a value whose spelling collides with the upstream abbreviation and means the country here.
- `docsis_GetDOCSISInfo()` reports the `DOCSIS` version the modem implements, which is what decides whether the `DOCSIS 3.1` tables under `Optional Components` have anything to return.
- `cm_hal_get_DiplexerSettings()` reports the diplexer band edges, which differ between regional plant designs.
- `docsis_LLDgetEnableStatus()` reports whether the modem's bootfile provisions Low Latency DOCSIS.

A caller that must behave differently per product should branch on those readings, because they are the only variability this interface exposes.

## Interface API Documentation

All `HAL` function prototypes and datatype definitions are available in the `cm_hal.h` file, grouped there as `CM_HAL_TYPES` for the data types and `CM_HAL_APIS` for the functions. The topics below describe how the interface is meant to be driven, index every declaration in it, and then show one complete exchange and the modem states a caller can observe. `Build Requirements` states what to include and link against.

### Theory of operation and key concepts

The three sub-topics below are derived from the declarations and comments in `include/cm_hal.h` together with the ordering this specification states under `Initialization and Startup`; where neither establishes something, that is said rather than filled in.

#### Object Lifecycles

- **Creation/Initialization:** The CM HAL interface is initialized using the `cm_hal_InitDB()` function. This function sets up the necessary database connections and initializes various subsystems required for further operations with the cable modem. The downstream and upstream `PHY` layers are then prepared by `docsis_InitDS()` and `docsis_InitUS()`.
- **Usage:** After initialization, the cable modem can be managed using various API functions that rely on the initialized state. These functions allow for configuring and querying modem parameters, managing downstream and upstream channels, handling events, and controlling operational states.
- **Destruction/Cleanup:** The CM HAL interface does not provide a specific function for system deinitialization. Applications are responsible for managing and freeing resources manually to prevent memory leaks - in particular the five allocations listed under `Caller Responsibilities` - and cleanup within the `HAL` is generally handled internally upon application termination. A caller cannot release and re-acquire the interface within one process, because there is nothing to release it with; the registered diplexer callback is bound by the same limitation, as `Asynchronous Notification Model` records.

#### Method Sequencing

- **Initialization is Mandatory:** The system must be initialized (`cm_hal_InitDB()`) before any other operations are performed. This ensures that all subsystems are properly configured.
- **Sequential Dependency:** While most functions can be called independently once initialization is complete, some operations logically depend on the state of the modem or previous API calls (e.g., configuring channels before retrieving channel-specific data).
- **Event Handling:** Functions such as `cm_hal_Register_DiplexerVariationCallback()` allow for dynamic event handling and should be set up early in the application lifecycle if needed, because the registration cannot be undone.
- **Write-then-read for the `void` setters:** `docsis_SetUSChannelId()` and `docsis_SetStartFreq()` report nothing, so a caller that needs confirmation reads the value back afterwards. `Internal Error Handling` gives the full return-shape breakdown.
- **Configure-then-start for a download:** the `HTTP` path is a sequence rather than a single call - set the `URL` and filename, optionally set the interface, initiate the download, then poll its status, and only then check reboot readiness and reboot.

#### State-Dependent Behavior

- **Implicit State Model:** The CM HAL interface operates under several implicit states:
  - **Uninitialized:** Before any initialization function has been called.
  - **Initialized:** The system has been initialized but may not yet be fully operational or connected to network services.
  - **Operational:** The modem is fully operational, and all functionality is available.
  - **Error states:** Various functions may return errors if the system is not in an appropriate state for the requested operation.
- **The modem's own state is observable but not controlled here.** `docsis_getCMStatus()` reports where the modem has reached in `DOCSIS` bring-up and `docsis_GetDOCSISInfo()` reports the same progression field by field; `State Diagram` draws both. A caller reads those states, and the only transitions it can drive are the initialization sequence above and the three recovery operations - `cm_hal_ReinitMac()`, `cm_hal_HTTP_Download_Reboot_Now()` and `cm_hal_FWupdateAndFactoryReset()`.

### Data Structures and Defines

A caller of this interface constructs or interprets the types below. Every one is declared in [`include/cm_hal.h`](../../include/cm_hal.h) under the `CM_HAL_TYPES` group, and the field-level documentation is on the declaration itself; the tables here name each type, say where it is declared and what it represents, and leave the members to the header.

**Type aliases** \- `cm_hal.h:47` to `cm_hal.h:82`. Each is wrapped in `#ifndef`, so a caller that already defines the name keeps its own definition. They are what the function signatures in `API Surface` are written in.

| Alias | Underlying type |
| --- | --- |
| `CHAR` | `char` |
| `UCHAR` | `unsigned char` |
| `BOOLEAN` | `unsigned char` |
| `USHORT` | `unsigned short` |
| `UINT8` | `unsigned char` |
| `INT` | `int` |
| `UINT` | `unsigned int` |
| `LONG` | `long` |
| `ULONG` | `unsigned long` |

**Status and boolean constants** \- `cm_hal.h:86` to `cm_hal.h:109`. `Internal Error Handling` gives the semantics.

| Constant | Value | Represents |
| --- | --- | --- |
| `RETURN_OK` | `0` | Success, returned by the status-returning declarations. |
| `RETURN_ERR` | `-1` | Failure. The interface reports no reason alongside it. |
| `TRUE`, `FALSE` | `1`, `0` | The two values a `BOOLEAN` field or out-parameter carries. |
| `ENABLE`, `DISABLE` | `1`, `0` | The enablement result of `docsis_LLDgetEnableStatus`, which returns one of these or `RETURN_ERR`. |

**Bounds and the address type** \- the constants a caller sizes buffers and tables against.

| Constant | Declared at | Represents |
| --- | --- | --- |
| `OFDM_PARAM_STR_MAX_LEN` | `cm_hal.h:39` | Maximum length of an `OFDM` parameter string, `64`. |
| `IPV4_ADDRESS_SIZE` | `cm_hal.h:118` | Octets in an `IPv4` address, `4`. |
| `ANSC_IPV4_ADDRESS` | `cm_hal.h:135` | A macro expanding to an anonymous union of a `4`-octet array in dotted-decimal order and a `32`-bit value in network byte order. It is the type of the upgrade-server address in `CMMGMT_CM_DOCSIS_INFO` and of the addresses in the `DHCP` information structures. |
| `EVM_MAX_EVENT_TEXT` | `cm_hal.h:241` | Maximum length of the event text in `CMMGMT_CM_EventLogEntry_t`, `255`. |
| `MAX_KICKSTART_ROWS` | `cm_hal.h:410` | Maximum number of rows in `snmpv3_kickstart_table_t`, `5`. |

Those twenty macros are the header's entire valued-macro surface; there is no version macro among them, which is the point `Version History` and `Variability Management` both make.

**Structures.** Seventeen are declared. The `Represents` column is the declaration's own summary.

| Structure | Declared at | Represents |
| --- | --- | --- |
| `CMMGMT_CM_DS_CHANNEL` | `cm_hal.h:167` | A downstream channel: identifier, frequency, power level, `SNR`, modulation, octet and error counts, and lock status. |
| `CMMGMT_CM_US_CHANNEL` | `cm_hal.h:188` | An upstream channel: identifier, frequency, transmit power, channel type, symbol rate, modulation and lock status. |
| `CMMGMT_CM_DOCSIS_INFO` | `cm_hal.h:202` | `DOCSIS`-related information for the modem: the registration progression tabulated under `State Diagram`, the config file name, attempt counters, `ToD` status, `BPI` state, network access, upgrade-server address, `CPE` allowance, upstream and downstream service-flow parameters including `QoS`, data rates and core version. |
| `CMMGMT_CM_ERROR_CODEWORDS` | `cm_hal.h:235` | Codeword error statistics: unerrored, correctable and uncorrectable counts. |
| `CMMGMT_CM_EventLogEntry_t` | `cm_hal.h:246` | One entry of the modem's event log: index, first and last timestamps, occurrence count, level, identifier and text. |
| `CMMGMT_DML_CM_LOG` | `cm_hal.h:266` | Configuration settings for modem logging. |
| `CMMGMT_DML_DOCSISLOG_FULL` | `cm_hal.h:281` | A single entry within a `DOCSIS` log. |
| `CMMGMT_CM_DHCP_INFO` | `cm_hal.h:293` | The modem's `DHCP` configuration. |
| `CMMGMT_CM_IPV6DHCP_INFO` | `cm_hal.h:312` | The modem's `IPv6` `DHCP` configuration. |
| `CMMGMT_DML_CPE_LIST` | `cm_hal.h:328` | A single `CPE` entry. |
| `DOCSIF31_CM_DS_OFDM_CHAN` | `cm_hal.h:338` | Parameters of a `DOCSIS 3.1` `OFDM` downstream channel. |
| `DOCSIF31_CM_US_OFDMA_CHAN` | `cm_hal.h:376` | Parameters of a `DOCSIS 3.1` `OFDMA` upstream channel. |
| `DOCSIF31_CMSTATUSOFDMA_US` | `cm_hal.h:399` | Status information for a `DOCSIS 3.1` `OFDMA` upstream channel, including its ranging state and whether the channel is muted. |
| `fixed_length_buffer_t` | `cm_hal.h:415` | A buffer of fixed length: a `USHORT` byte count and a pointer to the data. |
| `snmp_kickstart_row_t` | `cm_hal.h:425` | One row of an `SNMP` v3 kickstart configuration: a security name and a security number, each a `fixed_length_buffer_t`. |
| `snmpv3_kickstart_table_t` | `cm_hal.h:434` | An `SNMP` v3 kickstart configuration table: a row count and up to `MAX_KICKSTART_ROWS` row pointers. |
| `CM_DIPLEXER_SETTINGS` | `cm_hal.h:443` | Diplexer frequency settings: the upstream and downstream upper band edges in `MHz`. This is the structure the notification handler receives. |

Twelve of those seventeen additionally declare a pointer alias formed by prefixing `P` to the structure name - `PCMMGMT_CM_DS_CHANNEL`, `PCMMGMT_CM_US_CHANNEL`, `PCMMGMT_CM_DOCSIS_INFO`, `PCMMGMT_CM_ERROR_CODEWORDS`, `PCMMGMT_DML_CM_LOG`, `PCMMGMT_DML_DOCSISLOG_FULL`, `PCMMGMT_CM_DHCP_INFO`, `PCMMGMT_CM_IPV6DHCP_INFO`, `PCMMGMT_DML_CPE_LIST`, `PDOCSIF31_CM_DS_OFDM_CHAN`, `PDOCSIF31_CM_US_OFDMA_CHAN` and `PDOCSIF31_CMSTATUSOFDMA_US`. Several signatures in `API Surface` are written in terms of those aliases, so a caller has to know them; the header records a coding-standard intention to retire them, so new code should prefer the structure name where it has the choice. `CMMGMT_CM_EventLogEntry_t`, `fixed_length_buffer_t`, `snmp_kickstart_row_t`, `snmpv3_kickstart_table_t` and `CM_DIPLEXER_SETTINGS` have no alias.

**The callback typedef.** `cm_hal_DiplexerVariationCallback` (`cm_hal.h:1209`) is the one handler type this interface defines. It is installed by `cm_hal_Register_DiplexerVariationCallback` (`cm_hal.h:1228`), it receives a `CM_DIPLEXER_SETTINGS` structure by value, and it returns `RETURN_OK` or `RETURN_ERR`. There is no matching unregister function; `Asynchronous Notification Model` states the obligations that follow.

### API Surface

This topic is the boundary between the two ways of reading this document. Everything above answers "what is this interface and how do I drive it"; from here on the document answers "exactly what is declared, and what happens when it fails". All **51** functions this interface declares are named below by exact identifier, grouped by functional area, mirroring the `CM_HAL_APIS` group in the header. Every one of them is declared in [`include/cm_hal.h`](../../include/cm_hal.h), and the `Declared at` column gives the line, where the per-`API` detail lives: parameter direction and ranges, buffer ownership, pre-conditions, and the return values each function can produce.

**Initialization \- 3 functions.** The sequence `Initialization and Startup` requires, in that order.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_InitDB` | `cm_hal.h:482` | Initializes the `HAL` and its dependencies; may block if the hardware is not ready. |
| `docsis_InitDS` | `cm_hal.h:494` | Initializes the downstream `PHY` layer and direct hardware access. |
| `docsis_InitUS` | `cm_hal.h:506` | Initializes the upstream `PHY` layer and direct hardware access. |

**Modem and DOCSIS status \- 2 functions.** The fast answer, and the detailed one behind it.

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_getCMStatus` | `cm_hal.h:547` | Retrieves and formats the modem's `DOCSIS` status into a caller-supplied buffer of at least `40` bytes; the value set is enumerated under `State Diagram`. |
| `docsis_GetDOCSISInfo` | `cm_hal.h:605` | Retrieves the current `DOCSIS` registration status into a caller-allocated `CMMGMT_CM_DOCSIS_INFO` structure. |

**Channel information \- 5 functions.** Per-channel parameters and how many channels are in use.

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_GetDSChannel` | `cm_hal.h:562` | Retrieves downstream channel information in a structure the `HAL` allocates and the caller frees. |
| `docsis_GetUsStatus` | `cm_hal.h:575` | Retrieves the status of one upstream channel, selected by index, into a caller-supplied structure. |
| `docsis_GetUSChannel` | `cm_hal.h:590` | Retrieves upstream channel information in a structure the `HAL` allocates and the caller frees. |
| `docsis_GetNumOfActiveTxChannels` | `cm_hal.h:618` | Reads how many upstream channels the current registration is using. |
| `docsis_GetNumOfActiveRxChannels` | `cm_hal.h:631` | Reads how many downstream channels the current registration is using. |

**DOCSIS 3.1 OFDM and OFDMA tables \- 3 functions.** Each allocates an array and reports its length; the caller frees it. `Optional Components` explains why the array may be empty.

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_GetDsOfdmChanTable` | `cm_hal.h:1077` | Retrieves the `DSOFDM` channel table as an allocated array of `DOCSIF31_CM_DS_OFDM_CHAN` entries. |
| `docsis_GetUsOfdmaChanTable` | `cm_hal.h:1094` | Retrieves the `USOFDMA` channel table as an allocated array of `DOCSIF31_CM_US_OFDMA_CHAN` entries. |
| `docsis_GetStatusOfdmaUsTable` | `cm_hal.h:1111` | Retrieves the `USOFDMA` channel status table as an allocated array of `DOCSIF31_CMSTATUSOFDMA_US` entries. |

**Channel and frequency control \- 4 functions.** The only pair in this interface where a setter reports nothing and its getter returns a bare value; `Internal Error Handling` and `Method Sequencing` both turn on that fact.

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_GetUSChannelId` | `cm_hal.h:690` | Returns the upstream channel identifier within its `MAC` domain as a `UINT8` value, not a status code. |
| `docsis_SetUSChannelId` | `cm_hal.h:699` | Sets the upstream channel identifier within its `MAC` domain. Declared `void`, so it reports nothing. |
| `docsis_GetDownFreq` | `cm_hal.h:708` | Returns the current primary downstream channel frequency as a `ULONG` value, not a status code. |
| `docsis_SetStartFreq` | `cm_hal.h:717` | Sets the primary downstream channel frequency. Declared `void`, so it reports nothing. |

**Provisioning, MDD override and certificates \- 5 functions.** How the modem is provisioned with an `IP` mode, and the state of its certificate.

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_GetMddIpModeOverride` | `cm_hal.h:666` | Reads the current `IP` provisioning-mode override status. |
| `docsis_SetMddIpModeOverride` | `cm_hal.h:681` | Sets the `IP` provisioning-mode override status. |
| `docsis_GetProvIpType` | `cm_hal.h:966` | Reads the provisioned `IP` type for the `WAN` interface. |
| `docsis_GetCert` | `cm_hal.h:979` | Reads the file path of the modem certificate. |
| `docsis_GetCertStatus` | `cm_hal.h:994` | Reads the modem certificate status. |

**Diagnostics: error codewords and event log \- 3 functions.**

| API | Declared at | Purpose |
| --- | --- | --- |
| `docsis_GetErrorCodewords` | `cm_hal.h:646` | Scans the active downstream channels and reports packet errors into a caller-allocated structure. |
| `docsis_GetDocsisEventLogItems` | `cm_hal.h:729` | Fills a caller-supplied array with up to a given number of event-log entries and **returns the number of entries retrieved**, not a status code. |
| `docsis_ClearDocsisEventLog` | `cm_hal.h:742` | Clears the event log asynchronously. The header requires this function not to block or use blocking system calls. |

**DHCP, CPE and market information \- 4 functions.** All read-only; see the addressing bullet under `Description`.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_GetDHCPInfo` | `cm_hal.h:757` | Reads the modem's `DHCP` information into a structure the caller allocates and frees. |
| `cm_hal_GetIPv6DHCPInfo` | `cm_hal.h:772` | Reads the modem's `IPv6` `DHCP` information into a structure the caller allocates and frees. |
| `cm_hal_GetCPEList` | `cm_hal.h:791` | Reads the list of connected `CPE` devices and their count; the caller allocates and frees both the list and the mode string, which is `router` or `bridge` and at most `100` bytes. |
| `cm_hal_GetMarket` | `cm_hal.h:804` | Reads the modem's market region, for example `EURO` for Europe or `US` for the United States. |

**HTTP firmware download \- 8 functions.** A configure-then-start-then-poll sequence rather than one call, as `Method Sequencing` describes.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_Set_HTTP_Download_Url` | `cm_hal.h:822` | Configures the download `URL` and image filename. Both buffers must be at least `200` bytes. |
| `cm_hal_Get_HTTP_Download_Url` | `cm_hal.h:838` | Reads back the configured download `URL` and filename, under the same buffer requirement. |
| `cm_hal_Set_HTTP_Download_Interface` | `cm_hal.h:853` | Selects the interface the download is to use. |
| `cm_hal_Get_HTTP_Download_Interface` | `cm_hal.h:868` | Reads back the interface the download is configured to use. |
| `cm_hal_HTTP_Download` | `cm_hal.h:879` | Initiates the download. |
| `cm_hal_Get_HTTP_Download_Status` | `cm_hal.h:897` | Reads the current download status; this is the interface's progress mechanism, in place of a notification. |
| `cm_hal_HTTP_Download_Reboot_Now` | `cm_hal.h:926` | Initiates a reboot, performing pre-reboot checks and updates. |
| `cm_hal_HTTP_LED_Flash` | `cm_hal.h:1060` | Controls flashing of the `HTTP` `LED` indicator. |

**Reboot, factory reset and MAC re-initialization \- 5 functions.** The recovery operations, and the threshold that governs the automatic one.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_Reboot_Ready` | `cm_hal.h:912` | Reports whether the system is ready for a reboot. |
| `cm_hal_FWupdateAndFactoryReset` | `cm_hal.h:940` | Initiates a firmware update from a given `URL` and image name, followed by a factory reset. |
| `cm_hal_ReinitMac` | `cm_hal.h:951` | Resets the modem's `MAC` layer while preserving its channels. |
| `cm_hal_set_ReinitMacThreshold` | `cm_hal.h:1168` | Sets the threshold at which `MAC`-layer re-initialization is triggered. |
| `cm_hal_get_ReinitMacThreshold` | `cm_hal.h:1181` | Reads the `MAC`-layer re-initialization threshold. |

**Reset counters \- 4 functions.** Four separately maintained counters; the interface declares no way to clear them.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_Get_CableModemResetCount` | `cm_hal.h:1007` | Reads how many times the modem has been reset. |
| `cm_hal_Get_LocalResetCount` | `cm_hal.h:1020` | Reads how many of those resets were local. |
| `cm_hal_Get_DocsisResetCount` | `cm_hal.h:1033` | Reads how many were `DOCSIS`-related. |
| `cm_hal_Get_ErouterResetCount` | `cm_hal.h:1046` | Reads how many times the eRouter has been reset. |

**Security, energy detection and diplexer \- 5 functions.** Includes the interface's only notification registration.

| API | Declared at | Purpose |
| --- | --- | --- |
| `cm_hal_snmpv3_kickstart_initialize` | `cm_hal.h:1140` | Initializes `SNMP` v3 security parameters from a kickstart table. |
| `docsis_IsEnergyDetected` | `cm_hal.h:1155` | Reports whether `DOCSIS` energy is present, which is how a caller decides whether the `WAN` is connected. |
| `docsis_LLDgetEnableStatus` | `cm_hal.h:1127` | Reports whether Low Latency `DOCSIS` is enabled, returning `ENABLE`, `DISABLE` or `RETURN_ERR`. |
| `cm_hal_get_DiplexerSettings` | `cm_hal.h:1196` | Reads the current diplexer band-edge settings. |
| `cm_hal_Register_DiplexerVariationCallback` | `cm_hal.h:1228` | Registers the handler invoked when the diplexer settings change. The registration cannot be undone, and `RETURN_ERR` may mean the platform does not implement the facility. |

The twelve groups hold `3`, `2`, `5`, `3`, `4`, `5`, `3`, `4`, `8`, `5`, `4` and `5` functions, which is `51` in total - every function the header declares, each named once. The callback type the last group registers is described under `Data Structures and Defines`.

### Sequence Diagram

The exchange below is the path `Method Sequencing` describes, with the three participants a `C` `HAL` has: the caller, the interface, and the vendor software behind it. Every function named is a declared identifier in `include/cm_hal.h`; where a step has a getter and a setter, both are named rather than abbreviated.

```mermaid
sequenceDiagram
participant Caller
participant CM HAL
participant Vendor

Note over Caller,CM HAL: Initialization Process
Caller->>CM HAL: cm_hal_InitDB()
CM HAL->>Vendor: Initialize database and dependencies
Vendor ->>CM HAL: Initialization complete
CM HAL->>Caller: RETURN_OK

Note over Caller,CM HAL: DOCSIS Initialization
Caller->>CM HAL: docsis_InitDS()
CM HAL->>Vendor: Initialize the downstream PHY layer
Vendor ->>CM HAL: Downstream initialized
CM HAL->>Caller: RETURN_OK
Caller->>CM HAL: docsis_InitUS()
CM HAL->>Vendor: Initialize the upstream PHY layer
Vendor ->>CM HAL: Upstream initialized
CM HAL->>Caller: RETURN_OK

Note over Caller,CM HAL: Notification registration, once and for the process lifetime
Caller->>CM HAL: cm_hal_Register_DiplexerVariationCallback(handler)
CM HAL->>Caller: RETURN_OK, or RETURN_ERR where unimplemented

Note over Caller,CM HAL: Normal Operation
Caller->>CM HAL: docsis_getCMStatus()
CM HAL->>Vendor: Read the modem registration state
Vendor ->>CM HAL: Status string
CM HAL->>Caller: RETURN_OK with the status buffer filled
Caller->>CM HAL: docsis_GetDOCSISInfo()
Caller->>CM HAL: docsis_GetDSChannel(), docsis_GetUSChannel()
CM HAL->>Caller: Allocated channel structures the caller must free
Caller->>CM HAL: docsis_GetNumOfActiveRxChannels(), docsis_GetNumOfActiveTxChannels()
Caller->>CM HAL: docsis_GetDsOfdmChanTable(), docsis_GetUsOfdmaChanTable(), docsis_GetStatusOfdmaUsTable()
Caller->>CM HAL: cm_hal_GetDHCPInfo(), cm_hal_GetIPv6DHCPInfo(), cm_hal_GetCPEList()
Caller->>CM HAL: docsis_GetErrorCodewords(), docsis_GetDocsisEventLogItems()
CM HAL->>Caller: Entry count returned directly, not a status code

Note over Caller,CM HAL: Provisioning override and channel control
Caller->>CM HAL: docsis_SetMddIpModeOverride()
CM HAL->>Vendor: Apply the IP provisioning mode override
Vendor ->>CM HAL: Applied
CM HAL->>Caller: RETURN_OK
Caller->>CM HAL: docsis_SetUSChannelId(), docsis_SetStartFreq()
CM HAL->>Vendor: Apply the channel identifier and start frequency
Caller->>CM HAL: docsis_GetUSChannelId(), docsis_GetDownFreq()
CM HAL->>Caller: Values read back, since the setters report nothing

Note over Caller,CM HAL: Energy detection
Caller->>CM HAL: docsis_IsEnergyDetected()
CM HAL->>Vendor: Detect DOCSIS energy
Vendor ->>CM HAL: Energy detection result
CM HAL->>Caller: RETURN_OK with the result flag set

Note over Caller,CM HAL: Firmware download and reboot
Caller->>CM HAL: cm_hal_Set_HTTP_Download_Url(), cm_hal_Set_HTTP_Download_Interface()
Caller->>CM HAL: cm_hal_HTTP_Download()
CM HAL->>Vendor: Start the image transfer
Caller->>CM HAL: cm_hal_HTTP_LED_Flash()
Caller->>CM HAL: cm_hal_Get_HTTP_Download_Status()
CM HAL->>Caller: Download progress, polled rather than pushed
Caller->>CM HAL: cm_hal_Reboot_Ready()
CM HAL->>Caller: RETURN_OK with the readiness flag set
Caller->>CM HAL: cm_hal_HTTP_Download_Reboot_Now()

Note over Caller,CM HAL: Recovery paths
Caller->>CM HAL: cm_hal_FWupdateAndFactoryReset()
CM HAL->>Vendor: Update firmware and reset to factory defaults
Caller->>CM HAL: cm_hal_set_ReinitMacThreshold()
Caller->>CM HAL: cm_hal_ReinitMac()
CM HAL->>Vendor: Reset the MAC layer, preserving channels
Vendor ->>CM HAL: Reinitialization done
CM HAL->>Caller: RETURN_OK

Note over Caller,CM HAL: SNMPv3 kickstart
Caller->>CM HAL: cm_hal_snmpv3_kickstart_initialize()
CM HAL->>Vendor: Initialize SNMPv3 security parameters
Vendor ->>CM HAL: SNMPv3 initialized
CM HAL->>Caller: RETURN_OK

Note over Caller,CM HAL: Asynchronous notification
Vendor ->>CM HAL: Diplexer settings change
CM HAL->>Caller: Registered handler invoked with CM_DIPLEXER_SETTINGS
Caller->>CM HAL: cm_hal_get_DiplexerSettings()
```

This is one path, not the whole interface. The declarations it does not walk - `docsis_GetUsStatus`, `docsis_GetProvIpType`, `docsis_GetCert`, `docsis_GetCertStatus`, `cm_hal_GetMarket`, `cm_hal_Get_HTTP_Download_Url`, `cm_hal_Get_HTTP_Download_Interface`, `cm_hal_get_ReinitMacThreshold`, `docsis_ClearDocsisEventLog`, `docsis_LLDgetEnableStatus`, `cm_hal_Get_CableModemResetCount`, `cm_hal_Get_LocalResetCount`, `cm_hal_Get_DocsisResetCount` and `cm_hal_Get_ErouterResetCount` - are queries a caller makes when it needs them rather than steps in a sequence, and every one of them is indexed under `API Surface`, which is the complete list.

### State Diagram

Two state vocabularies exist here and they are not the same thing. The interface's **own** lifecycle is short and is established by this specification: uninitialized, initialized, then operational. The **modem's** state is longer, is reported rather than driven, and is established by `include/cm_hal.h`, which enumerates on the declaration of `docsis_getCMStatus` (`cm_hal.h:547`) every value that function can return. The diagram draws the bring-up path those values name, in the order the header lists them.

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    Uninitialized --> Initialized: cm_hal_InitDB()
    Initialized --> PhyInitialized: docsis_InitDS() then docsis_InitUS()
    PhyInitialized --> NOT_READY: modem bring-up begins
    NOT_READY --> NOT_SYNCHRONIZED
    NOT_SYNCHRONIZED --> PHY_SYNCHRONIZED
    PHY_SYNCHRONIZED --> US_PARAMETERS_ACQUIRED
    US_PARAMETERS_ACQUIRED --> RANGING_COMPLETE
    RANGING_COMPLETE --> DHCPV4_COMPLETE
    DHCPV4_COMPLETE --> TOD_ESTABLISHED
    TOD_ESTABLISHED --> SECURITY_ESTABLISHED
    SECURITY_ESTABLISHED --> CONFIG_FILE_DOWNLOAD_COMPLETE
    CONFIG_FILE_DOWNLOAD_COMPLETE --> REGISTRATION_COMPLETE
    REGISTRATION_COMPLETE --> OPERATIONAL
    OPERATIONAL --> Initialized: cm_hal_ReinitMac()
    OPERATIONAL --> Uninitialized: cm_hal_HTTP_Download_Reboot_Now()
    OPERATIONAL --> Uninitialized: cm_hal_FWupdateAndFactoryReset()
```

**What the diagram asserts, and what it deliberately does not.** The three transitions out of `OPERATIONAL` are the only ones a caller can drive, and each is a declared function. The path from `NOT_READY` to `REGISTRATION_COMPLETE` is the sequence of values the header lists for `docsis_getCMStatus`, which is the order in which a modem synchronises with the `CMTS`, acquires an address, establishes time of day and security, downloads its configuration file and registers. **The interface does not state which transitions between those values are legal**, does not state a timeout for any phase, and does not state a retry limit; the only retry evidence it exposes is two counters in `CMMGMT_CM_DOCSIS_INFO`, for `DHCP` and `TFTP` attempts. A caller must therefore poll and interpret, not predict.

Nine further values `docsis_getCMStatus` can return are not drawn as states, because the interface establishes no edge that reaches them:

| Value | What it reports |
| --- | --- |
| `RANGING_IN_PROGRESS`, `DHCPV4_IN_PROGRESS`, `DHCPV6_IN_PROGRESS`, `REGISTRATION_IN_PROGRESS`, `EAE_IN_PROGRESS`, `DS_TOPOLOGY_RESOLUTION_IN_PROGRESS`, `BPI_INIT` | A phase under way rather than complete. Each pairs with a completion value in the diagram above, except the last two, which report early authentication and `BPI` initialization. |
| `DHCPV6_COMPLETE` | The `IPv6` counterpart of `DHCPV4_COMPLETE`. Which of the two a modem reaches depends on its provisioning mode, which `docsis_GetProvIpType` reads. |
| `ACCESS_DENIED`, `FORWARDING_DISABLED`, `RF_MUTE_ALL` | A condition that stops progress: the modem was refused, forwarding is off, or the upstream transmitter is muted. |
| `OTHER`, `Unsupported status` | The implementation could not map the modem's state onto the values above. |

The same progression is readable field by field from `CMMGMT_CM_DOCSIS_INFO` (`cm_hal.h:202`), which `docsis_GetDOCSISInfo` fills. Each field carries its own ordered value set, and the correspondence with the status values above is by phase name only - the header states no mapping between the two.

| Field | Values it takes | Reports |
| --- | --- | --- |
| `DOCSISDownstreamScanning`, `DOCSISDownstreamRanging`, `DOCSISUpstreamScanning`, `DOCSISUpstreamRanging` | `NotStarted`, `InProgress`, `Complete` | Each of the four `PHY`-level bring-up phases, downstream then upstream. |
| `DOCSISTftpStatus` | `NotStarted`, `InProgress`, `DownloadComplete` | The configuration-file download; the file name lands in `DOCSISConfigFileName`. |
| `DOCSISDataRegComplete` | `InProgress`, `RegistrationComplete` | Data registration, the last phase before the modem is operational. |
| `ToDStatus` | `NotStarted`, `Complete` | Time-of-day synchronisation. |
| `DOCSISDHCPAttempts`, `DOCSISTftpAttempts` | A count | How many attempts address acquisition and configuration download have taken. These are the only retry evidence the interface exposes. |

Two fields of that structure are **not** states and must not be read as a progression: `BPIState` and `NetworkAccess` are `BOOLEAN`, so they annotate whichever state the modem is in - whether link-layer privacy is established, and whether network access is permitted - rather than sitting in a sequence. A caller reads them alongside the status value, not instead of it.
