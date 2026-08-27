/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file cm_hal.h
 * @brief Interface definition for the RDK-B CM HAL: the DOCSIS cable modem contract.
 *
 * The cm_hal module provides an interface for interacting with cable modems adhering
 * to the DOCSIS (Data Over Cable Service Interface Specification) standard. This
 * header is the whole of that contract: it declares the scalar aliases and the two
 * status codes every entry point uses, the structures a caller allocates or reads,
 * one callback typedef, and 51 function prototypes covering initialization, DOCSIS
 * status and registration detail, channel information and statistics, the limited set
 * of writable channel and frequency controls, DHCP and CPE information, HTTP firmware
 * download and recovery, and diagnostics and identity. A vendor supplies the
 * implementation behind it; the caller is the RDK-B middleware.
 *
 * Three properties of the interface bind every declaration below and are stated once
 * here rather than argued per function, each with the source it comes from:
 * - Initialization is mandatory. cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS()
 *   come first, and no return code distinguishes "not initialized" from any other
 *   failure, so a caller enforces the order itself (`Initialization and Startup`,
 *   the HAL specification).
 * - There is no teardown. This header declares no de-initialization call, so the
 *   interface cannot be released and re-acquired within a process, and the one
 *   registered callback cannot be removed (`Object Lifecycles`, same document).
 * - Failure is reported synchronously and coarsely, through the return value only
 *   (`Internal Error Handling`, same document).
 *
 * For detailed information about DOCSIS 3.1, refer to the following specifications:
 * * Physical Layer Specification: https://account.cablelabs.com/server/alfresco/6f4e0e98-cea4-465b-af19-28b1143c3c4e
 * * Cable Modem Operations Support System Interface Specification: https://account.cablelabs.com/server/alfresco/3fb47021-ef6f-499f-a319-84fc2a0ccc0f
 *
 * Behaviour stated in this header is derived from these declarations and from the
 * repository specification. Where neither establishes a
 * behaviour, the block says so rather than filling the gap; nothing here is presented
 * as observed runtime behaviour.
 */

#ifndef __CM_HAL_H__
#define __CM_HAL_H__
/*
 * The header includes <stdint.h> for uint32_t, which ANSC_IPV4_ADDRESS uses, and
 * <sys/time.h> for the struct timeval members of CMMGMT_CM_EventLogEntry_t. The
 * scalar vocabulary of the interface itself is not the fixed-width one: it is the
 * `#ifndef`-guarded aliases defined below, which resolve to the target's plain
 * `char`, `short`, `int` and `long` types. A caller must not assume a fixed width
 * for any of them - see the note on UINT8, and `Data Structures and Defines` in
 * the HAL specification, which tabulates them.
 */
#include <stdint.h>
#include <sys/time.h>

/**********************************************************************
               CONSTANT DEFINITIONS
**********************************************************************/
/** Size in bytes of every fixed OFDM text array in this interface: the `averageSNR`
 *  and `PowerLevel` members of DOCSIF31_CM_DS_OFDM_CHAN. It is the array size, so a
 *  caller bounds its reads by it. Terminating the value inside it is the requirement
 *  `Memory Model` -> `Module Responsibilities` in the HAL specification places on every
 *  string this module produces; no truncation behaviour is stated anywhere in this
 *  interface, so a caller must not assume a value it reads is complete. */
#define OFDM_PARAM_STR_MAX_LEN 64

