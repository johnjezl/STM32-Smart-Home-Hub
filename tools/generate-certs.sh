#!/bin/bash
#
# SmartHub Certificate Generation Script
# Generates self-signed CA and server certificates for HTTPS
#

set -e

# Default values
CERT_DIR="${CERT_DIR:-/etc/smarthub/certs}"
HOSTNAME="${HOSTNAME:-smarthub.local}"
IP_ADDRESS="${IP_ADDRESS:-}"
DAYS_VALID="${DAYS_VALID:-365}"
CA_DAYS="${CA_DAYS:-3650}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -d, --dir DIR       Certificate directory (default: /etc/smarthub/certs)"
    echo "  -h, --hostname NAME Server hostname (default: smarthub.local)"
    echo "  -i, --ip ADDRESS    Server IP address (optional)"
    echo "  -v, --days DAYS     Server cert validity in days (default: 365)"
    echo "  -f, --force         Overwrite existing certificates"
    echo "  --help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --hostname smarthub.local --ip 192.168.1.100"
    echo "  $0 -d ./certs -h myhub.local"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

FORCE=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--dir)
            CERT_DIR="$2"
            shift 2
            ;;
        -h|--hostname)
            HOSTNAME="$2"
            shift 2
            ;;
        -i|--ip)
            IP_ADDRESS="$2"
            shift 2
            ;;
        -v|--days)
            DAYS_VALID="$2"
            shift 2
            ;;
        -f|--force)
            FORCE=1
            shift
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Check for openssl
if ! command -v openssl &> /dev/null; then
    log_error "openssl is required but not installed"
    exit 1
fi

# Check if certificates already exist
if [ -f "$CERT_DIR/server.crt" ] && [ "$FORCE" -eq 0 ]; then
    log_warn "Certificates already exist in $CERT_DIR"
    log_warn "Use --force to regenerate"
    exit 0
fi

# Create certificate directory
log_info "Creating certificate directory: $CERT_DIR"
mkdir -p "$CERT_DIR"

# Build Subject Alternative Name extension
SAN="DNS:${HOSTNAME},DNS:smarthub"
if [ -n "$IP_ADDRESS" ]; then
    SAN="${SAN},IP:${IP_ADDRESS}"
fi

# Create OpenSSL config for SAN
OPENSSL_CONF=$(mktemp)
cat > "$OPENSSL_CONF" << EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]
C = US
ST = Home
L = Home
O = SmartHub
CN = SmartHub CA

[v3_ca]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints = critical, CA:true
keyUsage = critical, digitalSignature, cRLSign, keyCertSign

[v3_server]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid,issuer
basicConstraints = CA:FALSE
keyUsage = critical, digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = ${SAN}
EOF

# Generate CA private key
log_info "Generating CA private key..."
openssl genrsa -out "$CERT_DIR/ca.key" 4096 2>/dev/null

# Generate CA certificate
log_info "Generating CA certificate (valid for $CA_DAYS days)..."
openssl req -new -x509 -days "$CA_DAYS" \
    -key "$CERT_DIR/ca.key" \
    -out "$CERT_DIR/ca.crt" \
    -config "$OPENSSL_CONF" \
    -extensions v3_ca

# Generate server private key
log_info "Generating server private key..."
openssl genrsa -out "$CERT_DIR/server.key" 2048 2>/dev/null

# Generate server CSR
log_info "Generating server certificate signing request..."
openssl req -new \
    -key "$CERT_DIR/server.key" \
    -out "$CERT_DIR/server.csr" \
    -subj "/C=US/ST=Home/L=Home/O=SmartHub/CN=${HOSTNAME}"

# Sign server certificate with CA
log_info "Signing server certificate (valid for $DAYS_VALID days)..."
openssl x509 -req -days "$DAYS_VALID" \
    -in "$CERT_DIR/server.csr" \
    -CA "$CERT_DIR/ca.crt" \
    -CAkey "$CERT_DIR/ca.key" \
    -CAcreateserial \
    -out "$CERT_DIR/server.crt" \
    -extfile "$OPENSSL_CONF" \
    -extensions v3_server 2>/dev/null

# Remove temporary files
rm -f "$OPENSSL_CONF" "$CERT_DIR/server.csr"

# Set permissions
log_info "Setting file permissions..."
chmod 600 "$CERT_DIR"/*.key
chmod 644 "$CERT_DIR"/*.crt

# Display certificate info
log_info "Certificate generation complete!"
echo ""
echo "Files created:"
echo "  CA Certificate:     $CERT_DIR/ca.crt"
echo "  CA Private Key:     $CERT_DIR/ca.key"
echo "  Server Certificate: $CERT_DIR/server.crt"
echo "  Server Private Key: $CERT_DIR/server.key"
echo ""
echo "Server certificate details:"
openssl x509 -in "$CERT_DIR/server.crt" -noout -subject -dates -ext subjectAltName 2>/dev/null | sed 's/^/  /'
echo ""
echo "To trust this certificate on client devices, install: $CERT_DIR/ca.crt"
