{
  description = "Set a KWin output's resolution and refresh rate via the kde-output-management-v2 Wayland protocol.";

  inputs = {
    flake-lib.url = "github:jgus-org/flake-lib/v1";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    {
      self,
      flake-lib,
      nixpkgs,
    }:
    flake-lib.lib.eachDefaultLinuxSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };

        orgFormatting = flake-lib.lib.mkFormatting {
          inherit self system;
        };

        kde-set-mode = pkgs.stdenv.mkDerivation {
          buildInputs = [ pkgs.wayland ];
          buildPhase = ''
            runHook preBuild
            proto=${pkgs.kdePackages.plasma-wayland-protocols}/share/plasma-wayland-protocols
            wayland-scanner client-header "$proto/kde-output-device-v2.xml" kde-output-device-v2-client-protocol.h
            wayland-scanner private-code "$proto/kde-output-device-v2.xml" kde-output-device-v2-protocol.c
            wayland-scanner client-header "$proto/kde-output-management-v2.xml" kde-output-management-v2-client-protocol.h
            wayland-scanner private-code "$proto/kde-output-management-v2.xml" kde-output-management-v2-protocol.c
            $CC -O2 -Wall -o kde-set-mode kde-set-mode.c \
              kde-output-device-v2-protocol.c kde-output-management-v2-protocol.c \
              -lwayland-client
            runHook postBuild
          '';
          installPhase = ''
            runHook preInstall
            install -Dm755 kde-set-mode "$out/bin/kde-set-mode"
            runHook postInstall
          '';
          meta = {
            description = "Set a KWin output's resolution and refresh rate via kde-output-management-v2";
            mainProgram = "kde-set-mode";
            platforms = flake-lib.lib.defaultLinuxSystems;
          };
          nativeBuildInputs = [ pkgs.wayland-scanner ];
          pname = "kde-set-mode";
          src = ./.;
          version = "0.1";
        };
      in
      {
        checks.formatting = orgFormatting.check;
        formatter = orgFormatting.formatter;
        packages = {
          inherit kde-set-mode;
          default = kde-set-mode;
        };
      }
    );
}
