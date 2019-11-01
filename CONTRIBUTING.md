# Contributing

## Hacking with [Nix](https://nixos.org/nix/)

The `nix/` folder contains helper scripts for working with the ledger via Nix.

### Installing
Use `./nix/install.sh` to install the apps onto the ledger using Nix.

### Developing
Use `nix/env.sh <s or x> to enter a shell where you can run `make` and it will just work. You can also pass a command instead, e.g. `nix/env.sh s --run '$(make clean)'`.

For development, use `nix/watch.sh s make APP=<tezos_baking|tezos_wallet>` to incrementally build on every change. Be sure to `nix/env.sh s --run '$(make clean)'` if you start watching a different `APP`.

### Building
To do a full Nix build run `nix/build.sh`. You can pass `nix-build` arguments to this to build specific attributes, e.g. `nix/build.sh -A nano.s.wallet`.

### Installing
`nix/install.sh` will install both the wallet and baking apps. Use
`nix/install.sh s baking` to install just the baking app or
`nix/install.sh s wallet` to install just the wallet.

### Editor Integration

#### Visual Studio Code

  1. Install `llvm-vs-code-extensions.vscode-clangd` extension.
  2. Run `nix/setup-vscode.sh` to create local configuration. Note that these files are non-relocatable so you need to rerun this if you move the directory.
  3. Restart Visual Studio Code.

### Releasing

`nix/build.sh -A nano.s.release.all`

`nix/build.sh -A nano.x.release.all`

### Notes on testing

Currently there are two types of tests: those that require the [Flextesa framework](https://gitlab.com/tezos/flextesa) and those that rely on the apdu script from LedgerLive.The apdu tests are inside of *test/apdu-tests/* while the flextesa tests are actually inside of the main tezos repo in /src/bin-flextesa/

The flextesa tests can be run using the *test/run-flextesa.sh* script.
The apdu tests can be run using *test/run-apdu-tests.sh* script.
