#!/bin/sh

KEYFILE="/tmp/fatrat-webui.key"
CONFIGFILE="/tmp/fatrat-webui.cnf"
CSRFILE="/tmp/fatrat-webui.csr"
CRTFILE="/tmp/fatrat-webui.crt"
PEMFILE="/tmp/fatrat-webui.pem"

if ! which sed >/dev/null; then
  echo "No sed installed"
  exit 1
fi

if ! which openssl >/dev/null; then
  echo "No openssl installed"
  exit 1
fi

sed "s/%HOSTNAME%/$2/g" < "$1" > "$CONFIGFILE"
openssl genrsa -out "$KEYFILE" 4096
openssl req -new -key "$KEYFILE" -config "$CONFIGFILE" -out "$CSRFILE"
openssl x509 -req -days 3650 -in "$CSRFILE" -signkey "$KEYFILE" -out "$CRTFILE"

cat "$KEYFILE" "$CRTFILE" > "$PEMFILE"

rm -f -- "$KEYFILE" "$CONFIGFILE" "$CSRFILE"

if [ ! -f "$CRTFILE" ]; then
  echo "Failed to generate a certificate"
  exit 1
fi

rm -f -- "$CRTFILE"

exit 0
