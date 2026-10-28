# Maya2glTF for Maya 2027

A glTF 2.0 exporter plug-in for Autodesk Maya, with a one-click installer, a
redesigned export UI, MASH-to-instance conversion and a viewport PBR shader
that round-trips to glTF.

This is a maintained fork of the original
[iimachines/Maya2glTF](https://github.com/iimachines/Maya2glTF) project,
updated with the approval of the original authors to support Maya 2026 and
2027 on Windows.

![Maya Helmet screenshot](img/maya_screenshot.jpg)

## Highlights

- **Maya 2027 and 2026, Windows x64.** Built against the official devkits with
  Visual Studio 2022.
- **Installer.** Every release ships a `Maya2glTF_Installer.bat` that copies
  the plug-in, scripts, shelf, icons and shader files into your Maya user
  folder. No manual file juggling.
- **Export UI.** A single window for naming, output folder, filters, animation
  clips and shader tools. Settings are stored in the scene so they survive a
  reload.
- **Pivots baked into nodes.** Maya pivots are exported as the glTF node origin
  with no helper nodes, so objects rotate around the same point in the engine
  as they do in Maya. See [Pivots](#pivots).
- **MASH to instances.** One click turns a MASH network into real DAG
  instances, which the exporter writes as one shared mesh referenced by many
  nodes. Combined with `EXT_mesh_gpu_instancing` this gives small files and
  few draw calls for scenes with thousands of repeated objects.
- **Materials.** Exports the bundled glTF PBR GLSL shader, `openPBRSurface`,
  `aiStandardSurface`, and the colour and transparency of Lambert, Phong and
  Blinn.
- **Animation clips manager.** Define any number of named clips with their own
  frame ranges directly in the exporter, and export them all in one go as
  separate glTF animations. Clips are saved with the scene, so the setup is
  there the next time the file is opened. Node and joint transforms, skin
  clusters and blend shape weights are sampled per frame.

## Installation

Requirements: Windows 10 or 11 x64, Maya 2027 (or 2026 with the matching
build), and the
[Microsoft Visual C++ 2022 redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
Most machines already have the redistributable.

1. Download the latest zip from the
   [Releases](https://github.com/JunkYardRobotBoy/Maya2glTF_Update/releases)
   page and extract it anywhere.
2. Make sure Maya is closed.
3. Double-click `Maya2glTF_Installer.bat`. It copies into your `Documents\maya`
   folder:

   | What | Where |
   |---|---|
   | `maya2glTF.mll` | `2027\plug-ins\` |
   | MEL and Python scripts | `scripts\` |
   | `glTF` shelf and icons | `2027\prefs\shelves\`, `2027\prefs\icons\` |
   | PBR shader and environment maps | `maya2glTF\PBR\` |

4. Start Maya. A `glTF` shelf appears with three buttons: open the exporter,
   set a logical parent, and clear logical parents. The plug-in is loaded and
   set to auto-load the first time the exporter runs.

If the shelf is missing, run `maya2glTF_UI();` in the MEL script editor.

## Using the exporter

Open the UI from the shelf or with `maya2glTF_UI();`. The exporter works on
the current selection, so select the meshes, groups or cameras you want before
pressing **RUN EXPORT**. With *Selected nodes only* off, everything under the
selection is exported too.

<table>
<tr>
<td valign="top" width="50%">

**Asset Identification**

- *Scene Name* becomes the file name.
- *Copyright* is embedded in the file.
- *Output Folder* is remembered between sessions.

**PBR Shader & Workflow Tools**

- *Assign PBR Shader* puts a GLSL material running the bundled `glTF_PBR.ogsfx` effect on the selected meshes, then opens a file picker for textures. Cancel the picker to assign the shader without textures. Files are matched to slots by name: `basecolor` or `albedo`, `metal`, `rough`, `occl` or `_ao` (or `_orm` for a packed map), `normal`, `emissive`. The exporter reads these materials straight back into glTF PBR.
- *Map Env Textures* applies the three environment maps below it (diffuse, specular, BRDF lookup) to every glTF PBR shader in the scene. The defaults are the maps shipped with the installer. They affect the viewport preview only, not the export.
- *Convert MASH to Instances* turns the selected MASH network into DAG instances under a `<network>_Instances` group. Select any node of the network and click. Hidden points are skipped, and clicking again rebuilds the group rather than adding a second set. Hide the original Repro mesh or Instancer before exporting so the geometry is not exported twice.
- *Ignore Selected Mesh Deformers* adds the skin clusters, blend shapes, clusters and lattices in the selection's history to the ignore list, for deformers that shape a character but should not export as animation.

**Mesh & Scene Filters**

- *Selected nodes only*, *Visible nodes only* (on by default).
- *Export as Binary (.glb)*, on by default.
- *GPU Instancing* writes `EXT_mesh_gpu_instancing` for shared meshes.
- *Keep pivot nodes* restores the two-node pivot form, see [Pivots](#pivots).
- *Scale Factor*, and a free-text field for extra [command flags](#command-reference).

**Animation & Clips**

This replaces the old single TRAX clip dropdown. Set the target frame rate, then build a list of clips: type a name, a start frame and an end frame and press *Add Clip*. Each entry becomes its own named animation in the exported file, so a character ships with `idle`, `walk` and `run` from one export. The list is sorted by start frame, entries can be removed individually, and the whole list is stored in the scene so it survives saving and reopening. Leave the list empty to export a static file.

</td>
<td valign="top" width="50%">

<img src="artwork/UI.gif" alt="Maya2glTF exporter window" width="100%">

</td>
</tr>
</table>

The viewport must use OpenGL (Preferences > Display > Viewport 2.0 rendering
engine) for the GLSL PBR shader to display. Export works with either backend.

## Pivots

glTF nodes have no pivot point, Maya transforms do. Older versions of this
exporter handled the difference by emitting two glTF nodes per pivoted
transform: an outer node carrying the rotation and scale, and an inner
`:PIV` node carrying the negative pivot offset. That doubled the node count of
most scenes.

Since this release the pivot is baked instead:

- The glTF node's origin is the Maya pivot, so rotating the node in an engine
  turns around the same point as in Maya.
- The offset is folded into the mesh vertex positions, and into the local
  transforms of any child nodes, every frame.
- Animation, instancing and parenting behave exactly as before, with no
  `:PIV` nodes in the file.

Baking is skipped automatically where it cannot be done safely: joints,
skinned meshes, and transforms whose pivot attributes are animated or
connected. Those keep the two-node form. Tick **Keep pivot nodes** in the UI,
or pass `-kpv`, to get the two-node form everywhere, for example if a
downstream tool depends on it.

## Materials

| Maya material | Export |
|---|---|
| GLSLShader running `glTF_PBR.ogsfx` | Full PBR: base colour, metallic, roughness, occlusion, normal, emissive, factors |
| `openPBRSurface` (Maya 2026+) | PBR parameters and textures mapped to glTF |
| `aiStandardSurface` (Arnold) | PBR parameters and textures mapped to glTF |
| Lambert, Phong, Blinn | Colour and transparency only |

Metallic and roughness textures are always packed into one glTF texture on
export. If you supply JPEGs, Maya's JPEG encoder is used for that packed
texture and its default quality is low. Raise it with:

```
putenv "AW_JPEG_Q_FACTOR" "100";
putenv "AW_JPEG_SUB_SAMPLING" "1x1,1x1,1x1";
```

## Command reference

The UI builds a `maya2glTF` MEL command from its settings and prints it to the
script editor before running, so anything the UI does can be scripted. Flags
can be given by short or long name. Values marked *multiple* may be repeated.

### Output

| Flag | Description |
|---|---|
| `-of` `-outputFolder STRING` | Output folder. Required. |
| `-sn` `-sceneName STRING` | File name without extension. Default: the Maya scene name. |
| `-glb` `-binary` | Write a single `.glb`. Default: `.gltf` plus `.bin`. |
| `-ext` `-externalTextures` | With `-glb`, keep textures as separate files. |
| `-cpr` `-copyright STRING` | Copyright text embedded in the asset. |
| `-cof` `-cleanOutputFolder` | Delete the output folder first. |
| `-gfe` `-gltfFileExtension STRING`, `-gbe` `-glbFileExtension STRING` | Override the file extensions. |
| `-nbu` `-niceBufferURIs` | Name `.bin` files after the scene without numeric suffixes. |
| `-hbu` `-hashBufferURI` | Name buffers by a hash of their contents. |
| `-sab` `-separateAccessorBuffers` | One buffer per accessor. |
| `-sma` `-splitMeshAnimation` | Separate buffers for mesh data and animation. |
| `-sbr` `-splitByReference` | Separate buffers per referenced file. Requires `-sma`. |
| `-cui` `-convertUnsupportedImages` | Convert unsupported image formats to PNG. |

### Selection and hierarchy

| Flag | Description |
|---|---|
| `-sno` `-selectedNodesOnly` | Export only the selected nodes, not their descendants. |
| `-vno` `-visibleNodesOnly` | Skip invisible nodes. |
| `-cam` `-cameras STRING` *multiple* | Export the named camera. |
| `-kpv` `-keepPivots` | Keep the two-node `:PIV` form for pivoted transforms. See [Pivots](#pivots). |
| `-ksn` `-keepShapeNodes` | Keep shape nodes that are the only child of their transform instead of merging them. |
| `-frn` `-forceRootNode` | Always add a root node. |
| `-isc` `-ignoreSegmentScaleCompensation` | Treat joints as if segment scale compensation were off. |
| `-dnn` `-disableNameAssignment` | Do not copy Maya node names into the file. |
| `-kon` `-keepMayaNamespaces` | Keep namespaces in node names. By default they are stripped. |
| `-smd` `-skipMeshDeduplication` | Write a separate mesh for every node, even for DAG instances. |
| `-gpu` `-extGpuInstancing` | Emit `EXT_mesh_gpu_instancing` for groups of unanimated instances that share a mesh. |

### Geometry

| Flag | Description |
|---|---|
| `-sf` `-scaleFactor FLOAT` | Global scale. By default applied through a root node. |
| `-bsf` `-bakeScalingFactor` | Apply the scale factor to vertex positions instead of a root node. |
| `-mpa` `-meshPrimitiveAttributes STRING` | Attributes to export, separated by `\|`. Default: all present, from `POSITION\|NORMAL\|TANGENT\|TEXCOORD\|COLOR\|JOINTS\|WEIGHTS`. |
| `-bpa` `-blendPrimitiveAttributes STRING` | Attributes for blend shape targets. Default: `NORMAL\|TANGENT`. |
| `-eut` `-excludeUnusedTexcoord` | Drop texture coordinates on primitives with no textures. |
| `-i32` `-force32bitIndices` | Always use 32-bit indices. |
| `-mts` `-mikkelsenTangentSpace` | Compute MikkTSpace tangents instead of using Maya's. Use for meshes imported from Blender without tangents. |
| `-mta` `-mikkelsenTangentAngularThreshold FLOAT` | Angular threshold for MikkTSpace. Debugging only. |

### Materials

| Flag | Description |
|---|---|
| `-ssm` `-skipStandardMaterials` | Export only PBR materials, skipping Lambert, Phong and Blinn. |
| `-smt` `-skipMaterialTextures` | Skip all textures, useful when exporting just an animation. |
| `-dm` `-defaultMaterial` | Create a default PBR material for meshes without one. |
| `-cm` `-colorizeMaterials` | Give every material a distinct debug colour. |
| `-gof` `-globalOpacityFactor FLOAT` | Opacity multiplier applied to all materials. |

### Animation

| Flag | Description |
|---|---|
| `-acn` `-animationClipName STRING` *multiple* | Clip name. |
| `-ast` `-animationClipStartTime TIME` *multiple* | Clip start frame. |
| `-aet` `-animationClipEndTime TIME` *multiple* | Clip end frame. |
| `-afr` `-animationClipFrameRate FLOAT` *multiple* | Sampling rate in frames per second. Give once or once per clip. |
| `-ivt` `-initialValuesTime TIME` | Frame holding the rest pose and default values. Should match the skin bind pose. |
| `-pbm` `-skinUsePreBindMatrixAndMesh` | Take the bind pose from the skin cluster's pre-bind matrices and input mesh. |
| `-dsa` `-detectStepAnimations INT` | Sample extra frames to detect stepped curves, for example `-dsa 2`. |
| `-fac` `-forceAnimationChannels` | Write channels for every node, even unanimated ones. |
| `-fas` `-forceAnimationSampling` | Sample every node, even unanimated ones. |
| `-ctt` `-crt` `-cst` `-cwt` `... FLOAT` | Thresholds below which translation, rotation, scale or weight curves count as constant. |
| `-rvp` `-redrawViewport` | Redraw the viewport while sampling. Slower. |

### Deformers

| Flag | Description |
|---|---|
| `-imd` `-ignoreMeshDeformers STRING` *multiple* | Deformer to ignore. Deformers carrying a `Maya2glTF_ignored` attribute are ignored too. |
| `-ssc` `-skipSkinClusters` | Export meshes unskinned. |
| `-sbs` `-skipBlendShapes` | Export meshes without blend shapes. |
| `-rsb` `-reportSkewedInverseBindMatrices` | Report inverse bind matrices with skew. |

### Precision and debugging

| Flag | Description |
|---|---|
| `-prp` `-prd` `-prc` `-prt` `-prs` `-prm` `... FLOAT` | Rounding precision for positions, directions, colours, texture coordinates, scales and matrices. |
| `-dmy` `-dumpMaya STRING` | Dump the Maya intermediate data to a file, or `CONSOLE`. Slow on large meshes. |
| `-dgl` `-dumpGTLF STRING` | Dump the glTF JSON to a file, or `CONSOLE`. |
| `-dac` `-dumpAccessorComponents` | Dump every accessor as a text file. |
| `-dnv` `-debugNormalVectors`, `-dtv` `-debugTangentVectors`, `-dvl` `-debugVectorLength FLOAT` | Add line geometry showing normals or tangents. |
| `-cow` `-clearOutputWindow` | Clear the output window before exporting. |

## Scripting

Everything the UI does is a MEL or Python call in `Documents\maya\scripts`:

```
maya2glTF_UI();                       // open the window
maya2glTF_assignPbrShader();          // PBR shader on the selection
maya2glTF_updateActiveEnvironmentMaps();
maya2glTF_ignoreSelectedDeformers();
```

```python
import importlib, maya2glTF_mash
importlib.reload(maya2glTF_mash)
maya2glTF_mash.createMashInstances()   # MASH network in the selection
```

Batch helpers for exporting or playblasting every scene in a folder live in
`maya2glTF_exportAllScenes.mel`, `maya2glTF_exportClipScenes.mel` and
`maya2glTF_playblastClipScenes.mel`.

## Limitations

- Windows only. The macOS and Linux build scripts from the original project
  are still in the repository but are untested with Maya 2027.
- Animation is sampled per frame. There is no curve compression.
- Blend shape targets are not sparse.
- Lights are not exported.
- A transform with different rotate and scale pivots exports with the rotate
  pivot only, with a warning.
- Maya's dependency graph allows arbitrary node networks. Unusual setups may
  need the `-imd` flag or the ignore list to export cleanly.

## Building from source

Requirements:

- [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with the
  *Desktop development with C++* workload.
- [CMake](https://cmake.org/download/) 3.13 or newer on the `PATH`.
- [Git](https://git-scm.com/downloads).
- The Maya 2027 devkit from the
  [Autodesk Platform Technologies](https://aps.autodesk.com/developer/overview/maya)
  page, extracted anywhere.

Steps:

1. Clone the repository.
2. Open `build_maya2027.bat` and set `MAYA_DEVKIT_BASE` to your devkit folder
   and `MAYA_LOCATION` to your Maya install if it is not the default.
3. Close Maya and run `build_maya2027.bat`. It configures a Visual Studio
   solution in `build_2027`, builds the Release plug-in, and as a post-build
   step:
   - installs the plug-in, scripts, shelf and shader into your `Documents\maya`
     folder so you can test immediately;
   - stages a relocatable package in `build_2027\redist` containing the
     plug-in, scripts, prefs, render data and a generated
     `Maya2glTF_Installer.bat`.

For incremental rebuilds after the first run:

```
cmake --build build_2027 --config Release --target maya2glTF
```

Maya must be closed for the install step, since Windows will not overwrite a
loaded plug-in. The exported file's `generator` field records the git tag and
commit captured when CMake configured, so re-run `build_maya2027.bat` (or
CMake's configure step) before packaging a release.

### Making a release

1. Commit, then tag: `git tag v1.6.0`.
2. Run `build_maya2027.bat` so the generator string picks up the tag.
3. Zip the contents of `build_2027\redist` and attach it to a GitHub release.

### Headless testing

The plug-in, the MASH converter and the shader scripts all run under
`mayabatch`, which is handy for verification without opening the UI:

```
"C:\Program Files\Autodesk\Maya2027\bin\mayabatch.exe" -command "loadPlugin maya2glTF; polyCube; select -r pCube1; maya2glTF -of \"C:/tmp/out\" -glb;"
```

## Credits and licence

Maya2glTF was created by [IIM](https://www.iim.media) and released under the
MIT licence; see `LICENSE`. The glTF writer is a modified fork of
[COLLADA2GLTF](https://github.com/KhronosGroup/COLLADA2GLTF). The PBR shader
is ported from the Khronos glTF WebGL PBR sample.

If the exporter works for you, star the repository. If it does not, open an
issue with your Maya version, the plug-in version from the file's `generator`
field, and a scene that reproduces the problem.
