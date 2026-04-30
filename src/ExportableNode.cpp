#include "externals.h"
#include <maya/MAnimUtil.h>

#include "Arguments.h"
#include "DagHelper.h"
#include "ExportableNode.h"
#include "ExportableResources.h"
#include "ExportableScene.h"
#include "MayaException.h"
#include "NodeAnimation.h"
#include "Transform.h"

ExportableNode::ExportableNode(const MDagPath &dagPath) : ExportableObject(dagPath.node()), dagPath(dagPath) {}

void ExportableNode::load(ExportableScene &scene, NodeTransformCache &transformCache) {
    MStatus status;

    auto &resources = scene.resources();
    auto &args = resources.arguments();

    // Is this a joint with segment scale compensation? (the default in Maya)
    bool maybeSegmentScaleCompensation = false;
    DagHelper::getPlugValue(obj, "segmentScaleCompensate", maybeSegmentScaleCompensation);

    // Remember scale factor and precision
    scaleFactor = args.getBakeScaleFactor();
    posPrecision = args.posPrecision;
    dirPrecision = args.dirPrecision;
    sclPrecision = args.sclPrecision;

    // // Get name
    // const auto name = dagPath.partialPathName(&status);
    // THROW_ON_FAILURE(status);
    //
    // Get parent
    parentNode = scene.getParent(this);

    // Deal with segment scale compensation
    // A root joint never has segment scale compensation, since the parent is
    // the world.
    if (maybeSegmentScaleCompensation && parentNode && parentNode->obj.hasFn(MFn::kJoint) &&
        !args.ignoreSegmentScaleCompensation) {
        transformKind = TransformKind::ComplexJoint;
    }

    // Deal with pivot points.
    // We currently only support a single pivot point,
    // but Maya has both a rotation and scaling pivot,
    // so warn the user if these are different.
    MFnTransform fnTransform(dagPath, &status);
    if (status) {
        const auto scalePivot = fnTransform.scalePivot(MSpace::kObject);
        const auto rotatePivot = fnTransform.rotatePivot(MSpace::kObject);

        if (scalePivot != rotatePivot) {
            MayaException::printError(formatted("Transform '%s' has different scaling and rotation pivots, "
                                                "this is not supported, ignoring scaling pivot!",
                                                dagPath.partialPathName().asChar()),
                                      MStatus::kNotImplemented);
        }

        pivotPoint = rotatePivot;

        if (pivotPoint != MPoint::origin) {
            cout << prefix << "Transform " << dagPath.partialPathName()
                 << " has a pivot point, extra GLTF nodes will be added to "
                    "handle this"
                 << endl;

            transformKind = TransformKind::ComplexTransform;
        }
    }

    // In the presence of segment scale compensation or pivot points,
    // parent.TRS <- child.TRS
    // becomes
    // parent.TU <- parent.RS <- child.TU <- child.RS

    auto &sNode = glSecondaryNode();
    auto &pNode = glPrimaryNode();

    switch (transformKind) {
    case TransformKind::ComplexJoint:
        args.assignName(sNode, dagPath, ":SSC");
        args.assignName(pNode, dagPath, "");
        sNode.children.emplace_back(&pNode);
        break;
    case TransformKind::ComplexTransform:
        args.assignName(pNode, dagPath, ":PIV");
        args.assignName(sNode, dagPath, "");
        sNode.children.emplace_back(&pNode);
        break;
    default:;
        args.assignName(pNode, dagPath, "");
        break;
    }

    if (parentNode) {
        // Register as child
        parentNode->glPrimaryNode().children.push_back(&sNode);
    } else {
        // Register node without parent
        // We do not yet add the glNode to the glScene,
        // since we might introduce an extra global root node (typically for
        // global scaling)
        scene.registerOrphanNode(this);
    }

    // Get transform
    initialTransformState = transformCache.getTransform(this, scaleFactor, posPrecision, sclPrecision, dirPrecision);
    m_glNodes[0].transform = &initialTransformState.localTransforms[0];
    m_glNodes[1].transform = &initialTransformState.localTransforms[1];

    if (initialTransformState.maxNonOrthogonality > MAX_NON_ORTHOGONALITY) {
        // TODO: Use SVG to decompose the 3x3 matrix into a product of rotation
        // and scale matrices.
        cerr << prefix << "WARNING: node '" << name()
             << "' has initial transforms that are not representable by glTF! "
                "Skewing is not supported, use 3 nodes to simulate this. "
                "Deviation = "
             << std::fixed << std::setprecision(2) << initialTransformState.maxNonOrthogonality * 100 << "%" << endl;
    }

    // Create mesh, if any
    // Get mesh, but only if the node was selected.
    if (args.meshShapes.find(dagPath) != args.meshShapes.end()) {
        MDagPath shapeDagPath = dagPath;
        status = shapeDagPath.extendToShape();

        if (status && shapeDagPath.hasFn(MFn::kMesh)) {
            // The shape is a mesh
            auto* cachedMesh = resources.getMesh(scene, *this, shapeDagPath);
            if (cachedMesh) {
                cachedMesh->attachToNode(pNode);
            } else {
                m_mesh = std::make_unique<ExportableMesh>(scene, *this, shapeDagPath);
                m_mesh->attachToNode(pNode);
            }
        }
    }

    // Set camera, but only if the node was selected.
    if (args.cameraShapes.count(dagPath)) {
        MDagPath shapeDagPath = dagPath;
        status = shapeDagPath.extendToShape();

        if (status && shapeDagPath.hasFn(MFn::kCamera)) {
            // The shape is a camera
            m_camera = std::make_unique<ExportableCamera>(scene, *this, shapeDagPath);
            m_camera->attachToNode(pNode);
        }
    }
}

