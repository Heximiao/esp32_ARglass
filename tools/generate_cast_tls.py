"""Create project-local HTTPS credentials; never install a trust certificate.
Requires cryptography. Run once before building, or use --force to rotate.
The CA signing key is discarded; the device only receives a scoped leaf key.
"""
from pathlib import Path
from datetime import datetime, timedelta, timezone
import argparse, ipaddress, hashlib
from cryptography import x509
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--force', action='store_true')
args = parser.parse_args()
private = root / 'src/cast/private'
output = private / 'TlsMaterial.h'
if output.exists() and not args.force:
    print('Existing HTTPS identity retained.'); raise SystemExit(0)
now = datetime.now(timezone.utc)
ca_key = ec.generate_private_key(ec.SECP256R1())
leaf_key = ec.generate_private_key(ec.SECP256R1())
ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'AR Glass Local Project CA')])
ca = (x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name)
      .public_key(ca_key.public_key()).serial_number(x509.random_serial_number())
      .not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=3650))
      .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
      .add_extension(x509.KeyUsage(False,False,False,False,False,True,True,False,False), critical=True)
      .add_extension(x509.SubjectKeyIdentifier.from_public_key(ca_key.public_key()), False)
      .sign(ca_key, hashes.SHA256()))
leaf = (x509.CertificateBuilder()
        .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'ar-glass.local')]))
        .issuer_name(ca_name).public_key(leaf_key.public_key()).serial_number(x509.random_serial_number())
        .not_valid_before(now-timedelta(days=1)).not_valid_after(now+timedelta(days=365))
        .add_extension(x509.BasicConstraints(ca=False, path_length=None), True)
        .add_extension(x509.SubjectAlternativeName([x509.DNSName('ar-glass.local'),
            x509.IPAddress(ipaddress.ip_address('192.168.4.1'))]), False)
        .add_extension(x509.KeyUsage(True,False,False,False,False,False,False,False,False), True)
        .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False)
        .add_extension(x509.AuthorityKeyIdentifier.from_issuer_public_key(ca_key.public_key()), False)
        .sign(ca_key, hashes.SHA256()))
ca_pem = ca.public_bytes(serialization.Encoding.PEM).decode()
chain = leaf.public_bytes(serialization.Encoding.PEM).decode()+ca_pem
key = leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                            serialization.NoEncryption()).decode()
private.mkdir(parents=True, exist_ok=True)
output.write_text('#pragma once\n#include <Arduino.h>\n'+''.join(
    f'static const char {name}[] PROGMEM = R"PEM({value})PEM";\n'
    for name,value in [('CastCertificate',chain),('CastPrivateKey',key),('CastRootCA',ca_pem)]), encoding='utf-8')
certs = root/'certs'; certs.mkdir(exist_ok=True)
(certs/'ar-glass-root.crt').write_bytes(ca.public_bytes(serialization.Encoding.PEM))
(certs/'fingerprint.txt').write_text('SHA256 '+ca.fingerprint(hashes.SHA256()).hex()+'\n', encoding='ascii')
print('Generated local HTTPS identity. Trust is NOT installed. Public root: certs/ar-glass-root.crt')
