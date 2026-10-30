#!/bin/sh
# Usage: genssl.sh config.cnf hostname [output.pem]
# Writes a self-signed key+certificate PEM to output.pem (stdout if omitted).

if ! which sed >/dev/null; then
  echo "No sed installed" >&2
  exit 1
fi

if ! which openssl >/dev/null; then
  echo "No openssl installed" >&2
  exit 1
fi

# Private (mode 0700) scratch directory: the private key must never sit in a predictable, shared place
WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/fatrat-webui.XXXXXX") || exit 1
trap 'rm -rf -- "$WORKDIR"' EXIT HUP INT TERM

KEYFILE="$WORKDIR/key"
CONFIGFILE="$WORKDIR/cnf"
CSRFILE="$WORKDIR/csr"
CRTFILE="$WORKDIR/crt"

sed "s/%HOSTNAME%/$2/g" < "$1" > "$CONFIGFILE" || exit 1
openssl genrsa -out "$KEYFILE" 4096 >&2 || exit 1
openssl req -new -key "$KEYFILE" -config "$CONFIGFILE" -out "$CSRFILE" || exit 1
openssl x509 -req -days 3650 -in "$CSRFILE" -signkey "$KEYFILE" -out "$CRTFILE" || exit 1

if [ ! -s "$CRTFILE" ]; then
  echo "Failed to generate a certificate" >&2
  exit 1
fi

if [ -n "$3" ]; then
  # Keep the key private even if the target did not exist yet
  (umask 077; cat "$KEYFILE" "$CRTFILE" > "$3") || exit 1
else
  cat "$KEYFILE" "$CRTFILE"
fi

exit 0
