let
  nixpkgs = fetchTarball "https://github.com/NixOS/nixpkgs/tarball/nixos-24.05";
  pkgs = import nixpkgs { config = {}; overlays = []; };
in
pkgs.mkShellNoCC {
  packages = with pkgs; [
    # essential programs
    git cmake clang gnumake patch jq pkg-config
    # libraries
    openssl boost fmt nlohmann_json lz4 zlib zstd
    enet libopus vulkan-headers vulkan-utility-libraries
    spirv-tools spirv-headers vulkan-loader unzip mbedtls
    glslang python3 httplib cpp-jwt ffmpeg-headless
    libusb1 cubeb
    # eden
    qt6.qtbase qt6.qtmultimedia qt6.qtwayland qt6.qttools
    qt6.qtwebengine qt6.qt5compat
    # eden-cli
    SDL2
    # optional components
    discord-rpc gamemode
  ];
}