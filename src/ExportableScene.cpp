#include "externals.h"

#include "ExportableNode.h"
#include "ExportableScene.h"
#include "MayaException.h"
#include "NodeAnimation.h"
#include "Transform.h"
#include "accessors.h"
#include "ExportableMesh.h"
#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>

ExportableScene::ExportableScene(ExportableResources &resources) : m_resources(resources) {}

ExportableScene::~ExportableScene() = default;

void ExportableScene::updateCurrentValues() {
    for (auto &&pair : m_table) {
        auto &node = pair.second;
        node->updateNodeTransforms(m_currentTransformCache);
        auto *mesh = node->mesh();
        if (mesh) {
            mesh->updateWeights();
        }
    }
}

void ExportableScene::mergeRedundantShapeNodes() {
    std::set<NodeTable::key_type> redundantKeys;

    for (auto &&pair : m_table) {
        auto &node = pair.second;
        if (node->tryMergeRedundantShapeNode()) {
            redundantKeys.insert(pair.first);
        }
    }

    for (auto &&key : redundantKeys) {
        m_table.erase(key);
    }
}

ExportableNode *ExportableScene::getNode(const MDagPath &dagPath) {
    MStatus status;

    const std::string fullDagPath{dagPath.fullPathName(&status).asChar()};
    THROW_ON_FAILURE(status);

    MObject mayaNode = dagPath.node(&status);
    if (mayaNode.isNull() || status.error()) {
        cerr << "glTF2Maya: skipping '" << fullDagPath << "' as it is not a node" << endl;
        return nullptr;
    }

    auto &ptr = m_table[fullDagPath];
    if (ptr == nullptr) {
        ptr.reset(new ExportableNode(dagPath));
        ptr->load(*this, m_initialTransformCache);
    }
    return ptr.get();
}

ExportableNode *ExportableScene::getParent(ExportableNode *node) {
    auto dagPath = node->dagPath;

    ExportableNode *parentNode = nullptr;

    MDagPath parentDagPath;
    if (findLogicalParent(dagPath, parentDagPath)) {
        // Logical parent overrides Maya's parent.
        // TODO: Check for cycles!
        parentNode = getNode(parentDagPath);
    }

    // Find first ancestor that is a Maya node.
    // That will become our glTF parent.
    while (!parentNode) {
        dagPath.pop();
        if (dagPath.length() <= 0)
            break;

        parentNode = getNode(dagPath);
    }

    if (parentNode) { parentNode->children.push_back(node); }
    return parentNode;
}

void ExportableScene::getAllAccessors(AccessorsPerDagPath &accessors) {
    for (auto &&pair : m_table) {
        auto &node = pair.second;

        node->getAllAccessors(accessors[node->dagPath]);
    }
}

void ExportableScene::registerOrphanNode(ExportableNode *node) { m_orphans[node->dagPath] = node; }

// int ExportableScene::distanceToRoot(MDagPath dagPath) {
//     int distance;
//
//     // Find first ancestor node.
//     // That is our logical parent.
//     for (distance = 0; dagPath.length() > 0; ++distance) {
//         dagPath.pop();
//     }
//
//     return distance;
// }

bool ExportableScene::findLogicalParent(const MFnDagNode &childDagNode, MDagPath &parentDagPath) {
    parentDagPath = MDagPath();

    const auto childName = childDagNode.partialPathName();

    const auto logicalParentPlug = childDagNode.findPlug("Maya2glTF_LogicalParent", false);
    if (logicalParentPlug.isNull())
        return false;

    MString logicalParentName;
    if (!logicalParentPlug.getValue(logicalParentName))
        return false;

    MSelectionList selection;
    selection.add(logicalParentName);
    if (selection.length() == 0) {
        cout << prefix << "WARNING: Logical parent '" << logicalParentName.asChar() << " not found on node '"
             << childName << "'" << endl;
        return false;
    }

    if (selection.length() > 1) {
        cout << prefix << "WARNING: More than one logical parent matching '" << logicalParentName.asChar()
             << " was found on node '" << childName << "'" << endl;
        return false;
    }

    MDagPath logicalParentDagPath;
    if (!selection.getDagPath(0, logicalParentDagPath)) {
        cout << prefix << "WARNING: Failed to get DAG path of logical parent '" << logicalParentName.asChar()
             << " on node '" << childName << "'" << endl;
        return false;
    }

    parentDagPath = logicalParentDagPath;
    cout << prefix << "Found logical parent '" << logicalParentDagPath.partialPathName().asChar() << " on node '"
         << childName << "'" << endl;
    return true;
}