ExportableNode::~ExportableNode() = default;

std::unique_ptr<NodeAnimation>
ExportableNode::createAnimation(const Arguments &args, const ExportableFrames &frameTimes, const double scaleFactor) {

    return std::make_unique<NodeAnimation>(*this, frameTimes, scaleFactor, args);
}

void ExportableNode::updateNodeTransforms(NodeTransformCache &transformCache) {
    currentTransformState = transformCache.getTransform(this, scaleFactor, posPrecision, sclPrecision, dirPrecision);
    m_glNodes[0].transform = &currentTransformState.localTransforms[0];
    m_glNodes[1].transform = &currentTransformState.localTransforms[1];

    if (currentTransformState.maxNonOrthogonality > MAX_NON_ORTHOGONALITY) {
        // TODO: Use SVG to decompose the 3x3 matrix into a product of rotation
        // and scale matrices.
        const auto currentFrameTime = MAnimControl::currentTime();

        cerr << prefix << "WARNING: node '" << name() << "' has transforms at the current frame " << currentFrameTime
             << " that are not representable by glTF! Skewing is not "
                "supported, use 3 nodes to simulate this. Deviation = "
             << std::fixed << std::setprecision(2) << currentTransformState.maxNonOrthogonality * 100 << "%" << endl;
    }
}

bool ExportableNode::tryMergeRedundantShapeNode() {
    if (!this->hasAttachedShape())
        return false;

    if (transformKind != TransformKind::Simple)
        return false;

    if (parentNode == nullptr)
        return false;

    if (parentNode->hasAttachedShape())
        return false;

    auto &glParentNode = parentNode->glPrimaryNode();
    if (glParentNode.children.size() != 1)
        return false;

    auto &glNode = this->glPrimaryNode();

    assert(glParentNode.children.at(0) == &glNode);

    auto &transform = glNode.transform;

    if (transform->type != GLTF::Node::Transform::TRS)
        return false;

    // Check if either node is animated. Merging animated nodes is complex, so we skip it.
    if (this->isAnimated() || parentNode->isAnimated())
        return false;

    auto *parentTrs = static_cast<GLTF::Node::TransformTRS *>(glParentNode.transform);
    auto *childTrs = static_cast<const GLTF::Node::TransformTRS *>(transform);

    // Bake the child's TRS into the parent's TRS.
    MTransformationMatrix childMatrix;
    childMatrix.setTranslation(MVector(childTrs->translation[0], childTrs->translation[1], childTrs->translation[2]), MSpace::kPostTransform);
    childMatrix.setRotationQuaternion(childTrs->rotation[0], childTrs->rotation[1], childTrs->rotation[2], childTrs->rotation[3]);
    double scale[3] = {childTrs->scale[0], childTrs->scale[1], childTrs->scale[2]};
    childMatrix.setScale(scale, MSpace::kPostTransform);

    MTransformationMatrix parentMatrix;
    parentMatrix.setTranslation(MVector(parentTrs->translation[0], parentTrs->translation[1], parentTrs->translation[2]), MSpace::kPostTransform);
    parentMatrix.setRotationQuaternion(parentTrs->rotation[0], parentTrs->rotation[1], parentTrs->rotation[2], parentTrs->rotation[3]);
    double pScale[3] = {parentTrs->scale[0], parentTrs->scale[1], parentTrs->scale[2]};
    parentMatrix.setScale(pScale, MSpace::kPostTransform);

    MMatrix combined = childMatrix.asMatrix() * parentMatrix.asMatrix();
    MTransformationMatrix finalMatrix(combined);

    MVector t = finalMatrix.getTranslation(MSpace::kPostTransform);
    parentTrs->translation[0] = static_cast<float>(t.x);
    parentTrs->translation[1] = static_cast<float>(t.y);
    parentTrs->translation[2] = static_cast<float>(t.z);

    double q[4];
    finalMatrix.getRotationQuaternion(q[0], q[1], q[2], q[3]);
    parentTrs->rotation[0] = static_cast<float>(q[0]);
    parentTrs->rotation[1] = static_cast<float>(q[1]);
    parentTrs->rotation[2] = static_cast<float>(q[2]);
    parentTrs->rotation[3] = static_cast<float>(q[3]);

    double s[3];
    finalMatrix.getScale(s, MSpace::kPostTransform);
    parentTrs->scale[0] = static_cast<float>(s[0]);
    parentTrs->scale[1] = static_cast<float>(s[1]);
    parentTrs->scale[2] = static_cast<float>(s[2]);

    cout << prefix << "Shape-only node '" << name() << "' is redundant, moving its shapes to parent node '"
         << parentNode->name() << "'" << endl;

    glParentNode.children.clear();
    glNode.mesh = nullptr;
    glNode.skin = nullptr;
    glNode.camera = nullptr;

    if (m_mesh) {
        m_mesh->attachToNode(glParentNode);
        m_mesh.swap(parentNode->m_mesh);
    }

    if (m_camera) {
        m_camera->attachToNode(glParentNode);
        m_camera.swap(parentNode->m_camera);
    }

    return true;
}

void ExportableNode::getAllAccessors(std::vector<GLTF::Accessor *> &accessors) const {
    if (m_mesh) {
        m_mesh->getAllAccessors(accessors);
    }
}

bool ExportableNode::isAnimated() const {
    return MAnimUtil::isAnimated(dagPath);
}