#ifdef __cplusplus
extern "C"{
#endif


#ifndef CHAR
/** Alias for `char`. Carries every text value this interface exchanges: the fixed
 *  arrays of the structures below and the caller-supplied string buffers of the
 *  getters. A definition the caller established before including this header wins,
 *  and the caller is then responsible for keeping it compatible with the
 *  declarations here, which are written in terms of this name. */
#define CHAR  char
#endif

#ifndef UCHAR
/** Alias for `unsigned char`. Used for the octet view of ANSC_IPV4_ADDRESS. */
#define UCHAR unsigned char
#endif

#ifndef BOOLEAN
/** Single-octet truth value holding TRUE or FALSE. Passed by value to
 *  cm_hal_HTTP_LED_Flash() and by pointer to docsis_IsEnergyDetected(), and used for
 *  the flag members of the structures below. Only the values TRUE and FALSE are
 *  defined by this interface; it states nothing about any other non-zero value. */
#define BOOLEAN  unsigned char
#endif

#ifndef USHORT
/** Alias for `unsigned short`. Used for the upstream channel index of
 *  docsis_GetUsStatus() and for the `length` member of fixed_length_buffer_t. */
#define USHORT  unsigned short
#endif

#ifndef UINT8
/** Alias for `unsigned char`, one byte wide. Note that it is defined here in terms
 *  of `unsigned char` and not in terms of `uint8_t`, even though this header includes
 *  <stdint.h>; the two names are not declared to be the same type by this interface,
 *  so a caller must not treat them as interchangeable in a declaration it writes
 *  against these prototypes. It is the return type of docsis_GetUSChannelId() and the
 *  type of the `n_rows` member of snmpv3_kickstart_table_t. */
#define UINT8 unsigned char
#endif

#ifndef INT
/** Alias for `int`. The return type of 47 of the 51 prototypes in this interface -
 *  every one except the two `void` setters, docsis_GetUSChannelId() which returns
 *  UINT8 and docsis_GetDownFreq() which returns ULONG. On 44 of those 47 it carries
 *  RETURN_OK or RETURN_ERR; on the remaining three - docsis_GetDocsisEventLogItems(),
 *  cm_hal_Get_HTTP_Download_Status() and docsis_LLDgetEnableStatus() - it carries a
 *  count, a progress or failure value, and a three-way enable result respectively,
 *  none of which may be compared against RETURN_OK. See `Internal Error Handling` in
 *  the HAL specification for the four return shapes - 44 binary status, 3 of these
 *  non-status INT results, 2 non-INT values and 2 `void` - and which declarations use
 *  them. */
#define INT   int
#endif

#ifndef UINT
/** Alias for `unsigned int`. Used for the event-log fields of
 *  CMMGMT_CM_EventLogEntry_t, the diplexer band edges of CM_DIPLEXER_SETTINGS, and
 *  the HTTP download interface selector. */
#define UINT  unsigned int
#endif

#ifndef LONG
/** Alias for `long`. No prototype in this header uses it; it is retained for callers
 *  written against it. */
#define LONG	long
#endif

#ifndef ULONG
/** Alias for `unsigned long`. Carries counts, identifiers, frequencies, byte totals
 *  and durations throughout this interface. Its width is whatever the target's
 *  `unsigned long` is; this interface fixes no width for it and states no upper bound
 *  on any value carried in it, so a caller must not assume a particular width and
 *  must not size a buffer or a wire field from one. */
#define ULONG unsigned long
#endif

#ifndef TRUE
/** Boolean true: the value a BOOLEAN carries for a set condition. */
#define TRUE     1
#endif

#ifndef FALSE
/** Boolean false: the value a BOOLEAN carries for a clear condition. */
#define FALSE    0
#endif

#ifndef ENABLE
/** Enabled state, numerically identical to TRUE. docsis_LLDgetEnableStatus() returns
 *  it to report that Low Latency DOCSIS is enabled in the modem's bootfile. */
#define ENABLE   1
#endif

#ifndef DISABLE
/** Disabled state, numerically identical to FALSE. docsis_LLDgetEnableStatus()
 *  returns it both when Low Latency DOCSIS is disabled and when the bootfile entry is
 *  absent, so the two cases are not distinguishable through that call. */
#define DISABLE  0
#endif

/*
 * The two status codes below are the complete failure vocabulary of this interface.
 * They are `int` macros rather than members of an enumeration: no enumeration of
 * return codes is declared here, so a caller compares an INT result against these
 * names and cannot switch over a named type. `Internal Error Handling`
 * in the HAL specification records the consequence - a caller learns that an operation
 * failed but not why, and the reason has to be found in the vendor log.
 */
#ifndef RETURN_OK
/** Success. The operation completed and every output documented for the call has
 *  been written. */
#define RETURN_OK   0
#endif

#ifndef RETURN_ERR
/** Failure. The only failure code this interface defines: it covers an invalid
 *  argument, an allocation failure, a communication failure with the modem hardware,
 *  a timeout or unexpected response, and an internal error in the implementation,
 *  without distinguishing them. Nothing documented for the call may be read. */
#define RETURN_ERR   -1
#endif

/** Size in bytes of an IPv4 address, 4, and therefore the element count of the `Dot`
 *  array of ANSC_IPV4_ADDRESS. It fixes that representation at four octets
 *  throughout this interface. */
#ifndef IPV4_ADDRESS_SIZE
#define  IPV4_ADDRESS_SIZE                          4
#endif

#ifndef ANSC_IPV4_ADDRESS
/*
 * ANSC_IPV4_ADDRESS is an IPv4-only representation, and this interface offers no
 * dual-form alternative: the members that use it - the upgrade server address of
 * CMMGMT_CM_DOCSIS_INFO and the address, mask, gateway and TFTP server of
 * CMMGMT_CM_DHCP_INFO - therefore carry IPv4 addresses only. IPv6 information is
 * reported through a separate structure, CMMGMT_CM_IPV6DHCP_INFO, whose addresses are
 * text rather than packed octets. A caller that needs an address form covering both
 * families must convert at its own boundary; nothing here does it.
 */

/**
 * @brief Represents an IPv4 address.
 *
 * This union provides two ways to represent an IPv4 address:
 * - Dot: An array of IPV4_ADDRESS_SIZE (4) bytes (octets) representing the IPv4
 *   address in dotted-decimal order (e.g., {192, 168, 0, 100}).
 * - Value: A 32-bit integer storing the IPv4 address in network byte order
 *   (big-endian).
 *
 * The two members alias the same four bytes, so a caller may read either after the
 * implementation has written the other; no discriminator is needed or provided. The
 * union is introduced by a macro rather than a typedef, so it is expanded in place at
 * each member that uses it and there is no type name for a caller to declare a
 * variable of.
 */
#define  ANSC_IPV4_ADDRESS                                                                  \
         union                                                                              \
         {                                                                                  \
            unsigned char           Dot[IPV4_ADDRESS_SIZE];                                 \
            uint32_t                Value;                                                  \
         }
#endif

/**
 * @defgroup CM_HAL Cable Modem HAL Interface
 * @brief Provides a standardized interface for interacting with cable modem hardware and software.
 *
 * This component enables communication between RDK-B (Reference Design Kit for Broadband) and cable modem implementations.
 *
 * @{
 * @defgroup CM_HAL_TYPES Data Types
 * @defgroup CM_HAL_APIS APIs
 * @}
 */

/**
 * @addtogroup CM_HAL_TYPES
 *
 * The definitions below are the whole vocabulary of this interface. There is no
 * opaque handle: a caller allocates, populates or reads these structures directly,
 * and the prototypes in CM_HAL_APIS are written in terms of them.
 *
 * Seventeen structures are declared, twelve of which also declare a pointer alias
 * formed by prefixing `P` to the structure name - `PCMMGMT_CM_DS_CHANNEL` and its
 * siblings. The alias and the structure name denote the same public type, and several
 * prototypes are written with the alias, so a caller has to know both. Neither
 * spelling is marked deprecated in this header, so either may be used where a
 * prototype leaves the choice; `Data Structures and Defines`
 * in the HAL specification tabulates every one of them with its location.
 *
 * Which side allocates a structure is decided per prototype, not per type, and it is
 * not uniform: most getters populate storage the caller owns, while five hand back
 * storage the caller must release. Each prototype states its own contract, and
 * `Memory Model` in the HAL specification is the summary.
 *
 * @{
 */

/**********************************************************************
                STRUCTURE DEFINITIONS
**********************************************************************/

 /**
  * @brief  Represents a cable modem's downstream channel information.
  *
  * The record a caller reads from docsis_GetDSChannel(): the channel's identity, its
  * frequency, power level and signal-to-noise ratio, its modulation and lock status,
  * and the octet and error counters accumulated since reset. Every value except the
  * identifier and the counters is text, and this interface states neither a format nor
  * a unit for those strings: the examples below appear as given, the frequency example
  * not being a figure in MHz, so a caller obtains the scaling from the implementation.
  */
typedef struct _CMMGMT_CM_DS_CHANNEL {  
    ULONG ChannelID;  /*!< Unique channel identifier (typically sequential, starting at 1). */
    CHAR Frequency[64];   /*!< Downstream frequency as text in a 64-byte field, not a number. Unit not stated; band 54 - 1002 MHz. Example: "64400". */
    CHAR PowerLevel[64];  /*!< Channel power level (dBmV or similar). Typical range: -15 to +15 dBmV. Example: "-1.5". */   
    CHAR SNRLevel[64];    /*!< Channel signal-to-noise ratio (dB). DOCSIS 3.1 typical range: 20 - 40 dB. Example: "38". */ 
    CHAR Modulation[64];  /*!< Modulation type (e.g., "QPSK", "256-QAM", "1024-QAM", "OFDM"). */ 
    ULONG Octets;         /*!< Total octets received on this channel since reset (range depends on traffic). */
    ULONG Correcteds;     /*!< Count of corrected errors since reset (varies based on channel conditions). */  
    ULONG Uncorrectables; /*!< Count of uncorrectable errors since reset (high values indicate potential issues). */  
    CHAR LockStatus[64];  /*!< Channel lock status. Expected values: "Locked", "Unlocked", "Not Available". */   

} CMMGMT_CM_DS_CHANNEL, *PCMMGMT_CM_DS_CHANNEL;
/*
 * Both channel structures above and below also declare a pointer alias -
 * `PCMMGMT_CM_DS_CHANNEL` and `PCMMGMT_CM_US_CHANNEL` - which is the form the
 * prototypes use. Each alias and its structure name denote the same public type,
 * neither spelling is marked deprecated in this header, and a caller may use either.
 */

 /**
  * @brief  Represents a cable modem's upstream channel information.
  *
  * The record a caller reads from docsis_GetUSChannel(), and, per index, from
  * docsis_GetUsStatus(): the channel's identity, its frequency, transmit power level
  * and channel type, its symbol rate and modulation, and its lock status. As with the
  * downstream record the values are text, with no format or unit stated for them.
  */
typedef struct _CMMGMT_CM_US_CHANNEL {
    ULONG ChannelID;      /*!< Unique channel identifier. */
    CHAR Frequency[64];   /*!< Upstream frequency as text in a 64-byte field, not a number. Unit not stated; band 5 - 204 MHz. Example: "12750". */
    CHAR PowerLevel[64];  /*!< Transmit power level (45 - 61 dBmV). Example: "60". */
    CHAR ChannelType[64]; /*!< Channel type (e.g., "ATDMA", "SCDMA", "OFDMA"). */
    CHAR SymbolRate[64];  /*!< Symbol rate (symbols/second, varies with configuration). */
    CHAR Modulation[64];  /*!< Modulation type (up to "4096-QAM"). */
    CHAR LockStatus[64];  /*!< Lock status ("Locked" or "Unlocked"). */

} CMMGMT_CM_US_CHANNEL, *PCMMGMT_CM_US_CHANNEL;

 /**
  * @brief  Represents DOCSIS-related information for a cable modem.
  *
  * The record a caller reads from docsis_GetDOCSISInfo(), and the field-by-field view
  * of the bring-up progression that docsis_getCMStatus() reports as a single string:
  * scanning and ranging in both directions, TFTP configuration download, data
  * registration, time-of-day synchronisation, BPI security state and network access,
  * together with the upgrade server address, the CPE allowance, the service flow
  * parameters, the data rates and the firmware core version. The caller allocates the
  * record; the implementation only writes into it.
  */
typedef struct _CMMGMT_CM_DOCSIS_INFO {
    CHAR DOCSISVersion[64];               /*!< DOCSIS version (e.g., "3.0", "3.1"). */
    CHAR DOCSISDownstreamScanning[64];    /*!< Downstream scanning status ("NotStarted", "InProgress", "Complete"). */
    CHAR DOCSISDownstreamRanging[64];     /*!< Downstream ranging status ("NotStarted", "InProgress", "Complete"). */
    CHAR DOCSISUpstreamScanning[64];      /*!< Upstream scanning status ("NotStarted", "InProgress", "Complete"). */
    CHAR DOCSISUpstreamRanging[64];       /*!< Upstream ranging status ("NotStarted", "InProgress", "Complete"). */
    CHAR DOCSISTftpStatus[64];            /*!< TFTP status for config download ("NotStarted", "InProgress", "DownloadComplete"). */
    CHAR DOCSISDataRegComplete[64];       /*!< Data registration status ("InProgress", "RegistrationComplete"). */
    ULONG DOCSISDHCPAttempts;             /*!< Number of DHCP attempts for IP acquisition (range depends on retries). */
    CHAR DOCSISConfigFileName[64];        /*!< Name of the downloaded DOCSIS config file. */ 
    ULONG DOCSISTftpAttempts;             /*!< Number of TFTP attempts for config download (range depends on retries). */
    CHAR ToDStatus[64];                   /*!< Time of Day sync status ("NotStarted", "Complete"). */
    BOOLEAN BPIState;                     /*!< Baseline Privacy Interface (BPI) security state (TRUE or FALSE). */
    BOOLEAN NetworkAccess;                /*!< Network access status for the modem (TRUE or FALSE). */
    ANSC_IPV4_ADDRESS UpgradeServerIP;    /*!< IP address of the firmware upgrade server. */
    ULONG MaxCpeAllowed;                  /*!< Maximum Customer Premises Equipment (CPE) allowed (typically 1 - 255). */
    CHAR UpstreamServiceFlowParams[64];   /*!< Upstream service flow parameters (including QoS). */
    CHAR DownstreamServiceFlowParams[64]; /*!< Downstream service flow parameters (including QoS). */
    CHAR DOCSISDownstreamDataRate[64];    /*!< Downstream data rate (bits per second, e.g., "10000"). */
    CHAR DOCSISUpstreamDataRate[64];      /*!< Upstream data rate (bits per second, e.g., "35000"). */
    CHAR CoreVersion[64];                 /*!< Modem firmware core version (e.g., "1.0"). */

} CMMGMT_CM_DOCSIS_INFO, *PCMMGMT_CM_DOCSIS_INFO; 

/*
 * The structure below also declares the pointer alias `PCMMGMT_CM_ERROR_CODEWORDS`,
 * which is the form docsis_GetErrorCodewords() is declared with. Both spellings
 * denote the same public type, and neither is marked deprecated in this header.
 */

 /**
  * @brief  Represents codeword error statistics for a cable modem.
  *
  * The three cumulative downstream codeword tallies a caller reads from
  * docsis_GetErrorCodewords(). They are counted since reset rather than per interval,
  * so a conclusion about signal quality comes from the difference between two
  * readings. The caller allocates this record - see the allocation note on that
  * prototype, which is the one place in this interface where the declared parameter
  * shape and the ownership rule have to be read together.
  */
typedef struct _CMMGMT_CM_ERROR_CODEWORDS {
    ULONG UnerroredCodewords;    /*!< Count of codewords received without detected errors. */
    ULONG CorrectableCodewords;  /*!< Count of codewords with errors that were corrected. */
    ULONG UncorrectableCodewords;/*!< Count of codewords with uncorrectable errors (indicating potential transmission issues). */
} CMMGMT_CM_ERROR_CODEWORDS, *PCMMGMT_CM_ERROR_CODEWORDS;

/** Element count of the `docsDevEvText` array of CMMGMT_CM_EventLogEntry_t, 255.
 *  It is the array size, so a caller bounds its reads by it. Terminating the
 *  description inside it is the requirement `Memory Model` -> `Module
 *  Responsibilities` in the HAL specification places on every string this module
 *  produces; no truncation behaviour is stated anywhere in this interface, so a caller
 *  must not assume a description it reads is complete. */
#define EVM_MAX_EVENT_TEXT      255

 /**
  * @brief  Represents a single entry within a cable modem's event log.
  *
  * One element of the array a caller passes to docsis_GetDocsisEventLogItems(): the
  * event's index, identifier, priority level and occurrence count, the first and most
  * recent times it was seen, and its description. The caller allocates the array and
  * the implementation fills as many elements as it has entries for, which is what
  * that prototype returns.
  */
typedef struct {
    UINT docsDevEvIndex;          /*!< Event index within the log (0 to UINT_MAX). */
    struct timeval docsDevEvFirstTime; /*!< Timestamp of the event's first occurrence. */
    struct timeval docsDevEvLastTime;  /*!< Timestamp of the event's most recent occurrence. */
    UINT docsDevEvCounts;         /*!< Total count of event occurrences (0 to UINT_MAX). */
    UINT docsDevEvLevel;          /*!< Event priority level (0 - 255). */
    UINT docsDevEvId;             /*!< Event identifier (0 to UINT_MAX). */
    CHAR docsDevEvText[EVM_MAX_EVENT_TEXT]; /*!< Textual description of the event. */

} CMMGMT_CM_EventLogEntry_t; 

/*
 * The structure below also declares the pointer alias `PCMMGMT_DML_CM_LOG`. Both
 * spellings denote the same public type, and neither is marked deprecated in this
 * header. No prototype in this header takes either form: the type is declared for
 * callers that model the logging controls it describes, and the log-clearing
 * operation itself is docsis_ClearDocsisEventLog().
 */

 /**
  * @brief  Represents configuration settings for cable modem (CM) logging.
  */
typedef struct _CMMGMT_DML_CM_LOG {
    BOOLEAN EnableLog;      /*!<  Enables or disables cable modem logging. */
    BOOLEAN ClearDocsisLog; /*!<  Controls whether the DOCSIS log should be cleared. */

} CMMGMT_DML_CM_LOG, *PCMMGMT_DML_CM_LOG;

/*
 * The structure below also declares the pointer alias `PCMMGMT_DML_DOCSISLOG_FULL`,
 * which denotes the same public type as the structure name, as with the aliases
 * above. No prototype in this header takes either form; the event log a caller can
 * actually read is delivered as CMMGMT_CM_EventLogEntry_t records by
 * docsis_GetDocsisEventLogItems().
 */

 /**
  * @brief  Represents a single entry within a DOCSIS log.
  */
typedef struct _CMMGMT_DML_DOCSISLOG_FULL {
    ULONG Index;         /*!< Index of the log entry within the full log. */
    ULONG EventID;       /*!< Unique identifier for the type of event logged. */   
    ULONG EventLevel;    /*!< Severity level of the event (e.g., error, warning, informational). */
    CHAR Time[64];       /*!< Timestamp of the event's occurrence. */
    CHAR Description[256];/*!< Textual description of the event. */

} CMMGMT_DML_DOCSISLOG_FULL, *PCMMGMT_DML_DOCSISLOG_FULL; 

 /**
  * @brief  Represents a cable modem's DHCP configuration.
  *
  * The record a caller reads from cm_hal_GetDHCPInfo(): the modem's own IPv4 lease
  * and the parameters that came with it - mask, gateway, TFTP and time servers, boot
  * file name, time offset, the lease, rebind and renew timers, the modem's MAC
  * address and the DHCP process status. It describes the modem's DOCSIS-side
  * provisioning, not a lease this interface can grant, change or release: every
  * addressing value here is read-only through this HAL.
  */
typedef struct _CMMGMT_CM_DHCP_INFO {
    ANSC_IPV4_ADDRESS IPAddress;        /*!< IPv4 address assigned to the cable modem. */
    CHAR BootFileName[256];             /*!< Name of the boot configuration file. */
    ANSC_IPV4_ADDRESS SubnetMask;      /*!< Subnet mask for the modem's IP address. */
    ANSC_IPV4_ADDRESS Gateway;         /*!< Default gateway IP address. */
    ANSC_IPV4_ADDRESS TFTPServer;      /*!< IP address of the TFTP server. */
    CHAR TimeServer[64];               /*!< Hostname or IP of the time server. */
    INT TimeOffset;                    /*!< Time offset from UTC (in seconds). */
    ULONG LeaseTimeRemaining;          /*!< Remaining IP lease time (in seconds). */
    CHAR RebindTimeRemaining[64];      /*!< Remaining time for DHCP rebind (in seconds). */
    CHAR RenewTimeRemaining[64];       /*!< Remaining time for DHCP renewal (in seconds). */
    CHAR MACAddress[64];               /*!< Modem's MAC address (e.g., "00:1A:2B:11:22:33"). */
    CHAR DOCSISDHCPStatus[64];         /*!< Status of the DOCSIS DHCP process. */

} CMMGMT_CM_DHCP_INFO, *PCMMGMT_CM_DHCP_INFO; 

 /**
  * @brief  Represents a cable modem's IPv6 DHCP configuration. 
  */
typedef struct _CMMGMT_CM_IPV6DHCP_INFO {
    CHAR IPv6Address[40];       /*!< IPv6 address assigned to the modem. */   
    CHAR IPv6BootFileName[256];  /*!< Name of the IPv6 boot configuration file. */
    CHAR IPv6Prefix[40];        /*!< IPv6 prefix assigned to the modem. */
    CHAR IPv6Router[40];        /*!< IPv6 address of the router. */
    CHAR IPv6TFTPServer[40];    /*!< IPv6 address of the TFTP server. */
    CHAR IPv6TimeServer[40];    /*!< IPv6 address or hostname of the time server. */
    ULONG IPv6LeaseTimeRemaining;  /*!< Remaining IPv6 lease time (in seconds). */  
    ULONG IPv6RebindTimeRemaining; /*!< Remaining time for IPv6 DHCP rebind (in seconds). */
    ULONG IPv6RenewTimeRemaining;  /*!< Remaining time for IPv6 DHCP renewal (in seconds). */

} CMMGMT_CM_IPV6DHCP_INFO, *PCMMGMT_CM_IPV6DHCP_INFO; 

 /**
  * @brief  Represents a single Customer Premises Equipment (CPE) entry. 
  */
typedef struct _CMMGMT_DML_CPE_LIST {
    CHAR IPAddress[32];  /*!<  IP address of the CPE (e.g., "192.168.0.1"). */
    CHAR MACAddress[32]; /*!<  MAC address of the CPE (e.g., "AA:BB:CC:DD:EE:FF"). */

} CMMGMT_DML_CPE_LIST, *PCMMGMT_DML_CPE_LIST; 

/**
 * @brief Represents parameters of a DOCSIS 3.1 OFDM downstream channel in a cable modem. 
 * @note for detailed information on Docsis3.1, please refer to the specification at the top of this file
 */ 
typedef struct _DOCSIF31_CM_DS_OFDM_CHAN {

    unsigned int ChannelId;           /*!< Downstream channel ID within a CMTS MAC interface. */
    unsigned int ChanIndicator;       /*!< Indicates channel role: primary (2), backup primary (3), non-primary (4). */
    unsigned int SubcarrierZeroFreq;  /*!< Center frequency (Hz) of subcarrier 0. */ 

    unsigned int FirstActiveSubcarrierNum; /*!< Index of the first non-excluded subcarrier (148 - 7895). */
    unsigned int LastActiveSubcarrierNum;  /*!< Index of the last non-excluded subcarrier (148 - 7895). */

    unsigned int NumActiveSubcarriers;   /*!< Count of active data subcarriers (excludes pilots, PLC). */ 

   /**! 
    * Max value depends on FFT mode (4K/8K) and subcarrier exclusions. 
    * See spec for details.
    */

    unsigned int SubcarrierSpacing;      /*!< Spacing between subcarriers (50 kHz for 4K mode, 25 kHz for 8K). */
    unsigned int CyclicPrefix;           /*!< Cyclic prefix length (in usec, multiple of 1/64 * 20us, see spec). */
    unsigned int RollOffPeriod;          /*!< Roll-off period (in usec, see spec for bandwidth/exclusion implications). */

    unsigned int PlcFreq;                /*!< Center frequency (Hz) of the PLC's lowest subcarrier. */
    unsigned int NumPilots;              /*!< Count of continuous pilots, from the OCD message. */
    unsigned int TimeInterleaverDepth;   /*!< Time interleaving depth, from the OCD message. */

    char averageSNR[OFDM_PARAM_STR_MAX_LEN];  /*!< Average downstream channel SNR. */
    char PowerLevel[OFDM_PARAM_STR_MAX_LEN];  /*!< Downstream channel power level (dBmV * 10). */

    unsigned long long PlcTotalCodewords;    /*!< Total PLC codewords received. */
    unsigned long long PlcUnreliableCodewords;/*!< PLC codewords failing LDPC syndrome check. */
    unsigned long long NcpTotalFields;       /*!< Total NCP fields received. */
    unsigned long long NcpFieldCrcFailures;  /*!< NCP fields failing CRC check. */

} DOCSIF31_CM_DS_OFDM_CHAN, *PDOCSIF31_CM_DS_OFDM_CHAN;

/**
 * @brief Represents parameters of a DOCSIS 3.1 OFDMA upstream channel in a cable modem.
 * @note for detailed information on Docsis3.1, please refer to the specification at the top of this file
 */ 
typedef struct _DOCSIF31_CM_US_OFDMA_CHAN {
    unsigned int ChannelId;         /*!< Upstream channel ID within a CMTS MAC interface. */
    unsigned int ConfigChangeCt;    /*!< Count of configuration changes (via the UCD message). */
    unsigned int SubcarrierZeroFreq;/*!< Lowest frequency (Hz) of the upstream channel. */

    unsigned int FirstActiveSubcarrierNum; /*!< Index of the first active subcarrier (range 74-3947). */
    unsigned int LastActiveSubcarrierNum;  /*!< Index of the last active subcarrier (range 74-3947). */
    unsigned int NumActiveSubcarriers;  /*!< Count of active data subcarriers (range 1-3800). */
    unsigned int SubcarrierSpacing;     /*!< Spacing between subcarriers (50 kHz for 2K, 25 kHz for 4K mode). */

    unsigned int CyclicPrefix;      /*!< Cyclic prefix length (in usec, see spec for values). */
    unsigned int RollOffPeriod;     /*!< Roll-off period (in usec, see spec for values). */

    unsigned int NumSymbolsPerFrame;/*!< Symbols per frame (bandwidth dependent, see spec). */
    unsigned int TxPower;           /*!< Transmit power level (quarter dBmV units, refer to PHYv3.1). */
    unsigned char PreEqEnabled;     /*!< Indicates if pre-equalization is enabled. */

} DOCSIF31_CM_US_OFDMA_CHAN, *PDOCSIF31_CM_US_OFDMA_CHAN; 

/**
 * @brief Represents status information for a DOCSIS 3.1 OFDMA upstream channel in a cable modem.
 * @note for detailed information on Docsis3.1, please refer to the specification at the top of this file
 */ 
typedef struct _DOCSIF31_CMSTATUSOFDMA_US {
    unsigned int ChannelId;        /*!< Upstream channel ID within a CMTS MAC interface. */
    unsigned int T3Timeouts;       /*!< Count of T3 timeout occurrences. */
    unsigned int T4Timeouts;       /*!< Count of T4 timeout occurrences. */
    unsigned int RangingAborteds;  /*!< Count of aborted ranging attempts. */
    unsigned int T3Exceededs;      /*!< Count of excessive T3 timeouts. */
    unsigned char IsMuted;         /*!< Indicates if the upstream channel is muted. */ 
    unsigned int RangingStatus;    /*!< Ranging state: other(1), aborted(2), retriesExceeded(3), success(4), continue(5), timeoutT4(6) */ 

} DOCSIF31_CMSTATUSOFDMA_US, *PDOCSIF31_CMSTATUSOFDMA_US;

/** Maximum number of rows of kickstart: the element count of the `kickstart_values`
 *  array of snmpv3_kickstart_table_t, 5. It is the array size, so it is also the largest
 *  meaningful value of that structure's `n_rows` member - a caller must not report more
 *  rows than this, and an implementation must not read past the array. */
#define MAX_KICKSTART_ROWS 5

/**
 * @brief Represents a buffer of fixed length.
 *
 * A length paired with a pointer, which is how this interface passes a byte sequence
 * whose extent is not implied by a fixed array size. Its only use here is inside
 * snmp_kickstart_row_t, so every instance a caller builds is input to
 * cm_hal_snmpv3_kickstart_initialize(), and the two members are read together: the
 * pointer says where the bytes are and the length says how many there are. The
 * allocation and lifetime contract for both is stated once, on the `@param` and notes
 * of that declaration, and is summarised per member below.
 */
typedef struct _fixed_length_buffer {
    /** Number of bytes addressed by `buffer`, and the only extent this interface
     *  provides for them: nothing else in the structure, and nothing on the consuming
     *  declaration, reports a size, so every access is bounded by this value. The
     *  `USHORT` type admits 0 to 65535. This interface states no minimum, no maximum
     *  below that width, and no behaviour either for a length of 0 or for a length that
     *  does not match the storage `buffer` addresses - a mismatch is a call-site defect
     *  the interface has no means of detecting or reporting. */
    USHORT length;
    /** Address of `length` bytes of storage the caller allocates and continues to own.
     *  The bytes are not required to be NUL-terminated, because `length` accompanies
     *  them, so a reader must not look for a terminator and must not read beyond
     *  `length`. This interface names neither an allocator nor a release function for
     *  the storage, and does not state whether an implementation copies the bytes or
     *  retains the pointer beyond the call; the caller therefore keeps the storage valid
     *  for at least the duration of the call and releases it itself afterwards, with the
     *  allocator it used. Whether NULL is accepted is not established either:
     *  cm_hal_snmpv3_kickstart_initialize() states no behaviour for a NULL member and has
     *  only RETURN_ERR with which to report one, so a caller must not pass one. */
    UINT8 *buffer;

} fixed_length_buffer_t;


 /**
  * @brief  Represents a single row in an SNMPv3 kickstart configuration. 
  *
  * One security name and one security number, each carried by value as a
  * fixed_length_buffer_t whose `buffer` still addresses caller storage. A row is
  * therefore not self-contained: copying it copies two pointers and not the bytes
  * behind them. Both members carry SNMPv3 credential material, and
  * cm_hal_snmpv3_kickstart_initialize() states what an implementation may and may not
  * be assumed to do with it.
  */
typedef struct _snmpv3_kickstart_row {
    /** SNMPv3 security name, as a byte count and a pointer to that many bytes of
     *  caller-owned storage. Its extent, nullability, ownership and lifetime are exactly
     *  the rules stated on the `length` and `buffer` members of fixed_length_buffer_t
     *  above; this interface adds no separate rule for this member, and states no valid
     *  value set and no maximum length for a security name. */
    fixed_length_buffer_t security_name;
    /** SNMPv3 security number, in the same byte-count-and-pointer form and under the same
     *  fixed_length_buffer_t member rules. This interface does not state whether the
     *  bytes are text or a numeric encoding, and states no valid range for them, so a
     *  caller supplies what its provisioning system defines and must not infer a format
     *  from the member name. */
    fixed_length_buffer_t security_number;

} snmp_kickstart_row_t; 

 /**
  * @brief  Represents an SNMPv3 kickstart configuration table. 
  *
  * The single argument of cm_hal_snmpv3_kickstart_initialize(), and a table of row
  * pointers rather than of rows: the structure itself, the rows it points at and the
  * buffers those rows point at are three separate allocations, all of them the
  * caller's. `n_rows` and `kickstart_values` are read together and neither is
  * meaningful without the other.
  */
typedef struct _snmpv3_kickstart_table {
    /** Number of leading `kickstart_values` entries that are populated: the valid range
     *  is 0 to MAX_KICKSTART_ROWS (5), and 0 means the table carries no rows. The
     *  declared `UINT8` admits 0 to 255, which is wider than the array, and this
     *  interface states no behaviour for a value above 5 - an implementation that trusted
     *  one would read past the array - so honouring the bound is the caller's
     *  responsibility and no return code reports its violation. */
    UINT8 n_rows;
    /** Fixed array of MAX_KICKSTART_ROWS (5) pointers to caller-allocated
     *  snmp_kickstart_row_t rows, of which only the first `n_rows` are read. The array is
     *  part of this structure and so needs no allocation of its own; each pointer in it
     *  does, and the caller allocates, owns and releases every row it populates. This
     *  interface names no release function for a row and does not state whether an
     *  implementation copies a row or retains its pointer beyond the call, so a caller
     *  keeps every populated row - and the buffers its members address - valid for at
     *  least the duration of the call. Nothing is stated about the entries at or beyond
     *  `n_rows` either: they are neither required to be NULL nor promised to be left
     *  unread, so setting the unused entries to NULL is the caller's own safeguard rather
     *  than a documented requirement. */
    snmp_kickstart_row_t *kickstart_values[MAX_KICKSTART_ROWS];

} snmpv3_kickstart_table_t; 

 /**
  * @brief  Represents diplexer frequency settings for a cable modem. 
  */
typedef struct _CM_DIPLEXER_SETTINGS {
    UINT usDiplexerSetting; /*!<  Upstream diplexer upper band edge (MHz). */
    UINT dsDiplexerSetting; /*!<  Downstream diplexer upper band edge (MHz). */ 

} CM_DIPLEXER_SETTINGS; 

/** @} */  //END OF GROUP CM_HAL_TYPES


/**********************************************************************************
 *
 *  CM Subsystem level function prototypes
 *
**********************************************************************************/

/*
 * Return-code contract for the prototypes below, stated once because it binds nearly
 * all of them. A status-returning declaration reports its outcome synchronously
 * through its INT return value, and the vocabulary is exactly RETURN_OK and
 * RETURN_ERR: this interface declares no error enumeration and no per-cause code. The
 * single RETURN_ERR therefore covers, without distinguishing them, an invalid input
 * parameter such as a null pointer or an out-of-range value, a resource allocation
 * failure, a communication failure with the modem hardware or an external system, a
 * timeout or unexpected response, and an internal error in the implementation. Two
 * consequences bind a caller and are repeated per declaration only where the client
 * action differs: a failure cannot be diagnosed from the return value, so the cause
 * has to be found in the vendor log `cm_vendor_hal.log` under `/rdklogs/logs/` as
 * `Logging and debugging requirements` in the HAL specification requires; and an
 * output must be treated as unspecified after a failure rather than as unmodified.
 *
 * Five declarations do not follow that shape, and `Internal Error Handling`
 * in the HAL specification tabulates them: docsis_GetUSChannelId(),
 * docsis_GetDownFreq(), docsis_GetDocsisEventLogItems() and
 * cm_hal_Get_HTTP_Download_Status() return a value rather than a status, and
 * docsis_LLDgetEnableStatus() returns a three-way ENABLE / DISABLE / RETURN_ERR
 * result. Two more return nothing at all: docsis_SetUSChannelId() and
 * docsis_SetStartFreq() are `void`, so a write through them cannot be confirmed by
 * the call. Each of those blocks states its own domain.
 */

/**
 * @addtogroup CM_HAL_APIS
 *
 * The 51 prototypes below are the whole callable surface of this interface, and
 * `API Surface` in the HAL specification indexes every one of them. Three properties
 * hold across the group: initialization comes first and there is no teardown; every
 * outcome is reported synchronously, in the vocabulary the note above describes; and
 * only a small set of values is writable - the upstream channel identifier, the
 * primary downstream start frequency, the MDD IP provisioning-mode override, the HTTP
 * download settings and the MAC re-initialisation threshold. Everything else is read
 * or is a recovery action.
 *
 * @{
 */

/**
 * @brief Brings up the CM HAL and the dependencies its implementation needs.
 *
 * This is the first call a caller makes into this interface. It establishes whatever
 * the vendor implementation needs in order to reach the modem - the header names
 * thread creation and file opening among the steps that can fail - and
 * `Object Lifecycles` in the HAL specification describes it as setting up the database
 * connections and subsystems the rest of the interface relies on. There is no
 * matching de-initialization call anywhere in this header, so what this function
 * establishes lives for the lifetime of the process.
 *
 * @pre None imposed by this interface: this is the entry point that establishes the
 *      pre-condition every other declaration carries. Calling it twice is not
 *      described by this interface, so a caller must not rely on a second call being
 *      either idempotent or an error.
 * @post On RETURN_OK the HAL is initialized and docsis_InitDS() and docsis_InitUS()
 *       may follow. On RETURN_ERR the state of the implementation is not specified by
 *       this interface: there is no de-initialization call with which to unwind a
 *       partial initialization, so a caller must treat the HAL as unusable rather
 *       than retry indefinitely.
 *
 * @returns The status of the initialization.
 * @retval RETURN_OK  - Initialization succeeded; the rest of the interface may be
 *         used once the two PHY initializers have been called.
 * @retval RETURN_ERR - Initialization failed, for example because a thread could not
 *         be created or a file could not be opened. No other call in this interface
 *         may be relied on. The client logs the failure and reports it rather than
 *         working around it, since nothing here reports the reason.
 *
 * @note Blocking: this is the one call `Initialization and Startup`
 *       in the HAL specification expects to block, and it blocks while the hardware is
 *       not ready. It is the exception to the non-blocking requirement the rest of
 *       the interface carries, which is why a caller performs initialization on a
 *       thread whose progress nothing else depends on. No numeric timeout is
 *       specified by this interface.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_InitDS, docsis_InitUS
 */
INT cm_hal_InitDB(void);

/**
 * @brief Prepares the downstream PHY layer and the caller's access to downstream
 *        hardware.
 *
 * The second step of the mandatory startup sequence: it establishes the global
 * PHY-level data structures for the downstream direction and direct access to the
 * downstream hardware, which is what the channel and statistics readers in this
 * interface then draw on.
 *
 * @pre cm_hal_InitDB() has returned RETURN_OK. `Initialization and Startup`
 *      in the HAL specification fixes that order, and no code here reports its
 *      violation, so the caller enforces it.
 * @post On RETURN_OK the downstream PHY layer is ready. RETURN_ERR reports only that
 *       the initialization did not complete, and this interface defines no
 *       de-initialization, so it does not establish what state a failed initialization
 *       leaves behind or provide any call with which to unwind it; a caller must not
 *       assume the downstream side is usable unless RETURN_OK was returned.
 *
 * @returns The status of the initialization.
 * @retval RETURN_OK  - The downstream PHY layer and hardware access are ready.
 *
 * @warning **RETURN_OK is the only value this interface documents for this call, and no
 *          failure code is established for it.** `cm_hal.h` defines `RETURN_ERR` for the
 *          `INT` status family, and cm_hal_InitDB() documents it explicitly, but nothing
 *          in this header, in the HAL specification or in the repository README states
 *          that this function returns it, so a caller must not infer one from the fact
 *          that an initialization call can fail: a value other than `RETURN_OK` means
 *          only that the call did not report success. The reason is not reported - a
 *          client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *          `/rdklogs/logs/` and must not continue to the upstream step or to any read
 *          on the downstream side.
 *
 * @note Blocking: synchronous, and part of the startup sequence whose first step
 *       cm_hal_InitDB() may block. `Blocking calls` in the HAL specification states no
 *       numeric timeout for it.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_InitDB, docsis_InitUS, docsis_GetDSChannel
 */
INT docsis_InitDS(void);

/**
 * @brief Prepares the upstream PHY layer and the caller's access to upstream
 *        hardware.
 *
 * The third step of the mandatory startup sequence, and the upstream counterpart of
 * docsis_InitDS(): it establishes the global PHY-level data structures for the
 * upstream direction and direct access to the upstream hardware.
 *
 * @pre cm_hal_InitDB() has returned RETURN_OK. `Initialization and Startup`
 *      in the HAL specification fixes that order, and no code here reports its
 *      violation, so the caller enforces it.
 * @post On RETURN_OK the upstream PHY layer is ready. As with docsis_InitDS() this
 *       interface does not establish what a failed initialization leaves behind, and
 *       provides no call with which to unwind it.
 *
 * @returns The status of the initialization.
 * @retval RETURN_OK  - The upstream PHY layer and hardware access are ready.
 *
 * @warning **RETURN_OK is the only value this interface documents for this call, and no
 *          failure code is established for it.** As with docsis_InitDS(), `RETURN_ERR` is
 *          defined for the `INT` status family but is not stated for this function by this
 *          header, by the HAL specification or by the repository README, so a caller must
 *          not treat any particular non-`RETURN_OK` value as a defined failure indication.
 *          The reason is not reported: a client looks for the cause in the vendor log
 *          `cm_vendor_hal.log` under `/rdklogs/logs/` and must not issue any read on the
 *          upstream side.
 *
 * @note Blocking: synchronous, and part of the startup sequence whose first step
 *       cm_hal_InitDB() may block. `Blocking calls` in the HAL specification states no
 *       numeric timeout for it.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_InitDB, docsis_InitDS, docsis_GetUSChannel
 */
INT docsis_InitUS(void);

/**
 * @brief Reports how far the cable modem has progressed through DOCSIS bring-up, as a
 *        text status.
 *
 * This is the quickest answer to "is the modem online": one string naming the stage
 * the modem has reached, from physical synchronisation through ranging, DHCP,
 * time-of-day, security and registration to OPERATIONAL, or naming the condition that
 * stopped it. docsis_GetDOCSISInfo() reports the same progression field by field for a
 * caller that needs the detail.
 *
 * @param[out] cm_status - Caller-allocated character array of at least 40 bytes, into
 *                         which the implementation writes one of the values listed
 *                         below as a NUL-terminated string. Termination here is the
 *                         requirement `Memory Model` -> `Module Responsibilities`
 *                         in the HAL specification places on every string this module
 *                         produces, rather than something the declaration itself
 *                         states, and no length out-parameter accompanies the buffer,
 *                         so the terminator is the only extent a caller has. It must
 *                         not be NULL. The 40-byte minimum is the bound this
 *                         interface fixes and `Memory Model` -> `Caller
 *                         Responsibilities` in the HAL specification repeats; a
 *                         shorter buffer is a caller defect, and the interface states
 *                         no truncation behaviour that would make one safe. The
 *                         caller owns the storage throughout and nothing here retains
 *                         the pointer. The status is text and not an enumeration -
 *                         this interface declares no enumeration for these values -
 *                         so a caller compares the buffer against the strings below
 *                         rather than switching on a named type.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the buffer holds a NUL-terminated status string, on the
 *       `Module Responsibilities` terms recorded on `cm_status` above. On RETURN_ERR
 *       the content of the buffer is not specified by this interface, so a caller
 *       must not assume it is unmodified and must not read it as a status.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A status string was written and may be read.
 * @retval RETURN_ERR - No status may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 *
 * @note Possible `cm_status` values. "Unsupported status" is the implementation's
 *       answer when it cannot map the modem's state onto any of the others, so it
 *       reports a gap in the mapping rather than a modem fault:
 *       - "Unsupported status"
 *       - "OTHER"
 *       - "NOT_READY"
 *       - "NOT_SYNCHRONIZED"
 *       - "PHY_SYNCHRONIZED"
 *       - "US_PARAMETERS_ACQUIRED"
 *       - "RANGING_COMPLETE"
 *       - "DHCPV4_COMPLETE"
 *       - "TOD_ESTABLISHED"
 *       - "SECURITY_ESTABLISHED"
 *       - "CONFIG_FILE_DOWNLOAD_COMPLETE"
 *       - "REGISTRATION_COMPLETE"
 *       - "OPERATIONAL"
 *       - "ACCESS_DENIED"
 *       - "EAE_IN_PROGRESS"
 *       - "DHCPV4_IN_PROGRESS"
 *       - "DHCPV6_IN_PROGRESS"
 *       - "DHCPV6_COMPLETE"
 *       - "REGISTRATION_IN_PROGRESS"
 *       - "BPI_INIT"
 *       - "FORWARDING_DISABLED"
 *       - "DS_TOPOLOGY_RESOLUTION_IN_PROGRESS"
 *       - "RANGING_IN_PROGRESS"
 *       - "RF_MUTE_ALL"
 * @note This interface reports these values but specifies neither which transitions
 *       between them are legal nor in what order they occur, so a caller must not
 *       infer a state machine from the list, must not wait for one value on the
 *       strength of having seen another, and reads each value only as the condition
 *       reported at the moment of the call. `State Diagram`
 *       in the HAL specification records the same limitation for this interface.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetDOCSISInfo, docsis_GetProvIpType
 */
INT docsis_getCMStatus(CHAR *cm_status); 

/**
 * @brief Reads the parameters of a downstream DOCSIS channel into a record the
 *        implementation allocates.
 *
 * Reports the frequency, power level, signal-to-noise ratio, modulation, lock status
 * and the octet, corrected-error and uncorrectable-error counters of a downstream
 * channel, which together are how a caller judges downstream reception. The channel
 * is not selected by the caller: unlike docsis_GetUsStatus() this declaration takes
 * no index, and neither this header nor the repository specification states whether
 * the record describes the primary downstream channel or one the implementation
 * chooses, so a caller that must identify it reads the `ChannelID` member of the
 * record it receives.
 *
 * @param[out] ppinfo - Address of a single `PCMMGMT_CM_DS_CHANNEL` pointer variable,
 *                      which is one level of indirection more than the structure
 *                      itself; it must not be NULL. On success the implementation
 *                      stores in `*ppinfo` the address of one `CMMGMT_CM_DS_CHANNEL`
 *                      record it has allocated, and the caller owns that storage
 *                      from then on. The result is a single record, so no element
 *                      count accompanies it. See the allocation note below for the
 *                      release obligation and for what this interface leaves open.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure,
 *      so a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*ppinfo` addresses a populated record the caller must later
 *       release. On RETURN_ERR the content of `*ppinfo` is not specified by this
 *       interface, so a caller must not assume it is unmodified, must not read
 *       through it and has no stated basis for releasing it either.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A record was allocated and populated; the caller reads it and
 *         is responsible for releasing it.
 * @retval RETURN_ERR - No record may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership. This is one of the five calls in this interface
 *       that hand back memory the caller must release: the caller is responsible for
 *       freeing the dynamically allocated memory of the returned structure, and
 *       `Memory Model` -> `Caller Responsibilities` in the HAL specification names
 *       docsis_GetDSChannel() among them, recording that failing to free the result
 *       is a leak in the caller rather than in the HAL. Two parts of that contract
 *       this interface does not establish, and a caller must not guess at either: it
 *       names neither the allocator that produced the storage nor the release
 *       function that matches it, so the pairing has to be agreed with the vendor
 *       implementation; and it does not say whether `*ppinfo` is written on failure,
 *       so a caller cannot tell an untouched pointer from an allocated one after
 *       RETURN_ERR and a leak on that path cannot be ruled out from the interface
 *       alone. Initialising `*ppinfo` to NULL before the call is what makes the
 *       difference observable at all.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_DS_CHANNEL, docsis_GetUSChannel, docsis_GetNumOfActiveRxChannels
 */
INT docsis_GetDSChannel(PCMMGMT_CM_DS_CHANNEL *ppinfo);

/**
 * @brief Reads the status of one upstream channel, selected by index, into a
 *        structure the caller supplies.
 *
 * The indexed alternative to docsis_GetUSChannel(): where that call returns a single
 * record for a channel it chooses, this one lets a caller walk the upstream channels
 * it knows about. The fields are the same - identity, frequency, transmit power,
 * channel type, symbol rate, modulation and lock status.
 *
 * @param[in]  i     - Index of the upstream channel, counting from 0. This interface
 *                     states no upper bound and publishes no count of upstream
 *                     channels from which one could be derived:
 *                     docsis_GetNumOfActiveTxChannels() reports how many are active
 *                     in the current registration, but this interface does not state
 *                     that the index range and that count coincide. An index that
 *                     identifies no channel is reported as RETURN_ERR like any other
 *                     failure.
 * @param[out] pinfo - Pointer to a `CMMGMT_CM_US_CHANNEL` the caller has allocated
 *                     and continues to own; the implementation writes through it and
 *                     nothing here retains it. It must not be NULL. Note the single
 *                     level of indirection: unlike docsis_GetUSChannel() this
 *                     declaration takes the address of the structure itself, so the
 *                     caller supplies the storage and no allocation is handed back.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK every member of `*pinfo` has been written. On RETURN_ERR the
 *       content is not specified by this interface, so a caller must not assume it is
 *       unmodified.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The channel status was read and `*pinfo` may be read.
 * @retval RETURN_ERR - Nothing may be read from `*pinfo`. An out-of-range index, a
 *         NULL `pinfo` and a failed read are reported identically, so a caller that
 *         is walking channels stops at the first failure rather than treating it as
 *         "this index only".
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_US_CHANNEL, docsis_GetUSChannel, docsis_GetNumOfActiveTxChannels
 */
INT docsis_GetUsStatus(USHORT i, PCMMGMT_CM_US_CHANNEL pinfo);

/**
 * @brief Reads the parameters of an upstream DOCSIS channel into a record the
 *        implementation allocates.
 *
 * Reports the frequency, transmit power level, channel type, symbol rate, modulation
 * and lock status of an upstream channel. As with docsis_GetDSChannel() the channel
 * is not selected by the caller - the declaration takes no index and the interface
 * does not state which channel the record describes - so a caller that must identify
 * it reads the `ChannelID` member. docsis_GetUsStatus() is the indexed alternative
 * for walking upstream channels one by one.
 *
 * @param[out] ppinfo - Address of a single `PCMMGMT_CM_US_CHANNEL` pointer variable,
 *                      which is one level of indirection more than the structure
 *                      itself; it must not be NULL. On success the implementation
 *                      stores in `*ppinfo` the address of one `CMMGMT_CM_US_CHANNEL`
 *                      record it has allocated, and the caller owns that storage
 *                      from then on. The result is a single record, so no element
 *                      count accompanies it.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure,
 *      so a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*ppinfo` addresses a populated record the caller must later
 *       release. On RETURN_ERR the content of `*ppinfo` is not specified by this
 *       interface, so a caller must not assume it is unmodified and must not read
 *       through it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A record was allocated and populated; the caller reads it and
 *         is responsible for releasing it.
 * @retval RETURN_ERR - No record may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership. Identical to docsis_GetDSChannel(): the
 *       implementation allocates the record and `Memory Model` ->
 *       `Caller Responsibilities` in the HAL specification places its release on the
 *       caller, while the interface names neither the allocator nor the matching
 *       release function and does not state whether `*ppinfo` is written on failure.
 *       A caller settles the release convention with the vendor implementation and
 *       initialises `*ppinfo` to NULL before the call.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_US_CHANNEL, docsis_GetUsStatus, docsis_GetNumOfActiveTxChannels
 */
INT docsis_GetUSChannel(PCMMGMT_CM_US_CHANNEL *ppinfo);

/**
 * @brief Reads the modem's DOCSIS registration detail into a structure the caller
 *        supplies.
 *
 * The field-by-field view of DOCSIS bring-up and registration: scanning and ranging
 * in both directions, TFTP configuration-file download, data registration,
 * time-of-day synchronisation, BPI security state and network access, together with
 * the DOCSIS version the modem implements, the upgrade server address, the CPE
 * allowance, the service flow parameters, the data rates and the firmware core
 * version. The DOCSIS version reported here is what decides whether the DOCSIS 3.1
 * channel tables in this interface have anything to return.
 *
 * @param[out] pinfo - Pointer to a `CMMGMT_CM_DOCSIS_INFO` the caller has allocated
 *                     and continues to own; it must not be NULL. The implementation
 *                     writes through the pointer and allocates nothing:
 *                     `Memory Model` -> `Caller Responsibilities`
 *                     in the HAL specification names this call among those where the
 *                     module allocates nothing on the caller's behalf. Nothing here
 *                     retains the pointer beyond the call. Every text member of the
 *                     record is a 64-byte array, which bounds what may be read from
 *                     it; terminating the value inside each is the requirement
 *                     `Memory Model` -> `Module Responsibilities`
 *                     in the HAL specification places on every string this module
 *                     produces, and no truncation behaviour is stated, so a caller
 *                     must not assume a value it reads is complete.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the members the implementation was able to read have been
 *       written. This interface does not state that members it could not read are
 *       cleared, so a caller must not treat an unset member as meaningful. On
 *       RETURN_ERR no member may be relied on.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The record was populated and may be read.
 * @retval RETURN_ERR - Nothing may be read from `*pinfo`.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_DOCSIS_INFO, docsis_getCMStatus, docsis_GetDsOfdmChanTable
 */
INT docsis_GetDOCSISInfo(PCMMGMT_CM_DOCSIS_INFO pinfo);

/**
 * @brief Reports how many active upstream channels the modem's current registration uses.
 *
 * The count comes from the registration in force, so it changes when the modem
 * re-registers rather than when a caller reads it. It is the size of the active upstream
 * channel set a caller is dealing with, not an index bound this interface guarantees:
 * nothing here states that the count and the index accepted by docsis_GetUsStatus() coincide.
 *
 * @param[out] cnt - Pointer to a caller-owned `ULONG` that receives the count. It
 *                   must not be NULL. This interface states no upper bound on the
 *                   value and publishes no maximum channel count from which one
 *                   could be derived. The caller owns the storage and nothing here
 *                   retains the pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*cnt` holds the count. On RETURN_ERR the content of `*cnt` is
 *       not specified by this interface, so a caller must not assume it is
 *       unmodified and in particular must not read it as zero channels.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was read and may be used.
 * @retval RETURN_ERR - No count may be read. A NULL `cnt` and a failed read are
 *         reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetNumOfActiveRxChannels, docsis_GetUSChannel, docsis_GetUsStatus
 */
INT docsis_GetNumOfActiveTxChannels(ULONG *cnt); 

/**
 * @brief Reports how many active downstream channels the modem's current registration uses.
 *
 * The count comes from the registration in force, so it changes when the modem
 * re-registers rather than when a caller reads it. It is the size of the active downstream
 * channel set a caller is dealing with, not an index bound this interface guarantees:
 * nothing here states that the count and the index accepted by any indexed reader coincide.
 *
 * @param[out] cnt - Pointer to a caller-owned `ULONG` that receives the count. It
 *                   must not be NULL. This interface states no upper bound on the
 *                   value and publishes no maximum channel count from which one
 *                   could be derived. The caller owns the storage and nothing here
 *                   retains the pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*cnt` holds the count. On RETURN_ERR the content of `*cnt` is
 *       not specified by this interface, so a caller must not assume it is
 *       unmodified and in particular must not read it as zero channels.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was read and may be used.
 * @retval RETURN_ERR - No count may be read. A NULL `cnt` and a failed read are
 *         reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetNumOfActiveTxChannels, docsis_GetDSChannel, docsis_GetErrorCodewords
 */
INT docsis_GetNumOfActiveRxChannels(ULONG *cnt); 

/**
 * @brief Scans the active downstream channels and reports their codeword error
 *        counts into a structure the caller supplies.
 *
 * Reports the three cumulative DOCSIS codeword tallies for the downstream direction
 * - codewords received without detected errors, codewords whose errors the receiver
 * corrected, and codewords whose errors it could not correct - which together are
 * how a caller judges downstream signal quality. The counters are cumulative rather
 * than per-interval, so a caller draws its conclusion from the difference between
 * two readings rather than from a single one.
 *
 * @param[out] ppinfo - Address of a single `PCMMGMT_CM_ERROR_CODEWORDS` pointer
 *                      variable, which is one level of indirection more than the
 *                      structure itself; it must not be NULL, and `*ppinfo` must
 *                      address a `CMMGMT_CM_ERROR_CODEWORDS` the caller has
 *                      allocated. The record holds three fixed counters, so there is
 *                      no element count to learn and no array to size. The allocation
 *                      contract is stated in full in the note below, because the
 *                      declared type and the ownership rule pull in opposite
 *                      directions and only the note settles them.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure,
 *      so a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself. In addition, `*ppinfo` must address
 *      caller-allocated storage of the declared type before the call; if it does
 *      not, the implementation writes through whatever the pointer holds, which this
 *      interface does not define and which may corrupt caller memory.
 * @post On RETURN_OK the three counters of the caller's record have been written. On
 *       RETURN_ERR the content of the record is not specified by this interface, so
 *       a caller must not assume it is unmodified and must not read it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The counters were read and may be read out of the caller's
 *         record.
 * @retval RETURN_ERR - Nothing may be read from the record. A NULL `ppinfo` and a
 *         failed scan are reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership - the one point on this declaration a caller must
 *       not get wrong. The declared parameter type is
 *       `PCMMGMT_CM_ERROR_CODEWORDS *`, the same pointer-to-pointer shape this
 *       header uses for docsis_GetDSChannel() and docsis_GetUSChannel(), where the
 *       implementation allocates the record and the caller releases it. This call is
 *       the opposite case, and the interface states so twice: this declaration
 *       requires the caller to provide a pre-allocated structure and does *not*
 *       manage memory allocation for it, and `Memory Model` ->
 *       `Module Responsibilities` in the HAL specification names
 *       docsis_GetErrorCodewords() among the calls where "the module allocates
 *       nothing on the caller's behalf", while the five calls that do hand back
 *       memory the caller must release are listed under `Caller Responsibilities`
 *       and this is not one of them. The contract is therefore: the caller allocates
 *       one `CMMGMT_CM_ERROR_CODEWORDS`, sets `*ppinfo` to its address before the
 *       call, and releases that storage itself with the allocator it used; the
 *       implementation writes the three counters through `*ppinfo` and allocates
 *       nothing. Nothing in the record is heap-owned either - the three members are
 *       `ULONG` counters, so releasing the record releases everything the call
 *       produced.
 * @note One consequence of that contract, and one thing this interface leaves
 *       unstated. The consequence: the interface declares neither an allocator nor a
 *       release function for this record, because it needs neither - the storage
 *       belongs to the caller for its whole lifetime and no allocation crosses the
 *       interface in either direction. A caller therefore never has a pointer from
 *       this call to free, and must not treat `*ppinfo` on return as anything but the
 *       address it passed in. What is unstated is the behaviour when the
 *       precondition is broken: this interface does not say what an implementation
 *       does when `ppinfo` is NULL or `*ppinfo` does not address caller-allocated
 *       storage of the declared type. Only RETURN_ERR is available to report it, and
 *       an implementation that writes through an unset pointer instead may corrupt
 *       caller memory, so a caller must never pass an uninitialised `*ppinfo` and
 *       must not read a returned RETURN_ERR as evidence that nothing was written.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_ERROR_CODEWORDS, docsis_GetDSChannel, docsis_GetNumOfActiveRxChannels
 */
INT docsis_GetErrorCodewords(PCMMGMT_CM_ERROR_CODEWORDS *ppinfo); 

/**
 * @brief Reads the modem's IP provisioning-mode override as a text value.
 *
 * The override says which address families the modem is allowed to provision,
 * independently of what the MDD (MAC Domain Descriptor) the network sends would
 * select. Reading it is how a caller tells a deliberately pinned mode from one the
 * network chose.
 *
 * @param[out] pValue - Caller-allocated character array of at least 64 bytes, into
 *                      which the implementation writes one of the values listed below
 *                      as a NUL-terminated string. Termination here is the
 *                      requirement `Memory Model` -> `Module Responsibilities`
 *                      in the HAL specification places on every string this module
 *                      produces, rather than something the declaration itself states,
 *                      and no length out-parameter accompanies the buffer, so the
 *                      terminator is the only extent a caller has. It must not be
 *                      NULL. The 64-byte minimum is the bound this interface fixes; a
 *                      shorter buffer is a caller defect and no truncation behaviour
 *                      is specified that would make one safe. The caller owns the
 *                      storage throughout.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the buffer holds a NUL-terminated mode string, on the
 *       `Module Responsibilities` terms recorded on `pValue` above. On RETURN_ERR its
 *       content is not specified by this interface, so a caller must not assume it is
 *       unmodified.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A mode string was written and may be read.
 * @retval RETURN_ERR - No mode may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 *
 * @note Possible values: "ipv4Only", "ipv6Only" and "honorMdd", the last meaning that
 *       no override is in force and the MDD decides. This interface declares no
 *       enumeration for them, so a caller compares strings.
 * @note "APM" and "DualStack" may be technically supported by an implementation, but
 *       the expected behaviour of this getter is to return only the three values
 *       above, and docsis_SetMddIpModeOverride() cannot set either of them. A caller
 *       that sees one has met an implementation departing from this contract.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_SetMddIpModeOverride, docsis_GetProvIpType
 */
INT docsis_GetMddIpModeOverride(CHAR *pValue); 

/**
 * @brief Sets or clears the modem's IP provisioning-mode override.
 *
 * Pins the address families the modem may provision, or hands the decision back to
 * the MDD the network sends. It is one of the few values this interface lets a caller
 * write, and this interface does not state whether the value survives a reboot -
 * `Persistence Model` in the HAL specification records that no persistence guarantee
 * is given, so a caller that needs one re-applies the value after a restart.
 *
 * @param[in] pValue - NUL-terminated mode string the caller owns. The settable values
 *                     are "ipv4Only", "ipv6Only" and "honorMdd"; an empty string ("")
 *                     clears the override. It must not be NULL. The declaration is
 *                     `CHAR *` rather than `const CHAR *`, so it does not prevent the
 *                     implementation from writing through the pointer; a caller
 *                     therefore passes writable storage it owns rather than a string
 *                     literal. This interface does not state whether the
 *                     implementation copies the string or retains the pointer, so the
 *                     storage is kept valid for at least the duration of the call.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the override has been set to the requested value, which
 *       docsis_GetMddIpModeOverride() then reports. On RETURN_ERR the override that is
 *       in force afterwards is not established by this interface, which says neither
 *       that the previous value survived nor that the requested one did not take effect.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The override was set.
 * @retval RETURN_ERR - The request did not succeed. An unrecognised value and a failed
 *         write are reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. Because the resulting override is unspecified, the
 *         client determines it by reading docsis_GetMddIpModeOverride() back rather
 *         than assuming either value, and corrects an unrecognised mode string at
 *         the call site, which a retry cannot fix.
 *
 * @note "APM" and "DualStack" may be returned by docsis_GetMddIpModeOverride() on
 *       some implementations but cannot be set through this function.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetMddIpModeOverride
 */
INT docsis_SetMddIpModeOverride(CHAR *pValue); 

/**
 * @brief Reports the upstream channel identifier the modem is using within its MAC
 *        domain.
 *
 * The identifier scopes the upstream channel to the local MAC domain - the set of
 * devices sharing that domain's addressing and protocols - so it is how a caller
 * names the channel it is transmitting on.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post None. The call reports state and changes none.
 *
 * @return The channel identifier, in the range 0 to 255 that its `UINT8` return type
 *         admits. The value is a channel identifier and not a status code: this
 *         function returns no RETURN_OK or RETURN_ERR, and this interface defines no
 *         sentinel value within the range that would mark a failed read. A caller
 *         consequently cannot distinguish a failure from a genuine identifier through
 *         the return value alone, which `Internal Error Handling`
 *         in the HAL specification states explicitly; where that matters it corroborates
 *         the reading against docsis_GetUsStatus() or the surrounding registration
 *         state.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_SetUSChannelId, docsis_GetUsStatus
 */
UINT8 docsis_GetUSChannelId(void); 

/**
 * @brief Sets the upstream channel identifier the modem uses within its MAC domain.
 *
 * The write counterpart of docsis_GetUSChannelId(). It reports nothing, so a caller
 * that must confirm the change reads the value back - `Method Sequencing`
 * in the HAL specification names this write-then-read pattern for the interface's two
 * `void` setters. This interface also states no persistence guarantee for the value,
 * so it is re-applied after a restart if it must hold.
 *
 * @param[in] index - The channel identifier to use. The parameter is declared `INT`
 *                    while docsis_GetUSChannelId() reports the identifier as a
 *                    `UINT8`, so a caller should keep the value within 0 to 255 to be
 *                    readable back through the getter. This interface states no valid
 *                    range of its own, and no negative or out-of-range value is
 *                    documented as being rejected.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post This interface does not state whether the identifier was accepted: the
 *       function reports nothing, and no output parameter carries an outcome. A
 *       caller that needs to know reads docsis_GetUSChannelId() afterwards and
 *       compares. An invalid value is not documented as being reported at all.
 *
 * @note This function returns nothing, deliberately: there is no status to test and a
 *       caller must not infer success from the call having returned.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetUSChannelId
 */
void docsis_SetUSChannelId(INT index); 

/**
 * @brief Reports the primary downstream channel frequency currently in the modem's
 *        Low-Level Kernel Filtering table.
 *
 * The frequency the modem is tuned to for its primary downstream channel, read from
 * the LKF table that docsis_SetStartFreq() writes. A caller uses it to confirm the
 * tuning it asked for, or to record where the modem locked.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post None. The call reports state and changes none.
 *
 * @return The channel frequency in hertz, the unit this declaration states for it, as
 *         an unsigned long - the declared `ULONG` width is the only bound this interface
 *         places on the quantity. It is a frequency and not a status code: this function
 *         returns no RETURN_OK or RETURN_ERR, and this interface defines no sentinel -
 *         not even zero - for a failed read, a gap `Internal Error Handling`
 *         in the HAL specification states explicitly. docsis_GetDSChannel() reports the
 *         same channel's frequency as text in a unit this interface does not state, so
 *         it corroborates this value only for plausibility, not for equality.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_SetStartFreq, docsis_GetDSChannel
 */
ULONG docsis_GetDownFreq(void); 

/**
 * @brief Sets the primary downstream channel frequency in the modem's Low-Level
 *        Kernel Filtering table.
 *
 * Directs the modem to use the given frequency for its primary downstream channel.
 * Like docsis_SetUSChannelId() it reports nothing, so a caller confirms the change by
 * reading docsis_GetDownFreq() afterwards, and this interface states no persistence
 * guarantee for the value.
 *
 * @param[in] value - The frequency to use, in hertz. This interface states no valid
 *                    range for it and documents no rejection of an unsupported
 *                    frequency; the physical range is a property of the DOCSIS
 *                    generation and plant rather than of this declaration, and the
 *                    downstream channel record from docsis_GetDSChannel() is where a
 *                    caller sees what the modem actually locked to.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post This interface does not state whether the frequency was accepted: the
 *       function reports nothing and no output parameter carries an outcome. A caller
 *       that needs to know reads docsis_GetDownFreq() afterwards and compares.
 *
 * @note This function returns nothing, deliberately: there is no status to test and a
 *       caller must not infer success from the call having returned.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetDownFreq, docsis_GetDSChannel
 */
void docsis_SetStartFreq(ULONG value); 

/**
 * @brief Fills a caller-supplied array with DOCSIS event-log entries and reports how
 *        many it wrote.
 *
 * The modem's own event log is where the reason for a DOCSIS-side failure is
 * recorded, so this is the diagnostic counterpart to the interface's coarse return
 * codes. The caller sizes the array and the implementation fills what it has.
 *
 * @param[out] entryArray - Pointer to the first element of an array of at least `len`
 *                          `CMMGMT_CM_EventLogEntry_t` records, allocated and owned by
 *                          the caller; it must not be NULL. The implementation writes
 *                          as many elements as the return value reports and this
 *                          interface does not state that it touches the remainder, so
 *                          a caller must not read beyond the returned count. Nothing
 *                          here retains the pointer. Each record's `docsDevEvText`
 *                          member is an array of EVM_MAX_EVENT_TEXT (255) bytes, which
 *                          bounds reads of the description.
 * @param[in]  len        - The number of elements the array can hold, and therefore
 *                          the maximum number of entries the implementation may write.
 *                          It must not exceed the array's real element count - this
 *                          interface has no other way to learn the size, so an
 *                          overstated `len` is a caller defect that may overrun the
 *                          array. This interface states no upper bound on `len` and no
 *                          behaviour for a negative or zero value.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On a non-negative return, that many leading elements of the array have been
 *       written and the rest are not specified by this interface. There is no failure
 *       code to test, so nothing is guaranteed about the array's contents beyond the
 *       returned count.
 *
 * @return The number of log entries written into `entryArray`, which is at most `len`.
 *         This is a count and not a status code: the function returns no RETURN_OK,
 *         and zero is a meaningful answer meaning the log held nothing to report.
 *         `Internal Error Handling` in the HAL specification calls this the well-behaved
 *         member of the value-returning set for that reason. This interface does not
 *         state how a failed read is reported through the count, so a caller must not
 *         read zero as an error, and - since the return type is signed - must treat a
 *         negative value as a failure rather than as a count even though none is
 *         documented.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_EventLogEntry_t, docsis_ClearDocsisEventLog
 */
INT docsis_GetDocsisEventLogItems(CMMGMT_CM_EventLogEntry_t *entryArray, INT len);

/**
 * @brief Requests that the modem's DOCSIS event log be cleared.
 *
 * The request is passed on rather than carried out inline: the header describes the
 * clearing as asynchronous, likely by sending a message to a driver event handler.
 * The consequence for a caller is that the return value reports only that the request
 * was accepted, and this interface provides no completion signal and no notification
 * for the clearing itself.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the request has been accepted. This interface does not state
 *       when the log is actually empty, so a caller that must see the effect polls
 *       docsis_GetDocsisEventLogItems() rather than assuming the log is clear on
 *       return. On RETURN_ERR the log state is not specified.
 *
 * @returns The status of the request.
 * @retval RETURN_OK  - The clear request was accepted.
 * @retval RETURN_ERR - The request was not accepted, for example because the log
 *         clear entry could not be set.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. The call takes no arguments, so there is nothing at the
 *         call site to correct in response: a client that sees the failure persist
 *         treats the operation as unavailable on the platform rather than
 *         retrying indefinitely.
 *
 * @note Blocking: this function must not block and must not use blocking system
 *       calls - the obligation is stated on this declaration and repeated under
 *       `Blocking calls` in the HAL specification, and it is the reason the clearing is
 *       asynchronous. A caller may therefore call it from a context that cannot
 *       tolerate a wait.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetDocsisEventLogItems
 */
INT docsis_ClearDocsisEventLog(void);

/**
 * @brief Reads the cable modem's own DHCPv4 provisioning into a structure the caller
 *        supplies.
 *
 * Reports the modem's DOCSIS-side IPv4 lease and everything that came with it: the
 * address, subnet mask, gateway, TFTP and time servers, boot file name, time offset,
 * the lease, rebind and renew timers, the modem's MAC address and the DHCP process
 * status. This is the modem's own provisioning and not the LAN's; this interface
 * declares no setter for any of these values, so a caller that must change addressing
 * does so outside this interface.
 *
 * @param[out] pInfo - Pointer to a `CMMGMT_CM_DHCP_INFO` the caller has allocated and
 *                     continues to own; it must not be NULL. The implementation writes
 *                     through the pointer and allocates nothing - `Memory Model` ->
 *                     `Caller Responsibilities` in the HAL specification names this call
 *                     among those where the module allocates nothing on the caller's
 *                     behalf - and nothing here retains the pointer. The record's text
 *                     members are fixed arrays (256 bytes for the boot file name, 64
 *                     for the others), which bound what may be read from them; the
 *                     four address members are `ANSC_IPV4_ADDRESS` unions and are IPv4
 *                     only.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the members the implementation was able to read have been
 *       written; this interface does not state that the others are cleared, so an
 *       unset member is not meaningful. On RETURN_ERR nothing in the record may be
 *       relied on.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The record was populated and may be read.
 * @retval RETURN_ERR - Nothing may be read from `*pInfo`. A NULL pointer, a failed
 *         acquisition and an internal error are reported identically, so a caller
 *         cannot tell "no lease held" from "read failed" here; docsis_getCMStatus()
 *         is where the DHCP stage of bring-up is visible.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_DHCP_INFO, cm_hal_GetIPv6DHCPInfo, docsis_getCMStatus
 */
INT cm_hal_GetDHCPInfo(PCMMGMT_CM_DHCP_INFO pInfo);

/**
 * @brief Reads the cable modem's own DHCPv6 provisioning into a structure the caller
 *        supplies.
 *
 * The IPv6 counterpart of cm_hal_GetDHCPInfo(): the modem's IPv6 address, prefix,
 * router, TFTP and time servers, boot file name and the lease, rebind and renew
 * timers. The addresses here are text rather than the packed IPv4 form, because
 * `ANSC_IPV4_ADDRESS` covers IPv4 only.
 *
 * @param[out] pInfo - Pointer to a `CMMGMT_CM_IPV6DHCP_INFO` the caller has allocated
 *                     and continues to own; it must not be NULL. The implementation
 *                     writes through the pointer and allocates nothing, per
 *                     `Memory Model` -> `Caller Responsibilities`
 *                     in the HAL specification, and nothing here retains it. The
 *                     address members are 40-byte arrays and the boot file name is
 *                     256 bytes, which bound what may be read from them.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the members the implementation was able to read have been
 *       written; unset members are not specified. On RETURN_ERR nothing in the record
 *       may be relied on.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The record was populated and may be read.
 * @retval RETURN_ERR - Nothing may be read from `*pInfo`. A NULL pointer, a retrieval
 *         failure and data the implementation judged invalid are reported
 *         identically, and in particular a modem provisioned IPv4-only is not
 *         distinguished from a failed read; docsis_GetMddIpModeOverride() and
 *         docsis_getCMStatus() are where the provisioning mode is visible.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_CM_IPV6DHCP_INFO, cm_hal_GetDHCPInfo, docsis_GetMddIpModeOverride
 */
INT cm_hal_GetIPv6DHCPInfo(PCMMGMT_CM_IPV6DHCP_INFO pInfo);

/**
 * @brief Reads the list of Customer Premises Equipment devices the cable modem sees,
 *        together with their number.
 *
 * Each entry pairs a CPE's IP address with its MAC address, which is what a caller
 * needs to report or police the devices behind the modem. The mode argument selects
 * whether the modem is being asked about its router or its bridge deployment, and the
 * entry count is reported separately because it is the only bound on the list.
 *
 * @param[out] ppCPEList - Address of a single `PCMMGMT_DML_CPE_LIST` pointer
 *                         variable, which is one level of indirection more than the
 *                         structure itself; it must not be NULL. It designates the
 *                         list of `CMMGMT_DML_CPE_LIST` entries, whose valid length
 *                         is the value written to `InstanceNum`. Which side allocates
 *                         it is not settled by the declaration; see the allocation
 *                         note below before calling.
 * @param[out] InstanceNum - Caller-owned location for the number of CPE entries the
 *                         list describes. It must not be NULL. This is the only
 *                         element count this interface reports for the list, and a
 *                         caller must bound every read of the list by it. This
 *                         interface publishes no maximum CPE count, so no constant
 *                         here bounds the value.
 * @param[in]  LanMode - NUL-terminated mode string, "router" or "bridge", of at most
 *                         100 bytes, which is the bound both this header and
 *                         `Memory Model` -> `Caller Responsibilities`
 *                         in the HAL specification state; neither says whether the
 *                         terminator counts toward it, so a caller that is anywhere
 *                         near the limit stays within 99 characters plus the
 *                         terminator. The two documented values are far shorter.
 *                         The caller allocates the string and releases it; this
 *                         interface does not state whether the implementation copies
 *                         it or retains the pointer, so a caller keeps it valid for
 *                         at least the duration of the call and must not assume it
 *                         may be reused earlier. The declaration is `CHAR *` rather
 *                         than `const CHAR *`, so it does not prevent the
 *                         implementation from writing through it.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure,
 *      so a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*InstanceNum` holds the entry count and the list holds that
 *       many entries. On RETURN_ERR neither output is specified by this interface, so
 *       a caller must not assume either is unmodified and must not read the list.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The list and its count were produced and may be read subject
 *         to the count.
 * @retval RETURN_ERR - Nothing may be read. An invalid mode string, a NULL argument
 *         and a failed read are reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership, and the part of it this interface leaves
 *       unusable. The declared type is `PCMMGMT_DML_CPE_LIST *`, and this interface
 *       states that the caller allocates the list and the mode string and frees both
 *       afterwards; `Memory Model` -> `Caller Responsibilities`
 *       in the HAL specification records the same release obligation. Those two statements
 *       do not compose into a rule a caller can follow for more than one entry, and
 *       the gap is worth stating plainly rather than papering over: `InstanceNum` is
 *       an output, and this interface publishes no maximum CPE count, so a caller has
 *       no information from which to size storage of its own before the call. What
 *       the interface establishes is that the caller releases both parameters. What
 *       it does not establish is which side allocates the entries, how a caller would
 *       size its own allocation, or which release function matches the allocation. A
 *       caller settles all three with the vendor implementation before using this
 *       function, and in the meantime initialises `*ppCPEList` to NULL rather than
 *       passing an uninitialised pointer, because under the caller-allocated reading
 *       the implementation would write through whatever it holds.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CMMGMT_DML_CPE_LIST, cm_hal_GetDHCPInfo
 */
INT cm_hal_GetCPEList(PCMMGMT_DML_CPE_LIST *ppCPEList, ULONG *InstanceNum, CHAR *LanMode);

/**
 * @brief Reports the market region the modem is built for.
 *
 * The region decides plant-dependent behaviour a caller may have to branch on -
 * `Platform or Product Customization` in the HAL specification names this call as one
 * of the four runtime readings that express product variation in this interface, since
 * it declares no compile-time variant flags at all.
 *
 * @param[out] market - Caller-allocated character array of at least 100 bytes, into
 *                      which the implementation writes the region identifier as a
 *                      NUL-terminated string. Termination here is the requirement
 *                      `Memory Model` -> `Module Responsibilities`
 *                      in the HAL specification places on every string this module
 *                      produces, rather than something the declaration itself states,
 *                      and no length out-parameter accompanies the buffer, so the
 *                      terminator is the only extent a caller has. It must not be
 *                      NULL. The documented values are "US" for the United States and
 *                      "EURO" for Europe; this interface declares no enumeration for
 *                      them and does not state that the set is closed, so a caller
 *                      compares strings and tolerates a value it does not recognise.
 *                      The 100-byte minimum is the bound this interface fixes, and an
 *                      insufficient buffer is named on this declaration as a failure
 *                      cause.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the buffer holds a NUL-terminated region identifier, on the
 *       `Module Responsibilities` terms recorded on `market` above. On RETURN_ERR its
 *       content is not specified by this interface.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A region identifier was written and may be read.
 * @retval RETURN_ERR - No region may be read. A NULL pointer, a buffer smaller than
 *         100 bytes and a failed read are reported identically. `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements this call, so a client treats a persistent
 *         failure as "not reported on this platform" rather than as a fault.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetDOCSISInfo, cm_hal_get_DiplexerSettings
 */
INT cm_hal_GetMarket(CHAR *market);

/*
 * HTTP download prototypes. These eight declarations are a sequence rather than eight
 * independent calls, and `Method Sequencing` in the HAL specification states the order:
 * set the URL and filename, optionally select the network interface, initiate the
 * download, poll its status until it completes, then check reboot readiness and
 * reboot. Progress is polled through cm_hal_Get_HTTP_Download_Status() rather than
 * pushed - this interface declares no download notification - which is how a caller
 * avoids waiting inside a call for the duration of a firmware transfer. None of the
 * configured values is documented as surviving a reboot; `Persistence Model` states
 * that this interface guarantees no persistence.
 */

/**
 * @brief Records the URL and target filename for the next HTTP firmware download.
 *
 * The first step of the download sequence: it writes the download configuration the
 * modem will use, without starting a transfer. cm_hal_HTTP_Download() is what starts
 * one, and cm_hal_Get_HTTP_Download_Url() reads back what was recorded here.
 *
 * @param[in] pHttpUrl - NUL-terminated download URL the caller owns, for example
 *                       "https://ci.xconfds.coast.xcal.tv/featureControl/getSettings".
 *                       It must not be NULL. This interface states no maximum length
 *                       for the value written here, while
 *                       cm_hal_Get_HTTP_Download_Url() requires a 200-byte buffer to
 *                       read it back, so a caller that intends to read it back keeps
 *                       the URL within 200 bytes including its terminator.
 * @param[in] pfilename - NUL-terminated target filename the caller owns, for example
 *                       "CGM4331COM_DEV_23Q3_sprint_20230817053130sdy_GRT". It must
 *                       not be NULL, and the same 200-byte consideration applies.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 *      No download may be in progress: this declaration names an in-progress download
 *      as a cause of RETURN_ERR, and this interface offers no way to cancel one, so a
 *      caller polls cm_hal_Get_HTTP_Download_Status() until the previous transfer has
 *      finished before reconfiguring.
 * @post On RETURN_OK the configuration holds both values and
 *       cm_hal_Get_HTTP_Download_Url() reports them. On RETURN_ERR the configuration
 *       in force afterwards is not established by this interface: it may hold the
 *       previous values, or one of the two new ones, and nothing here says which.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - Both values were recorded.
 * @retval RETURN_ERR - The request did not succeed. A download already in progress and
 *         an invalid URL are the causes this declaration names, and they are not
 *         distinguished from each other.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. Because the resulting configuration is unspecified, the
 *         client determines it by reading cm_hal_Get_HTTP_Download_Url() back and
 *         reconfigures from what it finds rather than assuming either. An
 *         invalid `URL` is a call-site defect a retry cannot fix; a download already
 *         in progress is not, and is cleared by polling
 *         cm_hal_Get_HTTP_Download_Status() until it finishes.
 *
 * @note Both parameters are declared `char *` rather than `const char *`. This
 *       interface does not state whether an implementation writes through them, and
 *       the declaration does not prevent it, so a caller passes writable storage it
 *       owns rather than a string literal and must not assume the strings are
 *       unchanged on return. Nor does this interface state whether the implementation
 *       copies the strings or retains the pointers, so the storage is kept valid for
 *       at least the duration of the call.
 * @warning The URL is caller-supplied and this interface performs no validation a
 *          caller can rely on: it states only that an invalid URL may be reported as
 *          RETURN_ERR, and it does not define what makes one invalid. A caller is
 *          therefore responsible for the scheme, host and path it passes, and for the
 *          trust decision that follows from downloading firmware from them.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_HTTP_Download_Url, cm_hal_HTTP_Download, cm_hal_Set_HTTP_Download_Interface
 */
INT cm_hal_Set_HTTP_Download_Url(char *pHttpUrl, char *pfilename); 

/**
 * @brief Reads back the URL and target filename recorded for the HTTP download.
 *
 * Reports what cm_hal_Set_HTTP_Download_Url() last recorded, which is how a caller
 * confirms the configuration before starting a transfer.
 *
 * @param[out] pHttpUrl  - Caller-allocated buffer of at least 200 bytes for the URL;
 *                         it must not be NULL. The 200-byte minimum is stated by this
 *                         interface and repeated under `Memory Model` ->
 *                         `Caller Responsibilities` in the HAL specification, which
 *                         also records the consequence of ignoring it: an
 *                         insufficient buffer can lead to memory corruption, because
 *                         no truncation behaviour is specified that would make a
 *                         shorter buffer safe. The caller owns the storage throughout.
 * @param[out] pfilename - Caller-allocated buffer of at least 200 bytes for the target
 *                         filename, on the same terms.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK both buffers hold NUL-terminated values, which is the requirement
 *       `Memory Model` -> `Module Responsibilities` in the HAL specification places on
 *       every string this module produces; no length out-parameter accompanies either
 *       buffer, so the terminator is the only extent a caller has. On RETURN_ERR their
 *       contents are not specified by this interface, so a caller must not assume
 *       either is unmodified.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - Both values were written and may be read.
 * @retval RETURN_ERR - Neither buffer may be read. An empty configured URL is one of
 *         the causes this declaration names, so a caller that has not yet called
 *         cm_hal_Set_HTTP_Download_Url() should expect this rather than an empty
 *         string.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Set_HTTP_Download_Url
 */
INT cm_hal_Get_HTTP_Download_Url(char *pHttpUrl, char *pfilename);

/**
 * @brief Selects the network interface the HTTP download will use.
 *
 * An optional step of the download sequence: it names the interface over which the
 * transfer is made, which matters where the modem's own WAN interface and the
 * eRouter's differ in routing or policy.
 *
 * @param[in] interface - Interface selector. The two values this interface defines are
 *                        0 for `wan0` and 1 for `erouter0`; no other value is defined,
 *                        and this declaration names an invalid selector as a cause of
 *                        RETURN_ERR. The parameter is `unsigned int`, so its width
 *                        admits values this interface gives no meaning to and a caller
 *                        must not pass one expecting a default.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the selection is recorded and
 *       cm_hal_Get_HTTP_Download_Interface() reports it. On RETURN_ERR the selection in
 *       force afterwards is not established by this interface, which says neither that
 *       the previous one survived nor that the requested one failed. Nor does it state
 *       what interface is used when the selection was never made.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The interface was selected.
 * @retval RETURN_ERR - The request did not succeed; an invalid selector is the
 *         cause this declaration names.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. Because the resulting selection is unspecified, the
 *         client determines it by reading cm_hal_Get_HTTP_Download_Interface() back
 *         rather than assuming either value, and corrects a selector outside
 *         {0, 1} at the call site, which a retry cannot fix, before starting a
 *         download.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_HTTP_Download_Interface, cm_hal_HTTP_Download
 */
INT cm_hal_Set_HTTP_Download_Interface(unsigned int interface); 

/**
 * @brief Reads back the network interface selected for the HTTP download.
 *
 * Reports what cm_hal_Set_HTTP_Download_Interface() last recorded.
 *
 * @param[out] pinterface - Pointer to a caller-owned `unsigned int` that receives the
 *                          selector: 0 for `wan0` or 1 for `erouter0`. It must not be
 *                          NULL. This interface defines no other value and does not
 *                          state what is reported when no selection has been made, so
 *                          a caller must not read an unexpected value as a default.
 *                          The caller owns the storage and nothing here retains the
 *                          pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*pinterface` holds the selector. On RETURN_ERR its content is
 *       not specified by this interface.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The selector was written and may be read.
 * @retval RETURN_ERR - No selector may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Set_HTTP_Download_Interface
 */
INT cm_hal_Get_HTTP_Download_Interface(unsigned int *pinterface); 

/**
 * @brief Starts the HTTP firmware download using the recorded configuration.
 *
 * The transfer itself is not carried out inside this call: it starts one and returns,
 * and a caller follows its progress with cm_hal_Get_HTTP_Download_Status(). That
 * split is deliberate - `Blocking calls` in the HAL specification names it as this
 * interface's answer to not blocking for the duration of a firmware transfer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 *      cm_hal_Set_HTTP_Download_Url() has recorded a URL and filename, and no download
 *      is already in progress; this declaration names an in-progress download as a
 *      cause of RETURN_ERR. This interface provides no way to cancel a running
 *      download.
 * @post On RETURN_OK a download has been started and its progress is readable through
 *       cm_hal_Get_HTTP_Download_Status(). Completion is not signalled by this call
 *       and this interface declares no notification for it. On RETURN_ERR this
 *       interface does not state whether a transfer is running, so a caller polls the
 *       status rather than assuming either that one started or that none did.
 *
 * @returns The status of the request to start.
 * @retval RETURN_OK  - A download was started. It reports acceptance of the request,
 *         not a completed transfer.
 * @retval RETURN_ERR - The request to start was not accepted; a download already in
 *         progress is the cause this declaration names.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. The call takes no arguments, so there is nothing at the
 *         call site to correct in response, and because a transfer may nonetheless be
 *         running, the client polls cm_hal_Get_HTTP_Download_Status() to establish
 *         what the download state actually is before retrying.
 *
 * @note The declaration takes an empty parameter list rather than `(void)`, so in C
 *       it does not prototype its arguments; a caller passes none.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Set_HTTP_Download_Url, cm_hal_Get_HTTP_Download_Status, cm_hal_HTTP_LED_Flash
 */
INT cm_hal_HTTP_Download();

/**
 * @brief Reports the progress or failure of the HTTP firmware download.
 *
 * This is the interface's whole progress mechanism - `Asynchronous Notification Model`
 * in the HAL specification records that download progress is polled here and not
 * pushed - so a caller calls it repeatedly after cm_hal_HTTP_Download() until it
 * reports completion or a failure.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 *      A download has been started with cm_hal_HTTP_Download(); this interface does not
 *      state what is reported before one has been.
 * @post None. The call reports state and changes none.
 *
 * @returns A progress or failure value rather than a status code, reported as a plain
 *          `INT`: this interface declares no enumeration for it. A caller must not
 *          compare the result against RETURN_OK or RETURN_ERR - RETURN_OK is 0, which
 *          here means "download not started", so the comparison would read a failure
 *          as success. Two of the outcomes this interface defines are bands rather
 *          than single codes, and are documented here for that reason: 1 to 99 is a
 *          download in progress, as a percentage completed, and a client keeps
 *          polling; 403 to 407 each report a failed download protection check -
 *          respectively HW_Type, HW Mask, DL Rev, DL Header and DL CVC - which a
 *          client resolves by staging an image that matches the device rather than by
 *          retrying the same one. The discrete codes are documented individually
 *          below. Any other value is outside the domain this interface defines, and a
 *          caller treats one as a failure rather than as progress.
 * @retval 0 No download has been started, or the previous one left no state to report. A
 *           client that expected a transfer to be running re-checks its configuration and
 *           starts one.
 * @retval 100 The image has been fetched and staged and the modem is waiting for a reboot.
 *             A client checks cm_hal_Reboot_Ready() and then calls
 *             cm_hal_HTTP_Download_Reboot_Now() when it is willing to lose service.
 * @retval 400 The configured URL is not a valid HTTP server URL. A client corrects it with
 *             cm_hal_Set_HTTP_Download_Url(); retrying without changing it fails again.
 * @retval 401 The server could not be reached. A client may retry later, and checks WAN
 *             connectivity and the configured download interface before doing so.
 * @retval 402 The server does not have the requested file. A client corrects the filename
 *             with cm_hal_Set_HTTP_Download_Url().
 * @retval 500 The download failed for a reason this interface does not enumerate. A client
 *             logs it and consults `cm_vendor_hal.log`; this interface does not state
 *             whether it is transient, so a bounded retry is reasonable but not guaranteed
 *             to succeed.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_HTTP_Download, cm_hal_Reboot_Ready, cm_hal_HTTP_Download_Reboot_Now
 */
INT cm_hal_Get_HTTP_Download_Status(); 

/**
 * @brief Reports whether the device is ready to be rebooted.
 *
 * The check a caller makes after a download has completed and before asking for the
 * reboot that installs it, so that the reboot is not requested while the device is
 * mid-operation.
 *
 * @param[out] pValue - Pointer to a caller-owned `ULONG` that receives the readiness
 *                      indication: 1 means the device is ready for reboot and 2 means
 *                      it is not. It must not be NULL. Note that the values are 1 and
 *                      2 rather than a boolean 1 and 0, so a caller must test for the
 *                      specific values and must not read zero as "not ready"; this
 *                      interface gives no meaning to any other value. The caller owns
 *                      the storage and nothing here retains the pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*pValue` holds 1 or 2. On RETURN_ERR its content is not
 *       specified by this interface, so a caller must not assume it is unmodified and
 *       must not proceed to reboot on the strength of it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The readiness indication was written and may be read.
 * @retval RETURN_ERR - No indication may be read; a failed status retrieval or
 *         resource access is the cause this declaration names.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_HTTP_Download_Reboot_Now, cm_hal_Get_HTTP_Download_Status
 */
INT cm_hal_Reboot_Ready(ULONG *pValue);

/**
 * @brief Reboots the device, recording the reboot first.
 *
 * The step that installs a completed firmware download. Before triggering the reboot
 * the implementation creates a reboot file and updates the reboot counter, which is
 * what makes the reboot visible afterwards through the reset counters this interface
 * exposes.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 *      cm_hal_Reboot_Ready() reports that the device is ready (the value 1), and
 *      normally cm_hal_Get_HTTP_Download_Status() has reported 100. This declaration
 *      names a reboot already in progress as a failure cause, so a caller does not
 *      call it twice.
 * @post On success the device reboots, so no code after the call in the caller's
 *       process can be relied on to run: a caller must not treat the return as a
 *       report of a completed reboot, and must complete anything that has to survive
 *       before calling. On RETURN_ERR this interface does not establish how far the
 *       sequence progressed - the reboot file and the reboot counter may already have
 *       been updated - so a caller reads the state back rather than assuming none ran.
 *
 * @returns The status of the request.
 * @retval RETURN_OK  - The reboot was requested successfully.
 * @retval RETURN_ERR - The request did not succeed, because the reboot file could not
 *         be created or the reboot itself failed. How much of the sequence ran is not
 *         reported, so a partly completed reboot cannot be told from one that never
 *         started. The reason is not reported either: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. The call takes no arguments, so there is nothing at the
 *         call site to correct: a client re-checks cm_hal_Reboot_Ready() and reads
 *         cm_hal_Get_CableModemResetCount() to see what happened before retrying.
 *
 * @note The declaration takes an empty parameter list rather than `(void)`, so in C
 *       it does not prototype its arguments; a caller passes none.
 * @note Blocking: this call ends with a device reboot, so the usual expectation that a
 *       call returns promptly is beside the point - a caller should treat it as the
 *       last thing it does. `Blocking calls` in the HAL specification states no numeric
 *       timeout for it.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Reboot_Ready, cm_hal_FWupdateAndFactoryReset, cm_hal_Get_CableModemResetCount
 */
INT cm_hal_HTTP_Download_Reboot_Now();

/**
 * @brief Updates the device firmware and then resets it to factory defaults.
 *
 * The most destructive operation in this interface: it replaces the firmware and
 * discards the device's configuration in one step. It is a recovery action rather than
 * a power or provisioning control - `Power Management Requirements`
 * in the HAL specification makes that distinction explicitly.
 *
 * @param[in] pUrl - NUL-terminated URL of the firmware image the caller owns, for
 *                   example
 *                   "https://ci.xconfds.coast.xcal.tv/featureControl/getSettings".
 *                   This interface does not state whether the parameter may be NULL or
 *                   empty to mean "use the configured URL", so a caller supplies both
 *                   values explicitly rather than relying on a default. No maximum
 *                   length is stated.
 * @param[in] pImagename - NUL-terminated firmware image filename the caller owns, for
 *                   example "CGM4331COM_DEV_23Q3_sprint_20230817053130sdy_GRT". The
 *                   same considerations apply.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 *      No reboot is already in progress; this declaration names that as a failure
 *      cause.
 * @post On success the firmware is updated and the device is reset to factory
 *       defaults, which reboots it: as with cm_hal_HTTP_Download_Reboot_Now() a caller
 *       must not expect to run afterwards, and any state it needs must be preserved
 *       before the call. On RETURN_ERR this interface does not state how far the
 *       operation progressed, so a caller must not assume the firmware and
 *       configuration are untouched.
 *
 * @returns The status of the request.
 * @retval RETURN_OK  - The update and reset were requested successfully.
 * @retval RETURN_ERR - The request did not succeed; a reboot already in progress
 *         is the cause this declaration names, among others it does not distinguish.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. How far the operation progressed is not reported either,
 *         so a failure here does not mean the firmware and the configuration are as
 *         they were: the client establishes the device's actual firmware version and
 *         configuration state - docsis_GetDOCSISInfo() reports the firmware core
 *         version, and cm_hal_Get_CableModemResetCount() the resets - before acting,
 *         and does not simply repeat the call.
 *
 * @note Both parameters are declared `char *` rather than `const char *`, so the
 *       declaration does not prevent the implementation from writing through them; a
 *       caller passes writable storage it owns and must not assume the strings are
 *       unchanged on return. This interface does not state whether they are copied or
 *       retained.
 * @note Blocking: this call ends with a firmware update and a factory reset, which
 *       reboot the device, so - as with cm_hal_HTTP_Download_Reboot_Now() - the
 *       expectation under `Blocking calls` in the HAL specification that a call returns
 *       promptly does not usefully bound it, and no numeric timeout is specified. A
 *       caller treats it as the last thing it does.
 * @warning The image URL and name are caller-supplied and this interface specifies no
 *          validation a caller can rely on. The consequence is more severe than for
 *          cm_hal_Set_HTTP_Download_Url(), because this call installs the image and
 *          then discards the configuration that could have been used to recover: the
 *          trust decision about the source is entirely the caller's.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_HTTP_Download, cm_hal_HTTP_Download_Reboot_Now
 */
INT cm_hal_FWupdateAndFactoryReset(char *pUrl, char *pImagename);

/**
 * @brief Re-initialises the cable modem's MAC layer, keeping its channels.
 *
 * The least disruptive of this interface's three recovery actions: the MAC layer is
 * brought back up while the existing downstream and upstream channels are maintained,
 * so the modem does not have to rescan. cm_hal_set_ReinitMacThreshold() configures
 * when the modem does this by itself.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the MAC layer has been re-initialised and the existing DS and US
 *       channels are maintained. This interface does not state how long the modem
 *       takes to become usable again afterwards, nor whether any value written through
 *       this interface is preserved across it, so a caller re-reads the state it cares
 *       about rather than assuming continuity. On RETURN_ERR the state the MAC layer is
 *       left in is not specified by this interface.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The MAC layer was re-initialised.
 * @retval RETURN_ERR - The request did not succeed; a failure to lock or unlock the
 *         modem is the cause this declaration names. Whether the MAC layer was left
 *         untouched, partly re-initialised or locked is not reported.
 *         The reason is not reported either: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. The call takes no arguments, so there is nothing at the
 *         call site to correct in response: a client reads docsis_getCMStatus() and
 *         cm_hal_Get_DocsisResetCount() to establish where the modem actually is
 *         before deciding, rather than retrying indefinitely.
 *
 * @note The declaration takes an empty parameter list rather than `(void)`, so in C
 *       it does not prototype its arguments; a caller passes none.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_set_ReinitMacThreshold, cm_hal_get_ReinitMacThreshold, cm_hal_Get_DocsisResetCount
 */
INT cm_hal_ReinitMac(); 

/**
 * @brief Reports how the `wan0` interface obtained its IP configuration.
 *
 * Says whether the modem's WAN address came from DHCP or was configured statically,
 * which is the provisioning fact a caller needs when interpreting the addressing
 * reported by cm_hal_GetDHCPInfo().
 *
 * @param[out] pValue - Pointer to caller-owned character storage that receives the
 *                      provisioning type; it must not be NULL. The documented values
 *                      are "DHCP" and "STATIC", or their numeric equivalents - this
 *                      declaration states both forms and does not say which an
 *                      implementation uses, so a caller must be prepared for either
 *                      and cannot assume a NUL-terminated string. This interface fixes
 *                      no buffer size for the parameter either, which is a gap a
 *                      caller cannot close from the header: the safe course is to
 *                      supply storage large enough for the longer text form including
 *                      its terminator and to agree the actual form with the vendor
 *                      implementation. The caller owns the storage throughout.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the provisioning type has been written. On RETURN_ERR the content
 *       is not specified by this interface, so a caller must not assume it is
 *       unmodified.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The provisioning type was written and may be read.
 * @retval RETURN_ERR - Nothing may be read; a NULL pointer and a failed read are
 *         reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_GetDHCPInfo, docsis_GetMddIpModeOverride
 */
INT docsis_GetProvIpType(CHAR *pValue); 

/**
 * @brief Reports the file path at which the modem's DOCSIS certificate is stored.
 *
 * The certificate is what the modem authenticates itself with during BPI
 * establishment, and this call reports where it lives rather than its contents -
 * nothing in this interface reads the certificate itself.
 *
 * @param[out] pCert - Pointer to caller-owned character storage that receives the
 *                     path as a NUL-terminated string, for example
 *                     "/nvram/cmcert.bin". Termination here is the requirement
 *                     `Memory Model` -> `Module Responsibilities`
 *                     in the HAL specification places on every string this module
 *                     produces, rather than something the declaration itself states,
 *                     and no length out-parameter accompanies the buffer, so the
 *                     terminator is the only extent a caller has. It must not be
 *                     NULL. This interface fixes no minimum size for the buffer and
 *                     states no maximum path length, so a caller cannot derive a safe
 *                     size from this header: it should supply storage sufficient for
 *                     the platform's longest path including its terminator, and no
 *                     truncation behaviour is specified that would make a short
 *                     buffer safe. The caller owns the storage throughout.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the buffer holds the path. On RETURN_ERR its content is not
 *       specified by this interface.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - A path was written and may be read.
 * @retval RETURN_ERR - No path may be read. A NULL pointer, a failed read and a file
 *         access problem are reported identically, so a caller cannot tell "no
 *         certificate installed" from "could not look" through this call.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 *
 * @warning The value is a file path the caller may go on to open. This interface
 *          states nothing about its provenance or its contents, so a caller that acts
 *          on it treats it as data from the implementation rather than as a trusted
 *          constant, and must not assume the file exists or is readable.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetCertStatus
 */
INT docsis_GetCert(CHAR *pCert); 

/**
 * @brief Reports whether the modem's DOCSIS certificate is enabled.
 *
 * The companion to docsis_GetCert(): where that reports where the certificate lives,
 * this reports whether it is in use.
 *
 * @param[out] pVal - Pointer to a caller-owned `ULONG` that receives the state: 0 for
 *                    disabled and 1 for enabled. It must not be NULL. This interface
 *                    gives no meaning to any other value, and does not state what is
 *                    reported when no certificate is installed. The caller owns the
 *                    storage and nothing here retains the pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*pVal` holds 0 or 1. On RETURN_ERR its content is not specified
 *       by this interface, so a caller must not read it as "disabled".
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The state was written and may be read.
 * @retval RETURN_ERR - No state may be read; a NULL pointer and an inability to reach
 *         the configuration are reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetCert
 */
INT docsis_GetCertStatus(ULONG *pVal); 

/**
 * @brief Reports how many times the cable modem has been reset.
 *
 * The total across every cause, which is the broadest of the four reset counters
 * this interface exposes; the three others attribute resets to a particular origin.
 *
 * The counters in this interface are the record of that history: this interface
 * declares no way to reset one, and no persistence guarantee for it, so a caller
 * compares successive readings rather than treating a value as absolute.
 *
 * @param[out] resetcnt - Pointer to a caller-owned `ULONG` that receives the count;
 *                        it must not be NULL. This interface states no upper bound on
 *                        the value and does not say whether the count survives a
 *                        reboot or wraps, so a caller must not infer either. The
 *                        caller owns the storage and nothing here retains the
 *                        pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*resetcnt` holds the count. On RETURN_ERR its content is not
 *       specified by this interface, so a caller must not read it as zero resets.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was written and may be read.
 * @retval RETURN_ERR - No count may be read. A NULL pointer and an inability to reach
 *         the reset data are reported identically, and `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements the reset counters, so a client treats a
 *         persistent failure as "not reported here" rather than as a fault.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_LocalResetCount, cm_hal_Get_DocsisResetCount, cm_hal_Get_ErouterResetCount
 */
INT cm_hal_Get_CableModemResetCount(ULONG *resetcnt); 

/**
 * @brief Reports how many times the cable modem has been reset locally.
 *
 * A local reset is one initiated on the device itself - by a user action or a
 * software command - rather than by the DOCSIS network.
 *
 * The counters in this interface are the record of that history: this interface
 * declares no way to reset one, and no persistence guarantee for it, so a caller
 * compares successive readings rather than treating a value as absolute.
 *
 * @param[out] resetcnt - Pointer to a caller-owned `ULONG` that receives the count;
 *                        it must not be NULL. This interface states no upper bound on
 *                        the value and does not say whether the count survives a
 *                        reboot or wraps, so a caller must not infer either. The
 *                        caller owns the storage and nothing here retains the
 *                        pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*resetcnt` holds the count. On RETURN_ERR its content is not
 *       specified by this interface, so a caller must not read it as zero resets.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was written and may be read.
 * @retval RETURN_ERR - No count may be read. A NULL pointer and an inability to reach
 *         the reset data are reported identically, and `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements the reset counters, so a client treats a
 *         persistent failure as "not reported here" rather than as a fault.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_CableModemResetCount, cm_hal_Get_DocsisResetCount
 */
INT cm_hal_Get_LocalResetCount(ULONG *resetcnt); 

/**
 * @brief Reports how many times the cable modem has been reset by a DOCSIS event.
 *
 * These are the resets attributable to DOCSIS operations rather than to a local
 * action, so the counter is where a caller sees network-driven instability;
 * cm_hal_ReinitMac() and the threshold that automates it are the related controls.
 *
 * The counters in this interface are the record of that history: this interface
 * declares no way to reset one, and no persistence guarantee for it, so a caller
 * compares successive readings rather than treating a value as absolute.
 *
 * @param[out] resetcnt - Pointer to a caller-owned `ULONG` that receives the count;
 *                        it must not be NULL. This interface states no upper bound on
 *                        the value and does not say whether the count survives a
 *                        reboot or wraps, so a caller must not infer either. The
 *                        caller owns the storage and nothing here retains the
 *                        pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*resetcnt` holds the count. On RETURN_ERR its content is not
 *       specified by this interface, so a caller must not read it as zero resets.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was written and may be read.
 * @retval RETURN_ERR - No count may be read. A NULL pointer and an inability to reach
 *         the reset data are reported identically, and `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements the reset counters, so a client treats a
 *         persistent failure as "not reported here" rather than as a fault.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_CableModemResetCount, cm_hal_ReinitMac, cm_hal_set_ReinitMacThreshold
 */
INT cm_hal_Get_DocsisResetCount(ULONG *resetcnt); 

/**
 * @brief Reports how many times the eRouter component has been reset.
 *
 * The eRouter is the routing function beside the modem's DOCSIS side, so this
 * counter is about that component rather than about the modem's MAC layer.
 *
 * The counters in this interface are the record of that history: this interface
 * declares no way to reset one, and no persistence guarantee for it, so a caller
 * compares successive readings rather than treating a value as absolute.
 *
 * @param[out] resetcnt - Pointer to a caller-owned `ULONG` that receives the count;
 *                        it must not be NULL. This interface states no upper bound on
 *                        the value and does not say whether the count survives a
 *                        reboot or wraps, so a caller must not infer either. The
 *                        caller owns the storage and nothing here retains the
 *                        pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*resetcnt` holds the count. On RETURN_ERR its content is not
 *       specified by this interface, so a caller must not read it as zero resets.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The count was written and may be read.
 * @retval RETURN_ERR - No count may be read. A NULL pointer and an inability to reach
 *         the reset data are reported identically, and `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements the reset counters, so a client treats a
 *         persistent failure as "not reported here" rather than as a fault.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_Get_CableModemResetCount, cm_hal_Get_LocalResetCount
 */
INT cm_hal_Get_ErouterResetCount(ULONG *resetcnt); 

/**
 * @brief Starts or stops flashing the HTTP download LED.
 *
 * The device's user-visible indication that a firmware download is under way. It is a
 * presentation control and nothing else: it neither starts nor observes a download,
 * and this interface does not tie its state to the download the status call reports.
 *
 * @param[in] LedFlash - `BOOLEAN` request: TRUE (1) to flash the LED, FALSE (0) to
 *                       stop. This interface defines no other value for the parameter
 *                       and states no behaviour for one.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the LED is flashing or not as requested. RETURN_ERR reports only
 *       that the request was not carried out: this interface does not establish what
 *       state the LED is left in after a failure, and there is no getter with which to
 *       read the LED state back.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The request was carried out.
 *
 * RETURN_OK is the **only** value this interface documents for this function. It does not
 * state that the call cannot fail, and `RETURN_ERR` remains defined for the interface as a
 * whole, so a caller should still test the result rather than discard it; but there is no
 * documented failure to distinguish. The reason is not reported and there is no getter for
 * the LED state, so a client has the vendor log `cm_vendor_hal.log` under `/rdklogs/logs/`
 * as its only evidence, must not infer the indicator's state from the call, and - the LED
 * being cosmetic, with no other operation depending on it - can only log and carry on.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_HTTP_Download, cm_hal_Get_HTTP_Download_Status
 */
INT cm_hal_HTTP_LED_Flash(BOOLEAN LedFlash);

/**
 * @brief Reads the DOCSIS 3.1 downstream OFDM channel table into an array the
 *        implementation allocates.
 *
 * Reports, per OFDM downstream channel, the channel identifier and role, the
 * subcarrier layout and spacing, the cyclic prefix and roll-off period, the PLC
 * frequency, the pilot and interleaver parameters, the average SNR and power level,
 * and the PLC and NCP codeword counters. This is the DOCSIS 3.1 counterpart of
 * docsis_GetDSChannel(), which reports a single pre-3.1 downstream channel.
 * @note For detailed information on DOCSIS 3.1 refer to the specifications cited at
 *       the top of this file.
 *
 * @param[out] ppinfo - Address of a single `PDOCSIF31_CM_DS_OFDM_CHAN` pointer
 *                      variable, which is one level of indirection more than the
 *                      structure itself; it must not be NULL. On success the
 *                      implementation stores in `*ppinfo` the address of an array of
 *                      `DOCSIF31_CM_DS_OFDM_CHAN` records it has allocated, and the
 *                      caller owns that storage from then on. The array holds exactly
 *                      as many consecutive records as `output_NumberOfEntries`
 *                      reports, and a caller must bound every read by that count
 *                      rather than by any constant in this header.
 * @param[out] output_NumberOfEntries - Caller-owned location for the number of records
 *                      in the array. It must not be NULL. Zero is a meaningful answer
 *                      rather than a failure: `Optional Components`
 *                      in the HAL specification records that a modem on an earlier
 *                      DOCSIS generation has no such channel to report and that this
 *                      interface defines no distinct code for that case.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*ppinfo` addresses an array of `*output_NumberOfEntries`
 *       records that the caller must later release. On RETURN_ERR neither output is
 *       specified by this interface, so a caller must not assume either is
 *       unmodified, must not read the array and has no stated basis for releasing it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The table was allocated and populated; the caller reads it
 *         subject to the reported count and is responsible for releasing it.
 * @retval RETURN_ERR - No table may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership. This is one of the five calls in this interface
 *       that hand back memory the caller must release: the caller deallocates
 *       `ppinfo`, and `Memory Model` -> `Caller Responsibilities`
 *       in the HAL specification names docsis_GetDsOfdmChanTable() among the three
 *       table calls that return a dynamically allocated array whose length they
 *       report through their entry-count argument. The interface names
 *       neither the allocator nor the release function that matches it, and does not
 *       state whether the outputs are written on failure, so a caller agrees the
 *       release convention with the vendor implementation and initialises `*ppinfo`
 *       to NULL before the call.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see DOCSIF31_CM_DS_OFDM_CHAN, docsis_GetUsOfdmaChanTable, docsis_GetStatusOfdmaUsTable
 */
INT docsis_GetDsOfdmChanTable(PDOCSIF31_CM_DS_OFDM_CHAN *ppinfo, int *output_NumberOfEntries);

/**
 * @brief Reads the DOCSIS 3.1 upstream OFDMA channel table into an array the
 *        implementation allocates.
 *
 * Reports, per OFDMA upstream channel, the channel identifier, the UCD
 * configuration change count, the subcarrier layout and spacing, the cyclic prefix
 * and roll-off period, the symbols per frame, the transmit power and whether
 * pre-equalisation is enabled. docsis_GetStatusOfdmaUsTable() reports the ranging and
 * timeout status of the same channels.
 * @note For detailed information on DOCSIS 3.1 refer to the specifications cited at
 *       the top of this file.
 *
 * @param[out] ppinfo - Address of a single `PDOCSIF31_CM_US_OFDMA_CHAN` pointer
 *                      variable, which is one level of indirection more than the
 *                      structure itself; it must not be NULL. On success the
 *                      implementation stores in `*ppinfo` the address of an array of
 *                      `DOCSIF31_CM_US_OFDMA_CHAN` records it has allocated, and the
 *                      caller owns that storage from then on. The array holds exactly
 *                      as many consecutive records as `output_NumberOfEntries`
 *                      reports, and a caller must bound every read by that count
 *                      rather than by any constant in this header.
 * @param[out] output_NumberOfEntries - Caller-owned location for the number of records
 *                      in the array. It must not be NULL. Zero is a meaningful answer
 *                      rather than a failure: `Optional Components`
 *                      in the HAL specification records that a modem on an earlier
 *                      DOCSIS generation has no such channel to report and that this
 *                      interface defines no distinct code for that case.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*ppinfo` addresses an array of `*output_NumberOfEntries`
 *       records that the caller must later release. On RETURN_ERR neither output is
 *       specified by this interface, so a caller must not assume either is
 *       unmodified, must not read the array and has no stated basis for releasing it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The table was allocated and populated; the caller reads it
 *         subject to the reported count and is responsible for releasing it.
 * @retval RETURN_ERR - No table may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership. This is one of the five calls in this interface
 *       that hand back memory the caller must release: the caller deallocates
 *       `ppinfo`, and `Memory Model` -> `Caller Responsibilities`
 *       in the HAL specification names docsis_GetUsOfdmaChanTable() among the three
 *       table calls that return a dynamically allocated array whose length they
 *       report through their entry-count argument. The interface names
 *       neither the allocator nor the release function that matches it, and does not
 *       state whether the outputs are written on failure, so a caller agrees the
 *       release convention with the vendor implementation and initialises `*ppinfo`
 *       to NULL before the call.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see DOCSIF31_CM_US_OFDMA_CHAN, docsis_GetDsOfdmChanTable, docsis_GetStatusOfdmaUsTable
 */
INT docsis_GetUsOfdmaChanTable(PDOCSIF31_CM_US_OFDMA_CHAN *ppinfo, int *output_NumberOfEntries);

/**
 * @brief Reads the DOCSIS 3.1 upstream OFDMA channel status table into an array the
 *        implementation allocates.
 *
 * Reports, per OFDMA upstream channel, the T3 and T4 timeout counts, the aborted
 * ranging count, the excessive-T3 count, whether the channel is muted and its ranging
 * state. It is the health view of the channels docsis_GetUsOfdmaChanTable()
 * describes: the parameters come from there and the errors from here.
 * @note For detailed information on DOCSIS 3.1 refer to the specifications cited at
 *       the top of this file.
 *
 * @param[out] ppinfo - Address of a single `PDOCSIF31_CMSTATUSOFDMA_US` pointer
 *                      variable, which is one level of indirection more than the
 *                      structure itself; it must not be NULL. On success the
 *                      implementation stores in `*ppinfo` the address of an array of
 *                      `DOCSIF31_CMSTATUSOFDMA_US` records it has allocated, and the
 *                      caller owns that storage from then on. The array holds exactly
 *                      as many consecutive records as `output_NumberOfEntries`
 *                      reports, and a caller must bound every read by that count
 *                      rather than by any constant in this header.
 * @param[out] output_NumberOfEntries - Caller-owned location for the number of records
 *                      in the array. It must not be NULL. Zero is a meaningful answer
 *                      rather than a failure: `Optional Components`
 *                      in the HAL specification records that a modem on an earlier
 *                      DOCSIS generation has no such channel to report and that this
 *                      interface defines no distinct code for that case.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*ppinfo` addresses an array of `*output_NumberOfEntries`
 *       records that the caller must later release. On RETURN_ERR neither output is
 *       specified by this interface, so a caller must not assume either is
 *       unmodified, must not read the array and has no stated basis for releasing it.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The table was allocated and populated; the caller reads it
 *         subject to the reported count and is responsible for releasing it.
 * @retval RETURN_ERR - No table may be read.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly typed argument is a call-site defect that retrying cannot fix.
 *
 * @note Allocation and ownership. This is one of the five calls in this interface
 *       that hand back memory the caller must release: the caller deallocates
 *       `ppinfo`, and `Memory Model` -> `Caller Responsibilities`
 *       in the HAL specification names docsis_GetStatusOfdmaUsTable() among the three
 *       table calls that return a dynamically allocated array whose length they
 *       report through their entry-count argument. The interface names
 *       neither the allocator nor the release function that matches it, and does not
 *       state whether the outputs are written on failure, so a caller agrees the
 *       release convention with the vendor implementation and initialises `*ppinfo`
 *       to NULL before the call.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see DOCSIF31_CMSTATUSOFDMA_US, docsis_GetUsOfdmaChanTable, docsis_GetDsOfdmChanTable
 */
INT docsis_GetStatusOfdmaUsTable(PDOCSIF31_CMSTATUSOFDMA_US *ppinfo, int *output_NumberOfEntries);

/**
 * @brief Reports whether Low Latency DOCSIS is enabled in the modem's bootfile.
 *
 * LLD is provisioned by the operator through the modem's bootfile rather than
 * configured through this interface, so this call reads a provisioning outcome. There
 * is no setter for it here.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post None. The call reports state and changes none.
 *
 * @returns One of three values - ENABLE (1), DISABLE (0) or RETURN_ERR (-1) - which are
 *          macros rather than members of an enumeration, so a caller compares an `INT`
 *          against the macro names and cannot switch over a named type. The result is a
 *          three-way outcome and not a status code in the RETURN_OK sense: RETURN_OK is
 *          0, which is DISABLE here, so a caller that compares the result against
 *          RETURN_OK would read "disabled" as "succeeded".
 * @retval ENABLE LLD is enabled in the bootfile, so a client may rely on the low-latency
 *         service being configured.
 * @retval DISABLE LLD is disabled in the bootfile, or the entry is absent altogether. This
 *         interface does not distinguish the two, so a client cannot tell "provisioned
 *         off" from "not provisioned" - which `Optional Components`
 *         in the HAL specification states explicitly - and must not read DISABLE as an
 *         error.
 * @retval RETURN_ERR The status could not be retrieved - for example an unreadable bootfile
 *         or firmware that does not support the query. It is the only value that reports a
 *         failure, so a client tests for it specifically, logs it and consults
 *         `cm_vendor_hal.log`, and must not treat DISABLE as an error.
 *
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_GetDOCSISInfo
 */
INT docsis_LLDgetEnableStatus(); 

/**
 * @brief Installs the SNMPv3 kickstart security parameters the modem is to use.
 *
 * Kickstart is how an operator seeds SNMPv3 credentials into the modem so that
 * management traffic can be authenticated before any other configuration exists. The
 * table the caller supplies carries up to MAX_KICKSTART_ROWS (5) rows, each a security
 * name and a security number.
 *
 * @param[in] pKickstart_Table - Pointer to a caller-allocated `snmpv3_kickstart_table_t`
 *                       the caller continues to own; it must not be NULL. Its `n_rows`
 *                       member states how many of the `kickstart_values` entries are
 *                       populated, and the array holds at most MAX_KICKSTART_ROWS (5)
 *                       row pointers, so a caller must not report more rows than that
 *                       and the implementation must not read past `n_rows`. Each row
 *                       is itself a pointer the caller supplies, and each of its two
 *                       `fixed_length_buffer_t` members carries a `length` in bytes
 *                       and a pointer to that many bytes - the buffers are not
 *                       required to be NUL-terminated, since the length accompanies
 *                       them. This interface does not state whether the implementation
 *                       copies the table, the rows or the buffers, or retains any of
 *                       those pointers beyond the call, so a caller keeps all of them
 *                       valid for at least the duration of the call and must not
 *                       release or reuse them earlier on the strength of the call
 *                       having returned. Three allocations are involved and all three
 *                       are the caller's: the table, each row it points at, and the
 *                       bytes each row's two buffers address. Nothing here releases any
 *                       of them, and this interface declares no release function for
 *                       any of them, so the caller frees each with the allocator it
 *                       used. The per-member rules - the byte count as the buffers'
 *                       only extent, the 0 to MAX_KICKSTART_ROWS bound on `n_rows`,
 *                       what is left unstated about NULL members and about the entries
 *                       at or beyond `n_rows` - are stated on the members themselves:
 *                       see `length` and `buffer` of fixed_length_buffer_t,
 *                       `security_name` and `security_number` of
 *                       snmp_kickstart_row_t, and `n_rows` and `kickstart_values` of
 *                       snmpv3_kickstart_table_t.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the parameters have been installed. On RETURN_ERR this interface
 *       does not state whether some rows were installed before the failure, so a
 *       caller must not assume the operation was atomic. There is no getter with which
 *       to read the installed parameters back and no call that removes them.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The parameters were installed.
 * @retval RETURN_ERR - The request did not succeed. An invalid table and a failure
 *         inside the SNMPv3 setup are reported identically. `Optional Components`
 *         in the HAL specification records that this interface does not establish
 *         whether every platform implements this call.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. How much of the table was applied is not reported either,
 *         so the client must not read the failure as meaning that no row was installed,
 *         nor as meaning that all of them were. There is no getter with which to read
 *         the installed parameters back, so the vendor log is the only evidence of what
 *         took effect and an operator re-establishes the credentials deliberately. An
 *         invalid table is a call-site defect a retry cannot fix.
 *
 * @warning The two buffers of every row carry SNMPv3 credential material. This
 *          interface states nothing about how an implementation stores or logs them,
 *          so a caller must not assume they are protected once handed over, and must
 *          not place them anywhere it would not place a credential - in particular not
 *          in its own diagnostics. Nothing in this interface returns them, which is
 *          the one property a caller can rely on.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see snmpv3_kickstart_table_t, snmp_kickstart_row_t, fixed_length_buffer_t
 */
INT cm_hal_snmpv3_kickstart_initialize(snmpv3_kickstart_table_t *pKickstart_Table);

/**
 * @brief Reports whether DOCSIS energy is present on the cable interface.
 *
 * Energy detection is the coarsest possible answer to "is this modem connected to a
 * plant": it says whether a DOCSIS signal is present at all, independently of whether
 * the modem has ranged, registered or obtained a lease. docsis_getCMStatus() is where
 * the progression beyond that is visible.
 *
 * @param[out] pEnergyDetected - Pointer to a caller-owned `BOOLEAN` that receives the
 *                       result: FALSE (0) when no DOCSIS energy is detected, meaning
 *                       no connection, and TRUE (1) when it is. It must not be NULL.
 *                       The caller owns the storage and nothing here retains the
 *                       pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*pEnergyDetected` holds TRUE or FALSE. On RETURN_ERR its content
 *       is not specified by this interface, so a caller must not read it as "no
 *       energy" - that is the reading most likely to be mistaken for a real answer.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The detection result was written and may be read.
 * @retval RETURN_ERR - No result may be read. A NULL pointer and firmware that does
 *         not support detection are reported identically, so a client that sees a
 *         persistent failure treats energy detection as unavailable on the platform
 *         rather than as "no signal".
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see docsis_getCMStatus, docsis_GetDOCSISInfo
 */
INT docsis_IsEnergyDetected(BOOLEAN *pEnergyDetected); 

/**
 * @brief Sets the threshold at which the modem re-initialises its MAC layer by itself.
 *
 * The automatic counterpart of cm_hal_ReinitMac(): once the modem's internal count of
 * the condition being watched reaches this threshold, the MAC layer is re-initialised
 * without a caller asking. It is one of the few values this interface lets a caller
 * write, and `Persistence Model` in the HAL specification states no persistence
 * guarantee for it, so it is re-applied after a restart if it must hold.
 *
 * @param[in] value - The threshold to apply. This interface states no valid range for
 *                    it beyond the width of `ULONG`, states no unit or event that the
 *                    threshold counts, and does not define the effect of zero. A caller
 *                    that needs those meanings must obtain them from the vendor
 *                    implementation; this header does not establish them, and the
 *                    declaration alone does not make the value safe to choose blindly.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK the threshold has been set and cm_hal_get_ReinitMacThreshold()
 *       reports it. On RETURN_ERR the threshold in force afterwards is not established
 *       by this interface: neither the previous value nor the requested one is assured.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The threshold was set.
 * @retval RETURN_ERR - The request did not succeed; a rejected value, a validation
 *         failure and a configuration error are reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client looks for the cause in the vendor log `cm_vendor_hal.log` under
 *         `/rdklogs/logs/`. Because the resulting threshold is unspecified, the client
 *         determines it by reading cm_hal_get_ReinitMacThreshold() back rather than
 *         assuming either value, and corrects a value the implementation rejects at the
 *         call site, which a retry cannot fix, before relying on automatic MAC
 *         re-initialisation.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_get_ReinitMacThreshold, cm_hal_ReinitMac, cm_hal_Get_DocsisResetCount
 */
INT cm_hal_set_ReinitMacThreshold(ULONG value);

/**
 * @brief Reads the threshold at which the modem re-initialises its MAC layer by
 *        itself.
 *
 * Reports the value cm_hal_set_ReinitMacThreshold() last applied, which is how a
 * caller confirms a write - and, since this interface guarantees no persistence,
 * how it checks whether the value survived a restart.
 *
 * @param[out] pValue - Pointer to a caller-owned `ULONG` that receives the threshold;
 *                      it must not be NULL. This interface states no range for the
 *                      value and does not say what is reported when no threshold has
 *                      been set, so a caller must not read a particular value as
 *                      "unset". The caller owns the storage and nothing here retains
 *                      the pointer.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK `*pValue` holds the threshold. On RETURN_ERR its content is not
 *       specified by this interface.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The threshold was written and may be read.
 * @retval RETURN_ERR - No threshold may be read; a NULL pointer and a failed read are
 *         reported identically.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_set_ReinitMacThreshold
 */
INT cm_hal_get_ReinitMacThreshold(ULONG *pValue);

/**
 * @brief Reads the modem's current diplexer band-edge settings.
 *
 * A diplexer separates upstream from downstream by frequency, and its band edges
 * differ between regional plant designs - `Platform or Product Customization`
 * in the HAL specification names this call as one of the runtime readings that express
 * product variation in this interface. The whole diplexer facility is optional: a
 * vendor may ship it as a stub, as the registration function's documented failure case
 * records.
 *
 * @param[out] pValue - Pointer to a `CM_DIPLEXER_SETTINGS` the caller has allocated and
 *                      continues to own; it must not be NULL. The implementation writes
 *                      the upstream and downstream upper band edges, both in MHz,
 *                      through the pointer and allocates nothing; nothing here retains
 *                      the pointer. This interface states no valid range for either
 *                      value.
 *
 * @pre cm_hal_InitDB(), docsis_InitDS() and docsis_InitUS() have been called and
 *      cm_hal_InitDB() returned RETURN_OK; `Initialization and Startup`
 *      in the HAL specification makes that sequence mandatory before any other
 *      operation. No code distinguishes "not initialized" from any other failure, so
 *      a caller that skips it gets RETURN_ERR at best and undefined behaviour at
 *      worst, and must enforce the order itself.
 * @post On RETURN_OK both members of `*pValue` have been written. On RETURN_ERR their
 *       content is not specified by this interface, so a caller must not assume they
 *       are unmodified.
 *
 * @returns The status of the operation.
 * @retval RETURN_OK  - The settings were read and may be read out of the record.
 * @retval RETURN_ERR - Nothing may be read from `*pValue`. A NULL pointer, a failed
 *         read and a platform that does not implement the diplexer facility are
 *         reported identically, so a client treats a persistent failure as "not
 *         reported on this platform" rather than as a fault.
 *         The reason is not reported: `Internal Error Handling`
 *         in the HAL specification makes RETURN_ERR the only failure code, so a
 *         client discards the output, looks for the cause in the vendor log
 *         `cm_vendor_hal.log` under `/rdklogs/logs/`, and may retry. A NULL or
 *         wrongly sized argument is a call-site defect that retrying cannot fix.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see CM_DIPLEXER_SETTINGS, cm_hal_Register_DiplexerVariationCallback
 */
INT cm_hal_get_DiplexerSettings(CM_DIPLEXER_SETTINGS *pValue);

/**
 * @}
 */

/*
 * The callback typedef below is a data type, so the APIs group is closed here and the
 * types group reopened around it; the APIs group resumes after it. The markers are
 * Doxygen comments and change nothing about the declarations they surround.
 */

/**
 * @addtogroup CM_HAL_TYPES
 * @{
 */

/**
 * @brief Type of the handler the CM HAL invokes when the diplexer settings change.
 *
 * The one asynchronous notification this interface declares. A caller installs a
 * function of this type with cm_hal_Register_DiplexerVariationCallback(), and the
 * implementation invokes it when the modem's diplexer band edges change; the handler
 * is the only path by which this interface delivers anything to a caller outside a
 * call the caller made.
 *
 * @param[in] stCMDiplexerValue - The updated settings, passed **by value** rather than
 *                       by pointer, so there is no lifetime or ownership question about
 *                       the data: the handler receives its own copy of the upstream and
 *                       downstream upper band edges, both in MHz, and neither frees nor
 *                       retains anything. This interface states no valid range for
 *                       either member.
 *
 * @pre The handler has been installed by cm_hal_Register_DiplexerVariationCallback().
 *      There is no unregister call, so it must remain valid for the lifetime of the
 *      process, along with anything it touches.
 * @post Whatever the handler does with the settings is the caller's business; this
 *       interface does not act on the returned status, and does not state what an
 *       implementation does with a failure report.
 *
 * @returns The status of the handler's own processing.
 * @retval RETURN_OK  - The handler processed the settings change.
 * @retval RETURN_ERR - The handler could not process the change. This interface does
 *         not state whether an implementation retries, logs or ignores the report, so a
 *         handler must not rely on the value having any effect and should record the
 *         condition itself.
 *
 * @note Blocking: the handler must not block. `Blocking calls` in the HAL specification
 *       requires calls in this interface not to suspend their context, and the handler
 *       runs in a context belonging to the implementation, so it should record the new
 *       settings and return, leaving any lengthy reaction to the caller's own thread.
 * @warning Thread safety is the handler's own problem, and this is the sharpest
 *          consequence of it. **This interface does not specify which thread the
 *          handler is invoked on** - neither this typedef nor its registration
 *          function states it, which `Threading Model` in the HAL specification records
 *          - so a handler must serialise its own access to any caller state it
 *          touches, must not assume it runs on the registering thread, and must not
 *          assume it is invoked only once at a time.
 * @see cm_hal_Register_DiplexerVariationCallback, cm_hal_get_DiplexerSettings, CM_DIPLEXER_SETTINGS
 */
typedef INT (*cm_hal_DiplexerVariationCallback)(CM_DIPLEXER_SETTINGS stCMDiplexerValue);

/**
 * @}
 */

/**
 * @addtogroup CM_HAL_APIS
 * @{
 */

/**
 * @brief Installs the handler invoked when the modem's diplexer settings change.
 *
 * The only registration function in this interface, and it is one-way: there is no
 * unregister call, so a caller installs the handler once, during initialization, and it
 * remains installed for the lifetime of the process. `Asynchronous Notification Model`
 * in the HAL specification sets out the four properties of the notification this binds.
 *
 * @param[in] callback_proc - The handler to install, of type
 *                       `cm_hal_DiplexerVariationCallback`. It is invoked with a
 *                       `CM_DIPLEXER_SETTINGS` value carrying the updated band edges.
 *                       The implementation retains this function pointer indefinitely -
 *                       that is the point of the call - so the function and everything
 *                       it depends on must remain valid for the lifetime of the
 *                       process. This interface does not state whether NULL is accepted
 *                       as a way of clearing the registration, and there is no
 *                       unregister call, so a caller must not pass NULL expecting to
 *                       remove a handler. Nor does it state what happens if the
 *                       function is called twice: a caller must not rely on a second
 *                       registration either replacing the first or being rejected.
 *
 * @pre cm_hal_InitDB() has returned RETURN_OK. This declaration states that the
 *      callback "should be provided during initialization", and `Method Sequencing`
 *      in the HAL specification repeats it: because the registration cannot be undone,
 *      doing it late leaves a window in which changes are missed and no later call
 *      closes it.
 * @post On RETURN_OK the handler is installed permanently and may be invoked at any
 *       time thereafter, on a thread this interface does not specify. On RETURN_ERR this
 *       interface does not specify whether a handler is installed, so a caller must
 *       neither rely on being called nor assume it never will be.
 *
 * @returns The status of the registration.
 * @retval RETURN_OK  - The handler was registered.
 * @retval RETURN_ERR - The registration did not succeed. This declaration names two
 *         causes and does not distinguish them: the facility is not supported or not
 *         implemented on the platform - a stub, which `Optional Components`
 *         in the HAL specification treats as a legitimate vendor choice - or a
 *         misconfiguration. The client action differs from the usual one accordingly:
 *         it treats a failure as "this platform does not report diplexer variation",
 *         reads the settings by polling cm_hal_get_DiplexerSettings() instead, and does
 *         not retry, because a retry cannot change either cause. Because there is no
 *         unregister call, it keeps the handler valid in case one was installed anyway.
 *
 * @note Extending the failure vocabulary of this call would be an interface change:
 *       RETURN_ERR is all it defines today, so the two causes above cannot be told
 *       apart from the return value, and a caller that needs to know consults the
 *       vendor log `cm_vendor_hal.log` under `/rdklogs/logs/`.
 * @note Blocking: synchronous. `Blocking calls` in the HAL specification requires
 *       every call in this interface to complete within a period commensurate with
 *       the operation and not to suspend the calling thread. No numeric timeout is
 *       specified by this interface, so a caller that cannot tolerate an unbounded
 *       wait imposes its own bound.
 * @warning Not thread safe. `Threading Model` in the HAL specification states that
 *          this interface is not required to be thread safe, so the caller
 *          serialises this call against every other CM HAL call, including calls
 *          made from another process.
 * @see cm_hal_DiplexerVariationCallback, cm_hal_get_DiplexerSettings
 */
INT cm_hal_Register_DiplexerVariationCallback(cm_hal_DiplexerVariationCallback callback_proc);

/** @} */  //END OF GROUP CM_HAL_APIS

#ifdef __cplusplus
}
#endif

#endif
