# Certificado de firma (desde Linux)

El paquete UWP se firma en CI con un certificado autofirmado. Su **CN debe ser exactamente `RenpyUWP`**,
porque `host/RenpyUWP/Package.appxmanifest` declara `Publisher="CN=RenpyUWP"`. El workflow comprueba que
coincidan y falla si no.

La clave privada vive solo en tu máquina (`~/.config/renpy-uwp/cert/`, permisos 700) y en GitHub Secrets.
**Nunca** en el repo (`.gitignore` bloquea `*.pfx`, `*.key`, `*.cer`, `*.crt`, `*.pem`).

## 1. Generar el certificado

```bash
mkdir -p ~/.config/renpy-uwp/cert && chmod 700 ~/.config/renpy-uwp ~/.config/renpy-uwp/cert
cd ~/.config/renpy-uwp/cert
umask 077

cat > codesign.cnf <<'EOF'
[req]
distinguished_name = dn
prompt = no
x509_extensions = v3
[dn]
CN = RenpyUWP
[v3]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature
extendedKeyUsage = codeSigning
subjectKeyIdentifier = hash
EOF

# Clave RSA 3072 + certificado autofirmado, válido 5 años
openssl req -x509 -newkey rsa:3072 -sha256 -days 1825 -noenc \
  -keyout renpy-uwp.key -out renpy-uwp.crt -config codesign.cnf

# Contraseña aleatoria para el PFX (se guarda en un archivo solo legible por ti)
openssl rand -base64 24 | tr -d '\n' > pfx-password.txt
export UWP_CERT_PASSWORD="$(cat pfx-password.txt)"

# PFX con cifrado 3DES/SHA1 (compatible con signtool en cualquier Windows)
openssl pkcs12 -export -inkey renpy-uwp.key -in renpy-uwp.crt -out renpy-uwp.pfx \
  -name RenpyUWP -passout env:UWP_CERT_PASSWORD \
  -keypbe PBE-SHA1-3DES -certpbe PBE-SHA1-3DES -macalg sha1

# Certificado público (por si hay que confiarlo en la consola)
openssl x509 -in renpy-uwp.crt -outform DER -out renpy-uwp.cer

# Comprobación: debe decir subject=CN=RenpyUWP y "Code Signing"
openssl x509 -in renpy-uwp.crt -noout -subject -ext extendedKeyUsage
openssl pkcs12 -in renpy-uwp.pfx -passin env:UWP_CERT_PASSWORD -noout && echo "PFX OK"
```

## 2. Cargar los secrets en GitHub

```bash
cd ~/.config/renpy-uwp/cert
base64 -w0 renpy-uwp.pfx | gh secret set UWP_CERT_PFX_BASE64 --repo alexroses47/renpy-uwp
gh secret set UWP_CERT_PASSWORD --repo alexroses47/renpy-uwp < pfx-password.txt
unset UWP_CERT_PASSWORD

# Comprobar (solo muestra nombres y fechas, nunca valores)
gh secret list --repo alexroses47/renpy-uwp
```

`gh secret set` sin `--body` lee el valor de la entrada estándar, así la contraseña no aparece en el
historial de la shell ni en `ps`.

## 3. Renovar o rotar

Repite el paso 1 (el CN debe seguir siendo `RenpyUWP`) y el paso 2. Un paquete firmado con un certificado
nuevo puede no actualizar al anterior: desinstala primero (`openappx deploy ... --uninstall <PackageFullName>`
o desde Dev Home).
