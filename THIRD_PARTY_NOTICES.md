# Third-Party Notices

This repository vendors source code from the projects below so the native GUI can be built without downloading C++ dependencies. Each project retains its original copyright and license notices.

| Component | Vendored version | Upstream | License in this repository |
|---|---:|---|---|
| GLFW | 3.4.0 | [glfw/glfw](https://github.com/glfw/glfw) | [zlib/libpng license](external/glfw/LICENSE.md) |
| Dear ImGui | 1.92.5 | [ocornut/imgui](https://github.com/ocornut/imgui) | [MIT License](external/imgui/LICENSE.txt) |
| ImPlot | 0.17 | [epezent/implot](https://github.com/epezent/implot) | [MIT License](external/implot/LICENSE) |

Versions above are taken from the vendored projects' own CMake files or public version macros. The original Git commit metadata was not included with these source snapshots, so this repository does not claim a more specific upstream commit.

The RV32 Pipeline Simulator's own source is covered by the repository's top-level [LICENSE](LICENSE). Third-party source files remain covered by their respective upstream licenses.