void ExportableScene::processGpuInstancing() {
    if (!arguments().gpuInstancing)
        return;

    cout << prefix << "Detecting GPU instancing candidates..." << endl;

    for (auto &&pair : m_table) {
        ExportableNode *parent = pair.second.get();
        if (parent->children.empty())
            continue;

        // Skip if parent already has a mesh or camera
        if (parent->hasAttachedShape())
            continue;

        // Skip if parent is animated
        if (parent->isAnimated())
            continue;

        ExportableMesh *sharedMesh = nullptr;
        bool allCompatible = true;
        std::vector<ExportableNode *> instances;

        for (auto child : parent->children) {
            // Must be a simple transform with one mesh and no children
            if (child->transformKind != TransformKind::Simple || !child->children.empty()) {
                allCompatible = false;
                break;
            }

            // Must have exactly one mesh and no camera
            if (!child->mesh() || child->camera()) {
                allCompatible = false;
                break;
            }

            // Must not be animated
            if (child->isAnimated()) {
                allCompatible = false;
                break;
            }

            if (sharedMesh == nullptr) {
                sharedMesh = child->mesh();
            } else if (sharedMesh != child->mesh()) {
                allCompatible = false;
                break;
            }

            instances.push_back(child);
        }

        // We only instance if there's more than one (though extension works for 1, it's pointless)
        if (allCompatible && instances.size() > 1) {
            cout << prefix << "Found GPU instancing candidate: '" << parent->name() << "' with " << instances.size() << " instances of '" << sharedMesh->name() << "'" << endl;

            GpuInstancedNode gin;
            gin.parent = parent;
            gin.mesh = sharedMesh;
            gin.instances = instances;

            std::vector<float> translations;
            std::vector<float> rotations;
            std::vector<float> scales;

            translations.reserve(instances.size() * 3);
            rotations.reserve(instances.size() * 4);
            scales.reserve(instances.size() * 3);

            for (auto child : instances) {
                auto &trs = child->initialTransformState.primaryTRS();
                
                translations.push_back(trs.translation[0]);
                translations.push_back(trs.translation[1]);
                translations.push_back(trs.translation[2]);

                rotations.push_back(trs.rotation[0]);
                rotations.push_back(trs.rotation[1]);
                rotations.push_back(trs.rotation[2]);
                rotations.push_back(trs.rotation[3]);

                scales.push_back(trs.scale[0]);
                scales.push_back(trs.scale[1]);
                scales.push_back(trs.scale[2]);
            }

            // Create Accessors
            std::string baseName = parent->name() + "/instances";
            
            auto tAcc = contiguousAccessor<float>(baseName + "/translation", GLTF::Accessor::Type::VEC3, GLTF::Constants::WebGL::FLOAT, static_cast<GLTF::Constants::WebGL>(-1), gsl::make_span(translations), 3);
            auto rAcc = contiguousAccessor<float>(baseName + "/rotation", GLTF::Accessor::Type::VEC4, GLTF::Constants::WebGL::FLOAT, static_cast<GLTF::Constants::WebGL>(-1), gsl::make_span(rotations), 4);
            auto sAcc = contiguousAccessor<float>(baseName + "/scale", GLTF::Accessor::Type::VEC3, GLTF::Constants::WebGL::FLOAT, static_cast<GLTF::Constants::WebGL>(-1), gsl::make_span(scales), 3);

            gin.translationAccessor = tAcc.get();
            gin.rotationAccessor = rAcc.get();
            gin.scaleAccessor = sAcc.get();

            m_gpuAccessors.push_back(std::move(tAcc));
            m_gpuAccessors.push_back(std::move(rAcc));
            m_gpuAccessors.push_back(std::move(sAcc));

            m_gpuInstancedNodes.push_back(gin);

            // COLLAPSE HIERARCHY in glTF
            auto &glParent = parent->glPrimaryNode();
            glParent.children.clear();
            sharedMesh->attachToNode(glParent);
        }
    }
}

