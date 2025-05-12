// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"

namespace Service::SSL {

enum class CaCertificateId : s32 {
    All = -1,
    NintendoCAG3 = 1,
    NintendoClass2CAG3 = 2,
    NintendoRootCAG4 = 3,
    AmazonRootCA1 = 1000,
    StarfieldServicesRootCertificateAuthorityG2 = 1001,
    AddTrustExternalCARoot = 1002,
    COMODOCertificationAuthority = 1003,
    UTNDATACorpSGC = 1004,
    UTNUSERFirstHardware = 1005,
    BaltimoreCyberTrustRoot = 1006,
    CybertrustGlobalRoot = 1007,
    VerizonGlobalRootCA = 1008,
    DigiCertAssuredIDRootCA = 1009,
    DigiCertAssuredIDRootG2 = 1010,
    DigiCertGlobalRootCA = 1011,
    DigiCertGlobalRootG2 = 1012,
    DigiCertHighAssuranceEVRootCA = 1013,
    EntrustnetCertificationAuthority2048 = 1014,
    EntrustRootCertificationAuthority = 1015,
    EntrustRootCertificationAuthorityG2 = 1016,
    GeoTrustGlobalCA2 = 1017,
    GeoTrustGlobalCA = 1018,
    GeoTrustPrimaryCertificationAuthorityG3 = 1019,
    GeoTrustPrimaryCertificationAuthority = 1020,
    GlobalSignRootCA = 1021,
    GlobalSignRootCAR2 = 1022,
    GlobalSignRootCAR3 = 1023,
    GoDaddyClass2CertificationAuthority = 1024,
    GoDaddyRootCertificateAuthorityG2 = 1025,
    StarfieldClass2CertificationAuthority = 1026,
    StarfieldRootCertificateAuthorityG2 = 1027,
    thawtePrimaryRootCAG3 = 1028,  // [8.0.0+] TrustedCertStatus is EnabledNotTrusted
    thawtePrimaryRootCA = 1029,     // [8.0.0+] TrustedCertStatus is EnabledNotTrusted
    VeriSignClass3PublicPrimaryCertificationAuthorityG3 = 1030,  // [8.0.0+] TrustedCertStatus is EnabledNotTrusted
    VeriSignClass3PublicPrimaryCertificationAuthorityG5 = 1031,  // [8.0.0+] TrustedCertStatus is EnabledNotTrusted
    VeriSignUniversalRootCertificationAuthority = 1032,           // [8.0.0+] TrustedCertStatus is EnabledNotTrusted
    DSTRootCAX3 = 1033,  // [6.0.0+]
    USERTrustRsaCertificationAuthority = 1034,  // [10.0.3+]
    ISRGRootX10 = 1035,  // [10.1.0+]
    USERTrustEccCertificationAuthority = 1036,  // [10.1.0+]
    COMODORsaCertificationAuthority = 1037,     // [10.1.0+]
    COMODOEccCertificationAuthority = 1038,     // [10.1.0+]
    AmazonRootCA2 = 1039,  // [11.0.0+]
    AmazonRootCA3 = 1040,  // [11.0.0+]
    AmazonRootCA4 = 1041,  // [11.0.0+]
    DigiCertAssuredIDRootG3 = 1042,  // [11.0.0+]
    DigiCertGlobalRootG3 = 1043,     // [11.0.0+]
    DigiCertTrustedRootG4 = 1044,    // [11.0.0+]
    EntrustRootCertificationAuthorityEC1 = 1045,  // [11.0.0+]
    EntrustRootCertificationAuthorityG4 = 1046,   // [11.0.0+]
    GlobalSignECCRootCAR4 = 1047,  // [11.0.0+]
    GlobalSignECCRootCAR5 = 1048,  // [11.0.0+]
    GlobalSignECCRootCAR6 = 1049,  // [11.0.0+]
    GTSRootR1 = 1050,  // [11.0.0+]
    GTSRootR2 = 1051,  // [11.0.0+]
    GTSRootR3 = 1052,  // [11.0.0+]
    GTSRootR4 = 1053,  // [11.0.0+]
    SecurityCommunicationRootCA = 1054,  // [12.0.0+]
    GlobalSignRootE4 = 1055,  // [15.0.0+]
    GlobalSignRootR4 = 1056,  // [15.0.0+]
    TTeleSecGlobalRootClass2 = 1057,  // [15.0.0+]
    DigiCertTLSECCP384RootG5 = 1058,  // [16.0.0+]
    DigiCertTLSRSA4096RootG5 = 1059,  // [16.0.0+]
    NintendoTempRootCAG4 = 65536,     // [16.0.0+] ([19.0.0+] Removed)
};

enum class TrustedCertStatus : s32 {
    Invalid = -1,
    Removed = 0,
    EnabledTrusted = 1,
    EnabledNotTrusted = 2,
    Revoked = 3,
};

struct BuiltInCertificateInfo {
    CaCertificateId cert_id;
    TrustedCertStatus status;
    u64 der_size;
    u64 der_offset;
};
static_assert(sizeof(BuiltInCertificateInfo) == 0x18, "BuiltInCertificateInfo has incorrect size.");

struct CertStoreHeader {
    u32 magic;
    u32 num_entries;
};
static_assert(sizeof(CertStoreHeader) == 0x8, "CertStoreHeader has incorrect size.");

struct CertStoreEntry {
    CaCertificateId certificate_id;
    TrustedCertStatus certificate_status;
    u32 der_size;
    u32 der_offset;
};
static_assert(sizeof(CertStoreEntry) == 0x10, "CertStoreEntry has incorrect size.");

} // namespace Service::SSL