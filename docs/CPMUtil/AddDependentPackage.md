# AddDependentPackage

Use `AddDependentPackage` when you have multiple packages that are required to all be from the system, OR bundled. This is useful in cases where e.g. versions must absolutely match.

## Versioning

Versioning must be handled by the package itself.

## Examples

### Vulkan

`cpmfile.json`

```json
{
    "vulkan-headers": {
        "repo": "KhronosGroup/Vulkan-Headers",
        "package": "VulkanHeaders",
        "version": "1.4.317",
        "hash": "26e0ad8fa34ab65a91ca62ddc54cc4410d209a94f64f2817dcdb8061dc621539a4262eab6387e9b9aa421db3dbf2cf8e2a4b041b696d0d03746bae1f25191272",
        "git_version": "1.4.342",
        "tag": "v%VERSION%"
    },
    "vulkan-utility-libraries": {
        "repo": "KhronosGroup/Vulkan-Utility-Libraries",
        "package": "VulkanUtilityLibraries",
        "hash": "8147370f964fd82c315d6bb89adeda30186098427bf3efaa641d36282d42a263f31e96e4586bfd7ae0410ff015379c19aa4512ba160630444d3d8553afd1ec14",
        "git_version": "1.4.342",
        "tag": "v%VERSION%"
    }
}
```

`CMakeLists.txt`:

```cmake
AddDependentPackages(vulkan-headers vulkan-utility-libraries)
```

If Vulkan Headers are installed, but NOT Vulkan Utility Libraries, then CPMUtil will throw an error.
