Maya2glTF - Exporter (2027 Edition)
High-performance glTF 2.0 exporter for Autodesk Maya
This is an updated and maintained fork of the original Maya2glTF project. With the approval of the original creators, this repository has been modernized to support Maya 2026, and 2027, featuring a redesigned UI and enhanced support for modern WebXR workflows.
🚀 Key Features in this Update
Maya 2027 Support: Fully compatible with the latest Maya releases and the LookdevX/OpenPBR shading models.
Updated UI: A consolidated interface for rapid asset identification and export.
Scale Factor Control: Dedicated -sf flag integration to ensure assets arrive in Babylon.js or Three.js at the correct real-world scale.
Enhanced Animation Manager: Easily define, name, and manage multiple animation clips for interactive browser experiences.
Flexible Ignore System: Manually or automatically ignore specific mesh deformers (blendshapes, skinclusters) to optimize export performance.
Custom Arguments: A new "Extra Flags" field allowing for the manual insertion of specialized plugin arguments without modifying code.

🛠 Installation
To install Maya2glTF for Maya 2027 (or earlier), follow these steps:
Download the Release: Grab the latest .zip from the Releases page.
Run the Installer:
Extract the contents to a permanent folder on your drive.
Double-click install_maya2glTF.bat.
This script will automatically register the plugin with Maya by creating a module file in your Documents/maya/modules folder.
Load the Plugin:
Open Maya.
Go to Windows > Settings/Preferences > Plug-in Manager.
Search for maya2glTF and check Loaded and Auto Load.
Launch: In the Maya Command Line (MEL), type: maya2glTF_UI;

📐 Professional Workflow
WebXR Compatibility
This exporter is tuned for Babylon.js and Three.js. It follows the glTF 2.0 specification strictly:
Coordinate System: Right-handed, Y-up.
Tangent Space: Uses Mikkelsen Tangent Space (-mts) for consistent normal map rendering across platforms.
Materials: Supports PBR Metallic/Roughness workflows. For best results in Maya 2027, use the integrated "Assign PBR Shader" tool.
Shading
I assume you already used something like Substance Painter to create glTF-PBR textures
Make sure you have selected OpenGL for rendering, DirectX is not supported yet 
select the polygons you want to shade
click the assign PBR shader to selection button
the first time, you need to select our PBR OpenGL shader at:
Documents\maya\maya2glTF\PBR\shaders\glTF_PBR.ogsfx
next, select all the PBR textures you want to apply in one go:
for example, for the damaged helmet model, multi-select the following textures:
Default_normal.jpg
Default_albedo.jpg
Default_AO.jpg
Default_emissive.jpg
Default_metalRoughness.jpg
now the PBR shader and all textures should be applied to your selection
by default we use the following keyword-in-filename convention to detect the kind of texture:
basecolor or albedo => base color texture
metal or _orm => metallic texture
rough or _orm => roughness texture
occl or _orm or _ao => occlusion texture
normal => tangent space normal texture
see also the -mts flag for MikkTSpace information if your models come from Blender
emissive => emissive texture
diffuse_env => Image-based-lighting (IBL) prefiltered diffuse environment map (PMREM)
specular_env => Image-based-lightning (IBL) prefiltered specular environment map (PMREM)
brdf => Bidirectional reflectance distribution function lookup table texture
you can customize these conventions, see maya2glTF_assignPbrShader.mel
all textures are optional
set the technique to transparent if desired
see the glTF PBR page page for more info.
the metallic and roughness textures are always merged into a single texture when exporting.
If you provide JPEGs, we use Maya's JPEG encoder to generate this texture. However, the default Maya JPEG encoding settings are very low quality.
The following MEL snippet sets the JPEG encoder quality:
putenv "AW_JPEG_Q_FACTOR" "92";


The following MEL code enables maximum possible JPEG quality:
putenv "AW_JPEG_Q_FACTOR" "100";
putenv "AW_JPEG_SUB_SAMPLING" "1x1,1x1,1x1";

Technical Flags Reference

