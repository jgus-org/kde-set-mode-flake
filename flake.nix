{
  description = "Set a KWin output's resolution and refresh rate via the kde-output-management-v2 Wayland protocol.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    let
      linuxSystems = [ "x86_64-linux" "aarch64-linux" ];
    in
    flake-utils.lib.eachSystem linuxSystems (system:
      let
        pkgs = import nixpkgs { inherit system; };

        kde-set-mode = pkgs.stdenv.mkDerivation {
          pname = "kde-set-mode";
          version = "0.1";
          src = ./.;

          nativeBuildInputs = [ pkgs.wayland-scanner ];
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
            platforms = linuxSystems;
          };
        };
      in
      {
        packages = {
          inherit kde-set-mode;
          default = kde-set-mode;
        };
      });
}
