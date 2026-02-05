# Example: libyang schema-mount example

Setup
- Initialize submodules (libyang (libyang) and yang (models)):

```sh
git submodule update --init --recursive
```

- Ensure libyang is available (headers/libraries). The included `Makefile` uses
  `/usr/local/include` and `/usr/local/lib` and also calls `pkg-config` for
  `libyang` if available.

Build

```sh
make
```

Run

```sh
./example_ni
```

Expected output

Parsed data:
```
<network-instances xmlns="urn:ietf:params:xml:ns:yang:ietf-network-instance">
  <network-instance>
    <name>VRF1</name>
    <vrf-root>
      <routing xmlns="urn:ietf:params:xml:ns:yang:ietf-routing">
        <ribs>
          <rib>
            <name>default</name>
            <address-family>ipv4</address-family>
          </rib>
        </ribs>
      </routing>
    </vrf-root>
  </network-instance>
</network-instances>
```

Notes
- If `make` fails to find libyang via `pkg-config`, ensure libyang is
  installed to `/usr/local` or update your `PKG_CONFIG_PATH` to include the
  location of `libyang.pc`.
