{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = pkgs.linuxPackages_zen.kernel.moduleBuildDependencies;

  KERNEL_SRC = "${pkgs.linuxPackages_zen.kernel.dev}/lib/modules/${pkgs.linuxPackages_zen.kernel.modDirVersion}/build";
}