_ -outputFolder (-of) STRING _(required)* * the output folder
-scaleFactor (-sf) FLOAT (optional)
scale factor to apply to the vertices
-copyright (-cpr) STRING (optional)
copyright text to be embedded in the GLTF file
-selectedNodesOnly (-sno) (optional)
only exports the directly selected nodes
by default all descendants of the selected nodes are exported too
-sceneName (-sn) STRING (optional)
the name of the glTF filename
default is Maya scene name
-binary (-glb) (optional)
exports a single glb asset file
default is a JSON glTF and binary bin file containing the buffers
-niceBufferURIs (-nbu) (optional)
removes 0 suffix from generated .bin files if only a single one is generated.
doesn't append /data to the buffer names, just used the scene name.
-hashBufferURIs (-hbu) (optional)
computes an 256-bit hash for each buffer, and uses that as the buffer name.
-externalTextures (-ext) (optional)
doesn't embed textures in the glb files.
only valid when exporting a -glb
-camera (-cam) STRING (optional, multiple)
exports camera given by name.
-initialValuesTime (-ivt) TIME (optional)
the time where the initial/default values can be found
by default frame 0 is used
this frame should match the skin bind pose
all nodes and meshes get their default transforms and weights from this time
-animationClipName (-acn) STRING (optional, multiple)
the name of the animation clip
-animationClipStartTime (-ast) TIME (optional, multiple)
the start time of the animation clip
required when exporting animation clips
-animationClipEndTime (-aet) TIME (optional, multiple)
the end time of the animation clip
required when exporting animation clips
-animationClipFrameRate (-afr) FLOAT (optional, multiple)
the frames-per-second of the animation clip
required when exporting animation clips
either you pass this for each clip, or once
-detectStepAnimations (-dsa) NUMBER (optional)
pass -dsa 2 to detect STEP "interpolations" in the sampled animations curves.
enable this e.g. when binding the shape.visiblity to node.scale.x, y z, to prevent interpolation.
currently this is all or nothing, animation curves are not yet split into discrete and continuous parts
-meshPrimitiveAttributes (-mpa) STRING (optional)
the attributes for the shapes to export, separated by a vertical bar |
by default all attributes are exported, e.g.
-mpa POSITION|NORMAL|TANGENT|TEXCOORD|COLOR|JOINTS|WEIGHTS
-blendPrimitiveAttributes (-bpa) STRING (optional)
the attributes for the blend-shapes to export, separated by a vertical bar |
by default all GLTF supported attributes are exported, e.g.
-mpa POSITION|NORMAL|TANGENT
-force32bitIndices (-i32) (optional)
forces 32-bit indices to be written to the GLTF buffers
by default 16-bit indices are used whenever possible
-disableNameAssignment (-dnn) (optional)
do not assign Maya node names to GLTF nodes
by default names are copied
-mikkelsenTangentSpace (-mts) (optional)
use the 'MikkTSpace' algoritm for computing the tangents instead of those from Maya
by default the Maya tangents are exported
if you imported meshes from Blender without importing the tangents, and you just use Maya for doing animation, you should use this flag.
-mikkelsenTangentAngularThreshold (-mta) (optional)
the angular threshold to be passed to the 'MikkTSpace' algoritm
by default 180 is passed
in general you should not use this flag, it is mainly for debugging
-skipStandardMaterials (-ssm) (optional)
do not export standard materials (lambert, phong, etc), only GLTF PBR materials.
by default standard materials are converted
but just the color and transparency is copied for now.
-excludeUnusedTexcoord (-eut) (optional)
exclude texture coordinates when the mesh primitive doesn't have textures?
by default texture coordinates are always exported
-defaultMaterial (-dm) (optional)
always generates a glTF PBR material, even if no material is assigned to a mesh in Maya
by default no default materials are generated
-colorizeMaterials (-cm) (optional)
for debugging, generate a unique PBR material with a different color for each material
by default no debug materials are generated
-dumpMaya (-dmy) STRING (optional)
dumps debugging info of the Maya objects to the given filepath argument
use the word CONSOLE to print to the Maya output window
WARNING: this can take a very long time if you have complex meshes!
by default nothing is printed
-dumpGLTF (-dgl) STRING (optional)
dumps a formatted version of the glTF asset file to the given filepath argument
use the word CONSOLE to print to the Maya output window
WARNING: this can take a very long time if you have complex meshes!
by default nothing is printed
-ignoreMeshDeformers (-imd) STRING (optional, multiple)
blend-shape or skin-cluster deformers to be ignored
by default no deformers are ignored
use this if you have deformers that are used to generate different characters, but not for animation
you can also add the custom attribute Maya2glTF_ignored (short name MGi) to the deformer to ignore it.
to maya2glTF_UI has a button to add this attribute to the selected deformer(s)
-skipSkinClusters (-ssc) (optional)
skip all skin cluster deformers, as if the mesh was not skinned
by default no skin clusters are skipped
-skipBlendShapes (-sbs) (optional)
skip all blend-shape deformers, as if the mesh was not morphed
by default no blend-shape deformers are skipped
-redrawViewport (-rvp) (optional)
redraw the viewport when exporting animation.
by default the viewport is not refreshed, since this slows down the exporter

🏗 Building from Source
If you are building for a specific Maya version, use the provided batch files:
Ensure the Maya Devkit and Visual Studio 2022 are installed.
Run build_maya2026.bat or build_maya2027.bat.
The build process will compile the .mll and stage the files in the build/redist folder.
🤝 Acknowledgments
This project is a continuation of the work started by the original Maya2glTF team. We are committed to keeping this tool alive and performant for the creative technology community.

Maintained by Gerard Baldsing JunkYardRobotBoy
