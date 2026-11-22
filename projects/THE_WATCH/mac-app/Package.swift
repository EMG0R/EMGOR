// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "TheWatchMac",
    platforms: [
        .macOS(.v13)   // Ventura+ (CoreBluetooth, Network, IOKit all present)
    ],
    targets: [
        .executableTarget(
            name: "TheWatchMac",
            path: "Sources/TheWatchMac",
            // tools-version 5.9 => Swift 5 language mode by default, which keeps
            // CoreBluetooth delegates / IOKit callbacks / singletons free of the
            // Swift 6 strict-concurrency errors. The runtime design still hops BLE
            // events and timers to the main queue.
            linkerSettings: [
                // Embed Info.plist directly into the executable's __TEXT,__info_plist
                // section so a bare `swift build` binary already carries the
                // LSUIElement flag + TCC usage strings (Bluetooth / local network).
                // The path is resolved relative to the package root, which is the
                // linker's working directory during `swift build`.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "Resources/Info.plist"
                ])
            ]
        )
    ]
)
