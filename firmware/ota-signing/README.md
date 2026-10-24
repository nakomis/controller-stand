# OTA signing keys

Every Grogu image is signed (RSA-3072, ESP-IDF's secure boot v2 scheme, without secure boot), and a
Grogu accepts an update only if it is signed by the key that signed the image it is running.

This directory holds the **public** halves, one per environment: `sandbox.pub.pem` and `prod.pub.pem`.
Commit them. Until `<env>.pub.pem` exists, `build.sh` still builds but warns that the image is
unsigned (it boots, but will refuse every update), and `sign.sh`, `flash.sh` and `publish-ota.sh`
refuse to proceed.

The private key is never in the repo: the master copy is SSM `/controller-stand/{env}/ota/signing-key`
(SecureString), and `sign.sh` caches it in the login Keychain as service
`controller-stand-ota-signing`, account `{env}`.

## One-off key generation (Martin, per environment)

**Replacing a key strands every Grogu already in the field**, so do this once per environment, and
only once.

```bash
# sandbox (AWS profile nakom.is-sandbox, account 975050268859)
NPM_ENVIRONMENT=sandbox AWS_PROFILE=nakom.is-sandbox infra/scripts/ota-signing-key.sh
git add firmware/ota-signing/sandbox.pub.pem

# prod (profile nakom.is-prod, account 637423226886), when there is a prod device
NPM_ENVIRONMENT=prod AWS_PROFILE=nakom.is-prod infra/scripts/ota-signing-key.sh
git add firmware/ota-signing/prod.pub.pem
```

The script makes the key with `espsecure.py`, writes the public half here, stores the private half in
SSM and the Keychain, and deletes its temporary copy. It refuses if either already exists.

## Using it

```bash
firmware/build.sh                      # builds, then signs with the Keychain key (NPM_ENVIRONMENT, default sandbox)
firmware/flash.sh <port> [thing]       # flashes the signed image and the thing's prov.bin
firmware/publish-ota.sh <x.y.z>        # signed build to s3://nakomis-firmware-{env}/controller-stand/<x.y.z>/
```

Never `idf.py flash`: it writes an unsigned image.
