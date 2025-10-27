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
    spirv-tools spirv-headers simpleini vulkan-memory-allocator
    vulkan-loader unzip mbedtls zydis glslang python3 httplib
    cpp-jwt ffmpeg-headless libusb1 cubeb
    qt6.full # eden
    SDL2 # eden-cli
    discord-rpc gamemode # optional components
  ];
}