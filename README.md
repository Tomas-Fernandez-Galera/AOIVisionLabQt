# AOI Vision Lab Qt

Desktop demonstration of image registration and visual difference detection for
automated optical inspection concepts.

The public project is intended as a technical portfolio demonstration. It is not
a production inspection system and must not be used to certify manufactured parts.

## Screenshots

### Geometric alignment and multiple findings

![AOI alignment and defect detection](docs/screenshots/aoi-vision-alignment-defects.png)

### Localized solder-bridge detection

![AOI solder bridge detection](docs/screenshots/aoi-vision-main.png)

## Current state

- Qt 6 / C++17 project that opens directly in Qt Creator.
- Responsive three-panel interface.
- Reference and inspection image loading.
- Light and dark themes.
- Prepared for the OpenCV registration and comparison stage.

## OpenCV dependency

OpenCV is declared in `vcpkg.json` and built for the same MinGW toolchain as
Qt. From the project directory, install or restore it with:

```powershell
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
C:\vcpkg\vcpkg.exe install --triplet x64-mingw-dynamic
```

## Open in Qt Creator

Open `AOIVisionLabQt.pro`, select the Desktop Qt 6 MinGW 64-bit kit and build.
# AI and MCP automation

The graphical application and external automation share the same C++/OpenCV
inspection engine. A board can be inspected without opening the interface:

```powershell
AOIVisionLabQt.exe --reference reference.png --inspect candidate.png `
  --report result.json --visualization marked-result.png
```

`automation/aoi_mcp_server.py` exposes this operation as the local MCP tool
`analyze_board`. It accepts absolute paths for the reference and candidate,
returns structured JSON and can save both the report and marked image. The
server uses standard input/output, has no Python package dependencies and does
not upload production images to any external service. A client configuration
example is included in `automation/mcp-config-example.json`.
